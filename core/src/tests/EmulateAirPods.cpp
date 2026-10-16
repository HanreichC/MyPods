// MyPods
// License: GPL-3.0

// Emulated AirPods Max against the real audio path: a null sink named like the headphones' bluez sink,
// AAP packets fed straight into the capabilities, everything after that is the daemon's own code.
// Then emulated AirPods Pro 3 switching between this computer and an iPhone, with MPRIS players on the session
// bus and a fake BlueZ adapter on the system bus (the container's; on a desktop org.bluez is taken).
// Needs a running PipeWire, no Bluetooth. magicpodscore --emulate-airpods

#include "device/AapDevice.h"
#include "device/BhfDevice.h"
#include "device/ZikDevice.h"
#include "device/capabilities/aap/AapAudioEffectsCapabilities.h"
#include "device/capabilities/aap/AapAudioSwitchCapability.h"
#include "device/capabilities/aap/AapEarDetectionCapability.h"
#include "dbus/DBusService.h"
#include "sdk/aap/Att.h"
#include "audio/AudioEffects.h"
#include "Logger.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>

using namespace MagicPodsCore;

namespace
{
    const std::string MAC = "AA:BB:CC:DD:EE:FF";
    const std::string BLUEZ_SINK = "bluez_output.AA_BB_CC_DD_EE_FF.1";

    std::string Sh(const std::string &cmd)
    {
        std::string out;
        if (FILE *p = popen(cmd.c_str(), "r"))
        {
            char buf[4096];
            while (size_t n = fread(buf, 1, sizeof buf, p))
                out.append(buf, n);
            pclose(p);
        }
        while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
            out.pop_back();
        return out;
    }

    bool WaitFor(const std::function<bool()> &ok, int ms = 5000)
    {
        for (int i = 0; i < ms / 100; i++, std::this_thread::sleep_for(std::chrono::milliseconds(100)))
            if (ok())
                return true;
        return ok();
    }

    // current value of a control of the running chain ("spL:Azimuth"), NAN if there is none
    double Param(const std::string &name)
    {
        auto dump = nlohmann::json::parse(Sh("pw-dump " + std::string(AudioEffects::SINK_NAME) + " 2>/dev/null"), nullptr, false);
        double value = NAN;
        if (!dump.is_array())
            return value;
        for (auto &obj : dump)
            for (auto &props : obj["info"]["params"]["Props"])
                if (props.contains("params"))
                    for (size_t i = 0; i + 1 < props["params"].size(); i += 2)
                        if (props["params"][i] == name)
                            value = props["params"][i + 1].get<double>();
        return value;
    }

    std::pair<double, double> Azimuths()
    {
        return {Param("spL:Azimuth"), Param("spR:Azimuth")};
    }

    // AAP head-tracking packet (opcode 0x17) with the two orientation fields LibrePods reads
    std::vector<unsigned char> HeadPacket(int16_t o2, int16_t o3)
    {
        std::vector<unsigned char> p(80, 0);
        p[0] = 0x04; p[2] = 0x04; p[4] = 0x17; p[8] = 0x10;
        p[45] = o2 & 0xFF; p[46] = (o2 >> 8) & 0xFF;
        p[47] = o3 & 0xFF; p[48] = (o3 >> 8) & 0xFF;
        return p;
    }

    struct EmulatedPods : AapDevice
    {
        EmulatedPods(std::shared_ptr<DBusDeviceInfo> info, std::shared_ptr<PulseAudioClient> pac, std::shared_ptr<SettingsService> settings)
            : AapDevice(info, pac, settings, nullptr)
        {
            _client = Client::CreateL2CAP(MAC, 0x1001); // never started: sent packets just queue
        }
        void Receive(const std::vector<unsigned char> &packet) { GetResponseDataRecived().FireEvent(packet); }

        std::mutex sentLock;
        std::vector<std::vector<unsigned char>> sent;
        void SendData(const std::vector<unsigned char> &data) override
        {
            std::lock_guard lock{sentLock};
            sent.push_back(data);
        }
        bool Sent(const std::vector<unsigned char> &packet)
        {
            std::lock_guard lock{sentLock};
            return std::ranges::find(sent, packet) != sent.end();
        }
        void ClearSent()
        {
            std::lock_guard lock{sentLock};
            sent.clear();
        }
    };

    // A media player on the session bus, as Spotify or a browser shows itself (MPRIS)
    struct FakePlayer
    {
        static constexpr const char *PLAYER = "org.mpris.MediaPlayer2.Player";
        std::unique_ptr<sdbus::IConnection> bus;
        std::unique_ptr<sdbus::IObject> object;
        std::mutex lock;
        std::string status = "Stopped";
        std::atomic<int> plays{0}, pauses{0};

        explicit FakePlayer(const std::string &name)
        {
            bus = sdbus::createSessionBusConnection(name);
            object = sdbus::createObject(*bus, "/org/mpris/MediaPlayer2");
            object->registerMethod("Play").onInterface(PLAYER).implementedAs([this] { plays++; Set("Playing"); });
            object->registerMethod("Pause").onInterface(PLAYER).implementedAs([this] { pauses++; Set("Paused"); });
            object->registerProperty("PlaybackStatus").onInterface(PLAYER).withGetter([this] { std::lock_guard l{lock}; return status; });
            object->finishRegistration();
            bus->enterEventLoopAsync();
        }
        ~FakePlayer()
        {
            bus->leaveEventLoop(); // no Play/Pause runs into the object while it goes
        }
        // the user presses play or pause in the player itself
        void Set(const std::string &newStatus)
        {
            {
                std::lock_guard l{lock};
                status = newStatus;
            }
            object->emitPropertiesChangedSignal(PLAYER, std::vector<std::string>{"PlaybackStatus"});
        }
        std::string Status()
        {
            std::lock_guard l{lock};
            return status;
        }
    };

