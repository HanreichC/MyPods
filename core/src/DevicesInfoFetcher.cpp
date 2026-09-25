// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "DevicesInfoFetcher.h"

#include "BtVendorIds.h"
#include "sdk/aap/AapHelper.h"
#include "sdk/sgb/GalaxyBudsHelper.h"
#include "StringUtils.h"
#include "Logger.h"
#include "device/GalaxyBudsDevice.h"
#include "device/AapDevice.h"
#include "device/BhfDevice.h"
#include "device/ZikDevice.h"
#ifdef _WIN32
#include "ble_ads/WinRtBleAdvertisingService.h"
#else
#include "ble_ads/DBusBasedBleAdvertisingService.h"
#endif

#include <regex>
#include <iostream>
#include <algorithm>

namespace MagicPodsCore {

    bool DevicesInfoFetcher::ShouldScan(bool animation, bool supportsL2CAP, bool anyAap, bool anyAapConnected, bool anyAutoSwitch)
    {
        if (!anyAap)
            return false; // AapDevice is the only consumer of BLE advertisements
        // Without AAP (Windows) the advertisements are all there is: battery and ear detection also while connected.
        if (!supportsL2CAP)
            return animation || anyAapConnected;
        // Connected, AAP carries battery and ear detection, and the scan's classic inquiry can make A2DP stutter.
        // Not connected, the advertisements drive the popup and tell automatic switching whether they are worn.
        // ponytail: with several AirPods paired, one connected pair pauses the scan for the others too
        return !anyAapConnected && (animation || anyAutoSwitch);
    }

    void DevicesInfoFetcher::UpdateBleState()
    {
        std::lock_guard<std::mutex> lock(_bleStateMutex);

        bool animation = _settingsService->GetValue<bool>("magicpods", "animation").value_or(true);
        bool anyAap = false, anyConnected = false, anyAutoSwitch = false;
        for (const auto& device : GetDevices()) {
            auto aap = std::dynamic_pointer_cast<AapDevice>(device);
            if (!aap)
                continue;
            anyAap = true;
            anyConnected = anyConnected || aap->GetConnected();
            anyAutoSwitch = anyAutoSwitch || aap->LoadSettingInt("autoSwitch").value_or(0) == 0;
        }
        bool shouldScan = ShouldScan(animation, Client::SupportsL2CAP(), anyAap, anyConnected, anyAutoSwitch);

        if (shouldScan == _bleScanActive)
            return;

        _bleScanActive = shouldScan;

        if (shouldScan) {
            _bleService->StartListening();
            _bleService->StartScan(true);
            Logger::Info("BLE scan started");
        }
        else {
            _bleService->StopScan();
            Logger::Info("BLE scan stopped");
        }
    }

    DevicesInfoFetcher::DevicesInfoFetcher(const std::shared_ptr<SettingsService> &settingsService): _settingsService{settingsService} {
        _audioClient = std::make_shared<PulseAudioClient>();
#ifdef _WIN32
        _bleService = std::make_shared<WinRtBleAdvertisingService>(_dbusService);
#else
        _bleService = std::make_shared<DBusBasedBleAdvertisingService>(_dbusService);
#endif
        _onSettingsChangeId = _settingsService->GetOnSettingUpdateEvent().Subscribe([this](size_t id, const UpdatedSettingNotification& notification){
            // the scan serves the popup and automatic switching
            if ((notification.GetContainerName() == "magicpods" && notification.GetSettingName() == "animation") ||
                notification.GetSettingName() == "autoSwitch")
                UpdateBleState();
        });

        ClearAndFillDevicesMap();

        UpdateBleState();

        for (auto& device : _dbusService.GetAllDevices()) {
            Logger::Info("Device with address (known): %s", device->GetAddress().c_str());
        }

        _dbusService.GetOnDeviceAddedEvent().Subscribe([this](size_t listenerId, const std::shared_ptr<DBusDeviceInfo>& addedDeviceInfo) {
            Logger::Info("Device with address (add): %s", addedDeviceInfo->GetAddress().c_str());
            std::shared_ptr<Device> device;
            {
                std::lock_guard lock{_devicesLock};
                if (_devicesMap.contains(addedDeviceInfo->GetAddress()))
                    return;
            }
            device = TryCreateDevice(addedDeviceInfo); // outside the lock: it connects and may take a while
            if (!device)
                return;
            {
                std::lock_guard lock{_devicesLock};
                _devicesMap.emplace(addedDeviceInfo->GetAddress(), device);
            }
            _onDeviceAddEvent.FireEvent(device);
            UpdateBleState();
            TrySelectNewActiveDevice(); // it may already be connected, so no Connected change will come
        });

        _dbusService.GetOnDeviceRemovedEvent().Subscribe([this](size_t listenerId, const std::shared_ptr<DBusDeviceInfo>& removedDeviceInfo) {
            std::shared_ptr<Device> device;
            {
                std::lock_guard lock{_devicesLock};
                auto it = _devicesMap.find(removedDeviceInfo->GetAddress());
                if (it != _devicesMap.end()) {
                    device = it->second;
                    _devicesMap.erase(it);
                }
            }
            if (device) {
                _onDeviceRemoveEvent.FireEvent(device); // after erase, listeners publish the list without it
                UpdateBleState();
            }

            TrySelectNewActiveDevice();
        });

        _dbusService.IsBluetoothAdapterPowered().GetEvent().Subscribe([this](size_t listenerId, bool newPoweredValue) {
            _onDefaultAdapterChangeEnabled.FireEvent(newPoweredValue);
        });
    }

DevicesInfoFetcher::~DevicesInfoFetcher()
{
    if (_bleScanActive)
        _bleService->StopScan();
    _settingsService->GetOnSettingUpdateEvent().Unsubscribe(_onSettingsChangeId);
}

