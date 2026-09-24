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
#include "sdk/aap/Att.h"
#include <atomic>
#include <chrono>
#include <deque>

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

        // ATT channel for the settings AirPods Pro 2/3 keep as GATT characteristics (sdk/aap/Att.h).
        // ATT allows one request at a time, so requests queue until the previous one is answered.
        std::unique_ptr<Client> _attClient{};
        size_t _attDataEventId = 0;
        std::mutex _attLock{};
        Att::RequestQueue _attQueue{};
        Event<std::pair<unsigned char, std::vector<unsigned char>>> _onAttValue{};
        void AttQueue(std::vector<unsigned char> pdu, unsigned char readHandle);
        void OnAttData(const std::vector<unsigned char> &data);

    protected:
        void OnClientStarted() override;
        void OnClientStopped() override;

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

        // Models with ATT settings (Loud Sound Reduction, customized transparency)
        static bool HasAttSettings(unsigned short productId);
        void AttRead(unsigned char handle);
        void AttWrite(unsigned char handle, const std::vector<unsigned char> &value);
        // Characteristic values as read or notified: handle, value
        Event<std::pair<unsigned char, std::vector<unsigned char>>> &GetAttValueEvent()
        {
            return _onAttValue;
        }

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