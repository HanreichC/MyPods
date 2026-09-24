// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "DBusDeviceInfo.h"
#include "ObservableVariable.h"

#include <memory>
#include <map>
#include <set>
#include <vector>
#include <regex>
#include <iostream>

namespace MagicPodsCore {

    // The Bluetooth adapter and its paired devices. Linux: BlueZ over D-Bus. Windows: WinRT (DBusService_win.cpp).
    class DBusService {
    private:
#ifdef _WIN32
        struct Native;
        std::unique_ptr<Native> _native;
        std::map<std::string, std::shared_ptr<DBusDeviceInfo>> _knownDevices{}; // WinRT device id -> device
#else
        std::unique_ptr<sdbus::IProxy> _rootProxy{};
        std::unique_ptr<sdbus::IProxy> _defaultBluetoothAdapterProxy{};
        std::unique_ptr<sdbus::IProxy> _busProxy{}; // watches bluetoothd leaving the bus

        std::map<sdbus::ObjectPath, std::shared_ptr<DBusDeviceInfo>> _knownDevices{};
#endif
        std::set<std::shared_ptr<DBusDeviceInfo>> _pairedDevices{};

        Event<std::shared_ptr<DBusDeviceInfo>> _onDeviceAddedEvent{};
        Event<std::shared_ptr<DBusDeviceInfo>> _onAnyDeviceAddedEvent{};
        Event<std::shared_ptr<DBusDeviceInfo>> _onDeviceRemovedEvent{};

        ObservableVariable<bool> _isBluetoothAdapterPowered{false};

    public:
        // Linux: uses BlueZ's first adapter, throws std::runtime_error if there is none
        explicit DBusService();
#ifdef _WIN32
        ~DBusService();
#else
        // D-Bus path of the adapter in use, e.g. "/org/bluez/hci0"
        static std::string GetAdapterPath();
#endif

        // Address of this computer's adapter, "AA:BB:CC:DD:EE:FF". Throws if there is none.
        static std::string GetAdapterAddress();
        // The adapter's name as other Bluetooth devices see it. Throws if there is none.
        static std::string GetAdapterAlias();

        std::set<std::shared_ptr<DBusDeviceInfo>> GetAllDevices();
        std::set<std::shared_ptr<DBusDeviceInfo>> GetPairedDevices();

        ObservableVariable<bool>& IsBluetoothAdapterPowered() {
            return _isBluetoothAdapterPowered;
        }

        void EnableBluetoothAdapter();
        void EnableBluetoothAdapterAsync(BtCallback&& callback);
        void DisableBluetoothAdapter();
        void DisableBluetoothAdapterAsync(BtCallback&& callback);

#ifndef _WIN32
        void SetDiscoveryFilter(const std::map<std::string, sdbus::Variant>& filter);
        void SetDiscoveryFilterAsync(const std::map<std::string, sdbus::Variant>& filter, std::function<void(const sdbus::Error*)>&& callback);

        void StartDiscovery();
        void StartDiscoveryAsync(std::function<void(const sdbus::Error*)>&& callback);
        void StopDiscovery();
        void StopDiscoveryAsync(std::function<void(const sdbus::Error*)>&& callback);
#endif

        Event<std::shared_ptr<DBusDeviceInfo>>& GetOnDeviceAddedEvent()
        {
            return _onDeviceAddedEvent;
        }

        Event<std::shared_ptr<DBusDeviceInfo>>& GetOnAnyDeviceAddedEvent()
        {
            return _onAnyDeviceAddedEvent;
        }

        Event<std::shared_ptr<DBusDeviceInfo>>& GetOnDeviceRemovedEvent()
        {
            return _onDeviceRemovedEvent;
        }

    private:
#ifdef _WIN32
        void TryCreateDevice(const std::string& id);
        void TryRemoveDevice(const std::string& id);
#else
        std::shared_ptr<DBusDeviceInfo> TryCreateDevice(sdbus::ObjectPath objectPath, std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces);
        bool TryRemoveDevice(sdbus::ObjectPath objectPath);

        bool TryUpdateInterfaceAddedForDevice(sdbus::ObjectPath objectPath, std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces);
        bool TryUpdateInterfaceRemovedForDevice(sdbus::ObjectPath objectPath);
#endif
    };

}