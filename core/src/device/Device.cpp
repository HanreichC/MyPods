// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "Device.h"
#include "DevicesInfoFetcher.h"

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
                    _client->Stop();
                    OnClientStopped();
                    Logger::Info("%s _client stopped from PropertiesChanged", GetName().c_str());
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
            _workerWake.wait(lock, [this]() { return _startRequested || _workerExit; });
            if (_workerExit)
                return;
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
