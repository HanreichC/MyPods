// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "ObservableVariable.h"

#include <regex>
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <array>
#include <memory>
#include <functional>
#ifndef _WIN32
#include <sdbus-c++/sdbus-c++.h>
#endif

namespace MagicPodsCore {

    // Reply of an asynchronous Bluetooth call: nullptr on success, the error message otherwise.
    using BtCallback = std::function<void(const std::string* error)>;

    // A paired Bluetooth device. Linux: a BlueZ object over D-Bus. Windows: WinRT's BluetoothDevice
    // (DBusDeviceInfo_win.cpp); the name stays so the rest of the daemon doesn't care which.
    class DBusDeviceInfo {
    private:
#ifdef _WIN32
        struct Native;
        std::unique_ptr<Native> _native;
        friend class DBusService;
#else
        std::unique_ptr<sdbus::IProxy> _deviceProxy{};
#endif

        std::string _address{};
        unsigned short _productId{};
        unsigned short _vendorId{};
        std::vector<std::string> _uuids{};
        std::optional<unsigned int> _clazz{};
        std::string _name{};
        ObservableVariable<bool> _connectionStatus{false};
        ObservableVariable<bool> _pairedStatus{false};
        ObservableVariable<bool> _servicesResolved{false};
        ObservableVariable<uint8_t> _handsFreeBatteryStatus{100};
        ObservableVariable<std::map<uint16_t, std::vector<uint8_t>>> _manufacturerData{{}};
        ObservableVariable<std::map<std::string, std::vector<uint8_t>>> _serviceData{{}};
        ObservableVariable<int16_t> _rssi{0};

    public:
#ifdef _WIN32
        explicit DBusDeviceInfo(uint64_t address);
        ~DBusDeviceInfo();
#else
        explicit DBusDeviceInfo(const sdbus::ObjectPath& objectPath, const std::map<std::string, std::map<std::string, sdbus::Variant>>& interfaces);
#endif

        DBusDeviceInfo(const DBusDeviceInfo& info) = delete;
        DBusDeviceInfo(DBusDeviceInfo&& info) noexcept = delete;
        DBusDeviceInfo& operator=(const DBusDeviceInfo& info) = delete;
        DBusDeviceInfo& operator=(DBusDeviceInfo&& info) noexcept = delete;

        const std::string& GetAddress() const {
            return _address;
        }

        unsigned short GetProductId() const {
            return _productId;
        }

        unsigned short GetVendorId() const {
            return _vendorId;
        }

        const std::vector<std::string> GetUuids() const {
            return _uuids;
        }

        const std::optional<unsigned int>& GetClass() const {
            return _clazz;
        }

        const std::string& GetName() const {
            return _name;
        }

        ObservableVariable<bool>& GetConnectionStatus() {
            return _connectionStatus;
        }

        ObservableVariable<bool>& GetPairedStatus() {
            return _pairedStatus;
        }

        ObservableVariable<bool>& GetServicesResolved() {
            return _servicesResolved;
        }

        ObservableVariable<uint8_t>& GetHandsFreeBatteryStatus() {
            return _handsFreeBatteryStatus;
        }

        ObservableVariable<std::map<uint16_t, std::vector<uint8_t>>>& GetManufacturerData() {
            return _manufacturerData;
        }

        ObservableVariable<std::map<std::string, std::vector<uint8_t>>>& GetServiceData() {
            return _serviceData;
        }

        ObservableVariable<int16_t>& GetRssi() {
            return _rssi;
        }

        void Connect();
        void ConnectAsync(BtCallback&& callback);
        void Disconnect();
        void DisconnectAsync(BtCallback&& callback);

#ifndef _WIN32
        void InterfaceAdded(const std::map<std::string, std::map<std::string, sdbus::Variant>>& interfaces);
#endif

        friend auto operator<=>(const DBusDeviceInfo& t1, const DBusDeviceInfo& t2) {
            return t1.GetAddress() <=> t2.GetAddress();
        }

        // "v004Cp200A" in a BlueZ modalias, "VID&0001004C_PID&200A" in a Windows hardware id
        static std::array<unsigned short, 2> ParseVidPid(const std::string& modalias);
    };

#ifndef _WIN32
    inline auto ToSdbusCallback(BtCallback callback) {
        return [callback](const sdbus::Error* error) {
            if (!callback)
                return;
            if (!error)
                return callback(nullptr);
            auto message = error->getMessage();
            callback(&message);
        };
    }
#endif

}