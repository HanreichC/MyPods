// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Event.h"
#include "Device.h"
#include "sdk/aap/setters/AapRequest.h"
#include "ble_ads/BleAdvertisingService.h"
#include "settings/SettingsService.h"
#include "audio/AudioEffects.h"
#include <atomic>

namespace MagicPodsCore
{
    class AapDevice : public Device
    {
    private:
        std::shared_ptr<BleAdvertisingService> _bleService{};
        size_t _getOnAdReceivedEventId = 0;
        Event<const std::vector<unsigned char>> _onResponseDataRecived{};
        Event<const BleAdertisingData> _onLeDataReceived{};        
        Event<const nlohmann::json> _onAnimationTriggered{};
        void OnResponseDataReceived(const std::vector<unsigned char> &data) override;

    public:
        explicit AapDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService, std::shared_ptr<BleAdvertisingService> bleService);
        ~AapDevice() override;
        Event<const std::vector<unsigned char>> &GetResponseDataRecived()
        {
            return _onResponseDataRecived;
        }

        Event<const BleAdertisingData> &GetLeDataReceived()
        {
            return _onLeDataReceived;
        }

        Event<const nlohmann::json> &GetAnimationTriggered()
        {
            return _onAnimationTriggered;
        }

        void SendData(const AapRequest &setter);
        void SendData(const std::vector<unsigned char> &data);

        // Shared by the ear-detection, audio-switch and spatial-audio capabilities
        std::atomic<bool> ownsAudio{true}; // this computer is the AirPods' audio source (until the AirPods say otherwise)
        std::atomic<int> podsInEar{-1};    // from AAP ear detection, -1 = unknown

        // Plays this computer's audio on the headphones: effect chain (spatial audio, EQ) in front of the
        // bluez sink, made the default sink. Blocking PulseAudio round trips, so never call it on the PulseAudio thread.
        void RouteAudio();
        EffectsConfig LoadEffectsConfig();
        void FireAnimation(const nlohmann::json &json);

        // Whether `ad` comes from these AirPods: its rotating address resolves with the IRK. Without AAP
        // (Windows) no IRK can be fetched unless it was imported, then the nearby ad of the same model counts.
        bool IsOwnAdvertisement(const BleAdertisingData &ad, const std::string &irk) const;
        // Their proximity message (Apple type 0x07) in `ad`, nullptr if there is none
        const std::vector<uint8_t> *OwnProximityMessage(const BleAdertisingData &ad);
        static std::unique_ptr<AapDevice> Create(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService, std::shared_ptr<BleAdvertisingService> bleService);
    };
}