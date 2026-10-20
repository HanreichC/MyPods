// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Event.h"
#include "Device.h"
#include "BeoplayE8.h"

namespace MyPods
{

    class BhfDevice : public Device
    {

    private:
    size_t cardEventId = 0;
    // Beoplay E8: its LE link while connected (BeoplayE8.h)
    std::unique_ptr<BeoplayE8::Link> beoplayLink;
    size_t beoplayConnectedId = 0;
    void OnResponseDataReceived(const std::vector<unsigned char> &data) override;

    public:
        explicit BhfDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService);
        ~BhfDevice() override;
        static std::unique_ptr<BhfDevice> Create(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService);        
    };
}
