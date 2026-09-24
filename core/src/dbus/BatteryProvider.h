// MyPods
// License: GPL-3.0

#pragma once

#include "device/structs/DeviceBatteryData.h"
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace sdbus { class IConnection; class IObject; }

namespace MagicPodsCore
{
    // Hands the exact battery level to BlueZ (org.bluez.BatteryProviderManager1), which publishes it as
    // the device's Battery1: the desktop's Bluetooth applet, UPower and other programs show it without
    // MyPods. Windows: nothing to hand it to, every call is a no-op.
    class BatteryProvider
    {
    public:
        static BatteryProvider &Instance();

        // The level for `address` ("AA:BB:…"), or nullopt to withdraw it (disconnected)
        void Set(const std::string &address, std::optional<uint8_t> percentage);

        // One number for the whole device: the emptier bud, like the level Apple reports over HFP.
        // The case isn't worn, so it doesn't count. nullopt if nothing is known.
        static std::optional<uint8_t> Level(const std::vector<DeviceBatteryData> &battery);

    private:
        BatteryProvider();
        ~BatteryProvider();
        struct Entry;
        std::mutex _lock;
        std::unique_ptr<sdbus::IConnection> _connection;
        std::unique_ptr<sdbus::IObject> _root;
        std::map<std::string, std::unique_ptr<Entry>> _entries;
        bool _registered = false;
        void Register();
    };
}
