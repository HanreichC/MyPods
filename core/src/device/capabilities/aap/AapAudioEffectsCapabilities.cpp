// MyPods
// License: GPL-3.0

#include "AapAudioEffectsCapabilities.h"
#include "sdk/aap/enums/AapModelIds.h"
#include <algorithm>

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

    AapSpatialAudioCapability::AapSpatialAudioCapability(AapDevice &device) : CmnSpatialAudioCapability(device), aap(device)
    {
        responseDataReceivedId = device.GetResponseDataRecived().Subscribe([this](size_t, const std::vector<unsigned char> &data)
        {
            OnReceivedData(data);
        });
    }

    AapSpatialAudioCapability::~AapSpatialAudioCapability()
    {
        aap.GetResponseDataRecived().Unsubscribe(responseDataReceivedId);
    }

    void AapSpatialAudioCapability::Reset()
    {
        talking = false;
        tracking = false;
        samples = 0;
        CmnSpatialAudioCapability::Reset();
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
        aap.SendData(want ? HEAD_TRACKING_START : HEAD_TRACKING_STOP);
        Logger::Info("Head tracking %s", want ? "started" : "stopped");
    }

    void AapSpatialAudioCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        if (!talking)
        {
            talking = true;
            // first packet of a session: nothing else applies saved effects (spatial and EQ) when the daemon starts next to connected headphones
            if (!device.LoadEffectsConfig().IsNeutral())
                device.RouteAudioAsync();
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
}
