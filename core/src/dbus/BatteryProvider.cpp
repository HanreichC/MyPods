// MyPods
// License: GPL-3.0

#include "BatteryProvider.h"
#include "Logger.h"

#include <algorithm>
#include <atomic>

#ifndef _WIN32
#include "dbus/DBusService.h"
#include <sdbus-c++/sdbus-c++.h>
#endif

namespace MagicPodsCore
{
    std::optional<uint8_t> BatteryProvider::Level(const std::vector<DeviceBatteryData> &battery)
    {
        std::optional<uint8_t> level;
        for (const auto &b : battery)
        {
            if (b.Type == DeviceBatteryType::Case || b.Status != DeviceBatteryStatus::Connected)
                continue;
            auto value = static_cast<uint8_t>(b.Battery);
            level = level ? std::min(*level, value) : value;
        }
        return level;
    }

    BatteryProvider &BatteryProvider::Instance()
    {
        static BatteryProvider instance;
        return instance;
    }

#ifdef _WIN32
    struct BatteryProvider::Entry {};
    BatteryProvider::BatteryProvider() = default;
    BatteryProvider::~BatteryProvider() = default;
    void BatteryProvider::Register() {}
    void BatteryProvider::Set(const std::string &, std::optional<uint8_t>) {}
#else
    static constexpr const char *ROOT = "/org/mypods/battery";
    static constexpr const char *INTERFACE = "org.bluez.BatteryProvider1";

    struct BatteryProvider::Entry
    {
        std::atomic<uint8_t> percentage{0};
        std::unique_ptr<sdbus::IObject> object;
    };

    BatteryProvider::BatteryProvider() = default;
    BatteryProvider::~BatteryProvider() = default;

    void BatteryProvider::Register()
    {
        // BlueZ reads our objects through the ObjectManager on ROOT once registered, and follows InterfacesAdded/Removed
        _connection = sdbus::createSystemBusConnection();
        _root = sdbus::createObject(*_connection, ROOT);
        _root->addObjectManager();
        _root->finishRegistration();
        _connection->enterEventLoopAsync();

        static std::unique_ptr<sdbus::IProxy> manager; // has to outlive the asynchronous call
        manager = sdbus::createProxy(*_connection, "org.bluez", DBusService::GetAdapterPath());
        manager->callMethodAsync("RegisterBatteryProvider").onInterface("org.bluez.BatteryProviderManager1")
            .withArguments(sdbus::ObjectPath{ROOT})
            .uponReplyInvoke([](const sdbus::Error *error) {
                if (error)
                    Logger::Info("BlueZ takes no battery levels from MyPods (%s); older BlueZ versions need bluetoothd --experimental",
                                 error->getMessage().c_str());
                else
                    Logger::Info("Battery levels are handed to BlueZ");
            });
    }

    void BatteryProvider::Set(const std::string &address, std::optional<uint8_t> percentage)
    {
        std::lock_guard lock{_lock};
        try
        {
            if (!_registered)
            {
                _registered = true; // one attempt; a BlueZ without the manager won't grow one while we run
                Register();
            }
            if (!_connection)
                return;

            std::string mac = address;
            std::replace(mac.begin(), mac.end(), ':', '_');
            auto it = _entries.find(address);

            if (!percentage)
            {
                if (it != _entries.end())
                {
                    it->second->object->emitInterfacesRemovedSignal({INTERFACE});
                    _entries.erase(it);
                }
                return;
            }

            if (it == _entries.end())
            {
                auto entry = std::make_unique<Entry>();
                entry->percentage = *percentage;
                auto *raw = entry.get();
                entry->object = sdbus::createObject(*_connection, std::string(ROOT) + "/dev_" + mac);
                entry->object->registerProperty("Percentage").onInterface(INTERFACE).withGetter([raw]() { return raw->percentage.load(); });
                entry->object->registerProperty("Device").onInterface(INTERFACE).withGetter([mac]() {
                    return sdbus::ObjectPath{DBusService::GetAdapterPath() + "/dev_" + mac};
                });
                entry->object->registerProperty("Source").onInterface(INTERFACE).withGetter([]() { return std::string{"MyPods"}; });
                entry->object->finishRegistration();
                entry->object->emitInterfacesAddedSignal({INTERFACE});
                _entries.emplace(address, std::move(entry));
                return;
            }

            if (it->second->percentage.exchange(*percentage) != *percentage)
                it->second->object->emitPropertiesChangedSignal(INTERFACE, {"Percentage"});
        }
        catch (const std::exception &e)
        {
            Logger::Error("Battery provider: %s", e.what());
        }
    }
#endif
}
