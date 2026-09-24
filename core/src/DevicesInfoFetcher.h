// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "device/Device.h"
#include "Event.h"
#include "./dbus/DBusService.h"
#include "./pulseaudio/PulseAudioClient.h"
#include "./ble_ads/BleAdvertisingService.h"

#include <string>
#include <vector>
#include <regex>
#include <functional>
#include <set>
#include <map>
#include <array>
#include <mutex>
#include <nlohmann/json.hpp>
#include "settings/SettingsService.h"

namespace MagicPodsCore {

    class DeviceComparator {
    public:
        bool operator()(const std::shared_ptr<Device>& device1, const std::shared_ptr<Device>& device2) const {
            return device1->GetAddress() < device2->GetAddress();
        }
    };

    class DevicesInfoFetcher {
    private:
        DBusService _dbusService{};
        std::shared_ptr<PulseAudioClient> _audioClient{};
        std::shared_ptr<BleAdvertisingService> _bleService{};
        std::shared_ptr<SettingsService> _settingsService{};
        size_t _onSettingsChangeId = 0;
        bool _bleScanActive = false;
        std::mutex _bleStateMutex{};
        void UpdateBleState();

        // Changed on the D-Bus thread, read by the WebSocket loop: every access holds _devicesLock,
        // events fire after it is released
        mutable std::mutex _devicesLock{};
        std::map<std::string, std::shared_ptr<Device>> _devicesMap{}; // address -> device
        std::shared_ptr<Device> _activeDevice{};

        Event<std::shared_ptr<Device>> _onActiveDeviceChangedEvent{};
        Event<std::shared_ptr<Device>> _onDeviceAddEvent{};
        Event<std::shared_ptr<Device>> _onDeviceRemoveEvent{};
        Event<bool> _onDefaultAdapterChangeEnabled{};

    public:
        DevicesInfoFetcher(const std::shared_ptr<SettingsService> &settingsService);
        ~DevicesInfoFetcher();
        DevicesInfoFetcher(const DevicesInfoFetcher&) = delete;
        DevicesInfoFetcher& operator=(const DevicesInfoFetcher&) = delete;

        // Whether the BLE advertisement scan runs (see the .cpp for why)
        static bool ShouldScan(bool animation, bool supportsL2CAP, bool anyAap, bool anyAapConnected, bool anyAutoSwitch);

        std::set<std::shared_ptr<Device>, DeviceComparator> GetDevices() const;
        std::shared_ptr<Device> GetDevice(const std::string& deviceAddress) const;
        std::shared_ptr<Device> GetActiveDevice() const;
        // Makes a connected device the one the UI shows and the broadcasts are about; false if it isn't connected
        bool SetActiveDevice(const std::string& address);

        void Connect(const std::string& deviceAddress);
        void Disconnect(const std::string& deviceAddress);
        void SetCapabilities(const nlohmann::json& json);

        bool IsBluetoothAdapterPowered() {
            return _dbusService.IsBluetoothAdapterPowered().GetValue();
        }
        void EnableBluetoothAdapter(); // TODO: выпилить?
        void EnableBluetoothAdapterAsync(BtCallback&& callback);
        void DisableBluetoothAdapter(); // TODO: выпилить?
        void DisableBluetoothAdapterAsync(BtCallback&& callback);

        Event<std::shared_ptr<Device>>& GetOnActiveDeviceChangedEvent() {
            return _onActiveDeviceChangedEvent;
        }

        Event<std::shared_ptr<Device>>& GetOnDeviceAddEvent() {
            return _onDeviceAddEvent;
        }

        Event<std::shared_ptr<Device>>& GetOnDeviceRemoveEvent() {
            return _onDeviceRemoveEvent;
        }

        Event<bool>& GetOnDefaultAdapterChangeEnabledEvent() {
            return _onDefaultAdapterChangeEnabled;
        }

    private:
        std::shared_ptr<Device> TryCreateDevice(const std::shared_ptr<DBusDeviceInfo>& deviceInfo);

        void ClearAndFillDevicesMap();
        // `preferred` (an address) becomes active if connected; otherwise the active one stays while connected
        void TrySelectNewActiveDevice(const std::string& preferred = {});

        public:
            static std::array<unsigned short, 2>ParseVidPid(const std::string& modalias);
    };
}
