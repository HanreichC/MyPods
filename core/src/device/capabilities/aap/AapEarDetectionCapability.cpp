// MyPods
// License: GPL-3.0

#include "AapEarDetectionCapability.h"
#include "media/MprisClient.h"
#include <thread>

namespace MagicPodsCore
{
    static constexpr unsigned char EAR_DETECTION_CONFIG = 0x0A;

    AapEarDetectionCapability::AapEarDetectionCapability(AapDevice &device) : AapCapability("earDetection", false, device)
    {
    }

    nlohmann::json AapEarDetectionCapability::CreateJsonBody()
    {
        return {{"selected", option}, {"primary", primary}, {"secondary", secondary}};
    }

    void AapEarDetectionCapability::Reset()
    {
        primary = secondary = -1;
        device.podsInEar = -1;
        std::lock_guard lock{pausedLock};
        paused.clear();
        AapCapability::Reset();
    }

    void AapEarDetectionCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        if (data.size() < 8 || data[0] != 0x04 || data[1] != 0x00 || data[2] != 0x04 || data[3] != 0x00)
            return;

        if (data[4] == 0x09 && data[6] == EAR_DETECTION_CONFIG)
        {
            option = data[7] == 0x01;
            _onChanged.FireEvent(*this);
            return;
        }
        if (data[4] != 0x06)
            return;

        primary = data[6];
        secondary = data[7];
        int inEar = (primary == 0) + (secondary == 0);
        int before = device.podsInEar.exchange(inEar);
        isAvailable = true;
        _onChanged.FireEvent(*this);
        Logger::Info("Ear detection: primary %d, secondary %d", primary, secondary);

        if (!option || !device.ownsAudio || before < 0 || inEar == before)
            return;

        // MPRIS calls go to other processes; keep a slow player from stalling the AAP reader
        std::thread([this, inEar, before]()
        {
            std::lock_guard lock{pausedLock};
            if (inEar < before && paused.empty())
            {
                paused = MprisClient::Instance().PausePlaying();
                inEarBeforePause = before;
            }
            else if (inEar >= inEarBeforePause && !paused.empty())
            {
                MprisClient::Instance().Play(paused);
                paused.clear();
            }
        }).detach();
    }

    void AapEarDetectionCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (!capability.contains("selected") || !capability["selected"].is_boolean())
        {
            Logger::Error("AapEarDetectionCapability::SetFromJson: selected must be a boolean");
            return;
        }
        option = capability["selected"].get<bool>();
        device.SendData({0x04, 0x00, 0x04, 0x00, 0x09, 0x00, EAR_DETECTION_CONFIG, static_cast<unsigned char>(option ? 0x01 : 0x02), 0x00, 0x00, 0x00});
        _onChanged.FireEvent(*this);
    }
}
