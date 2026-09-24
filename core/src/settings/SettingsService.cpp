// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "SettingsService.h"

#include <stdexcept>
#include <fstream>
#include <filesystem>

#include "Logger.h"

namespace MagicPodsCore {

SettingsService::SettingsService(const std::string& filePath) : _filePath(filePath), _onSettingUpdate{} {
    LoadFromFile();
    MergeDefaults(_settings, GetDefaults());
    WriteToFile();
}

void SettingsService::LoadFromFile() {
    std::ifstream file(_filePath);
    if (!file.is_open()) {
        return;
    }

    try {
        _settings = toml::parse_file(_filePath);
    } catch (const toml::parse_error& e) {
        // Throwing here would stop the daemon on every start (and the UI restarts it in a loop).
        // Keep the damaged file for recovering the keys by hand and start from the defaults.
        file.close();
        std::error_code ignored;
        std::filesystem::rename(_filePath, _filePath + ".broken", ignored);
        Logger::Error("Settings file unreadable (%s), moved to %s.broken, using defaults", e.what(), _filePath.c_str());
    }
}

void SettingsService::WriteToFile() {
    std::filesystem::create_directories(std::filesystem::path(_filePath).parent_path());

    // Write a temporary file and rename it over the old one: a crash or full disk mid-write
    // must not truncate the settings, they hold the AirPods keys.
    const std::string tempPath = _filePath + ".tmp";
    {
        std::ofstream file(tempPath, std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + tempPath);
        }
        // private before any content lands in it: the keys identify and track the headphones
        std::filesystem::permissions(tempPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace);

        file << _settings;
        file.flush();

        if (!file.good()) {
            throw std::runtime_error("Failed to write to file: " + tempPath);
        }
    }
    std::filesystem::rename(tempPath, _filePath);
}

toml::table SettingsService::GetDefaults() {
    toml::table magicpods{};
    magicpods.insert_or_assign("animation", true);
    magicpods.insert_or_assign("logLevel", static_cast<int>(LogLevel::Info));

    toml::table defaults{};
    defaults.insert_or_assign("magicpods", std::move(magicpods));
    return defaults;
}

const toml::table& SettingsService::GetSettingsAll() {
    std::lock_guard lock{_lock};
    return _settings;
}

toml::table SettingsService::GetSettingsAllWrapped() {
    std::lock_guard lock{_lock};
    toml::table wrapped;
    wrapped.insert_or_assign("settings", _settings);
    return wrapped;
}

const toml::table& SettingsService::GetSettings(const std::string& container) {
    std::lock_guard lock{_lock};
    auto containerTable = _settings[container].as_table();
    if (!containerTable) {
        _settings.insert_or_assign(container, toml::table{});
        containerTable = _settings[container].as_table();
    }
    return *containerTable;
}

toml::node_view<toml::node> SettingsService::GetSetting(const std::string& container, const std::string& name) {
    std::lock_guard lock{_lock};
    auto containerTable = _settings[container].as_table();
    if (!containerTable) {
        return toml::node_view<toml::node>{};
    }
    return (*containerTable)[name];
}

void SettingsService::SaveSettings(const toml::table& table) {
    std::lock_guard lock{_lock};
    auto settingsRoot = table["settings"].as_table();
    if (!settingsRoot) {
        throw std::invalid_argument("Table must contain 'settings' root key");
    }

    for (const auto& [containerName, containerData] : *settingsRoot) {
        if (auto incomingContainer = containerData.as_table()) {
            auto existingContainer = _settings[containerName].as_table();
            if (!existingContainer) {
                _settings.insert_or_assign(containerName, toml::table{});
                existingContainer = _settings[containerName].as_table();
            }

            for (const auto& [key, value] : *incomingContainer) {
                existingContainer->insert_or_assign(key, value);
            }
        }
    }

    WriteToFile();

    for (const auto& [containerName, containerData] : *settingsRoot) {
        if (auto incomingContainer = containerData.as_table()) {
            auto existingContainer = _settings[containerName].as_table();
            for (const auto& [key, value] : *incomingContainer) {
                auto savedValue = (*existingContainer)[key];
                _onSettingUpdate.FireEvent(UpdatedSettingNotification{containerName, key, savedValue});
            }
        }
    }
}

void SettingsService::SaveSetting(const std::string& container, const std::string& name, const toml::node& value) {
    std::lock_guard lock{_lock};
    auto containerTable = _settings[container].as_table();
    if (!containerTable) {
        _settings.insert_or_assign(container, toml::table{});
        containerTable = _settings[container].as_table();
    }
    containerTable->insert_or_assign(name, value);
    WriteToFile();

    auto savedValue = (*containerTable)[name];
    _onSettingUpdate.FireEvent(UpdatedSettingNotification{container, name, savedValue});
}

void SettingsService::SaveSetting(const std::string& container, const std::string& name, const toml::node_view<const toml::node>& value) {
    std::lock_guard lock{_lock};
    if (value) {
        auto containerTable = _settings[container].as_table();
        if (!containerTable) {
            _settings.insert_or_assign(container, toml::table{});
            containerTable = _settings[container].as_table();
        }
        containerTable->insert_or_assign(name, *value.node());
        WriteToFile();

        auto savedValue = (*containerTable)[name];
        _onSettingUpdate.FireEvent(UpdatedSettingNotification{container, name, savedValue});
    }
}

std::string SettingsService::GetConfigPath(const std::string &fileName) {
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"))
        return std::string(appData) + "\\mypods\\" + fileName;
#endif
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::string(xdg) + "/mypods/" + fileName;

    if (const char* home = std::getenv("HOME"))
        return std::string(home) + "/.config/mypods/" + fileName;

    throw std::runtime_error("Cannot determine config path");    
}
}
