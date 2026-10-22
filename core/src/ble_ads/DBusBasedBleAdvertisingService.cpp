// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "DBusBasedBleAdvertisingService.h"
#include <iostream>

namespace MyPods
{

DBusBasedBleAdvertisingService::DBusBasedBleAdvertisingService(DBusService& dbusService)
    : _dbusService(dbusService)
{
    _adapterPoweredSubscriptionId = _dbusService.IsBluetoothAdapterPowered().GetEvent().Subscribe(
        [this](size_t listenerId, bool isPowered) {
            this->OnAdapterPowerChanged(listenerId, isPowered);
        }
    );

    // A discovery always scans actively from a random address, and the controller then drops directed
    // advertisements, which is how a paired LE mouse or keyboard asks to reconnect after a long link loss
    // (after every boot): it stays away for as long as we discover. Without our session the kernel scans
    // passively from the public address and connects it; btmon showed the MX Master back 1.1 s after the
    // stop. So the discovery pauses briefly now and then.
    // ponytail: a fixed duty cycle; a popup can come up to 1.5 s late. The directed ads are never reported
    // to the host while we scan, so there is nothing to pause on demand, and with MX Anywhere and the Xbox
    // controller paired some LE input device is always disconnected anyway.
    _gapper = std::jthread([this](std::stop_token stop) {
        std::mutex m;
        std::unique_lock lock{m};
        while (true) {
            // after a stop request wait_for returns false at once, so the stop has to end the loop itself
            _gapWake.wait_for(lock, stop, std::chrono::seconds(15), [] { return false; });
            if (stop.stop_requested())
                return;
            if (!_scanDesired)
                continue;
            _dbusService.StopDiscovery();
            _gapWake.wait_for(lock, stop, std::chrono::milliseconds(1500), [] { return false; });
            if (stop.stop_requested())
                return;
            std::lock_guard scan{_scanMutex};
            if (!_scanDesired) // StopScan ran during the pause
                continue;
            try {
                StartDiscoverySession();
            } catch (const sdbus::Error& e) {
                std::cerr << "[DBus] Failed to resume scan after pause: " << e.getMessage() << std::endl;
            }
        }
    });
}

DBusBasedBleAdvertisingService::~DBusBasedBleAdvertisingService()
{
    _gapper.request_stop();
    _gapper.join();

    if (_adapterPoweredSubscriptionId != 0) {
        _dbusService.IsBluetoothAdapterPowered().GetEvent().Unsubscribe(_adapterPoweredSubscriptionId);
        _adapterPoweredSubscriptionId = 0;
    }

    if (_onDeviceAddedSubscriptionId != 0) {
        _dbusService.GetOnAnyDeviceAddedEvent().Unsubscribe(_onDeviceAddedSubscriptionId);
        _onDeviceAddedSubscriptionId = 0;
    }
    if (_onDeviceRemovedSubscriptionId != 0) {
        _dbusService.GetOnDeviceRemovedEvent().Unsubscribe(_onDeviceRemovedSubscriptionId);
        _onDeviceRemovedSubscriptionId = 0;
    }
    {
        std::lock_guard<std::mutex> lock(_deviceSubscriptionsMutex);
        for (auto& [address, subs] : _deviceSubscriptions) {
            if (auto device = subs.device.lock()) {
                if (subs.manufacturerDataSubscriptionId != 0) {
                    device->GetManufacturerData().GetEvent().Unsubscribe(subs.manufacturerDataSubscriptionId);
                }
            }
        }
        _deviceSubscriptions.clear();
    }

    // NOTE: This assumes that _dbusService is still valid and alive
    // at the time this destructor is called. If _dbusService is destroyed
    // earlier, this call may result in undefined behavior (e.g. use-after-free).
    try {
        _dbusService.StopDiscovery();
    } catch (const sdbus::Error& e) {}
}

void DBusBasedBleAdvertisingService::OnDeviceAdded(std::shared_ptr<DBusDeviceInfo> deviceInfo, bool emitNow)
{
    std::string address = deviceInfo->GetAddress();
    RemoveDeviceSubscriptions(address);

    if (emitNow)
        EmitAd(deviceInfo, deviceInfo->GetManufacturerData().GetValue());

    DeviceSubscriptions subs{};
    subs.device = deviceInfo;
    subs.manufacturerDataSubscriptionId =
        deviceInfo->GetManufacturerData().GetEvent().Subscribe(
            [this, deviceInfo](size_t, const std::map<uint16_t, std::vector<uint8_t>>& data) {
                EmitAd(deviceInfo, data);
            });

    {
        std::lock_guard<std::mutex> lock(_deviceSubscriptionsMutex);
        _deviceSubscriptions[address] = subs;
    }
}

void DBusBasedBleAdvertisingService::OnAdapterPowerChanged(size_t listenerId, bool isPowered)
{
    if (!isPowered) {
        std::cout << "[DBus] Bluetooth adapter powered OFF, scanning stopped" << std::endl;
    } else if (_scanDesired) {
        std::cout << "[DBus] Bluetooth adapter powered ON, restarting scan..." << std::endl;

        // Start new thread to avoid blocking
        std::thread([this]() {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                StartListening();
                StartScan(false);
                std::cout << "[DBus] Scan successfully restarted" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[DBus] Failed to restart scan: " << e.what() << std::endl;
            }
        }).detach();
    }
}

