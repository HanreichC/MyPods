// MyPods
// License: GPL-3.0

#pragma once

#include "Device.h"

namespace MagicPodsCore
{
    // Headphones on a jack or USB: no Bluetooth and no control channel, only the effects that run on this computer
    // (equalizer, on Linux also spatial audio without head tracking). One per output, keyed by its sink (Windows:
    // its endpoint); "connected" while that output plays into headphones.
    class WiredDevice : public Device
    {
    private:
        std::shared_ptr<DBusDeviceInfo> info;
        std::string sink;
        bool usb;
        void OnResponseDataReceived(const std::vector<unsigned char> &) override {}

    public:
        std::string SinkPart() const override { return sink; }
        WiredDevice(std::shared_ptr<DBusDeviceInfo> info, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService, const WiredOutput &output);
        ~WiredDevice() override { Shutdown(); } // the capabilities' workers use SinkPart() and HeadphonesSink()
        // `plugged`: already playing into them, as when the daemon starts next to them
        static std::unique_ptr<WiredDevice> Create(const WiredOutput &output, bool plugged, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService);
        // The address the device has for an output; also names its settings
        static std::string AddressFor(const std::string &sink) { return "wired:" + sink; }

        void SetPlugged(bool plugged) { info->GetConnectionStatus().SetValue(plugged); }
        // The sink only while it plays into the headphones: a jack's sink stays when they're pulled, playing on the speakers
        std::optional<std::string> HeadphonesSink() override { return GetConnected() ? Device::HeadphonesSink() : std::nullopt; }
        const char *WiredConnection() const override { return usb ? "usb" : "jack"; }
    };
}
