// MyPods
// License: GPL-3.0

// Windows side of DBusService: paired classic Bluetooth devices from a DeviceWatcher, the adapter's
// power state from its radio. Events fire on WinRT thread-pool threads, as they fire on the sd-bus
// thread on Linux.

#include "DBusService.h"
#include "Logger.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Radios.h>

#include <mutex>

namespace MagicPodsCore {

    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Enumeration;
    using namespace winrt::Windows::Devices::Radios;

    struct DBusService::Native {
        Radio radio{nullptr};
        DeviceWatcher watcher{nullptr};
        std::recursive_mutex lock; // watcher callbacks run concurrently
    };

    static std::string FormatAddress(uint64_t address) {
        char buf[18];
        std::snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X",
                      unsigned(address >> 40) & 0xFF, unsigned(address >> 32) & 0xFF, unsigned(address >> 24) & 0xFF,
                      unsigned(address >> 16) & 0xFF, unsigned(address >> 8) & 0xFF, unsigned(address) & 0xFF);
        return buf;
    }

    std::string DBusService::GetAdapterAddress() {
        auto adapter = BluetoothAdapter::GetDefaultAsync().get();
        if (!adapter)
            throw std::runtime_error("no Bluetooth adapter");
        return FormatAddress(adapter.BluetoothAddress());
    }

    std::string DBusService::GetAdapterAlias() {
        return {}; // only smart routing uses it, which needs AAP
    }

    DBusService::DBusService() : _native{std::make_unique<Native>()} {
        try {
            if (auto adapter = BluetoothAdapter::GetDefaultAsync().get()) {
                _native->radio = adapter.GetRadioAsync().get();
                _isBluetoothAdapterPowered.SetValue(_native->radio.State() == RadioState::On);
                _native->radio.StateChanged([this](const Radio& radio, const auto&) {
                    _isBluetoothAdapterPowered.SetValue(radio.State() == RadioState::On);
                });
            }
            else {
                Logger::Error("No Bluetooth adapter");
            }
        }
        catch (const winrt::hresult_error& e) {
            Logger::Error("Bluetooth adapter: %s", winrt::to_string(e.message()).c_str());
        }

        // Paired classic devices; BLE-only ones (mice, keyboards) are none of our business.
        // Filled synchronously first, like BlueZ's GetManagedObjects, so the device list is complete when the constructor returns.
        const auto selector = BluetoothDevice::GetDeviceSelectorFromPairingState(true);
        try {
            for (const auto& info : DeviceInformation::FindAllAsync(selector).get())
                TryCreateDevice(winrt::to_string(info.Id()));
        }
        catch (const winrt::hresult_error& e) {
            Logger::Error("Bluetooth devices: %s", winrt::to_string(e.message()).c_str());
        }

        _native->watcher = DeviceInformation::CreateWatcher(selector);
        _native->watcher.Added([this](const DeviceWatcher&, const DeviceInformation& info) {
            TryCreateDevice(winrt::to_string(info.Id())); // also replays the devices found above; those are skipped
        });
        _native->watcher.Removed([this](const DeviceWatcher&, const DeviceInformationUpdate& update) {
            TryRemoveDevice(winrt::to_string(update.Id()));
        });
        _native->watcher.Start();
    }

    DBusService::~DBusService() {
        auto status = _native->watcher.Status();
        if (status == DeviceWatcherStatus::Started || status == DeviceWatcherStatus::EnumerationCompleted)
            _native->watcher.Stop();
    }

    void DBusService::TryCreateDevice(const std::string& id) {
        std::lock_guard lock{_native->lock};
        if (_knownDevices.contains(id))
            return;
        try {
            auto device = BluetoothDevice::FromIdAsync(winrt::to_hstring(id)).get();
            if (!device)
                return;
            auto deviceInfo = std::make_shared<DBusDeviceInfo>(device.BluetoothAddress());
            _knownDevices.emplace(id, deviceInfo);
            _pairedDevices.emplace(deviceInfo);
            _onAnyDeviceAddedEvent.FireEvent(deviceInfo);
            _onDeviceAddedEvent.FireEvent(deviceInfo);
        }
        catch (const std::exception& e) {
            Logger::Error("Bluetooth device %s: %s", id.c_str(), e.what());
        }
        catch (const winrt::hresult_error& e) {
            Logger::Error("Bluetooth device %s: %s", id.c_str(), winrt::to_string(e.message()).c_str());
        }
    }

    void DBusService::TryRemoveDevice(const std::string& id) {
        std::lock_guard lock{_native->lock};
        auto it = _knownDevices.find(id);
        if (it == _knownDevices.end())
            return;
        auto deviceInfo = it->second;
        _onDeviceRemovedEvent.FireEvent(deviceInfo);
        _pairedDevices.erase(deviceInfo);
        _knownDevices.erase(it);
    }

    std::set<std::shared_ptr<DBusDeviceInfo>> DBusService::GetAllDevices() {
        std::lock_guard lock{_native->lock};
        std::set<std::shared_ptr<DBusDeviceInfo>> devices;
        for (const auto& [id, device] : _knownDevices)
            devices.emplace(device);
        return devices;
    }

    std::set<std::shared_ptr<DBusDeviceInfo>> DBusService::GetPairedDevices() {
        std::lock_guard lock{_native->lock};
        return _pairedDevices;
    }

    static void SetRadio(Radio radio, RadioState state, BtCallback callback) {
        std::thread([radio, state, callback = std::move(callback)] {
            std::optional<std::string> error;
            try {
                if (!radio)
                    error = "no Bluetooth adapter";
                else if (Radio::RequestAccessAsync().get() != RadioAccessStatus::Allowed)
                    error = "access to the Bluetooth radio denied";
                else if (radio.SetStateAsync(state).get() != RadioAccessStatus::Allowed)
                    error = "the Bluetooth radio refused the change";
            }
            catch (const winrt::hresult_error& e) {
                error = winrt::to_string(e.message());
            }
            if (error)
                Logger::Error("Bluetooth adapter: %s", error->c_str());
            if (callback)
                callback(error ? &*error : nullptr);
        }).detach();
    }

    void DBusService::EnableBluetoothAdapter() {
        SetRadio(_native->radio, RadioState::On, {});
    }

    void DBusService::EnableBluetoothAdapterAsync(BtCallback&& callback) {
        SetRadio(_native->radio, RadioState::On, std::move(callback));
    }

    void DBusService::DisableBluetoothAdapter() {
        SetRadio(_native->radio, RadioState::Off, {});
    }

    void DBusService::DisableBluetoothAdapterAsync(BtCallback&& callback) {
        SetRadio(_native->radio, RadioState::Off, std::move(callback));
    }
}