    std::set<std::shared_ptr<Device>, DeviceComparator> DevicesInfoFetcher::GetDevices() const {
        std::lock_guard lock{_devicesLock};
        std::set<std::shared_ptr<Device>, DeviceComparator> devices{};
        for (const auto& [key, value] : _devicesMap) {
            devices.emplace(value);
        }
        return devices;
    }

    std::shared_ptr<Device> DevicesInfoFetcher::GetDevice(const std::string& deviceAddress) const {
        std::lock_guard lock{_devicesLock};
        auto it = _devicesMap.find(deviceAddress);
        return it != _devicesMap.end() ? it->second : nullptr;
    }

    std::shared_ptr<Device> DevicesInfoFetcher::GetActiveDevice() const {
        std::lock_guard lock{_devicesLock};
        return _activeDevice;
    }

    void DevicesInfoFetcher::Connect(const std::string& deviceAddress) {
        if (auto device = GetDevice(deviceAddress))
            device->Connect();
    }

    void DevicesInfoFetcher::Disconnect(const std::string& deviceAddress) {
        if (auto device = GetDevice(deviceAddress))
            device->Disconnect();
    }

    void DevicesInfoFetcher::SetCapabilities(const nlohmann::json &json)
    {
        Logger::Info("DevicesInfoFetcher::SetCapabilities");

        if (!json.contains("arguments") || !json["arguments"].contains("address") || !json["arguments"]["address"].is_string() ||
            !json["arguments"].contains("capabilities"))
        {
            Logger::Info("Error: missing required fields in SetCapabilities");
            return;
        }

        const auto& arguments = json.at("arguments");
        if (auto device = GetDevice(arguments.at("address").get<std::string>()))
            device->SetCapabilities(arguments.at("capabilities"));
    }

    void DevicesInfoFetcher::EnableBluetoothAdapter() {
        _dbusService.EnableBluetoothAdapter();
    }

    void DevicesInfoFetcher::EnableBluetoothAdapterAsync(BtCallback&& callback) {
        _dbusService.EnableBluetoothAdapterAsync(std::move(callback));
    }

    void DevicesInfoFetcher::DisableBluetoothAdapter() {
        _dbusService.DisableBluetoothAdapter();
    }

    void DevicesInfoFetcher::DisableBluetoothAdapterAsync(BtCallback&& callback) {
        _dbusService.DisableBluetoothAdapterAsync(std::move(callback));
    }

