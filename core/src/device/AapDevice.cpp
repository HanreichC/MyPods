// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "AapDevice.h"
#include "capabilities/aap/AapAncCapability.h"
#include "capabilities/aap/AapConversationAwarenessCapability.h"
#include "capabilities/aap/AapConversationAwarenessStateCapability.h"
#include "capabilities/aap/AapNoiseCancellationOneAirPodModeCapability.h"
#include "capabilities/aap/AapPressAndHoldDurationCapability.h"
#include "capabilities/aap/AapPressSpeedCapability.h"
#include "capabilities/aap/AapVolumeSwipeCapability.h"
#include "capabilities/aap/AapVolumeSwipeLengthCapability.h"
#include "capabilities/aap/AapPersonalizedVolumeCapability.h"
#include "capabilities/aap/AapToneVolumeCapability.h"
#include "capabilities/aap/AapMuteMicrophoneEndCallCapability.h"
#include "capabilities/aap/AapAdaptiveAudioNoiseCapability.h"
#include "capabilities/aap/AapBatteryCapability.h"
#include "capabilities/aap/AppAnimationCapability.h"
#include "sdk/aap/setters/AapInit.h"
#include "sdk/aap/setters/AapInitExt.h"
#include "sdk/aap/setters/AapEnableNotifications.h"
#include "sdk/aap/setters/AapPrivateKeys.h"
#include "capabilities/cmn/CmnBluetoothCodecCapability.h"
#include "capabilities/aap/AapEarDetectionCapability.h"
#include "capabilities/aap/AapDeviceInfoCapability.h"
#include "capabilities/aap/AapAudioSwitchCapability.h"
#include "capabilities/aap/AapAudioEffectsCapabilities.h"
#include "sdk/aap/Aes.h"
#include <algorithm>
#include <thread>

namespace MagicPodsCore
{
    void AapDevice::OnResponseDataReceived(const std::vector<unsigned char> &data)
    {
        _onResponseDataRecived.FireEvent(data);
    }

