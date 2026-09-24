// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <toml++/toml.h>
#include "Event.h"
#include "UpdatedSettingNotification.h"

namespace MagicPodsCore {

class SettingsService {
private:
    toml::table _settings{};
    std::string _filePath;
    // Settings are written from the AAP reader (keys), the BLE thread (color) and the WebSocket loop.
    // Readers get copies, and update events fire after the lock is released, so a listener that takes
    // its own lock and then reads settings can't deadlock against a writer.
    std::mutex _lock{};
    Event<UpdatedSettingNotification> _onSettingUpdate{};

    void LoadFromFile();
    // false when the file couldn't be written; the value stays in memory and the next write tries again
    bool WriteToFile();
    static toml::table GetDefaults();

    static void MergeDefaults(toml::table& settings, const toml::table& defaults) {
        for (const auto& [containerName, containerData] : defaults) {
            auto* defaultContainer = containerData.as_table();
            if (!defaultContainer) continue;

            if (!settings[containerName].as_table()) {
                settings.insert_or_assign(containerName, toml::table{});
            }
            auto* settingsContainer = settings[containerName].as_table();

            for (const auto& [key, value] : *defaultContainer) {
                if (!(*settingsContainer)[key]) {
                    settingsContainer->insert_or_assign(key, value);
                }
            }
        }
    }

public:
    SettingsService(const std::string& filePath);
    toml::table GetSettingsAllWrapped();
    // Copy of one container, empty if it doesn't exist
    toml::table GetSettings(const std::string& container);

    // The setting if it exists and has type T (bool, int64_t, double, std::string)
    template<typename T>
    std::optional<T> GetValue(const std::string& container, const std::string& name) {
        std::lock_guard lock{_lock};
        auto containerTable = _settings[container].as_table();
        if (!containerTable)
            return std::nullopt;
        auto node = (*containerTable)[name];
        if constexpr (std::is_same_v<T, bool>)
            return node.is_boolean() ? std::optional<T>{node.as_boolean()->get()} : std::nullopt;
        else if constexpr (std::is_same_v<T, std::string>)
            return node.is_string() ? std::optional<T>{node.as_string()->get()} : std::nullopt;
        else if constexpr (std::is_integral_v<T>)
            return node.is_integer() ? std::optional<T>{static_cast<T>(node.as_integer()->get())} : std::nullopt;
        else
            return node.template value<T>();
    }

    template<typename T>
    void SaveSetting(const std::string& container, const std::string& name, T&& value);

    Event<UpdatedSettingNotification>& GetOnSettingUpdateEvent() {
        return _onSettingUpdate;
    }

    static std::string GetConfigPath(const std::string &fileName);
};

template<typename T>
void SettingsService::SaveSetting(const std::string& container, const std::string& name, T&& value) {
    std::optional<UpdatedSettingNotification> notification;
    {
        std::lock_guard lock{_lock};
        auto containerTable = _settings[container].as_table();
        if (!containerTable) {
            _settings.insert_or_assign(container, toml::table{});
            containerTable = _settings[container].as_table();
        }
        containerTable->insert_or_assign(name, std::forward<T>(value));
        WriteToFile();
        notification.emplace(container, name, *(*containerTable)[name].node());
    }
    _onSettingUpdate.FireEvent(*notification);
}

}
