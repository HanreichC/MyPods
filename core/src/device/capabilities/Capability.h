// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include "Event.h"

namespace MagicPodsCore
{
    class Capability
    {
    protected:
        std::string name{};
        bool isReadOnly = false;
        bool isAvailable = false;
        Event<Capability> _onChanged{};
        virtual nlohmann::json CreateJsonBody();
        virtual void Reset();

    public:
        explicit Capability(const std::string &name, bool isReadOnly) : name(name), isReadOnly(isReadOnly) {}
        Event<Capability> &GetChangedEvent()
        {
            return _onChanged;
        }

        const std::string& GetName() const {
            return name;
        }

        // "selected" as a byte, nullopt when missing, not an integer or outside 0..255.
        // Checked before narrowing, so 257 isn't taken for 1.
        static std::optional<unsigned char> SelectedByte(const nlohmann::json &capability)
        {
            if (!capability.contains("selected") || !capability["selected"].is_number_integer())
                return std::nullopt;
            auto value = capability["selected"].get<int64_t>();
            if (value < 0 || value > 255)
                return std::nullopt;
            return static_cast<unsigned char>(value);
        }

        nlohmann::json GetAsJson();
        virtual void SetFromJson(const nlohmann::json &json);
        virtual ~Capability() = default;
    };
}