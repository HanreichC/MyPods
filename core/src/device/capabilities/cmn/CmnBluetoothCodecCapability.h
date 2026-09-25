// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once
#include "../Capability.h"
#include "device/Device.h"
#include <mutex>

namespace MagicPodsCore
{
    class CmnBluetoothCodecCapability : public Capability
    {
    private:        
        size_t onConnectedPropertyChangedId;                        
        size_t onAudioCardPropertyChangedId;
        CardInfo info{}; // written on the PulseAudio and D-Bus threads, read on the API thread
        std::mutex infoLock;
        std::mutex switching;
        std::atomic<int> request{0};
        std::optional<SinkDetails> details; // what the headphones' sink plays, under infoLock
        std::atomic<int> detailsRequest{0};
        // Reads the sink's details on a worker: after a profile change the sink comes back a moment later
        void UpdateDetails();
        void UpdateCodecInfo();
        void UpdateCardInfo(const CardInfo& newinfo);
        bool IsValidSelected(const std::string& selected);
    protected:
        Device& device;
        nlohmann::json CreateJsonBody() override;
        void Reset() override;

    public:
        explicit CmnBluetoothCodecCapability(Device& device);
        ~CmnBluetoothCodecCapability() override;
        void SetFromJson(const nlohmann::json &json) override;
    };
}