    std::shared_ptr<Device> DevicesInfoFetcher::TryCreateDevice(const std::shared_ptr<DBusDeviceInfo>& deviceInfo) {

        Logger::Debug("%s (%s)", deviceInfo->GetName().c_str(), deviceInfo->GetAddress().c_str());
        Logger::Debug("    Vendor: %d",deviceInfo->GetVendorId());
        Logger::Debug("    Product: %d",deviceInfo->GetProductId());
        Logger::Debug("    Services:");
        for (const auto& uuid : deviceInfo->GetUuids()) {
            Logger::Debug("        %s", uuid.c_str());
        }

        std::shared_ptr<Device> newDevice;
        if (AapHelper::IsAapDevice(deviceInfo->GetVendorId(), deviceInfo->GetProductId())){
            newDevice = AapDevice::Create(deviceInfo, _audioClient, _settingsService, _bleService);
            // the BLE scan pauses while AirPods are connected (UpdateBleState)
            newDevice->GetConnectedPropertyChangedEvent().Subscribe([this](size_t listenerId, bool newValue) {
                UpdateBleState();
            });
        }

        else if (std::pair<GalaxyBudsModelIds, std::string> keyPair{};
                 GalaxyBudsHelper::IsGalaxyBudsDevice(deviceInfo->GetUuids()) &&
                 ((keyPair = GalaxyBudsHelper::SearchModelColor(deviceInfo->GetUuids(), deviceInfo->GetName())).first != GalaxyBudsModelIds::Unknown))
        {
            newDevice = GalaxyBudsDevice::Create(deviceInfo,_audioClient, _settingsService, static_cast<unsigned short>(keyPair.first));
        }
        else if (ZikDevice::IsZikDevice(deviceInfo->GetUuids())) {
            newDevice = ZikDevice::Create(deviceInfo, _audioClient, _settingsService);
        }
        //search headphones with handsfree service, or A2DP sink only (DIY headphones without a microphone)
        else if (auto uuids = deviceInfo->GetUuids();
                std::find(uuids.begin(), uuids.end(), "0000111e-0000-1000-8000-00805f9b34fb") != uuids.end() ||
                std::find(uuids.begin(), uuids.end(), "0000110b-0000-1000-8000-00805f9b34fb") != uuids.end()) {
            newDevice = BhfDevice::Create(deviceInfo,_audioClient, _settingsService);
        }

        // the device that just connected becomes the active one, like on a Mac
        if (newDevice)
            newDevice->GetConnectedPropertyChangedEvent().Subscribe([this, address = deviceInfo->GetAddress()](size_t listenerId, bool connected) {
                TrySelectNewActiveDevice(connected ? address : std::string{});
            });
        return newDevice;
    }

    void DevicesInfoFetcher::ClearAndFillDevicesMap() {
        {
            std::lock_guard lock{_devicesLock};
            _devicesMap.clear();
            _activeDevice = nullptr;
        }

        for (const auto& deviceInfo : _dbusService.GetPairedDevices()) {
            if (auto device = TryCreateDevice(deviceInfo)) {
                {
                    std::lock_guard lock{_devicesLock};
                    _devicesMap.emplace(deviceInfo->GetAddress(), device);
                }
                _onDeviceAddEvent.FireEvent(device);
            }
        }

        TrySelectNewActiveDevice();

        Logger::Info("Devices created: %zu", GetDevices().size());
    }

    bool DevicesInfoFetcher::SetActiveDevice(const std::string& address) {
        auto device = GetDevice(address);
        if (!device || !device->GetConnected())
            return false;
        TrySelectNewActiveDevice(address);
        return true;
    }

    void DevicesInfoFetcher::TrySelectNewActiveDevice(const std::string& preferred) {
        std::shared_ptr<Device> previousActiveDevice, newActiveDevice;
        {
            std::lock_guard lock{_devicesLock};
            previousActiveDevice = _activeDevice;

            if (auto it = _devicesMap.find(preferred); it != _devicesMap.end() && it->second->GetConnected())
                _activeDevice = it->second;

            if (_activeDevice != nullptr && (!_devicesMap.contains(_activeDevice->GetAddress()) || !_activeDevice->GetConnected()))
                _activeDevice = nullptr;

            if (_activeDevice == nullptr) {
                for (auto& [address, device] : _devicesMap) {
                    if (device->GetConnected())
                        _activeDevice = device;
                }
            }
            newActiveDevice = _activeDevice;
        }

        if (previousActiveDevice != newActiveDevice)
            _onActiveDeviceChangedEvent.FireEvent(newActiveDevice);
    }
}
