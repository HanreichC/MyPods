// MyPods
// License: GPL-3.0

#include "AapAttCapabilities.h"
#include <algorithm>

namespace MagicPodsCore
{
    AapAttCapability::AapAttCapability(const std::string &name, unsigned char handle, AapDevice &device)
        : AapCapability(name, false, device), handle(handle)
    {
        attEventId = device.GetAttValueEvent().Subscribe([this](size_t, const std::pair<unsigned char, std::vector<unsigned char>> &value)
        {
            if (value.first == this->handle)
                OnValue(value.second);
        });
    }

    AapAttCapability::~AapAttCapability()
    {
        device.GetAttValueEvent().Unsubscribe(attEventId);
    }

    nlohmann::json AapLoudSoundReductionCapability::CreateJsonBody()
    {
        return {{"selected", enabled.load()}};
    }

    void AapLoudSoundReductionCapability::OnValue(const std::vector<unsigned char> &value)
    {
        if (value.empty())
            return;
        bool newValue = value[0] == 0x01;
        if (isAvailable && newValue == enabled)
            return;
        enabled = newValue;
        isAvailable = true;
        _onChanged.FireEvent(*this);
    }

    void AapLoudSoundReductionCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name) || !json.at(name).contains("selected") || !json.at(name)["selected"].is_boolean())
            return;
        enabled = json.at(name)["selected"].get<bool>();
        device.AttWrite(handle, {static_cast<unsigned char>(enabled ? 0x01 : 0x00)});
        _onChanged.FireEvent(*this); // a write answers without the value
    }

    nlohmann::json AapTransparencyCapability::ToJson(const Att::TransparencySettings &s)
    {
        return {
            {"enabled", s.enabled},
            {"amplification", std::clamp((s.left.amplification + s.right.amplification) / 2, -1.0f, 1.0f)},
            {"balance", std::clamp(s.right.amplification - s.left.amplification, -1.0f, 1.0f)},
            {"tone", std::clamp((s.left.tone + s.right.tone) / 2, -1.0f, 1.0f)},
            {"ambientNoiseReduction", std::clamp((s.left.ambientNoiseReduction + s.right.ambientNoiseReduction) / 2, 0.0f, 1.0f)},
            {"conversationBoost", s.left.conversationBoost > 0.5f || s.right.conversationBoost > 0.5f},
        };
    }

    void AapTransparencyCapability::Apply(Att::TransparencySettings &s, const nlohmann::json &json)
    {
        auto number = [&](const char *key, float low, float high) -> std::optional<float> {
            if (!json.contains(key) || !json[key].is_number())
                return std::nullopt;
            return std::clamp(json[key].get<float>(), low, high);
        };
        if (json.contains("enabled") && json["enabled"].is_boolean())
            s.enabled = json["enabled"].get<bool>();

        // amplification is the average of both buds, balance their difference
        auto current = ToJson(s);
        float amplification = number("amplification", -1, 1).value_or(current["amplification"].get<float>());
        float balance = number("balance", -1, 1).value_or(current["balance"].get<float>());
        if (json.contains("amplification") || json.contains("balance"))
        {
            s.left.amplification = amplification - balance / 2;
            s.right.amplification = amplification + balance / 2;
        }
        if (auto tone = number("tone", -1, 1))
            s.left.tone = s.right.tone = *tone;
        if (auto reduction = number("ambientNoiseReduction", 0, 1))
            s.left.ambientNoiseReduction = s.right.ambientNoiseReduction = *reduction;
        if (json.contains("conversationBoost") && json["conversationBoost"].is_boolean())
            s.left.conversationBoost = s.right.conversationBoost = json["conversationBoost"].get<bool>() ? 1.0f : 0.0f;
    }

    nlohmann::json AapTransparencyCapability::CreateJsonBody()
    {
        std::lock_guard guard{lock};
        return ToJson(settings);
    }

    void AapTransparencyCapability::OnValue(const std::vector<unsigned char> &value)
    {
        auto parsed = Att::TransparencySettings::Parse(value);
        if (!parsed)
            return;
        {
            std::lock_guard guard{lock};
            settings = *parsed;
        }
        isAvailable = true;
        _onChanged.FireEvent(*this);
    }

    void AapTransparencyCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!isAvailable || !json.contains(name) || !json.at(name).is_object())
            return; // written only on top of the values read from the AirPods, never from scratch
        std::vector<unsigned char> data;
        {
            std::lock_guard guard{lock};
            Apply(settings, json.at(name));
            data = settings.Encode();
        }
        device.AttWrite(handle, data);
        _onChanged.FireEvent(*this);
    }
}
