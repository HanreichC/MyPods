// MyPods
// License: GPL-3.0

// Emulated AirPods Max against the real audio path: a null sink named like the headphones' bluez sink,
// AAP packets fed straight into the capabilities, everything after that is the daemon's own code.
// Needs a running PipeWire, no Bluetooth. magicpodscore --emulate-airpods

#include "device/AapDevice.h"
#include "device/capabilities/aap/AapAudioEffectsCapabilities.h"
#include "audio/AudioEffects.h"
#include "Logger.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
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

    // current "spL:Azimuth"/"spR:Azimuth" of the running chain, NAN if there is none
    std::pair<double, double> Azimuths()
    {
        auto dump = nlohmann::json::parse(Sh("pw-dump " + std::string(AudioEffects::SINK_NAME) + " 2>/dev/null"), nullptr, false);
        std::pair<double, double> az{NAN, NAN};
        if (!dump.is_array())
            return az;
        for (auto &obj : dump)
            for (auto &props : obj["info"]["params"]["Props"])
                if (props.contains("params"))
                    for (size_t i = 0; i + 1 < props["params"].size(); i += 2)
                    {
                        if (props["params"][i] == "spL:Azimuth")
                            az.first = props["params"][i + 1].get<double>();
                        if (props["params"][i] == "spR:Azimuth")
                            az.second = props["params"][i + 1].get<double>();
                    }
        return az;
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
        AapEqualizerCapability equalizer{pods};
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

        spatial.SetFromJson({{"spatialAudio", {{"selected", 0}}}});
        Check("Off: headphones are the default sink again", defaultSinkIs(BLUEZ_SINK), sink);
        AudioEffects::Instance().Stop();
    }

    if (!previousDefault.empty())
        Sh("pactl set-default-sink " + previousDefault);
    Sh("pactl unload-module " + module);
    std::filesystem::remove(settingsPath);
    Logger::Info("Emulator: %d failure(s)", failures);
    return failures == 0 ? 0 : 1;
}