void DBusBasedBleAdvertisingService::StartScan(bool isPassive)
{
    std::lock_guard lock{_scanMutex};
    StartDiscoverySession();
    _scanDesired = true;
}

void DBusBasedBleAdvertisingService::StartDiscoverySession()
{
    std::map<std::string, sdbus::Variant> filter;
    // No "Transport" filter: an LE-only discovery starved paired LE mice even more (see the pause in the
    // constructor). DuplicateData is what makes BlueZ pass on every advertisement: without it, not one
    // ManufacturerData update arrived in 70 s, and the popup and automatic switching live on them.
    filter.emplace("DuplicateData", sdbus::Variant(true));
    try {
        _dbusService.SetDiscoveryFilter(filter);
    } catch (const sdbus::Error& e) {
        std::cout << "Note: Could not set discovery filter (may already be set): "
                  << e.getMessage() << std::endl;
    }

    try {
        _dbusService.StartDiscovery();
    } catch (const sdbus::Error& e) {
        std::string errorName = e.getName();
        if (errorName.find("InProgress") != std::string::npos) {
            std::cout << "Note: Discovery already in progress (joining existing scan)" << std::endl;
        } else if (errorName.find("NotReady") != std::string::npos) {
            std::cerr << "Warning: Bluetooth adapter is powered OFF. Waiting for adapter to be enabled..." << std::endl;
        } else {
            throw;
        }
    }
}

void DBusBasedBleAdvertisingService::StopScan()
{
    std::lock_guard lock{_scanMutex};
    _scanDesired = false;

    if (_onDeviceAddedSubscriptionId != 0) {
        _dbusService.GetOnAnyDeviceAddedEvent().Unsubscribe(_onDeviceAddedSubscriptionId);
        _onDeviceAddedSubscriptionId = 0;
    }
    if (_onDeviceRemovedSubscriptionId != 0) {
        _dbusService.GetOnDeviceRemovedEvent().Unsubscribe(_onDeviceRemovedSubscriptionId);
        _onDeviceRemovedSubscriptionId = 0;
    }
    {
        std::lock_guard<std::mutex> lock(_deviceSubscriptionsMutex);
        for (auto& [address, subs] : _deviceSubscriptions) {
            if (auto device = subs.device.lock()) {
                if (subs.manufacturerDataSubscriptionId != 0) {
                    device->GetManufacturerData().GetEvent().Unsubscribe(subs.manufacturerDataSubscriptionId);
                }
            }
        }
        _deviceSubscriptions.clear();
    }

    // Use async to avoid stuck
    _dbusService.StopDiscoveryAsync(
        [](const sdbus::Error* error) {
            if (error) {
                std::cout << "Note: Could not stop discovery (may be used by other processes): "
                          << error->getMessage() << std::endl;
            }
        }
    );
}

void DBusBasedBleAdvertisingService::StartListening()
{
    if (_onDeviceAddedSubscriptionId != 0) {
        _dbusService.GetOnAnyDeviceAddedEvent().Unsubscribe(_onDeviceAddedSubscriptionId);
    }
    if (_onDeviceRemovedSubscriptionId != 0) {
        _dbusService.GetOnDeviceRemovedEvent().Unsubscribe(_onDeviceRemovedSubscriptionId);
    }
    _onDeviceAddedSubscriptionId = _dbusService.GetOnAnyDeviceAddedEvent().Subscribe(
        [this](size_t, std::shared_ptr<DBusDeviceInfo> dev) {
            this->OnDeviceAdded(dev);
        });
    _onDeviceRemovedSubscriptionId = _dbusService.GetOnDeviceRemovedEvent().Subscribe(
        [this](size_t, std::shared_ptr<DBusDeviceInfo> dev) {
            RemoveDeviceSubscriptions(dev->GetAddress());
        });
    // StopScan dropped every subscription, but BlueZ keeps the devices it saw for a while (the AirPods' rotating
    // address among them) and sends no InterfacesAdded for them again. What they advertised before is stale.
    for (const auto& device : _dbusService.GetAllDevices())
        OnDeviceAdded(device, false);
}

void DBusBasedBleAdvertisingService::EmitAd(
    const std::shared_ptr<DBusDeviceInfo>& deviceInfo,
    const std::map<uint16_t, std::vector<uint8_t>>& manufacturerData)
{
    if (manufacturerData.empty()) {
        return;
    }

    std::string address = deviceInfo->GetAddress();
    auto rssi = static_cast<int8_t>(deviceInfo->GetRssi().GetValue());

    _onAdReceivedEvent.FireEvent(BleAdvertisingData(address, rssi, manufacturerData));
}

void DBusBasedBleAdvertisingService::RemoveDeviceSubscriptions(const std::string& address)
{
    std::lock_guard<std::mutex> lock(_deviceSubscriptionsMutex);
    auto it = _deviceSubscriptions.find(address);
    if (it == _deviceSubscriptions.end()) {
        return;
    }
    auto device = it->second.device.lock();
    if (device) {
        if (it->second.manufacturerDataSubscriptionId != 0) {
            device->GetManufacturerData().GetEvent().Unsubscribe(it->second.manufacturerDataSubscriptionId);
        }
    }
    _deviceSubscriptions.erase(it);
}

}
