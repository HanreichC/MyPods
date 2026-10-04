// MyPods
// License: GPL-3.0

#include "WiredDevice.h"
#include "capabilities/cmn/CmnAudioEffectsCapabilities.h"

namespace MagicPodsCore
{
    WiredDevice::WiredDevice(std::shared_ptr<DBusDeviceInfo> info, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService, const WiredOutput &output)
        : Device(info, audioClient, settingsService), info(info), sink(output.sink), usb(output.usb)
    {
    }

    std::unique_ptr<WiredDevice> WiredDevice::Create(const WiredOutput &output, bool plugged, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService)
    {
        auto info = std::make_shared<DBusDeviceInfo>(AddressFor(output.sink), output.name, plugged);
        auto device = std::make_unique<WiredDevice>(info, audioClient, settingsService, output);
        device->capabilities.push_back(std::make_unique<CmnSpatialAudioCapability>(*device));
        device->capabilities.push_back(std::make_unique<CmnEqualizerCapability>(*device));

        // Like BhfDevice: the saved effects go in front of the headphones when they're plugged in and away when
        // they're pulled, so the speakers behind the same jack play without them
        auto *raw = device.get();
        device->GetConnectedPropertyChangedEvent().Subscribe([raw](size_t, bool connected)
        {
            if (!connected)
                return raw->StopEffects();
            // off the thread that noticed the plug, RouteAudio waits for PulseAudio replies
            std::thread([raw, keep = raw->KeepAlive()]()
            {
                if (!raw->LoadEffectsConfig().IsNeutral())
                    raw->RouteAudio();
            }).detach();
        });
        device->Init();
        if (plugged && !device->LoadEffectsConfig().IsNeutral())
            device->RouteAudio();
        return device;
    }
}
