// MyPods
// License: GPL-3.0

#include "AapAudioEffectsCapabilities.h"
#include "audio/AudioEffects.h"
#include "sdk/aap/enums/AapModelIds.h"
#include <algorithm>
#include <map>
#include <thread>

namespace MagicPodsCore
{
    // LibrePods "alternate" head-tracking start/stop packets (their default)
    static const std::vector<unsigned char> HEAD_TRACKING_START{0x04, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00, 0x10, 0x00, 0x0F, 0x00, 0x08, 0x73, 0x42, 0x0B, 0x08, 0x10, 0x10, 0x02, 0x1A, 0x05, 0x01, 0x40, 0x9C, 0x00, 0x00};
    static const std::vector<unsigned char> HEAD_TRACKING_STOP{0x04, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00, 0x10, 0x00, 0x0F, 0x00, 0x08, 0x75, 0x42, 0x0B, 0x08, 0x10, 0x10, 0x02, 0x1A, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00};
    static constexpr int CALIBRATION_SAMPLES = 10;
    // Apple re-centers when you keep looking elsewhere (e.g. turned towards a second screen)
    static constexpr double RECENTER_SECONDS = 20.0;

    static bool IsAapPacket(const std::vector<unsigned char> &data, unsigned char opcode)
    {
        return data.size() >= 6 && data[0] == 0x04 && data[1] == 0x00 && data[2] == 0x04 && data[3] == 0x00 && data[4] == opcode;
    }

    static void RouteIfOurs(AapDevice &device)
    {
        if (device.ownsAudio && device.GetConnected())
            std::thread([&device, keep = device.KeepAlive()]() { device.RouteAudio(); }).detach();
    }

    bool AapSpatialAudioCapability::HasHeadTracking(unsigned short model)
    {
        // models without motion sensors; unknown (newer) models are assumed to have them
        switch (static_cast<AapModelIds>(model))
        {
        case AapModelIds::airpods1:
        case AapModelIds::airpods2:
        case AapModelIds::powerbeatspro:
        case AapModelIds::powerbeats3:
        case AapModelIds::powerbeats4:
        case AapModelIds::beatsx:
        case AapModelIds::beatsflex:
        case AapModelIds::beatsSolo3:
        case AapModelIds::beatsstudio3:
        case AapModelIds::beatsstudiobuds:
        case AapModelIds::beatsstudiobudsplus:
        case AapModelIds::beatssolobuds:
            return false;
        default:
            return true;
        }
    }

    AapSpatialAudioCapability::AapSpatialAudioCapability(AapDevice &device) : AapCapability("spatialAudio", false, device),
        headTracking(HasHeadTracking(device.GetProductId()))
    {
        mode = static_cast<int>(std::clamp<int64_t>(device.LoadSettingInt("spatialAudio").value_or(0), 0, headTracking ? 2 : 1));
    }

    nlohmann::json AapSpatialAudioCapability::CreateJsonBody()
    {
        return {{"selected", mode}, {"headTracking", headTracking}};
    }

    void AapSpatialAudioCapability::Reset()
    {
        tracking = false;
        samples = 0;
        AapCapability::Reset();
    }

    double AapSpatialAudioCapability::Yaw(const std::vector<unsigned char> &packet, double neutral2, double neutral3)
    {
        auto i16 = [&](size_t at) { return static_cast<int16_t>(packet[at] | (packet[at + 1] << 8)); };
        // ponytail: orientation fields and scale from LibrePods' HeadOrientation.kt heuristic, not a real quaternion; sign unverified on hardware
        return ((i16(45) - neutral2) - (i16(47) - neutral3)) / 2.0 / 32000.0 * 180.0;
    }

    void AapSpatialAudioCapability::UpdateTracking()
    {
        // the AAP client queues while disconnected; a stale start must not go out before the next handshake
        bool want = mode == static_cast<int>(SpatialMode::HeadTracked) && device.ownsAudio && device.GetConnected();
        if (want == tracking)
            return;
        tracking = want;
        samples = 0;
        device.SendData(want ? HEAD_TRACKING_START : HEAD_TRACKING_STOP);
        Logger::Info("Head tracking %s", want ? "started" : "stopped");
    }

    void AapSpatialAudioCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        if (!isAvailable)
        {
            isAvailable = true;
            _onChanged.FireEvent(*this);
            // first packet of a session: nothing else applies saved effects (spatial and EQ) when the daemon starts next to connected headphones
            if (!device.LoadEffectsConfig().IsNeutral())
                RouteIfOurs(device);
        }
        UpdateTracking(); // ownership may have moved since the last packet

        if (!tracking || !IsAapPacket(data, 0x17) || data.size() < 70 || data[8] != 0x10)
            return;

