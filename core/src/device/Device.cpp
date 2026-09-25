// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "Device.h"
#include "DevicesInfoFetcher.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace MagicPodsCore {
    // A channel the headphones keep closing (another tool holds it, firmware quirk) isn't retried forever
    static constexpr int MAX_RESTARTS_PER_CONNECTION = 5;

    void Device::SubscribeCapabilitiesChanges()
    {
        for (auto& c: capabilities){
            size_t id = c->GetChangedEvent().Subscribe([this](size_t id, const Capability &capability)
            {
                _onCapabilityChangedEvent.FireEvent(capability);
                Logger::Debug("%s: capability: %s changed", this->GetName().c_str(), capability.GetName().c_str());
            });
            capabilityEventIds.push_back(id);
        }
    }

    void Device::UnsubscribeCapabilitiesChanges()
    {
        if (capabilityEventIds.size() != capabilities.size())
            throw std::runtime_error("Size of capabilityEventIds and capabilities different");

        for (int i=0; i<capabilities.size(); i++){
            auto& c = capabilities[i];
            c->GetChangedEvent().Unsubscribe(capabilityEventIds[i]);
        }
        capabilityEventIds.clear();
    }

    std::string Device::GetContainerName()
    {
        std::string name = GetAddress();
        std::replace(name.begin(), name.end(), ':', '_');
        return name;
    }

    Device::Device(std::shared_ptr<DBusDeviceInfo> deviceInfo, std::shared_ptr<PulseAudioClient> audioClient, std::shared_ptr<SettingsService> settingsService) : _deviceInfo{deviceInfo}, _audioClient{audioClient}, _settingsService{settingsService}
    {
    }

    void Device::Init()
    {
        Logger::Info("%s: Init", GetName().c_str());
        SubscribeCapabilitiesChanges();

        if (_client){
            clientReceivedDataEventId = _client->GetOnReceivedDataEvent().Subscribe([this](size_t id, const std::vector<unsigned char> &data)
            { OnResponseDataReceived(data); });
            clientClosedEventId = _client->GetOnClosedEvent().Subscribe([this](size_t id, bool)
            {
                std::lock_guard lock{_workerLock};
                if (++_restarts > MAX_RESTARTS_PER_CONNECTION) {
                    Logger::Error("%s control channel keeps closing, giving up until the next connection", GetName().c_str());
                    return;
                }
                _startRequested = true;
                _startDelayed = true;
                _workerWake.notify_one();
            });
            _clientWorker = std::thread([this]() { ClientWorker(); });
        }

        _deviceHandsFreeBatteryStatusChangedEvent = _deviceInfo->GetHandsFreeBatteryStatus().GetEvent().Subscribe([this](size_t listener_id, uint8_t newBatteryValue) {
            Logger::Debug("%s: PropertiesChanged:HandsFreeBattery %u",GetName().c_str(), newBatteryValue);
            _onHandsFreeBatteryPropertyChangedEvent.FireEvent(newBatteryValue);
        });

        _deviceConnectedStatusChangedEvent = _deviceInfo->GetConnectionStatus().GetEvent().Subscribe([this](size_t listenerId, bool newConnectedValue) {
            if (_connected != newConnectedValue) {
                _connected = newConnectedValue;
                Logger::Debug("%s: PropertiesChanged:Connected %s",GetName().c_str(), _connected ? "true" : "false");
                _onConnectedPropertyChangedEvent.FireEvent(_connected);
            }
            if (_client){
                if (_connected){
                    RequestClientStart(false);
                }
                else{
                    std::lock_guard lock{_workerLock};
                    _stopRequested = true;
                    _workerWake.notify_one();
                }
            }
        });

        _connected = _deviceInfo->GetConnectionStatus().GetValue();
        Logger::Debug("%s: Init:Connected %s",GetName().c_str(), _connected ? "true" : "false");
        if (_connected && _client)
            RequestClientStart(false);
    }

    void Device::RequestClientStart(bool delayed)
    {
        std::lock_guard lock{_workerLock};
        if (!delayed)
            _restarts = 0;
        _startRequested = true;
        _startDelayed = delayed;
        _workerWake.notify_one();
    }

    void Device::ClientWorker()
    {
        std::unique_lock lock{_workerLock};
        while (true) {
            _workerWake.wait(lock, [this]() { return _startRequested || _stopRequested || _workerExit; });
            if (_workerExit)
                return;
            // before a pending start: disconnected and connected again stops the old session first
            if (std::exchange(_stopRequested, false)) {
                lock.unlock();
                _client->Stop();
                OnClientStopped();
                Logger::Info("%s _client stopped from PropertiesChanged", GetName().c_str());
                lock.lock();
                continue;
            }
            bool delayed = std::exchange(_startDelayed, false);
            _startRequested = false;
            // give the headphones a moment after they closed the channel
            if (delayed && _workerWake.wait_for(lock, std::chrono::seconds(1), [this]() { return _workerExit; }))
                return;
            lock.unlock();
            if (GetConnected())
                StartClient();
            lock.lock();
        }
    }

    void Device::StartClient()
    {
        bool started = _client->Start([this](Client& _client) {
            for (auto& data: this->_clientStartData){
                _client.SendData(data);
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        });
        // audio keeps working without the control channel; the next connection tries again
        if (started) {
            Logger::Info("%s _client started", GetName().c_str());
            OnClientStarted();
        }
        else
            Logger::Error("%s control channel unavailable, device settings stay off until it reconnects", GetName().c_str());
    }

    void Device::Shutdown()
    {
        _deviceInfo->GetConnectionStatus().GetEvent().Unsubscribe(_deviceConnectedStatusChangedEvent);
        _deviceInfo->GetHandsFreeBatteryStatus().GetEvent().Unsubscribe(_deviceHandsFreeBatteryStatusChangedEvent);

        if (_clientWorker.joinable()) {
            {
                std::lock_guard lock{_workerLock};
                _workerExit = true;
                _workerWake.notify_one();
            }
            _clientWorker.join();
        }

        // the reading thread feeds the capabilities, so it has to be gone before they are
        if (_client) {
            _client->Stop();
            OnClientStopped();
            _client->GetOnReceivedDataEvent().Unsubscribe(clientReceivedDataEventId);
            _client->GetOnClosedEvent().Unsubscribe(clientClosedEventId);
        }
    }

    Device::~Device()
    {
        Shutdown(); // idempotent; derived classes with a reading thread already called it
        UnsubscribeCapabilitiesChanges();
        capabilities.clear();
        Logger::Debug("Device::~Device");
    }

    void Device::Connect() {
        _deviceInfo->Connect();
    }

    void Device::ConnectAsync(BtCallback&& callback) {
        _deviceInfo->ConnectAsync(std::move(callback));
    }

    void Device::Disconnect() {
        _deviceInfo->Disconnect();
    }

    void Device::DisconnectAsync(BtCallback&& callback) {
        _deviceInfo->DisconnectAsync(std::move(callback));
    }

    void Device::SetCapabilities(const nlohmann::json &json)
    {
        for (auto& capability : capabilities)
        {
            capability->SetFromJson(json);
        }
    }

    void Device::SaveSettingString(const std::string &settingName, const std::string &value)
    {
        _settingsService->SaveSetting(GetContainerName(), settingName, value);
    }

    std::optional<std::string> Device::LoadSettingString(const std::string &settingName)
    {
        return _settingsService->GetValue<std::string>(GetContainerName(), settingName);
    }

    void Device::SaveSettingInt(const std::string &settingName, const int64_t value)
    {
        _settingsService->SaveSetting(GetContainerName(), settingName, value);
    }

    std::optional<int64_t> Device::LoadSettingInt(const std::string &settingName)
    {
        return _settingsService->GetValue<int64_t>(GetContainerName(), settingName);
    }

    EffectsConfig Device::LoadEffectsConfig()
    {
        EffectsConfig config;
        config.spatial = static_cast<SpatialMode>(std::clamp<int64_t>(LoadSettingInt("spatialAudio").value_or(0), 0, HasHeadTracking() ? 2 : 1));
        config.surround = LoadSettingInt("surround").value_or(0) != 0;
        if (auto gains = AudioEffects::Preset(LoadSettingString("equalizer").value_or("Off")))
            config.eq = *gains;
        config.correction = Correction();
        config.corrected = LoadSettingInt("headphoneCorrection").value_or(0) != 0;
        config.crossfeed = LoadSettingInt("crossfeed").value_or(0) != 0;
        config.loudness = LoadSettingInt("loudness").value_or(0) != 0;
        config.loudnessReference = std::clamp(static_cast<double>(LoadSettingInt("loudnessReference").value_or(90)), 60.0, 120.0);
        if (LoadSettingInt("hearingProfile").value_or(0) != 0)
            for (int ear : {0, 1})
                if (auto thresholds = AudioEffects::ParseAudiogram(LoadSettingString(ear ? "audiogramRight" : "audiogramLeft").value_or("")))
                    config.hearing[ear] = AudioEffects::HearingGains(*thresholds);
        config.bypass = effectsBypass;
        config.sofa = LoadSettingString("sofa").value_or("");
        return config;
    }

    std::vector<Biquad> Device::Correction()
    {
        auto path = LoadSettingString("eqFile").value_or("");
        std::error_code ec;
        // a regular file only: a FIFO would block the WebSocket thread that builds the capability JSON
        if (!path.empty() && std::filesystem::is_regular_file(path, ec))
        {
            std::ifstream file(path);
            std::string text(64 * 1024, '\0'); // ParametricEQ.txt is a few hundred bytes; don't slurp whatever the path points at
            file.read(text.data(), text.size());
            text.resize(file.gcount());
            if (auto filters = AudioEffects::ParseParametricEq(text))
                return *filters;
        }
        static std::mutex reportLock; // asked on every capability update from several threads, said once
        static std::string reported;
        std::lock_guard guard{reportLock};
        if (!path.empty() && reported != path)
            Logger::Error("%s: %s is no readable ParametricEQ.txt, using the built-in correction", GetName().c_str(), path.c_str());
        reported = path;
        return ModelCorrection();
    }

    double Device::ListeningVolume()
    {
        auto pac = GetAudioClient();
        std::string mac = GetAddress();
        std::replace(mac.begin(), mac.end(), ':', '_');
        auto sink = pac->FindSink("bluez_output." + mac);
        double volume = sink ? pac->GetSinkVolume(*sink).value_or(1) : 1;
        return volume * pac->GetSinkVolume(AudioEffects::SINK_NAME).value_or(1);
    }

    void Device::RouteAudio()
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
        auto config = LoadEffectsConfig();
        if (config.loudness)
            config.volume = ListeningVolume();
        auto target = AudioEffects::Instance().Apply(*sink, GetName(), config);
        // the chain's sink appears a moment after its process starts
        for (int i = 0; i < 30 && !pac->FindSink(target); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pac->SetDefaultSink(target);
        Logger::Info("%s: audio routed to %s", GetName().c_str(), target.c_str());
    }

    void Device::RouteAudioAsync()
    {
        if (ownsAudio && GetConnected())
            std::thread([this, keep = KeepAlive()]() { RouteAudio(); }).detach();
    }

    nlohmann::json Device::GetAsJson()
    {
        auto capabilitiesJson = nlohmann::json::object();
        auto deviceJson = nlohmann::json::object();

        deviceJson["name"] = GetName();
        deviceJson["address"] = GetAddress();
        deviceJson["connected"] = GetConnected();
        deviceJson["model"] = GetProductId();
        deviceJson["vendor"] = GetVendorId();

        std::optional<int64_t> settingColor = LoadSettingInt("color");
        deviceJson["color"] = settingColor.has_value()? settingColor.value() : 0;

        for (auto& capability : capabilities)
        {
            auto capabilityJson = capability->GetAsJson();
            if (!capabilityJson.empty())
                capabilitiesJson.update(capabilityJson);
        }

        deviceJson["capabilities"] = capabilitiesJson;

        return deviceJson;
    }
}
