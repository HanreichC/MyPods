// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "BhfDevice.h"
#include "capabilities/bhf/BhfBatteryCapability.h"
#include "capabilities/cmn/CmnBluetoothCodecCapability.h"
#include "capabilities/cmn/CmnAudioEffectsCapabilities.h"

namespace MagicPodsCore
{
    void BhfDevice::OnResponseDataReceived(const std::vector<unsigned char> &data){}

    BhfDevice::BhfDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService) : Device(deviceInfo, audioClient, settingsService)
    {
    }

    BhfDevice::~BhfDevice()
    {
        GetAudioClient()->GatAudioCardPropertyChangedEvent().Unsubscribe(cardEventId);
    }

    std::unique_ptr<BhfDevice> BhfDevice::Create(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService)
    {
        auto device = std::make_unique<BhfDevice>(deviceInfo, audioClient, settingsService);

        device->capabilities.push_back(std::make_unique<BhfBatteryCapability>(*device));
        device->capabilities.push_back(std::make_unique<CmnBluetoothCodecCapability>(*device));
#ifndef _WIN32 // effects run in PipeWire (AudioEffects.h)
        device->capabilities.push_back(std::make_unique<CmnSpatialAudioCapability>(*device));
        device->capabilities.push_back(std::make_unique<CmnEqualizerCapability>(*device));

        // No audio handover like AirPods have, so the saved effects follow the headphones' A2DP sink from here:
        // put in front of it when it (re)appears, taken away when the headphones go.
        auto *raw = device.get();
        device->cardEventId = audioClient->GatAudioCardPropertyChangedEvent().Subscribe([raw](size_t, const CardInfo &info)
        {
            if (info.name != raw->GetAudioClient()->GetNameFromMac(raw->GetAddress()) || !info.activeProfile.starts_with("a2dp"))
                return;
            // off the PulseAudio thread, RouteAudio waits for PulseAudio replies
            std::thread([raw, keep = raw->KeepAlive()]()
            {
                if (!raw->LoadEffectsConfig().IsNeutral())
                    raw->RouteAudio();
            }).detach();
        });
        device->GetConnectedPropertyChangedEvent().Subscribe([raw](size_t, bool connected)
        {
            // ponytail: one chain for all headphones (AudioEffects), so this also stops it for other headphones playing with effects
            if (!connected && !raw->LoadEffectsConfig().IsNeutral())
                AudioEffects::Instance().Stop();
        });
#endif
        device->Init();
#ifndef _WIN32
        // the daemon starts next to connected headphones: no card change will come
        if (device->GetConnected() && !device->LoadEffectsConfig().IsNeutral())
            device->RouteAudio();
#endif
        return device;
    }
}
