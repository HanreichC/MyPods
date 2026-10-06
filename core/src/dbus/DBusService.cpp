// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "./dbus/DBusService.h"
#include "Logger.h"

#include <cstdlib>
#include <stdexcept>

namespace MagicPodsCore {

    // Set once in the constructor, before any thread reads it
    static std::string adapterPath;

    using ManagedObjects = std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>>;

    // The first adapter BlueZ knows ("/org/bluez/hci0" on most machines, not on all)
    static std::string FindAdapterPath(const ManagedObjects &objects) {
        for (const auto& [objectPath, interfaces] : objects)
            if (interfaces.contains("org.bluez.Adapter1"))
                return objectPath;
        return {};
    }

    DBusService::DBusService() : _rootProxy{sdbus::createProxy("org.bluez", "/")} {
        ManagedObjects managedObjects{};
        _rootProxy->callMethod("GetManagedObjects").onInterface("org.freedesktop.DBus.ObjectManager").storeResultsTo(managedObjects);
        adapterPath = FindAdapterPath(managedObjects);
        if (adapterPath.empty())
            throw std::runtime_error("no Bluetooth adapter");
        _defaultBluetoothAdapterProxy = sdbus::createProxy("org.bluez", adapterPath);
        Logger::Info("Bluetooth adapter %s", adapterPath.c_str());

        _rootProxy->uponSignal("InterfacesAdded").onInterface("org.freedesktop.DBus.ObjectManager").call([this](sdbus::ObjectPath objectPath, std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces) {
            TryCreateDevice(objectPath, interfaces);
            TryUpdateInterfaceAddedForDevice(objectPath, interfaces);
        });
        _rootProxy->uponSignal("InterfacesRemoved").onInterface("org.freedesktop.DBus.ObjectManager").call([this](sdbus::ObjectPath objectPath, std::vector<std::string> array) {
            if (objectPath == adapterPath && std::find(array.begin(), array.end(), "org.bluez.Adapter1") != array.end()) {
                // ponytail: every proxy and device hangs off this adapter; instead of rebuilding them all in place,
                // exit and let systemd (Restart=on-failure) or the UI start the daemon again with the next adapter
                Logger::Error("Bluetooth adapter %s went away, restarting", adapterPath.c_str());
                std::_Exit(1);
            }
            if (std::find(array.begin(), array.end(), "org.bluez.Device1") != array.end()) {
                TryRemoveDevice(objectPath);
            }
        });
        _rootProxy->finishRegistration();

        // bluetoothd crashing (or restarted) sends no InterfacesRemoved, it just drops off the bus
        _busProxy = sdbus::createProxy("org.freedesktop.DBus", "/org/freedesktop/DBus");
        _busProxy->uponSignal("NameOwnerChanged").onInterface("org.freedesktop.DBus").call([](std::string name, std::string oldOwner, std::string newOwner) {
            if (name == "org.bluez" && newOwner.empty()) {
                Logger::Error("bluetoothd went away, restarting");
                std::_Exit(1);
            }
        });
        _busProxy->finishRegistration();

        _defaultBluetoothAdapterProxy->uponSignal("PropertiesChanged").onInterface("org.freedesktop.DBus.Properties").call([this](std::string interfaceName, std::map<std::string, sdbus::Variant> values, std::vector<std::string> stringArray) {
            if (values.contains("Powered")) {
                _isBluetoothAdapterPowered.SetValue(values["Powered"].get<bool>());
            }
        });
        _defaultBluetoothAdapterProxy->finishRegistration();

        _isBluetoothAdapterPowered.SetValue(_defaultBluetoothAdapterProxy->getProperty("Powered").onInterface("org.bluez.Adapter1").get<bool>());

        for (const auto& [objectPath, interfaces] : managedObjects)
            TryCreateDevice(objectPath, interfaces);
    }

    std::string DBusService::GetAdapterPath() {
        return adapterPath;
    }