    // bluetoothd's adapter, as far as the daemon asks it: its address and name
    struct FakeBluez
    {
        std::unique_ptr<sdbus::IConnection> bus;
        std::unique_ptr<sdbus::IObject> root, adapter; // destroyed before the bus they live on

        explicit FakeBluez(const std::string &address)
        {
            bus = sdbus::createSystemBusConnection("org.bluez");
            root = sdbus::createObject(*bus, "/");
            root->addObjectManager();
            adapter = sdbus::createObject(*bus, "/org/bluez/hci0");
            adapter->registerProperty("Address").onInterface("org.bluez.Adapter1").withGetter([address] { return address; });
            adapter->registerProperty("Alias").onInterface("org.bluez.Adapter1").withGetter([] { return std::string("chris-p1"); });
            adapter->registerProperty("Powered").onInterface("org.bluez.Adapter1").withGetter([] { return true; });
            adapter->finishRegistration();
            bus->enterEventLoopAsync();
        }
        ~FakeBluez()
        {
            bus->leaveEventLoop(); // no call runs into the objects while they go
            adapter.reset();
            root.reset();
        }
    };

    // An AAP packet from the AirPods: header, opcode, body
    std::vector<unsigned char> Aap(unsigned char opcode, std::vector<unsigned char> body)
    {
        std::vector<unsigned char> packet{0x04, 0x00, 0x04, 0x00, opcode, 0x00};
        packet.insert(packet.end(), body.begin(), body.end());
        return packet;
    }

    std::vector<unsigned char> MacBytes(const std::string &mac, bool reversed)
    {
        std::vector<unsigned char> bytes;
        for (size_t i = 0; i < 17; i += 3)
            bytes.push_back(static_cast<unsigned char>(std::stoi(mac.substr(i, 2), nullptr, 16)));
        if (reversed)
            std::ranges::reverse(bytes);
        return bytes;
    }

    int failures = 0;
    void Check(const char *name, bool ok, const std::string &detail = "")
    {
        failures += !ok;
        Logger::Info("%-55s %s %s", name, ok ? "OK" : "FAILED", detail.c_str());
    }
}

