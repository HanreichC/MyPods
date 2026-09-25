// MyPods
// License: GPL-3.0

// Emulated AirPods Max against the real audio path: a null sink named like the headphones' bluez sink,
// AAP packets fed straight into the capabilities, everything after that is the daemon's own code.
// Needs a running PipeWire, no Bluetooth. magicpodscore --emulate-airpods

#include "device/AapDevice.h"
#include "device/BhfDevice.h"
#include "device/capabilities/aap/AapAudioEffectsCapabilities.h"
#include "audio/AudioEffects.h"
#include "Logger.h"

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
    };

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
        auto json = headphones->GetAsJson()["capabilities"];
        Check("Generic: spatial audio without head tracking, EQ", json.contains("spatialAudio") && json["spatialAudio"]["headTracking"] == false &&
                                                                  json.contains("equalizer") && !json["equalizer"].contains("correction"), json.dump());
        headphones->SetCapabilities({{"spatialAudio", {{"selected", 2}}}});
        Check("Generic: head tracked refused", headphones->GetAsJson()["capabilities"]["spatialAudio"]["selected"] == 0);
        headphones->SetCapabilities({{"spatialAudio", {{"selected", 1}}}});
        auto az = Azimuths();
        Check("Generic: fixed speakers at +-30 degrees", WaitFor([&] { az = Azimuths(); return std::abs(az.first - 30) < 0.5 && std::abs(az.second - 330) < 0.5; }),
              std::to_string(az.first) + " " + std::to_string(az.second));

        info->GetConnectionStatus().SetValue(false);
        Check("Generic: disconnected, chain gone and settings hidden", WaitFor([] { return Sh("pactl list short sinks | grep 'mypods_fx\s'").empty(); }) &&
                                                                       !headphones->GetAsJson()["capabilities"].contains("equalizer"), Sh("pactl list short sinks"));
        info->GetConnectionStatus().SetValue(true);
        headphones->SetCapabilities({{"spatialAudio", {{"selected", 0}}}});
        headphones->SetCapabilities({{"equalizer", {{"selected", "Off"}}}});
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // detached routing threads finish before the device goes
        Check("Generic: effects off, headphones are the default sink again", defaultSinkIs(BLUEZ_SINK), sink);
        AudioEffects::Instance().Stop();
    }

    if (!previousDefault.empty())
        Sh("pactl set-default-sink " + previousDefault);
    Sh("pactl unload-module " + module);
    std::filesystem::remove(settingsPath);
    Logger::Info("Emulator: %d failure(s)", failures);
    return failures == 0 ? 0 : 1;
}