    std::string DBusService::GetAdapterAddress() {
        return sdbus::createProxy("org.bluez", adapterPath)->getProperty("Address").onInterface("org.bluez.Adapter1").get<std::string>();
    }

    std::string DBusService::GetAdapterAlias() {
        return sdbus::createProxy("org.bluez", adapterPath)->getProperty("Alias").onInterface("org.bluez.Adapter1").get<std::string>();
    }

    std::set<std::shared_ptr<DBusDeviceInfo>> DBusService::GetAllDevices() {
        std::lock_guard lock{_devicesLock};
        std::set<std::shared_ptr<DBusDeviceInfo>> devices;
        for (const auto& [path, device] : _knownDevices) {
            devices.emplace(device);
        }
        return devices;
    }

    std::set<std::shared_ptr<DBusDeviceInfo>> DBusService::GetPairedDevices() {
        std::lock_guard lock{_devicesLock};
        return _pairedDevices;
    }

    void DBusService::EnableBluetoothAdapter() {
        _defaultBluetoothAdapterProxy->setPropertyAsync("Powered").onInterface("org.bluez.Adapter1").toValue(true).uponReplyInvoke([this](const sdbus::Error* err) {});
    }

    void DBusService::EnableBluetoothAdapterAsync(BtCallback&& callback) {
        _defaultBluetoothAdapterProxy->setPropertyAsync("Powered").onInterface("org.bluez.Adapter1").toValue(true).uponReplyInvoke(ToSdbusCallback(std::move(callback)));
    }

    void DBusService::DisableBluetoothAdapter() {
        _defaultBluetoothAdapterProxy->setPropertyAsync("Powered").onInterface("org.bluez.Adapter1").toValue(false).uponReplyInvoke([this](const sdbus::Error* err) {});
    }

    void DBusService::DisableBluetoothAdapterAsync(BtCallback&& callback) {
        _defaultBluetoothAdapterProxy->setPropertyAsync("Powered").onInterface("org.bluez.Adapter1").toValue(false).uponReplyInvoke(ToSdbusCallback(std::move(callback)));
    }

    void DBusService::SetDiscoveryFilter(const std::map<std::string, sdbus::Variant> &filter) {
        if (!_defaultBluetoothAdapterProxy) {
            return;
        }
        _defaultBluetoothAdapterProxy->callMethod("SetDiscoveryFilter").onInterface("org.bluez.Adapter1").withArguments(filter);
    }

    void DBusService::SetDiscoveryFilterAsync(const std::map<std::string, sdbus::Variant> &filter, std::function<void(const sdbus::Error *)> &&callback) {
        if (!_defaultBluetoothAdapterProxy) {
            if (callback) {
                callback(nullptr);
            }
            return;
        }
        _defaultBluetoothAdapterProxy->callMethodAsync("SetDiscoveryFilter").onInterface("org.bluez.Adapter1").withArguments(filter).uponReplyInvoke(callback);
    }

    void DBusService::StartDiscovery() {
        if (!_defaultBluetoothAdapterProxy) {
            return;
        }
        _defaultBluetoothAdapterProxy->callMethod("StartDiscovery").onInterface("org.bluez.Adapter1");
    }

    void DBusService::StartDiscoveryAsync(std::function<void(const sdbus::Error*)>&& callback) {
        _defaultBluetoothAdapterProxy->callMethodAsync("StartDiscovery").onInterface("org.bluez.Adapter1").uponReplyInvoke(callback);
    }

    void DBusService::StopDiscovery() {
        _defaultBluetoothAdapterProxy->callMethod("StopDiscovery").onInterface("org.bluez.Adapter1").dontExpectReply();
    }

    void DBusService::StopDiscoveryAsync(std::function<void(const sdbus::Error*)>&& callback) {
        _defaultBluetoothAdapterProxy->callMethodAsync("StopDiscovery").onInterface("org.bluez.Adapter1").uponReplyInvoke(callback);
    }

