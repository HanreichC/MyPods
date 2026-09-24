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

bool SettingsService::WriteToFile() {
    // Write a temporary file and rename it over the old one: a crash or full disk mid-write
    // must not truncate the settings, they hold the AirPods keys.
    // Never throws: it runs on the AAP reader and D-Bus threads, where an exception ends the daemon.
    const std::string tempPath = _filePath + ".tmp";
    try {
        std::filesystem::create_directories(std::filesystem::path(_filePath).parent_path());
        {
            std::ofstream file(tempPath, std::ios::trunc);
            if (!file.is_open())
                throw std::runtime_error("cannot open " + tempPath);
            // private before any content lands in it: the keys identify and track the headphones
            std::filesystem::permissions(tempPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::replace);

            file << _settings;
            file.flush();

            if (!file.good())
                throw std::runtime_error("cannot write " + tempPath);
        }
        std::filesystem::rename(tempPath, _filePath);
        return true;
    } catch (const std::exception& e) {
        std::error_code ignored;
        std::filesystem::remove(tempPath, ignored);
        Logger::Error("Settings not saved (%s), keeping them in memory", e.what());
        return false;
    }
}

toml::table SettingsService::GetDefaults() {
    toml::table magicpods{};
    magicpods.insert_or_assign("animation", true);
    magicpods.insert_or_assign("logLevel", static_cast<int>(LogLevel::Info));

    toml::table defaults{};
    defaults.insert_or_assign("magicpods", std::move(magicpods));
    return defaults;
}

toml::table SettingsService::GetSettingsAllWrapped() {
    std::lock_guard lock{_lock};
    toml::table wrapped;
    wrapped.insert_or_assign("settings", _settings);
    return wrapped;
}

toml::table SettingsService::GetSettings(const std::string& container) {
    std::lock_guard lock{_lock};
    if (auto containerTable = _settings[container].as_table())
        return *containerTable;
    return {};
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