        auto i16 = [&](size_t at) { return static_cast<int16_t>(data[at] | (data[at + 1] << 8)); };
        if (samples < CALIBRATION_SAMPLES)
        {
            neutral2 = (neutral2 * samples + i16(45)) / (samples + 1);
            neutral3 = (neutral3 * samples + i16(47)) / (samples + 1);
            center = 0;
            samples++;
            lastSample = std::chrono::steady_clock::now();
            return;
        }
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastSample).count();
        lastSample = now;
        double yaw = Yaw(data, neutral2, neutral3);
        center += (yaw - center) * std::min(1.0, dt / RECENTER_SECONDS);
        AudioEffects::Instance().SetYaw(yaw - center);
    }

    void AapSpatialAudioCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (!capability.contains("selected") || !capability["selected"].is_number_integer() ||
            capability["selected"].get<int>() < 0 || capability["selected"].get<int>() > (headTracking ? 2 : 1))
        {
            Logger::Error("AapSpatialAudioCapability::SetFromJson: selected must be 0, 1 or (with head tracking) 2");
            return;
        }
        mode = capability["selected"].get<int>();
        device.SaveSettingInt("spatialAudio", mode);
        UpdateTracking();
        _onChanged.FireEvent(*this);
        RouteIfOurs(device);
    }

    std::vector<Biquad> AapEqualizerCapability::Correction(unsigned short model)
    {
        // AutoEQ (github.com/jaakkopasanen/AutoEq, MIT) ParametricEQ.txt, measurement source in the comment;
        // the USB-C Max and Pro 2 share their predecessors' acoustics. Its preamp is left out, AudioEffects::HeadroomDb covers it.
        constexpr auto P = Biquad::Peaking, L = Biquad::LowShelf, H = Biquad::HighShelf;
        static const std::map<AapModelIds, std::vector<Biquad>> CORRECTIONS{
            {AapModelIds::airpodsmax, {{L, 105, -3.0, 0.70}, {P, 7273, 3.6, 2.41}, {P, 218, -2.9, 1.41}, {P, 1031, -3.2, 0.99}, {P, 3185, 3.1, 0.56}, {H, 10000, -5.5, 0.70}, {P, 9508, 2.7, 2.20}, {P, 66, 0.6, 1.65}, {P, 4045, 2.1, 5.82}, {P, 4834, -1.8, 6.00}}}, // oratory1990
            {AapModelIds::airpods1, {{L, 105, 3.2, 0.70}, {P, 9383, 4.7, 0.74}, {P, 526, -3.3, 1.28}, {P, 1979, -2.0, 0.92}, {P, 4485, 2.9, 2.30}, {H, 10000, -3.0, 0.70}, {P, 154, 0.9, 1.15}, {P, 47, -0.8, 1.53}, {P, 308, -0.6, 1.95}, {P, 102, 0.3, 2.03}}}, // oratory1990
            {AapModelIds::airpods2, {{L, 105, 5.8, 0.70}, {P, 5446, -6.5, 0.18}, {P, 2138, 8.3, 0.30}, {P, 89, 1.5, 0.98}, {P, 2124, -6.3, 1.11}, {H, 10000, -2.5, 0.70}, {P, 4173, 1.4, 2.74}, {P, 5484, -1.6, 6.00}, {P, 2846, -0.9, 4.81}, {P, 240, 0.2, 1.68}}}, // Rtings
            {AapModelIds::airpods3, {{L, 105, 6.9, 0.70}, {P, 1838, -3.9, 2.08}, {P, 3879, 2.5, 2.13}, {P, 64, -5.0, 0.96}, {P, 43, 2.7, 3.38}, {H, 10000, 5.5, 0.70}, {P, 9961, 2.4, 1.61}, {P, 6052, -2.7, 3.91}, {P, 724, 1.9, 2.80}, {P, 420, -0.6, 1.89}}}, // Rtings
            {AapModelIds::airpods4, {{L, 105, 11.4, 0.70}, {P, 4622, 6.0, 1.39}, {P, 49, -11.7, 0.39}, {P, 1277, -2.6, 0.86}, {P, 3036, 4.1, 2.43}, {H, 10000, -1.9, 0.70}, {P, 354, -1.4, 1.50}, {P, 176, 1.5, 2.04}, {P, 618, 1.0, 2.35}, {P, 106, -0.9, 2.47}}}, // Rtings
            {AapModelIds::airpods4anc, {{L, 105, 12.7, 0.70}, {P, 49, -13.2, 0.44}, {P, 3573, 6.0, 1.40}, {P, 1318, -2.8, 1.16}, {P, 5138, 3.2, 2.82}, {H, 10000, -2.0, 0.70}, {P, 166, 1.2, 3.49}, {P, 100, -0.7, 2.50}, {P, 438, -0.8, 2.38}, {P, 654, 0.6, 2.93}}}, // Rtings, ANC on
            {AapModelIds::airpodspro, {{L, 105, 2.6, 0.70}, {P, 514, -4.4, 0.68}, {P, 8903, 6.0, 1.66}, {P, 183, 2.0, 0.77}, {P, 4613, 3.4, 2.46}, {H, 10000, -0.5, 0.70}, {P, 1517, -1.0, 2.33}, {P, 929, 1.2, 2.75}, {P, 44, -0.5, 2.16}, {P, 618, -0.5, 2.86}}}, // crinacle
            {AapModelIds::airpodspro2, {{L, 105, 0.5, 0.70}, {P, 427, -2.6, 0.80}, {P, 3647, 2.3, 0.77}, {P, 9516, 2.8, 3.44}, {P, 76, 1.9, 1.34}, {H, 10000, -1.8, 0.70}, {P, 5938, 2.6, 1.27}, {P, 6486, -6.3, 5.92}, {P, 1172, 1.1, 5.05}, {P, 3326, -1.9, 4.40}}}, // crinacle, ANC on
            {AapModelIds::powerbeatspro, {{L, 105, -0.8, 0.70}, {P, 6596, 6.1, 2.51}, {P, 2703, -4.4, 2.80}, {P, 1362, -2.1, 1.94}, {P, 3607, 3.7, 4.72}, {H, 10000, 3.7, 0.70}, {P, 143, -1.2, 1.56}, {P, 374, 0.7, 1.21}, {P, 4688, -2.2, 6.00}, {P, 60, 0.4, 1.34}}}, // oratory1990
            {AapModelIds::beatssolopro, {{L, 105, -2.0, 0.70}, {P, 332, 1.9, 0.50}, {P, 3735, -4.7, 0.44}, {P, 2106, 5.4, 1.58}, {P, 5816, 5.0, 3.86}, {H, 10000, -1.5, 0.70}, {P, 58, -0.4, 1.36}, {P, 31, 0.4, 1.68}, {P, 163, 0.7, 3.66}, {P, 233, -0.4, 2.45}}}, // oratory1990
            {AapModelIds::beatsstudio3, {{L, 105, 7.7, 0.70}, {P, 327, -5.9, 1.56}, {P, 5209, 6.7, 2.64}, {P, 68, -8.1, 0.78}, {P, 1978, 4.3, 2.19}, {H, 10000, -2.5, 0.70}, {P, 3257, -1.9, 4.42}, {P, 666, 1.5, 2.52}, {P, 6665, 1.5, 3.92}, {P, 428, -1.1, 4.35}}}, // oratory1990
            {AapModelIds::beatsstudiobuds, {{L, 105, -2.2, 0.70}, {P, 1819, -4.7, 0.74}, {P, 145, 3.3, 0.23}, {P, 6271, 4.8, 1.67}, {P, 568, -2.4, 1.88}, {H, 10000, 0.3, 0.70}, {P, 3619, 2.3, 4.81}, {P, 2771, -1.7, 3.92}, {P, 68, 0.8, 1.75}, {P, 127, -0.8, 1.91}}}, // oratory1990
            {AapModelIds::beatsstudiobudsplus, {{L, 105, 10.6, 0.70}, {P, 702, 5.2, 0.41}, {P, 1352, -7.5, 0.72}, {P, 195, 2.1, 0.80}, {P, 42, -11.0, 0.47}, {H, 10000, -3.1, 0.70}, {P, 6487, -4.4, 6.00}, {P, 9846, -2.1, 2.19}, {P, 3674, 1.9, 4.28}, {P, 918, 1.1, 4.94}}}, // Rtings
            {AapModelIds::beatsstudiopro, {{L, 105, -3.1, 0.70}, {P, 8990, -5.1, 2.02}, {P, 304, 3.8, 0.85}, {P, 1720, -3.0, 1.19}, {P, 67, 6.0, 2.55}, {H, 10000, 0.5, 0.70}, {P, 5052, 3.7, 4.43}, {P, 3394, -2.5, 5.33}, {P, 6509, -2.0, 5.62}, {P, 734, 0.7, 3.38}}}, // Rtings
            {AapModelIds::beatsfitpro, {{L, 105, 1.8, 0.70}, {P, 313, 2.3, 0.84}, {P, 5866, -4.4, 4.07}, {P, 2445, -2.7, 2.06}, {P, 40, -3.7, 0.92}, {H, 10000, 1.0, 0.70}, {P, 1263, -1.4, 2.35}, {P, 4035, 1.8, 4.11}, {P, 5017, -1.4, 6.00}, {P, 942, 0.7, 4.33}}}, // Rtings
            {AapModelIds::beatsflex, {{L, 105, -5.0, 0.70}, {P, 3827, 5.8, 1.86}, {P, 168, -2.4, 1.04}, {P, 1260, -3.7, 2.62}, {P, 5114, 3.5, 3.66}, {H, 10000, -5.5, 0.70}, {P, 746, 1.2, 1.80}, {P, 988, -1.2, 4.10}, {P, 7408, 1.9, 5.43}, {P, 287, -0.5, 3.23}}}, // Rtings
            {AapModelIds::powerbeats3, {{L, 105, -4.4, 0.70}, {P, 3769, 4.9, 1.87}, {P, 1386, -3.1, 1.09}, {P, 500, 2.9, 1.21}, {P, 153, -2.2, 1.28}, {H, 10000, 2.9, 0.70}, {P, 6217, 3.2, 5.88}, {P, 5198, -2.1, 6.00}, {P, 8129, -1.6, 4.52}, {P, 2533, -0.6, 4.38}}}, // Rtings
            {AapModelIds::powerbeats4, {{L, 105, -1.4, 0.70}, {P, 5806, 4.7, 4.11}, {P, 799, 3.0, 1.23}, {P, 1254, -3.1, 1.29}, {P, 148, -1.5, 2.27}, {H, 10000, -1.3, 0.70}, {P, 3710, 4.2, 4.64}, {P, 2703, -2.7, 4.21}, {P, 8095, -1.6, 4.28}, {P, 60, 0.3, 1.81}}}, // Rtings
            {AapModelIds::beatssolobuds, {{L, 105, -0.2, 0.70}, {P, 425, 4.1, 1.19}, {P, 1890, -4.9, 0.80}, {P, 155, 2.2, 1.43}, {P, 42, -1.6, 1.25}, {H, 10000, 6.3, 0.70}, {P, 8413, 2.0, 3.52}, {P, 4999, -1.6, 5.02}, {P, 2837, -0.6, 4.40}, {P, 839, -0.2, 1.59}}}, // Rtings
            {AapModelIds::beatssolo4, {{L, 105, 1.4, 0.70}, {P, 920, 3.4, 0.19}, {P, 3842, -5.2, 1.98}, {P, 1054, -4.7, 0.97}, {P, 7703, -2.0, 0.67}, {H, 10000, 2.7, 0.70}, {P, 75, -1.4, 2.28}, {P, 9444, -1.1, 2.03}, {P, 137, 0.7, 2.00}, {P, 51, 1.3, 4.12}}}, // Rtings
        };
        auto id = static_cast<AapModelIds>(model);
        id = id == AapModelIds::airpodsmax2024 ? AapModelIds::airpodsmax : id == AapModelIds::airpodsprousbc ? AapModelIds::airpodspro2 : id;
        auto it = CORRECTIONS.find(id);
        return it == CORRECTIONS.end() ? std::vector<Biquad>{} : it->second;
    }

    AapEqualizerCapability::AapEqualizerCapability(AapDevice &device) : AapCapability("equalizer", false, device),
        hasCorrection(!Correction(device.GetProductId()).empty())
    {
        preset = device.LoadSettingString("equalizer").value_or("Off");
        corrected = device.LoadSettingInt("headphoneCorrection").value_or(0) != 0;
        crossfeed = device.LoadSettingInt("crossfeed").value_or(0) != 0;
    }

    nlohmann::json AapEqualizerCapability::CreateJsonBody()
    {
        nlohmann::json body{{"selected", preset}, {"options", AudioEffects::PresetNames()}, {"crossfeed", crossfeed}};
        if (hasCorrection)
            body["correction"] = corrected;
        return body;
    }

    void AapEqualizerCapability::OnReceivedData(const std::vector<unsigned char> &)
    {
        if (!isAvailable)
        {
            isAvailable = true;
            _onChanged.FireEvent(*this);
        }
    }

    void AapEqualizerCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (capability.contains("selected"))
        {
            if (!capability["selected"].is_string() || !AudioEffects::Preset(capability["selected"].get<std::string>()))
            {
                Logger::Error("AapEqualizerCapability::SetFromJson: unknown preset");
                return;
            }
            preset = capability["selected"].get<std::string>();
            device.SaveSettingString("equalizer", preset);
        }
        else if (capability.contains("correction") && capability["correction"].is_boolean() && hasCorrection)
        {
            corrected = capability["correction"].get<bool>();
            device.SaveSettingInt("headphoneCorrection", corrected);
        }
        else if (capability.contains("crossfeed") && capability["crossfeed"].is_boolean())
        {
            crossfeed = capability["crossfeed"].get<bool>();
            device.SaveSettingInt("crossfeed", crossfeed);
        }
        else
        {
            Logger::Error("AapEqualizerCapability::SetFromJson: expected selected, correction or crossfeed");
            return;
        }
        _onChanged.FireEvent(*this);
        RouteIfOurs(device);
    }
}