int EmulateAirPods()
{
    std::string previousDefault = Sh("pactl get-default-sink");
    std::string module = Sh("pactl load-module module-null-sink sink_name=" + BLUEZ_SINK + " sink_properties=device.description=Emulated-AirPods");
    if (module.empty())
    {
        Logger::Error("Emulator: pactl could not create the fake headphones sink");
        return 1;
    }
    auto settingsPath = std::filesystem::temp_directory_path() / "mypods-emulate.toml";
    std::filesystem::remove(settingsPath);

    {
        std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces{{"org.bluez.Device1", {
            {"Address", sdbus::Variant{MAC}},
            {"Name", sdbus::Variant{std::string("AirPods Max (emuliert)")}},
            {"Modalias", sdbus::Variant{std::string("bluetooth:v004Cp200Ad0001")}},
            {"Connected", sdbus::Variant{true}},
        }}};
        auto info = std::make_shared<DBusDeviceInfo>(sdbus::ObjectPath{"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"}, interfaces);
        EmulatedPods pods{info, std::make_shared<PulseAudioClient>(), std::make_shared<SettingsService>(settingsPath.string())};
        pods.SaveSettingInt("spatialAudio", 1); // "Fixed" from an earlier session, the daemon (re)starts with the headphones connected
        AapSpatialAudioCapability spatial{pods};
        CmnEqualizerCapability equalizer{pods};
        std::string sink;
        auto defaultSinkIs = [&](const std::string &name) { return WaitFor([&] { return (sink = Sh("pactl get-default-sink")) == name; }); };

        pods.Receive({0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x03}); // any AAP packet: the headphones talk
        auto json = spatial.GetAsJson();
        Check("Spatial audio offered, with head tracking", json.contains("spatialAudio") && json["spatialAudio"]["headTracking"] == true &&
                                                            json["spatialAudio"]["selected"] == 1 && json["spatialAudio"]["readonly"] == false, json.dump());
        Check("Saved Fixed: applied when the headphones start talking", defaultSinkIs(AudioEffects::SINK_NAME), sink);
        Check("Fixed: chain plays into the headphones", WaitFor([] { return Sh("pw-link -l").find(BLUEZ_SINK + ":playback_FL\n  |<- mypods_fx.out:output_FL") != std::string::npos; }));
        auto az = Azimuths();
        Check("Fixed: speakers at +-30 degrees", std::abs(az.first - 30) < 0.5 && std::abs(az.second - 330) < 0.5, std::to_string(az.first) + " " + std::to_string(az.second));

        spatial.SetFromJson({{"spatialAudio", {{"selected", 2}}}});
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (int i = 0; i < 10; i++)
            pods.Receive(HeadPacket(0, 0)); // calibration: looking at the screen
        for (int i = 0; i < 10; i++, std::this_thread::sleep_for(std::chrono::milliseconds(60)))
            pods.Receive(HeadPacket(8000, -8000)); // turned by 45 degrees by the LibrePods formula
        bool turned = WaitFor([&] { az = Azimuths(); return std::abs(az.first - 75) < 3 && std::abs(az.second - 15) < 3; }, 2000);
        Check("Head tracked: speakers turn against the head", turned, std::to_string(az.first) + " " + std::to_string(az.second));

        equalizer.SetFromJson({{"equalizer", {{"selected", "Bass Booster"}}}});
        Check("EQ change keeps the chain as default sink", defaultSinkIs(AudioEffects::SINK_NAME), sink);
        equalizer.SetFromJson({{"equalizer", {{"selected", "Off"}}}});
        equalizer.SetFromJson({{"equalizer", {{"correction", true}}}});
        Check("Correction on: live in the running chain", WaitFor([] { return std::abs(Param("coL0:Gain") + 3.0) < 0.01; }, 2000),
              std::to_string(Param("coL0:Gain")));

        spatial.SetFromJson({{"spatialAudio", {{"selected", 0}}}});
        Check("Spatial off, correction on: chain stays", defaultSinkIs(AudioEffects::SINK_NAME), sink);
        equalizer.SetFromJson({{"equalizer", {{"crossfeed", true}}}});
        Check("Crossfeed on: cross path live", WaitFor([] { return std::abs(Param("xmL:Gain 3") - 0.37) < 0.01; }, 2000),
              std::to_string(Param("xmL:Gain 3")));
        equalizer.SetFromJson({{"equalizer", {{"crossfeed", false}}}});
        equalizer.SetFromJson({{"equalizer", {{"correction", false}}}});
        Check("Correction off: flat chain keeps playing", WaitFor([] { return Param("coL0:Gain") == 0 && Param("xmL:Gain 3") == 0; }, 2000));
        if (!AudioEffects::FindLimiter().empty())
            Check("Limiter: loaded at the end of the chain", std::abs(Param("lim:Limit (dB)") + 1) < 0.01, std::to_string(Param("lim:Limit (dB)")));

        equalizer.SetFromJson({{"equalizer", {{"loudness", true}}}});
        Sh("pactl set-sink-volume " + BLUEZ_SINK + " 30%");
        Check("Loudness: the bass comes up when the volume goes down", WaitFor([] { return Param("ldL0:Gain") > 5; }, 3000), std::to_string(Param("ldL0:Gain")));
        Sh("pactl set-sink-volume " + BLUEZ_SINK + " 100%");
        Check("Loudness: flat at full volume", WaitFor([] { return Param("ldL0:Gain") == 0; }, 3000), std::to_string(Param("ldL0:Gain")));
        AudioEffects::Instance().SetVolume("bluez_output.11_22_33_44_55_66", 0.1); // a second pair, turned down, without the chain
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        Check("Loudness: another pair's volume leaves the chain alone", Param("ldL0:Gain") == 0, std::to_string(Param("ldL0:Gain")));
        equalizer.SetFromJson({{"equalizer", {{"loudness", false}}}});

        equalizer.SetFromJson({{"equalizer", {{"audiogramRight", "0 0 0 0 0 40"}}}});
        equalizer.SetFromJson({{"equalizer", {{"hearing", true}}}});
        Check("Hearing profile: right ear only", WaitFor([] { return Param("hlR5:Gain") == 20 && Param("hlL5:Gain") == 0; }, 2000),
              std::to_string(Param("hlR5:Gain")));

        equalizer.SetFromJson({{"equalizer", {{"selected", "Bass Booster"}}}});
        WaitFor([] { return Param("eqL0:Gain") == 5.5; }, 2000);
        double pre = Param("preL:Gain");
        equalizer.SetFromJson({{"equalizer", {{"bypass", true}}}});
        Check("A/B: effects off, pre-gain kept", WaitFor([&] { return Param("eqL0:Gain") == 0 && Param("hlR5:Gain") == 0 && Param("preL:Gain") == pre; }, 2000),
              std::to_string(pre));
        equalizer.SetFromJson({{"equalizer", {{"bypass", false}}}});
        Check("A/B: effects back", WaitFor([] { return Param("eqL0:Gain") == 5.5; }, 2000));

        // the user's own ParametricEQ.txt replaces the built-in correction (two filters instead of ten: the chain restarts)
        auto eqFile = std::filesystem::temp_directory_path() / "mypods-emulate-eq.txt";
        std::ofstream(eqFile) << "Preamp: -4 dB\nFilter 1: ON LSC Fc 105 Hz Gain 4.0 dB Q 0.70\nFilter 2: ON PK Fc 3000 Hz Gain -2.0 dB Q 2.00\n";
        pods.SaveSettingString("eqFile", eqFile.string());
        equalizer.SetFromJson({{"equalizer", {{"correction", true}}}});
        Check("ParametricEQ.txt: its filters in the chain", WaitFor([] { return Param("coL0:Gain") == 4 && Param("coR1:Gain") == -2 && std::isnan(Param("coL2:Gain")); }, 3000),
              std::to_string(Param("coL0:Gain")));
        // another file with as many filters: the live update would only set the gains, the frequencies must follow too
        std::ofstream(eqFile) << "Filter 1: ON LSC Fc 105 Hz Gain 4.0 dB Q 0.70\nFilter 2: ON PK Fc 6000 Hz Gain -2.0 dB Q 2.00\n";
        equalizer.SetFromJson({{"equalizer", {{"correction", true}}}});
        Check("ParametricEQ.txt swapped, same filter count: new frequencies", WaitFor([] { return Param("coR1:Freq") == 6000; }, 3000),
              std::to_string(Param("coR1:Freq")));
        std::filesystem::remove(eqFile);
        pods.SaveSettingString("eqFile", "");

        spatial.SetFromJson({{"spatialAudio", {{"surround", true}}}});
        spatial.SetFromJson({{"spatialAudio", {{"selected", 1}}}});
        Check("Surround: the chain takes 7.1", WaitFor([] { return Sh("pactl list short sinks | grep 'mypods_fx\\s'").find("8ch") != std::string::npos; }, 3000),
              Sh("pactl list short sinks | grep mypods_fx"));
        Check("Surround: side and rear speakers placed", std::abs(Param("spSL:Azimuth") - 90) < 0.5 && std::abs(Param("spRR:Azimuth") - 225) < 0.5,
              std::to_string(Param("spSL:Azimuth")) + " " + std::to_string(Param("spRR:Azimuth")));
        Check("Surround: still plays into the headphones", WaitFor([] { return Sh("pw-link -l").find(BLUEZ_SINK + ":playback_FL\n  |<- mypods_fx.out:output_FL") != std::string::npos; }));
        spatial.SetFromJson({{"spatialAudio", {{"surround", false}}}});
        spatial.SetFromJson({{"spatialAudio", {{"selected", 0}}}});
        equalizer.SetFromJson({{"equalizer", {{"selected", "Off"}}}});
        equalizer.SetFromJson({{"equalizer", {{"correction", false}}}});
        equalizer.SetFromJson({{"equalizer", {{"hearing", false}}}});
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // detached routing threads finish before the device goes

        AudioEffects::Instance().Stop();
        pods.RouteAudio();
        Check("Off: headphones are the default sink again", defaultSinkIs(BLUEZ_SINK), sink);
        AudioEffects::Instance().Stop();
    }

    // Generic headphones (no vendor protocol, just A2DP) get the same effects on this computer
    std::filesystem::remove(settingsPath);
    {
        std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces{{"org.bluez.Device1", {
            {"Address", sdbus::Variant{MAC}},
            {"Name", sdbus::Variant{std::string("HW-BT (emuliert)")}},
            {"Connected", sdbus::Variant{true}},
        }}};
        auto info = std::make_shared<DBusDeviceInfo>(sdbus::ObjectPath{"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"}, interfaces);
        auto settings = std::make_shared<SettingsService>(settingsPath.string());
        settings->SaveSetting("AA_BB_CC_DD_EE_FF", "equalizer", std::string("Bass Booster")); // from an earlier session
        std::string sink;
        auto defaultSinkIs = [&](const std::string &name) { return WaitFor([&] { return (sink = Sh("pactl get-default-sink")) == name; }); };

        auto headphones = BhfDevice::Create(info, std::make_shared<PulseAudioClient>(), settings);
        Check("Generic: saved EQ applied when the daemon starts", defaultSinkIs(AudioEffects::SINK_NAME) && Param("eqL0:Gain") == 5.5, sink);

        // Picked in the sidebar while the sound plays elsewhere: it comes back through the chain, and the client sees
        // the default change (what DevicesInfoFetcher::RefreshOutput marks the headphones by)
        auto pac = std::make_shared<PulseAudioClient>();
        std::atomic<int> sinkEvents{0};
        auto eventId = pac->GetSinkChangedEvent().Subscribe([&](size_t, const uint32_t &) { sinkEvents++; });
        std::string speakers = Sh("pactl load-module module-null-sink sink_name=mypods_speakers");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        sinkEvents = 0;
        Sh("pactl set-default-sink mypods_speakers");
        Check("Output elsewhere: the client reports the speakers", WaitFor([&] { return pac->GetDefaultSink() == "mypods_speakers"; }));
        Check("Output elsewhere: the default change is an event", WaitFor([&] { return sinkEvents > 0; }));
        headphones->MakeDefaultOutput();
        Check("Picked: the headphones play again, through the chain", defaultSinkIs(AudioEffects::SINK_NAME) &&
              pac->GetDefaultSink() == AudioEffects::SINK_NAME && AudioEffects::Instance().PlaysInto(headphones->SinkPart()), sink);
        pac->GetSinkChangedEvent().Unsubscribe(eventId);
        Sh("pactl unload-module " + speakers);
        auto json = headphones->GetAsJson()["capabilities"];
        Check("Generic: spatial audio without head tracking, EQ", json.contains("spatialAudio") && json["spatialAudio"]["headTracking"] == false &&
                                                                  json.contains("equalizer") && !json["equalizer"].contains("correction"), json.dump());
        headphones->SetCapabilities({{"spatialAudio", {{"selected", 2}}}});
        Check("Generic: head tracked refused", headphones->GetAsJson()["capabilities"]["spatialAudio"]["selected"] == 0);
        headphones->SetCapabilities({{"spatialAudio", {{"selected", 1}}}});
        auto az = Azimuths();
        Check("Generic: fixed speakers at +-30 degrees", WaitFor([&] { az = Azimuths(); return std::abs(az.first - 30) < 0.5 && std::abs(az.second - 330) < 0.5; }),
              std::to_string(az.first) + " " + std::to_string(az.second));

        headphones->SetCapabilities({{"equalizer", {{"custom", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}}}}});
        Check("Generic: custom bands live in the chain", WaitFor([] { return Param("eqL9:Gain") == 10 && Param("eqR0:Gain") == 1; }, 2000),
              std::to_string(Param("eqL9:Gain")));
        headphones->SetCapabilities({{"equalizer", {{"tilt", 4}}}});
        Check("Generic: tilt live in the chain", WaitFor([] { return Param("tlR1:Gain") == 4 && Param("tlL0:Gain") == -4; }, 2000),
              std::to_string(Param("tlR1:Gain")));
        auto eq = headphones->GetAsJson()["capabilities"]["equalizer"];
        Check("Generic: the curve follows bands and tilt", eq["selected"] == "Custom" && eq["bands"][9] == 10 && eq["tilt"] == 4 &&
                                                           eq["response"]["left"].size() == 40 && std::ranges::max(eq["response"]["left"].get<std::vector<double>>()) > 11,
              eq["selected"].dump() + " " + eq["bands"].dump() + " " + eq["tilt"].dump() + " " + eq["response"]["left"].dump());
        headphones->SetCapabilities({{"equalizer", {{"tilt", 0}}}});

        info->GetConnectionStatus().SetValue(false);
        Check("Generic: disconnected, chain gone and settings hidden", WaitFor([] { return Sh("pactl list short sinks | grep 'mypods_fx\\s'").empty(); }) &&
                                                                       !headphones->GetAsJson()["capabilities"].contains("equalizer"), Sh("pactl list short sinks"));
        info->GetConnectionStatus().SetValue(true);
        headphones->SetCapabilities({{"spatialAudio", {{"selected", 0}}}});
        headphones->SetCapabilities({{"equalizer", {{"selected", "Off"}}}});
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // detached routing threads finish before the device goes
        Check("Generic: effects off, headphones are the default sink again", defaultSinkIs(BLUEZ_SINK), sink);

        // effects switched off leave a flat chain as the default sink; it has to go with the headphones
        headphones->SetCapabilities({{"equalizer", {{"crossfeed", true}}}});
        Check("Generic: crossfeed starts the chain", defaultSinkIs(AudioEffects::SINK_NAME), sink);
        headphones->SetCapabilities({{"equalizer", {{"crossfeed", false}}}});
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // detached routing thread
        Check("Generic: crossfeed off, the flat chain stays", Sh("pactl get-default-sink") == AudioEffects::SINK_NAME);
        info->GetConnectionStatus().SetValue(false);
        Check("Generic: disconnected with a flat chain, the chain goes", WaitFor([] { return Sh("pactl list short sinks | grep 'mypods_fx\\s'").empty(); }),
              Sh("pactl list short sinks"));
        info->GetConnectionStatus().SetValue(true);
        AudioEffects::Instance().Stop();
    }

    // Parrot Zik: its own 5-band equalizer on the headphones, "Custom" included (no audio path, the answers are fed in)
    std::filesystem::remove(settingsPath);
    {
        std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces{{"org.bluez.Device1", {
            {"Address", sdbus::Variant{MAC}},
            {"Name", sdbus::Variant{std::string("Zik 2.0 (emuliert)")}},
            {"Connected", sdbus::Variant{false}}, // no RFCOMM attempts; requests just queue
        }}};
        auto info = std::make_shared<DBusDeviceInfo>(sdbus::ObjectPath{"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"}, interfaces);
        auto zik = ZikDevice::Create(info, std::make_shared<PulseAudioClient>(), std::make_shared<SettingsService>(settingsPath.string()));
        auto answer = [&](bool on) { zik->GetAnswerEvent().FireEvent(std::string(R"(<answer path="/api/audio/equalizer/enabled/get"><audio><equalizer enabled=")") +
                                                                     (on ? "true" : "false") + R"("/></audio></answer>)"); };
        answer(false);
        auto eq = zik->GetAsJson()["capabilities"]["equalizer"];
        Check("Zik: Custom offered, 5 bands", eq["options"].back() == "Custom" && eq["frequencies"].size() == 5 && eq["bands"].size() == 5, eq.dump());
        zik->SetCapabilities({{"equalizer", {{"selected", "Bass Booster"}}}});
        zik->SetCapabilities({{"equalizer", {{"selected", "Custom"}}}});
        answer(true);
        eq = zik->GetAsJson()["capabilities"]["equalizer"];
        Check("Zik: the first Custom starts from the preset", eq["selected"] == "Custom" && eq["bands"][0] == 9.75 && eq["bands"][1] == 6.0, eq.dump());
        zik->SetCapabilities({{"equalizer", {{"custom", {3, -1.5, 0, 12, -12}}}}});
        eq = zik->GetAsJson()["capabilities"]["equalizer"];
        Check("Zik: own bands kept", eq["selected"] == "Custom" && eq["bands"] == nlohmann::json({3.0, -1.5, 0.0, 12.0, -12.0}), eq.dump());
        zik->SetCapabilities({{"equalizer", {{"custom", {3, 0, 0, 0, 13}}}}});
        Check("Zik: out of range refused", zik->GetAsJson()["capabilities"]["equalizer"]["bands"][4] == -12.0);
    }

    // AirPods Pro 3 between this computer and an iPhone ("Connect to This Mac: Automatically"): what plays gets the
    // AirPods. The iPhone's side is the AAP traffic the AirPods relay; the players are real MPRIS players on the bus.
    std::filesystem::remove(settingsPath);
    const std::string LOCAL = "10:20:30:40:50:60", IPHONE = "A1:B2:C3:D4:E5:F6";
    std::unique_ptr<FakeBluez> bluez;
    try
    {
        bluez = std::make_unique<FakeBluez>(LOCAL);
        DBusService service; // finds the adapter, whose address and name the switching asks for
    }
    catch (const std::exception &e)
    {
        Check("Handoff: fake BlueZ on the system bus", false, e.what());
    }
    if (bluez)
    {
        std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces{{"org.bluez.Device1", {
            {"Address", sdbus::Variant{MAC}},
            {"Name", sdbus::Variant{std::string("AirPods Pro (emuliert)")}},
            {"Modalias", sdbus::Variant{std::string("bluetooth:v004Cp2027d0001")}},
            {"Connected", sdbus::Variant{true}},
        }}};
        auto info = std::make_shared<DBusDeviceInfo>(sdbus::ObjectPath{"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"}, interfaces);
        EmulatedPods pods{info, std::make_shared<PulseAudioClient>(), std::make_shared<SettingsService>(settingsPath.string())};
        AapAudioSwitchCapability handoff{pods};
        AapEarDetectionCapability ear{pods};
        FakePlayer music{"org.mpris.MediaPlayer2.spotify"};
        FakePlayer phone{"org.mpris.MediaPlayer2.kdeconnect.mpris_iphone"}; // KDE Connect's mirror of the phone's player
        std::string speakers = Sh("pactl load-module module-null-sink sink_name=mypods_speakers");
        auto inEar = [](unsigned char primary, unsigned char secondary) { return Aap(0x06, {primary, secondary}); };
        auto source = [&](int type) { auto body = MacBytes(IPHONE, true); body.push_back(static_cast<unsigned char>(type)); return Aap(0x0E, body); };
        auto owns = [](bool yes) { return Aap(0x09, {0x06, static_cast<unsigned char>(yes ? 0x01 : 0x00), 0x00, 0x00, 0x00}); };
        auto iphoneTakes = [&]
        {
            std::string text = "Hijackv2 audioRoutingSetOwnershipToFalse btName iPhone von Chris";
            auto body = MacBytes(IPHONE, true);
            body.insert(body.end(), {static_cast<unsigned char>(text.size()), 0x00});
            body.insert(body.end(), text.begin(), text.end());
            return Aap(0x11, body);
        };
        // PC playback starts and the takeover runs to its end (it resumes the player twice, 1.5 s apart)
        auto playHere = [&]
        {
            int before = music.plays;
            music.Set("Paused");
            music.Set("Playing");
            return WaitFor([&] { return music.plays >= before + 2; }, 6000);
        };
        // ... or doesn't take them: playback starts and nothing happens
        auto startHere = [&]
        {
            music.Set("Paused");
            music.Set("Playing");
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        };
        auto json = [&] { return handoff.GetAsJson()["autoSwitch"]; };

        pods.Receive(Aap(0x2E, [&] {
            std::vector<unsigned char> body{0x00, 0x00, 0x02};
            for (auto *mac : {&LOCAL, &IPHONE})
            {
                auto bytes = MacBytes(*mac, false);
                body.insert(body.end(), bytes.begin(), bytes.end());
                body.insert(body.end(), {0x01, 0x01});
            }
            return body;
        }()));
        pods.Receive(Aap(0x09, {0x0A, 0x01, 0x00, 0x00, 0x00})); // ear detection on
        pods.Receive(inEar(0x00, 0x00));
        Check("Pro 3: both worn", pods.podsInEar == 2 && json()["owns"] == true, json().dump());

        Sh("pactl set-default-sink mypods_speakers");
        bool took = playHere();
        Check("Pro 3: playback here takes the AirPods", took && pods.ownsAudio && music.Status() == "Playing" &&
                                                        pods.Sent(AapAudioSwitchCapability::OwnsConnection(true)), music.Status());
        Check("Pro 3: takeover asks the iPhone, never this computer", pods.Sent(AapAudioSwitchCapability::HijackRequest(IPHONE)) &&
                                                                    pods.Sent(AapAudioSwitchCapability::ShowNearbyUI(IPHONE)) &&
                                                                    pods.Sent(AapAudioSwitchCapability::MediaInformation(IPHONE, LOCAL, "chris-p1")) &&
                                                                    !pods.Sent(AapAudioSwitchCapability::HijackRequest(LOCAL)));
        Check("Pro 3: the AirPods are the output", WaitFor([] { return Sh("pactl get-default-sink") == BLUEZ_SINK; }), Sh("pactl get-default-sink"));

        pods.Receive(source(2)); // the iPhone's last report before it stopped
        Check("Pro 3: stale report right after the takeover ignored", pods.ownsAudio);

        std::this_thread::sleep_for(std::chrono::milliseconds(3100));
        pods.ClearSent();
        pods.Receive(iphoneTakes());
        Check("Pro 3: the iPhone takes them, this computer lets go", !pods.ownsAudio && pods.Sent(AapAudioSwitchCapability::OwnsConnection(false)) &&
                                                                    json()["source"] == "iPhone" && json()["owns"] == false, json().dump());

        // during the takeover the AirPods still report the iPhone; that must not bounce the audio back
        pods.ClearSent();
        int before = music.plays;
        music.Set("Paused");
        music.Set("Playing");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        pods.Receive(source(2));
        pods.Receive(owns(false));
        WaitFor([&] { return music.plays >= before + 2; }, 6000);
        Check("Pro 3: reports during the takeover don't bounce it back", pods.ownsAudio && pods.Sent(AapAudioSwitchCapability::HijackRequest(IPHONE)));
        pods.Receive(owns(false));
        Check("Pro 3: nor in the 3 s after it", pods.ownsAudio);
        std::this_thread::sleep_for(std::chrono::milliseconds(3100));
        pods.Receive(owns(false));
        Check("Pro 3: the AirPods say another device owns them: let go", !pods.ownsAudio);

        // Apple never steals a call
        pods.Receive(source(1));
        pods.ClearSent();
        startHere();
        Check("Pro 3: a call on the iPhone keeps them there", !pods.ownsAudio && !pods.Sent(AapAudioSwitchCapability::OwnsConnection(true)));
        pods.Receive(source(0));

        // nobody wears them: playing here stays on the speakers, and ear detection leaves the player alone
        int pauses = music.pauses;
        pods.Receive(inEar(0x01, 0x01));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        Check("Pro 3: taken out while the iPhone has them, nothing paused here", music.pauses == pauses && music.Status() == "Playing");
        startHere();
        Check("Pro 3: not worn, not taken", !pods.ownsAudio && !pods.Sent(AapAudioSwitchCapability::OwnsConnection(true)));
        pods.Receive(inEar(0x00, 0x00));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        Check("Pro 3: put back in while the iPhone has them, nothing resumed here", music.pauses == pauses);

        // "When Last Connected to This Mac": only by hand
        handoff.SetFromJson({{"autoSwitch", {{"selected", 1}}}});
        startHere();
        Check("Pro 3: switching by hand only, playback doesn't take them", !pods.ownsAudio);
        before = music.plays;
        handoff.SetFromJson({{"autoSwitch", {{"takeover", true}}}});
        Check("Pro 3: \"Move here\" takes them", WaitFor([&] { return pods.ownsAudio.load() && pods.Sent(AapAudioSwitchCapability::HijackRequest(IPHONE)); }, 6000));
        handoff.SetFromJson({{"autoSwitch", {{"selected", 0}}}});
        WaitFor([&] { return music.plays >= before + 2; }, 6000);
        std::this_thread::sleep_for(std::chrono::milliseconds(3100));
        pods.Receive(iphoneTakes());

        // the phone's own player, mirrored by KDE Connect, is the iPhone playing: neither a takeover nor paused
        pods.ClearSent();
        music.Set("Paused");
        phone.Set("Playing");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        Check("Pro 3: the phone's mirrored player doesn't pull them back", !pods.ownsAudio && !pods.Sent(AapAudioSwitchCapability::OwnsConnection(true)));
        playHere();
        Check("Pro 3: taking them over pauses only this computer's players", pods.ownsAudio && phone.pauses == 0 && phone.Status() == "Playing",
              std::to_string(phone.pauses));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // detached workers finish before the device goes
        Sh("pactl unload-module " + speakers);
    }

    // AirPods Pro 3 as the daemon builds them (AapDevice::Create), told what a pair reports after connecting:
    // every setting the model has must be offered, and nothing of another model's
    std::filesystem::remove(settingsPath);
    if (bluez)
    {
        std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces{{"org.bluez.Device1", {
            {"Address", sdbus::Variant{MAC}},
            {"Name", sdbus::Variant{std::string("AirPods Pro von Chris")}},
            {"Modalias", sdbus::Variant{std::string("bluetooth:v004Cp2027d0001")}},
            {"Connected", sdbus::Variant{true}},
        }}};
        auto info = std::make_shared<DBusDeviceInfo>(sdbus::ObjectPath{"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"}, interfaces);
        auto pro3 = AapDevice::Create(info, std::make_shared<PulseAudioClient>(), std::make_shared<SettingsService>(settingsPath.string()), nullptr);
        auto feed = [&](const std::vector<unsigned char> &packet) { pro3->GetResponseDataRecived().FireEvent(packet); };
        feed(Aap(0x04, {0x03, 0x02, 0x01, 87, 0x02, 0x01, 0x04, 0x01, 84, 0x02, 0x01, 0x08, 0x01, 62, 0x01, 0x01}));
        feed(Aap(0x06, {0x00, 0x00}));
        // control commands: adaptive, its level, CA, ANC with one AirPod, press speed and hold, volume swipe and its
        // length, adaptive volume, tone volume, mute/end call, press-and-hold modes, "Off" not allowed, microphone,
        // hearing aid enrolled but off, sleep detection, connect automatically, ear detection
        for (auto control : std::vector<std::vector<unsigned char>>{
                 {0x0D, 0x04}, {0x2E, 50}, {0x28, 0x01}, {0x1B, 0x01}, {0x17, 0x00}, {0x18, 0x00}, {0x25, 0x01}, {0x23, 0x00},
                 {0x26, 0x01}, {0x1F, 80}, {0x24, 0x21, 0x22, 0x03}, {0x1A, 0x0E}, {0x34, 0x02}, {0x01, 0x00}, {0x2C, 0x01, 0x02},
                 {0x35, 0x01}, {0x20, 0x01}, {0x0A, 0x01}})
        {
            control.resize(5);
            feed(Aap(0x09, control));
        }
        std::vector<unsigned char> about{0x00, 0x00, 0x00, 0x00, 0x00};
        for (std::string field : {"AirPods Pro von Chris", "A3064", "Apple Inc.", "H3XYZ0000000", "8A356"})
            about.insert(about.end(), field.c_str(), field.c_str() + field.size() + 1);
        feed(Aap(0x1D, about));
        pro3->GetAttValueEvent().FireEvent({Att::LOUD_SOUND_REDUCTION, {0x01}});
        std::vector<unsigned char> tuning(100, 0x00);
        tuning[2] = 0x80; tuning[3] = 0x3F; // enabled = 1.0f
        pro3->GetAttValueEvent().FireEvent({Att::TRANSPARENCY, tuning});

        auto json = pro3->GetAsJson();
        auto &caps = json["capabilities"];
        std::string missing;
        for (const char *key : {"battery", "anc", "adaptiveAudioNoise", "conversationAwareness", "ancOneAirPod", "pressSpeed",
                                "pressAndHoldDuration", "volumeSwipe", "volumeSwipeLength", "personalizedVolume", "toneVolume", "endCall",
                                "listeningModes", "allowOff", "micMode", "hearingAid", "sleepDetection", "autoConnect", "earDetection",
                                "deviceInfo", "loudSoundReduction", "transparencyTuning", "autoSwitch", "spatialAudio", "equalizer"})
            if (!caps.contains(key))
                missing += std::string(key) + " ";
        Check("Pro 3: every setting it reports is offered", missing.empty() && !caps.contains("crownReversed") &&
                                                            caps["spatialAudio"]["headTracking"] == true && caps["anc"]["selected"] == 4 &&
                                                            caps["deviceInfo"]["model"] == "A3064",
              missing + json.dump());
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // detached workers finish before the device goes
    }

    if (!previousDefault.empty())
        Sh("pactl set-default-sink " + previousDefault);
    Sh("pactl unload-module " + module);

    // The sound server restarts (an update, `systemctl --user restart pipewire-pulse`): the client comes back on
    // its own and says so, so wired headphones and the effect chain are looked at again
    {
        PulseAudioClient pac;
        std::atomic<bool> told{false};
        pac.GetSinkChangedEvent().Subscribe([&](size_t, const uint32_t &) { told = true; });
        Sh("pkill -x pipewire-pulse; sleep 1; (pipewire-pulse >/dev/null 2>&1 &); sleep 1");
        bool back = WaitFor([] { return !Sh("pactl info 2>/dev/null").empty(); });
        Sh("pactl load-module module-null-sink sink_name=mypods_reconnect >/dev/null");
        Check("Sound server restarted: client reconnects", back && WaitFor([&] { return pac.FindSink("mypods_reconnect").has_value(); }, 5000));
        Check("Sound server restarted: listeners told", told.load());
    }

    // Hi-Res: the drop-ins, then a 96 kHz source through the effect chain and straight into the headphones
    {
        AudioEffects::ApplySoundServerOptions(true, {"bluez_card.AA_BB_CC_DD_EE_FF"});
        auto config = std::string(std::getenv("XDG_CONFIG_HOME") ? std::getenv("XDG_CONFIG_HOME") : std::string(std::getenv("HOME")) + "/.config");
        auto read = [&](const std::string &file) { std::ifstream in(config + "/" + file); return std::string((std::istreambuf_iterator<char>(in)), {}); };
        Check("Hi-Res: rates, resampler and Bluetooth drop-ins written",
              read("pipewire/pipewire.conf.d/60-mypods-hires.conf").find("96000 176400 192000") != std::string::npos &&
              read("pipewire/pipewire-pulse.conf.d/60-mypods-hires.conf").find("resample.quality = 10") != std::string::npos &&
              read("wireplumber/wireplumber.conf.d/60-mypods-bluetooth.conf").find("bluez_card.AA_BB_CC_DD_EE_FF") != std::string::npos);

        // no systemd in the container: the restart the daemon asks systemctl for, by hand
        Sh("pkill -x wireplumber; pkill -x pipewire-pulse; pkill -x pipewire; sleep 1; (pipewire >/dev/null 2>&1 &); sleep 1;"
           "(wireplumber >/dev/null 2>&1 &); (pipewire-pulse >/dev/null 2>&1 &); sleep 2");
        WaitFor([] { return !Sh("pactl info 2>/dev/null").empty(); });
        std::string hiResModule = Sh("pactl load-module module-null-sink sink_name=" + BLUEZ_SINK);

        auto settings = std::make_shared<SettingsService>(settingsPath.string());
        settings->SaveSetting("AA_BB_CC_DD_EE_FF", "equalizer", std::string("Bass Booster"));
        auto info = std::make_shared<DBusDeviceInfo>(MAC, "HW-BT (emuliert)", true);
        std::shared_ptr<Device> headphones = BhfDevice::Create(info, std::make_shared<PulseAudioClient>(), settings);
        auto play96k = [] { Sh("(head -c 30000000 /dev/zero | pw-cat -p --raw --rate 96000 --format s24 --channels 2 - >/dev/null 2>&1 &)"); };
        nlohmann::json path;
        // an empty object until the first look is done
        auto signal = [&] { path = headphones->GetAsJson()["capabilities"]["signalPath"]; return path.is_object() ? path : nlohmann::json::object(); };
        auto rate = [](const nlohmann::json &spec) { return spec.is_object() ? spec.value("rate", 0) : 0; }; // null while nothing plays

        Check("Hi-Res: chain in front of the headphones", WaitFor([] { return Sh("pactl get-default-sink") == AudioEffects::SINK_NAME; }));
        play96k();
        Check("Hi-Res: 96 kHz through the chain, the headphones at 96 kHz too",
              WaitFor([] { return Sh("pactl list short sinks | grep " + BLUEZ_SINK).find("96000Hz") != std::string::npos; }),
              Sh("pactl list short sinks"));
        Check("Signal path: processed, not resampled", WaitFor([&] {
                  auto s = signal();
                  return s.value("playing", false) && rate(s["source"]) == 96000 && rate(s["output"]) == 96000 &&
                         s["reasons"] == nlohmann::json({"processed"});
              }), path.dump());
        Sh("pkill -x pw-cat");

        headphones->SetCapabilities({{"equalizer", {{"selected", "Off"}}}});
        AudioEffects::Instance().Stop();
        headphones->RouteAudio();
        WaitFor([] { return Sh("pactl get-default-sink") == BLUEZ_SINK; });
        play96k();
        Check("Signal path: straight into the headphones, bit-perfect", WaitFor([&] {
                  auto s = signal();
                  return s.value("bitPerfect", false) && rate(s["source"]) == 96000;
              }), path.dump());
        Sh("pkill -x pw-cat");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // detached workers finish before the device goes
        headphones.reset();

        AudioEffects::ApplySoundServerOptions(false, {});
        Check("Hi-Res off: the drop-ins are gone", read("pipewire/pipewire.conf.d/60-mypods-hires.conf").empty() &&
                                                    read("wireplumber/wireplumber.conf.d/60-mypods-bluetooth.conf").empty());
        Sh("pactl unload-module " + hiResModule);
    }
    std::filesystem::remove(settingsPath);
    Logger::Info("Emulator: %d failure(s)", failures);
    return failures == 0 ? 0 : 1;
}
