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
#include "capabilities/aap/AapControlCapability.h"
#include "capabilities/aap/AapAttCapabilities.h"
#include "sdk/aap/Aes.h"
#include "sdk/aap/Att.h"
#include "sdk/aap/enums/AapModelIds.h"
#include <algorithm>
#include <map>
#include <optional>
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
        Shutdown();
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

    bool AapDevice::HasAttSettings(unsigned short productId)
    {
        switch (static_cast<AapModelIds>(productId))
        {
        case AapModelIds::airpodspro2: case AapModelIds::airpodsprousbc: case AapModelIds::airpodspro3:
            return true;
        default:
            return false;
        }
    }

    void AapDevice::OnClientStarted()
    {
        if (!_attClient || _attClient->IsStarted())
            return;
        // Runs on the client worker, so the connect may block. Without it only these settings stay hidden.
        if (!_attClient->Start())
        {
            Logger::Info("%s: no ATT channel, Loud Sound Reduction and transparency tuning stay hidden", GetName().c_str());
            return;
        }
        AttRead(Att::LOUD_SOUND_REDUCTION);
        AttRead(Att::TRANSPARENCY);
        AttWrite(Att::TRANSPARENCY + 1, {0x01}); // notifications, e.g. when the iPhone changes the tuning
    }

    void AapDevice::OnClientStopped()
    {
        if (!_attClient)
            return;
        _attClient->Stop();
        std::lock_guard lock{_attLock};
        _attQueue.Clear();
    }

    void AapDevice::AttRead(unsigned char handle)
    {
        AttQueue(Att::Read(handle), handle);
    }

    void AapDevice::AttWrite(unsigned char handle, const std::vector<unsigned char> &value)
    {
        AttQueue(Att::Write(handle, value), 0);
    }

    void AapDevice::AttQueue(std::vector<unsigned char> pdu, unsigned char readHandle)
    {
        if (!_attClient || !_attClient->IsStarted())
            return;
        std::lock_guard lock{_attLock};
        if (auto send = _attQueue.Push(std::move(pdu), readHandle, std::chrono::steady_clock::now()))
            _attClient->SendData(*send);
    }

    void AapDevice::OnAttData(const std::vector<unsigned char> &data)
    {
        if (data.empty())
            return;
        std::optional<std::pair<unsigned char, std::vector<unsigned char>>> value;
        switch (data[0])
        {
        case Att::READ_RSP:
        case Att::WRITE_RSP:
        case Att::ERROR_RSP:
        {
            std::lock_guard lock{_attLock};
            auto [readHandle, next] = _attQueue.Answered(std::chrono::steady_clock::now());
            if (data[0] == Att::READ_RSP && readHandle != 0)
                value.emplace(readHandle, std::vector<unsigned char>(data.begin() + 1, data.end()));
            if (data[0] == Att::ERROR_RSP && data.size() >= 5)
                Logger::Info("%s: ATT request 0x%02x on handle 0x%02x refused (0x%02x)", GetName().c_str(), data[1], data[2], data[4]);
            if (next)
                _attClient->SendData(*next);
            break;
        }
        case Att::NOTIFY:
            if (data.size() >= 3)
                value.emplace(data[1], std::vector<unsigned char>(data.begin() + 3, data.end()));
            break;
        }
        if (value)
            _onAttValue.FireEvent(*value);
    }

    bool AapDevice::HasHeadTracking() const
    {
        return AapSpatialAudioCapability::HasHeadTracking(GetProductId());
    }

    std::vector<Biquad> AapDevice::MeasuredCorrection(unsigned short model)
    {
        // AutoEQ (github.com/jaakkopasanen/AutoEq, MIT) ParametricEQ.txt, measurement source in the comment;
        // the USB-C Max and Pro 2 share their predecessors' acoustics. Its preamp is left out, AudioEffects::HeadroomDb covers it.
        constexpr auto P = Biquad::Peaking, L = Biquad::LowShelf, H = Biquad::HighShelf;
        static const std::map<AapModelIds, std::vector<Biquad>> CORRECTIONS{
            {AapModelIds::airpodsmax, {{L, 105, -3.0, 0.70}, {P, 7273, 3.6, 2.41}, {P, 218, -2.9, 1.41}, {P, 1031, -3.2, 0.99}, {P, 3185, 3.1, 0.56}, {H, 10000, -5.5, 0.70}, {P, 9508, 2.7, 2.20}, {P, 66, 0.6, 1.65}, {P, 4045, 2.1, 5.82}, {P, 4834, -1.8, 6.00}}}, // oratory1990
            {AapModelIds::airpods1, {{L, 105, 3.2, 0.70}, {P, 9383, 4.7, 0.74}, {P, 526, -3.3, 1.28}, {P, 1979, -2.0, 0.92}, {P, 4485, 2.9, 2.30}, {H, 10000, -3.0, 0.70}, {P, 154, 0.9, 1.15}, {P, 47, -0.8, 1.53}, {P, 308, -0.6, 1.95}, {P, 102, 0.3, 2.03}}}, // oratory1990
            {AapModelIds::airpods2, {{L, 105, 5.8, 0.70}, {P, 5446, -6.5, 0.18}, {P, 2138, 8.3, 0.30}, {P, 89, 1.5, 0.98}, {P, 2124, -6.3, 1.11}, {H, 10000, -2.5, 0.70}, {P, 4173, 1.4, 2.74}, {P, 5484, -1.6, 6.00}, {P, 2846, -0.9, 4.81}, {P, 240, 0.2, 1.68}}}, // Rtings
            {AapModelIds::airpods3, {{L, 105, 6.9, 0.70}, {P, 1838, -3.9, 2.08}, {P, 3879, 2.5, 2.13}, {P, 64, -5.0, 0.96}, {P, 43, 2.7, 3.38}, {H, 10000, 5.5, 0.70}, {P, 9961, 2.4, 1.61}, {P, 6052, -2.7, 3.91}, {P, 724, 1.9, 2.80}, {P, 420, -0.6, 1.89}}}, // Rtings
            {AapModelIds::airpods4, {{L, 105, 11.4, 0.70}, {P, 4622, 6.0, 1.39}, {P, 49, -11.7, 0.39}, {P, 1277, -2.6, 0.86}, {P, 3036, 4.1, 2.43}, {H, 10000, -1.9, 0.70}, {P, 354, -1.4, 1.50}, {P, 176, 1.5, 2.04}, {P, 618, 1.0, 2.35}, {P, 106, -0.9, 2.47}}}, // Rtings
            {AapModelIds::airpods4anc, {{L, 105, 12.7, 0.70}, {P, 49, -13.2, 0.44}, {P, 3573, 6.0, 1.40}, {P, 1318, -2.8, 1.16}, {P, 5138, 3.2, 2.82}, {H, 10000, -2.0, 0.70}, {P, 166, 1.2, 3.49}, {P, 100, -0.7, 2.50}, {P, 438, -0.8, 2.38}, {P, 654, 0.6, 2.93}}}, // Rtings, ANC on
            {AapModelIds::airpodspro, {{L, 105, 2.6, 0.70}, {P, 514, -4.4, 0.68}, {P, 8903, 6.0, 1.66}, {P, 183, 2.0, 0.77}, {P, 4613, 3.4, 2.46}, {H, 10000, -0.5, 0.70}, {P, 1517, -1.0, 2.33}, {P, 929, 1.2, 2.75}, {P, 44, -0.5, 2.16}, {P, 618, -0.5, 2.86}}}, // crinacle
            {AapModelIds::airpodspro2, {{L, 105, 0.5, 0.70}, {P, 427, -2.6, 0.80}, {P, 3647, 2.3, 0.77}, {P, 9516, 2.8, 3.44}, {P, 76, 1.9, 1.34}, {H, 10000, -1.8, 0.70}, {P, 5938, 2.6, 1.27}, {P, 6486, -6.3, 5.92}, {P, 1172, 1.1, 5.05}, {P, 3326, -1.9, 4.40}}}, // crinacle, ANC on
            {AapModelIds::powerbeatspro, {{L, 105, -0.8, 0.70}, {P, 6596, 6.1, 2.51}, {P, 2703, -4.4, 2.80}, {P, 1362, -2.1, 1.94}, {P, 3607, 3.7, 4.72}, {H, 10000, 3.7, 0.70}, {P, 143, -1.2, 1.56}, {P, 374, 0.7, 1.21}, {P, 4688, -2.2, 6.00}, {P, 60, 0.4, 1.34}}}, // oratory1990
            {AapModelIds::beatssolopro, {{L, 105, -2.0, 0.70}, {P, 332, 1.9, 0.50}, {P, 3735, -4.7, 0.44}, {P, 2106, 5.4, 1.58}, {P, 5816, 5.0, 3.86}, {H, 10000, -1.5, 0.70}, {P, 58, -0.4, 1.36}, {P, 31, 0.4, 1.68}, {P, 163, 0.7, 3.66}, {P, 233, -0.4, 2.45}}}, // oratory1990
            {AapModelIds::beatsstudio3, {{L, 105, 7.7, 0.70}, {P, 327, -5.9, 1.56}, {P, 5209, 6.7, 2.64}, {P, 68, -8.1, 0.78}, {P, 1978, 4.3, 2.19}, {H, 10000, -2.5, 0.70}, {P, 3257, -1.9, 4.42}, {P, 666, 1.5, 2.52}, {P, 6665, 1.5, 3.92}, {P, 428, -1.1, 4.35}}}, // oratory1990
            {AapModelIds::beatsstudiobuds, {{L, 105, -2.2, 0.70}, {P, 1819, -4.7, 0.74}, {P, 145, 3.3, 0.23}, {P, 6271, 4.8, 1.67}, {P, 568, -2.4, 1.88}, {H, 10000, 0.3, 0.70}, {P, 3619, 2.3, 4.81}, {P, 2771, -1.7, 3.92}, {P, 68, 0.8, 1.75}, {P, 127, -0.8, 1.91}}}, // oratory1990
            {AapModelIds::beatsstudiobudsplus, {{L, 105, 10.6, 0.70}, {P, 702, 5.2, 0.41}, {P, 1352, -7.5, 0.72}, {P, 195, 2.1, 0.80}, {P, 42, -11.0, 0.47}, {H, 10000, -3.1, 0.70}, {P, 6487, -4.4, 6.00}, {P, 9846, -2.1, 2.19}, {P, 3674, 1.9, 4.28}, {P, 918, 1.1, 4.94}}}, // Rtings
            {AapModelIds::beatsstudiopro, {{L, 105, -3.1, 0.70}, {P, 8990, -5.1, 2.02}, {P, 304, 3.8, 0.85}, {P, 1720, -3.0, 1.19}, {P, 67, 6.0, 2.55}, {H, 10000, 0.5, 0.70}, {P, 5052, 3.7, 4.43}, {P, 3394, -2.5, 5.33}, {P, 6509, -2.0, 5.62}, {P, 734, 0.7, 3.38}}}, // Rtings
            {AapModelIds::beatsfitpro, {{L, 105, 1.8, 0.70}, {P, 313, 2.3, 0.84}, {P, 5866, -4.4, 4.07}, {P, 2445, -2.7, 2.06}, {P, 40, -3.7, 0.92}, {H, 10000, 1.0, 0.70}, {P, 1263, -1.4, 2.35}, {P, 4035, 1.8, 4.11}, {P, 5017, -1.4, 6.00}, {P, 942, 0.7, 4.33}}}, // Rtings
            {AapModelIds::beatsflex, {{L, 105, -5.0, 0.70}, {P, 3827, 5.8, 1.86}, {P, 168, -2.4, 1.04}, {P, 1260, -3.7, 2.62}, {P, 5114, 3.5, 3.66}, {H, 10000, -5.5, 0.70}, {P, 746, 1.2, 1.80}, {P, 988, -1.2, 4.10}, {P, 7408, 1.9, 5.43}, {P, 287, -0.5, 3.23}}}, // Rtings
            {AapModelIds::powerbeats3, {{L, 105, -4.4, 0.70}, {P, 3769, 4.9, 1.87}, {P, 1386, -3.1, 1.09}, {P, 500, 2.9, 1.21}, {P, 153, -2.2, 1.28}, {H, 10000, 2.9, 0.70}, {P, 6217, 3.2, 5.88}, {P, 5198, -2.1, 6.00}, {P, 8129, -1.6, 4.52}, {P, 2533, -0.6, 4.38}}}, // Rtings
            {AapModelIds::powerbeats4, {{L, 105, -1.4, 0.70}, {P, 5806, 4.7, 4.11}, {P, 799, 3.0, 1.23}, {P, 1254, -3.1, 1.29}, {P, 148, -1.5, 2.27}, {H, 10000, -1.3, 0.70}, {P, 3710, 4.2, 4.64}, {P, 2703, -2.7, 4.21}, {P, 8095, -1.6, 4.28}, {P, 60, 0.3, 1.81}}}, // Rtings
            {AapModelIds::beatssolobuds, {{L, 105, -0.2, 0.70}, {P, 425, 4.1, 1.19}, {P, 1890, -4.9, 0.80}, {P, 155, 2.2, 1.43}, {P, 42, -1.6, 1.25}, {H, 10000, 6.3, 0.70}, {P, 8413, 2.0, 3.52}, {P, 4999, -1.6, 5.02}, {P, 2837, -0.6, 4.40}, {P, 839, -0.2, 1.59}}}, // Rtings
            {AapModelIds::beatssolo4, {{L, 105, 1.4, 0.70}, {P, 920, 3.4, 0.19}, {P, 3842, -5.2, 1.98}, {P, 1054, -4.7, 0.97}, {P, 7703, -2.0, 0.67}, {H, 10000, 2.7, 0.70}, {P, 75, -1.4, 2.28}, {P, 9444, -1.1, 2.03}, {P, 137, 0.7, 2.00}, {P, 51, 1.3, 4.12}}}, // Rtings
        };
        auto id = static_cast<AapModelIds>(model);
        id = id == AapModelIds::airpodsmax2024 ? AapModelIds::airpodsmax : id == AapModelIds::airpodsprousbc ? AapModelIds::airpodspro2 : id;
        auto it = CORRECTIONS.find(id);
        return it == CORRECTIONS.end() ? std::vector<Biquad>{} : it->second;
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
        // Each appears once the AirPods report it, so models without the setting don't show it
        using Kind = AapControlCapability::Kind;
        device->capabilities.push_back(std::make_unique<AapControlCapability>("listeningModes", 0x1A, Kind::ListeningModes, *device));
        device->capabilities.push_back(std::make_unique<AapControlCapability>("allowOff", 0x34, Kind::Toggle, *device));
        device->capabilities.push_back(std::make_unique<AapControlCapability>("micMode", 0x01, Kind::Choice, *device, std::vector<int>{0, 1, 2}));
        device->capabilities.push_back(std::make_unique<AapControlCapability>("hearingAid", 0x2C, Kind::HearingAid, *device));
        device->capabilities.push_back(std::make_unique<AapControlCapability>("crownReversed", 0x1C, Kind::Toggle, *device));  // AirPods Max: 0x01 reversed
        device->capabilities.push_back(std::make_unique<AapControlCapability>("sleepDetection", 0x35, Kind::Toggle, *device)); // pause when you fall asleep
        device->capabilities.push_back(std::make_unique<AapControlCapability>("autoConnect", 0x20, Kind::Toggle, *device));
        if (HasAttSettings(deviceInfo->GetProductId()))
        {
            device->capabilities.push_back(std::make_unique<AapLoudSoundReductionCapability>(*device));
            device->capabilities.push_back(std::make_unique<AapTransparencyCapability>(*device));
        }
        // Handing the audio over is AAP smart routing; without it there is nothing to negotiate with
        if (Client::SupportsL2CAP())
            device->capabilities.push_back(std::make_unique<AapAudioSwitchCapability>(*device));
#ifndef _WIN32 // effects run in PipeWire (AudioEffects.h)
        device->capabilities.push_back(std::make_unique<AapSpatialAudioCapability>(*device));
        device->capabilities.push_back(std::make_unique<CmnEqualizerCapability>(*device));
#endif

        device->_clientStartData.push_back(AapInit{}.Request());
        device->_clientStartData.push_back(AapEnableNotifications{AapNotificationsMode::Unknown2}.Request());
        device->_clientStartData.push_back(AapEnableNotifications{AapNotificationsMode::Unknown1}.Request());
        if (AapInitExt::IsSupported(deviceInfo->GetProductId()))
            device->_clientStartData.push_back(AapInitExt{}.Request());
        // Asked on every connection: deleting the stored keys takes effect on the next one, and AirPods that
        // were reset and paired again come with new keys
        device->_clientStartData.push_back(AapPrivateKeys{}.Request());

        device->_client = Client::CreateL2CAP(deviceInfo->GetAddress(), 0x1001);
        if (Client::SupportsL2CAP() && HasAttSettings(deviceInfo->GetProductId()))
        {
            device->_attClient = Client::CreateL2CAP(deviceInfo->GetAddress(), Att::PSM);
            auto *raw = device.get();
            device->_attDataEventId = device->_attClient->GetOnReceivedDataEvent().Subscribe([raw](size_t, const std::vector<unsigned char> &data)
            {
                raw->OnAttData(data);
            });
        }

        device->Init();
        return device;
    }
}