#include "ZikDevice.h"
#include "capabilities/zik/ZikCapabilities.h"
#include "capabilities/cmn/CmnBluetoothCodecCapability.h"

#include <algorithm>

namespace MagicPodsCore
{
    ZikDevice::ZikDevice(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService)
        : Device(deviceInfo, audioClient, settingsService)
    {
        GetConnectedPropertyChangedEvent().Subscribe([this](size_t, bool) { ResetSession(); });

        // The battery only notifies on state changes (charging/in use), the percentage has to be polled.
        // Also frees the queue if the Zik swallowed a request, so one lost answer can't stall it.
        _poller = std::jthread([this](std::stop_token stop) {
            std::mutex m;
            std::unique_lock lock{m};
            while (true)
            {
                // after a stop request wait_for returns false at once, so the stop has to end the loop itself
                _pollWake.wait_for(lock, stop, std::chrono::seconds(30), [] { return false; });
                if (stop.stop_requested())
                    return;
                if (!_client || !_client->IsStarted())
                    continue;
                {
                    std::lock_guard q{_queueMutex};
                    if (_inFlight && std::chrono::steady_clock::now() - _sentAt > std::chrono::seconds(5))
                    {
                        Logger::Warn("%s: no answer from Zik, skipping request", GetName().c_str());
                        _inFlight = false;
                        SendNextLocked();
                    }
                }
                Get("/api/system/battery");
            }
        });
    }

    void ZikDevice::ResetSession()
    {
        std::lock_guard lock{_queueMutex};
        _pending.clear();
        _inFlight = false;
        _framer.Reset();
    }

    void ZikDevice::Query(const std::string &query)
    {
        std::lock_guard lock{_queueMutex};
        // notifications and polls can ask for the same thing repeatedly, once is enough
        if (std::find(_pending.begin(), _pending.end(), query) == _pending.end())
            _pending.push_back(query);
        if (!_inFlight)
            SendNextLocked();
    }

    void ZikDevice::SendNextLocked()
    {
        if (_pending.empty() || !_client)
            return;
        Logger::Debug("%s: zik > %s", GetName().c_str(), _pending.front().c_str());
        _client->SendData(Zik::EncodeRequest(_pending.front()));
        _pending.pop_front();
        _inFlight = true;
        _sentAt = std::chrono::steady_clock::now();
    }

    void ZikDevice::OnResponseDataReceived(const std::vector<unsigned char> &data)
    {
        std::vector<Zik::Message> messages;
        {
            std::lock_guard lock{_queueMutex};
            messages = _framer.Feed(data);
        }

        for (const auto &m : messages)
        {
            if (m.id == Zik::Ack)
            {
                Logger::Info("%s: Zik session open", GetName().c_str());
                for (const auto &q : _initialQueries)
                    Query(q);
                continue;
            }
            if (m.id != Zik::Request)
                continue;

            Logger::Debug("%s: zik < %s", GetName().c_str(), m.xml.c_str());
            // "/api/audio/noise_control/get" -> fetch "/api/audio/noise_control" again.
            // A notify can also ride inside an answer, which still completes the request.
            if (auto path = Zik::Attr(m.xml, "notify", "path"))
                Get(path->substr(0, path->rfind('/')));
            if (!Zik::IsAnswer(m.xml))
                continue;
            if (Zik::BoolAttr(m.xml, "answer", "error") == true)
                Logger::Warn("%s: Zik rejected %s", GetName().c_str(), Zik::Attr(m.xml, "answer", "path").value_or("?").c_str());
            if (auto version = Zik::Attr(m.xml, "software", "sip6"))
                Logger::Info("%s: Zik firmware %s", GetName().c_str(), version->c_str());

            _onAnswer.FireEvent(m.xml);
            std::lock_guard lock{_queueMutex};
            _inFlight = false;
            SendNextLocked();
        }
    }

    bool ZikDevice::IsZikDevice(const std::vector<std::string> &uuids)
    {
        return std::find(uuids.begin(), uuids.end(), Zik::ServiceUuid) != uuids.end();
    }

    std::unique_ptr<ZikDevice> ZikDevice::Create(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService)
    {
        auto device = std::make_unique<ZikDevice>(deviceInfo, audioClient, settingsService);

        const std::vector<std::string> onOff{"false", "true"};
        const std::vector<ZikSetting> settings{
            {"earDetection", "/api/system/head_detection/enabled", "head_detection", "enabled", onOff},
            {"concertHall", "/api/audio/sound_effect/enabled", "sound_effect", "enabled", onOff},
            {"concertHallRoom", "/api/audio/sound_effect/room_size", "sound_effect", "room_size", {"silent", "living", "jazz", "concert"}},
            {"concertHallAngle", "/api/audio/sound_effect/angle", "sound_effect", "angle", {"30", "60", "90", "120", "150", "180"}},
            {"smartAudioTune", "/api/audio/smart_audio_tune", "smart_audio_tune", "enabled", onOff},
            {"ancPhoneMode", "/api/system/anc_phone_mode/enabled", "anc_phone_mode", "enabled", onOff},
            {"autoConnection", "/api/system/auto_connection/enabled", "auto_connection", "enabled", onOff},
            // answers <tts enabled="true"/>; <software tts="none"/> in the version answer is something else
            {"voicePrompts", "/api/software/tts", "tts", "enabled", onOff, true},
            // minutes, 0 = never; the values the Parrot app offers
            {"autoPowerOff", "/api/system/auto_power_off", "auto_power_off", "value", {"0", "5", "10", "15", "30", "60"}},
        };

        device->capabilities.push_back(std::make_unique<CmnBluetoothCodecCapability>(*device));
        device->capabilities.push_back(std::make_unique<ZikBatteryCapability>(*device));
        device->capabilities.push_back(std::make_unique<ZikAncCapability>(*device));
        device->capabilities.push_back(std::make_unique<ZikEqualizerCapability>(*device));
        for (const auto &s : settings)
        {
            device->capabilities.push_back(std::make_unique<ZikSettingCapability>(*device, s));
            // sound_effect/get answers enabled, room_size and angle at once
            if (s.element != "sound_effect")
                device->_initialQueries.push_back(s.path + "/get");
        }
        for (const char *path : {"/api/software/version", "/api/system/battery", "/api/audio/noise_control/enabled",
                                 "/api/audio/noise_control", "/api/audio/equalizer/enabled", "/api/audio/sound_effect"})
            device->_initialQueries.push_back(std::string{path} + "/get");

        device->_clientStartData.push_back(Zik::Frame(Zik::OpenSession));
        device->_client = Client::CreateRFCOMM(deviceInfo->GetAddress(), Zik::ServiceUuid);
        device->Init();
        return device;
    }
}
