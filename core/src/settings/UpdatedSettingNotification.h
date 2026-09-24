// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include <string>
#include <toml++/toml.h>

namespace MagicPodsCore {

// Owns a copy of the new value: it is fired after the settings lock is released, when the table may change again
struct UpdatedSettingNotification {
private:
    std::string _containerName;
    std::string _settingName;
    toml::table _value;

public:
    UpdatedSettingNotification(std::string containerName, std::string settingName, const toml::node& value)
    : _containerName(std::move(containerName)), _settingName(std::move(settingName)) {
        _value.insert_or_assign("v", value);
    }

    std::string_view GetContainerName() const {
        return _containerName;
    }

    std::string_view GetSettingName() const {
        return _settingName;
    }

    toml::node_view<const toml::node> GetValue() const {
        return _value["v"];
    }
};

}