    // Devices of our adapter only: BlueZ lists a device once per adapter that knows it
    static bool IsDevicePath(const std::string& objectPath) {
        static const std::regex DEVICE_RE{"^/dev(_[0-9A-F]{2}){6}$"};
        return objectPath.starts_with(adapterPath) && std::regex_match(objectPath.substr(adapterPath.size()), DEVICE_RE);
    }

    std::shared_ptr<DBusDeviceInfo> DBusService::TryCreateDevice(sdbus::ObjectPath objectPath, std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces) {
        if (IsDevicePath(objectPath)) {
            if (interfaces.contains("org.bluez.Device1")) {
                auto deviceInfo = std::make_shared<DBusDeviceInfo>(objectPath, interfaces);
                bool paired = deviceInfo->GetPairedStatus().GetValue();
                {
                    std::lock_guard lock{_devicesLock};
                    if (auto old = _knownDevices.find(objectPath); old != _knownDevices.end()) {
                        _pairedDevices.erase(old->second);
                        _knownDevices.erase(old);
                    }
                    _knownDevices.emplace(objectPath, deviceInfo);
                    if (paired)
                        _pairedDevices.emplace(deviceInfo);
                }
                _onAnyDeviceAddedEvent.FireEvent(deviceInfo);

                if (paired) {
                    _onDeviceAddedEvent.FireEvent(deviceInfo);
                }
                else {
                    // BlueZ reports Paired before SDP has delivered the UUIDs and Modalias the
                    // device type is derived from, so wait until the services are resolved too.
                    // On the device's own proxy thread, so the check and the insert happen under the lock.
                    auto tryAddPaired = [this, objectPath](size_t listenerId, bool newValue) {
                        std::shared_ptr<DBusDeviceInfo> added;
                        {
                            std::lock_guard lock{_devicesLock};
                            auto it = _knownDevices.find(objectPath);
                            if (it == _knownDevices.end())
                                return;
                            auto device = it->second;
                            if (device->GetPairedStatus().GetValue() && device->GetServicesResolved().GetValue() && _pairedDevices.emplace(device).second)
                                added = device;
                        }
                        if (added)
                            _onDeviceAddedEvent.FireEvent(added);
                    };
                    deviceInfo->GetPairedStatus().GetEvent().Subscribe(tryAddPaired);
                    deviceInfo->GetServicesResolved().GetEvent().Subscribe(tryAddPaired);
                    tryAddPaired(0, false); // paired and resolved before the listeners were there
                }

                return deviceInfo;
            }
        }
        return nullptr;
    }

    bool DBusService::TryRemoveDevice(sdbus::ObjectPath objectPath) {
        std::shared_ptr<DBusDeviceInfo> deviceInfo;
        {
            std::lock_guard lock{_devicesLock};
            auto it = _knownDevices.find(objectPath);
            if (it == _knownDevices.end())
                return false;
            deviceInfo = it->second;
        }
        _onDeviceRemovedEvent.FireEvent(deviceInfo); // listeners may still ask for the list with it in

        std::lock_guard lock{_devicesLock};
        _pairedDevices.erase(deviceInfo);
        if (auto it = _knownDevices.find(objectPath); it != _knownDevices.end() && it->second == deviceInfo)
            _knownDevices.erase(it);
        return true;
    }

    bool DBusService::TryUpdateInterfaceAddedForDevice(sdbus::ObjectPath objectPath, std::map<std::string, std::map<std::string, sdbus::Variant>> interfaces)
    {
        if (!IsDevicePath(objectPath))
            return false;
        std::shared_ptr<DBusDeviceInfo> device;
        {
            std::lock_guard lock{_devicesLock};
            auto it = _knownDevices.find(objectPath);
            if (it == _knownDevices.end())
                return false;
            device = it->second;
        }
        device->InterfaceAdded(interfaces);
        return true;
    }
    
    bool DBusService::TryUpdateInterfaceRemovedForDevice(sdbus::ObjectPath objectPath)
    {
        return false;
    }
}