    AapDevice::AapDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo,
        std::shared_ptr<PulseAudioClient> audioClient,
        std::shared_ptr<SettingsService> settingsService,
        std::shared_ptr<BleAdvertisingService> bleService) : Device(deviceInfo, audioClient, settingsService), _bleService{bleService}
    {
        if (_bleService) // none in the emulator (tests/EmulateAirPods.cpp)
            _getOnAdReceivedEventId = _bleService->GetOnAdReceivedEvent().Subscribe([this](size_t id,  const MagicPodsCore::BleAdertisingData& adData){
                _onLeDataReceived.FireEvent(adData);
            });
    }
    
    AapDevice::~AapDevice()
    {
        if (_bleService && _getOnAdReceivedEventId != 0)
            _bleService->GetOnAdReceivedEvent().Unsubscribe(_getOnAdReceivedEventId);
    }

    void AapDevice::SendData(const AapRequest &setter) //TODO: MAKE COMMON CLASS FOR SETTERS
    {
        _client->SendData(setter.Request());
    }

    void AapDevice::SendData(const std::vector<unsigned char> &data)
    {
        _client->SendData(data);
    }

    EffectsConfig AapDevice::LoadEffectsConfig()
    {
        EffectsConfig config;
        config.spatial = static_cast<SpatialMode>(std::clamp<int64_t>(LoadSettingInt("spatialAudio").value_or(0), 0, AapSpatialAudioCapability::HasHeadTracking(GetProductId()) ? 2 : 1));
        if (auto gains = AudioEffects::Preset(LoadSettingString("equalizer").value_or("Off")))
            config.eq = *gains;
        return config;
    }

    void AapDevice::RouteAudio()
    {
        static std::mutex routing;
        std::lock_guard lock{routing};

        auto pac = GetAudioClient();
        std::string mac = GetAddress();
        std::replace(mac.begin(), mac.end(), ':', '_');
        auto sink = pac->FindSink("bluez_output." + mac);
        if (!sink)
        {
            AudioEffects::Instance().Stop();
            return;
        }
        auto target = AudioEffects::Instance().Apply(*sink, GetName(), LoadEffectsConfig());
        // the chain's sink appears a moment after its process starts
        for (int i = 0; i < 30 && !pac->FindSink(target); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pac->SetDefaultSink(target);
        Logger::Info("%s: audio routed to %s", GetName().c_str(), target.c_str());
    }

    void AapDevice::FireAnimation(const nlohmann::json &json)
    {
        _onAnimationTriggered.FireEvent(json);
    }

    bool AapDevice::IsOwnAdvertisement(const BleAdertisingData &ad, const std::string &irk) const
    {
        if (!irk.empty())
            return Aes::VerifyRPA(ad.GetAddress(), irk);
        // ponytail: RSSI guess, so a second pair of the same model right next to you gets mixed up.
        // Copying irk/enc from a Linux config.toml upgrades to the real check.
        return !Client::SupportsL2CAP() && ad.GetRssi() >= -60;
    }

    const std::vector<uint8_t> *AapDevice::OwnProximityMessage(const BleAdertisingData &ad)
    {
        for (const auto &[company, bytes] : ad.GetManufacturerData())
            if (company == GetVendorId() && bytes.size() >= 27 && bytes[0] == 0x07 &&
                ((bytes[4] << 8) | bytes[3]) == GetProductId() && IsOwnAdvertisement(ad, LoadSettingString("irk").value_or("")))
                return &bytes;
        return nullptr;
    }

    std::unique_ptr<AapDevice> AapDevice::Create(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService, std::shared_ptr<BleAdvertisingService> bleService)
    {
        auto device = std::make_unique<AapDevice>(deviceInfo, audioClient, settingsService, bleService);

        device->capabilities.push_back(std::make_unique<CmnBluetoothCodecCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapBatteryCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapAncCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapConversationAwarenessCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapConversationAwarenessStateCapability>(*device));
        // over-ear headphones are a single unit, so "ANC with one AirPod" does not apply
        switch (static_cast<AapModelIds>(deviceInfo->GetProductId()))
        {
        case AapModelIds::airpodsmax: case AapModelIds::airpodsmax2024: case AapModelIds::airpodsmax2:
        case AapModelIds::beatsSolo3: case AapModelIds::beatssolopro: case AapModelIds::beatssolo4:
        case AapModelIds::beatsstudio3: case AapModelIds::beatsstudiopro:
            break;
        default:
            device->capabilities.push_back(std::make_unique<AapNoiseCancellationOneAirPodModeCapability>(*device));
        }
        device->capabilities.push_back(std::make_unique<AapPressAndHoldDurationCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapPressSpeedCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapVolumeSwipeCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapVolumeSwipeLengthCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapPersonalizedVolumeCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapToneVolumeCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapMuteMicrophoneEndCallCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapAdaptiveAudioNoiseCapability>(*device));
        device->capabilities.push_back(std::make_unique<AppAnimationCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapEarDetectionCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapDeviceInfoCapability>(*device));
        // Handing the audio over is AAP smart routing; without it there is nothing to negotiate with
        if (Client::SupportsL2CAP())
            device->capabilities.push_back(std::make_unique<AapAudioSwitchCapability>(*device));
#ifndef _WIN32 // effects run in PipeWire (AudioEffects.h)
        device->capabilities.push_back(std::make_unique<AapSpatialAudioCapability>(*device));
        device->capabilities.push_back(std::make_unique<AapEqualizerCapability>(*device));
#endif

        device->_clientStartData.push_back(AapInit{}.Request());
        device->_clientStartData.push_back(AapEnableNotifications{AapNotificationsMode::Unknown2}.Request());
        device->_clientStartData.push_back(AapEnableNotifications{AapNotificationsMode::Unknown1}.Request());
        if (AapInitExt::IsSupported(deviceInfo->GetProductId()))
            device->_clientStartData.push_back(AapInitExt{}.Request());
        
        if (!device->LoadSettingString("irk").has_value() || !device->LoadSettingString("enc").has_value())
            device->_clientStartData.push_back(AapPrivateKeys{}.Request());
        
        //TODO: Add initData to client
        device->_client = Client::CreateL2CAP(deviceInfo->GetAddress(), 0x1001);

        device->Init();
        return device;
    }
}