#include "ZikCapabilities.h"
#include "dbus/BatteryProvider.h"
#include "audio/AudioEffects.h"
#include "device/enums/DeviceAncModes.h"

#include <algorithm>
#include <cstdio>

namespace MagicPodsCore
{
    ZikCapability::ZikCapability(const std::string &name, bool isReadOnly, ZikDevice &device)
        : Capability(name, isReadOnly), device(device)
    {
        // Released together with the device, see ~Device
        device.GetAnswerEvent().Subscribe([this](size_t, const std::string &xml) { OnAnswer(xml); });
        device.GetConnectedPropertyChangedEvent().Subscribe([this](size_t, bool connected) {
            if (!connected)
                Reset();
        });
    }

    // --- battery ---

    ZikBatteryCapability::ZikBatteryCapability(ZikDevice &device) : ZikCapability("battery", true, device)
    {
        battery.GetBatteryChangedEvent().Subscribe([this](size_t, const std::vector<DeviceBatteryData> &) {
            isAvailable = true;
            _onChanged.FireEvent(*this);
            BatteryProvider::Instance().Set(this->device.GetAddress(), BatteryProvider::Level(battery.GetBatteryStatus()));
        });
    }

    void ZikBatteryCapability::OnAnswer(const std::string &xml)
    {
        auto state = Zik::Attr(xml, "battery", "state");
        if (!state)
            return;
        // Zik 1 firmware calls it "level"; while charging some firmwares leave the percentage empty
        auto percent = Zik::Attr(xml, "battery", "percent");
        if (!percent || percent->empty())
            percent = Zik::Attr(xml, "battery", "level");
        short value = battery.GetBatteryStatus()[0].Battery;
        if (percent && !percent->empty())
            value = SmoothedPercent(lastState, value, *state, static_cast<short>(std::atoi(percent->c_str())));
        lastState = *state;
        battery.UpdateBattery({DeviceBatteryData(DeviceBatteryType::Single, DeviceBatteryStatus::Connected, value, *state == "charging")});
    }

    short ZikBatteryCapability::SmoothedPercent(const std::string &previousState, short previous, const std::string &state, short reported)
    {
        // ponytail: never rises in use, so charging from a source the Zik doesn't report as charging
        // shows only after the next connection
        if (state == "in_use" && previousState == "in_use" && previous > 0 && reported > previous)
            return previous;
        return reported;
    }

    void ZikBatteryCapability::Reset()
    {
        battery.ClearBattery();
        BatteryProvider::Instance().Set(device.GetAddress(), std::nullopt);
        lastState.clear();
        Capability::Reset();
    }

    // --- noise control ---

    ZikAncCapability::ZikAncCapability(ZikDevice &device) : ZikCapability("anc", false, device) {}

    static DeviceAncModes ZikAncMode(std::optional<bool> enabled, const std::string &type)
    {
        if (enabled == false || type == "off")
            return DeviceAncModes::Off;
        return type == "aoc" ? DeviceAncModes::Transparency : DeviceAncModes::NoiseCancellation;
    }

    nlohmann::json ZikAncCapability::CreateJsonBody()
    {
        return {
            {"selected", ZikAncMode(enabled, type)},
            // no Off: checked by ear on a Zik 2 (fw 2.05), noise control off mutes the music until it is
            // switched back on, both via enabled=false and via type=off. Still shown if the Zik reports it.
            {"options", static_cast<int>(DeviceAncModes::Transparency) | static_cast<int>(DeviceAncModes::NoiseCancellation)},
            {"level", level},
        };
    }

    void ZikAncCapability::OnAnswer(const std::string &xml)
    {
        auto before = CreateJsonBody();
        if (auto e = Zik::BoolAttr(xml, "noise_control", "enabled"))
            enabled = e;
        if (auto t = Zik::Attr(xml, "noise_control", "type"))
            type = *t;
        if (auto v = Zik::Attr(xml, "noise_control", "value"))
            level = std::clamp(std::atoi(v->c_str()), 1, 2);
        if (type.empty())
            return;
        if (!isAvailable || before != CreateJsonBody())
        {
            isAvailable = true;
            Logger::Info("Zik noise control: %s level %d", DeviceAncModesToString(ZikAncMode(enabled, type)).c_str(), level);
            _onChanged.FireEvent(*this);
        }
    }

    void ZikAncCapability::Reset()
    {
        enabled.reset();
        type.clear();
        level = 1;
        Capability::Reset();
    }

    void ZikAncCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &c = json.at(name);
        std::string newType = type;
        int newLevel = level;

        if (c.contains("selected") && c["selected"].is_number_integer())
        {
            switch (static_cast<DeviceAncModes>(SelectedByte(c).value_or(0))) // 0: no mode
            {
            case DeviceAncModes::Transparency: newType = "aoc"; break;
            case DeviceAncModes::NoiseCancellation: newType = "anc"; break;
            default:
                Logger::Info("Error: ZikAncCapability::SetFromJson got unexpected option: %s", c["selected"].dump().c_str());
                return;
            }
        }
        if (c.contains("level") && c["level"].is_number_integer())
            newLevel = std::clamp(c["level"].get<int>(), 1, 2);
        // a level alone means nothing while noise control is off (never set by us, see CreateJsonBody)
        if (newType.empty() || newType == "off")
            return;

        // the master switch overrides the type
        if (enabled == false)
            device.Set("/api/audio/noise_control/enabled", "true");
        device.Set("/api/audio/noise_control", newType + "&value=" + std::to_string(newLevel));
    }

    // --- equalizer ---

    ZikEqualizerCapability::ZikEqualizerCapability(ZikDevice &device) : ZikCapability("equalizer", false, device) {}

    // ponytail: the presets are 10-band (31 Hz..16 kHz), the Zik's thumb equalizer has 5 bands, so
    // neighbouring bands are averaged. Close, not exact; a per-band Zik preset table would be exact.
    // The trailing "0,0" is the thumb position the Parrot app draws: checked by ear on a Zik 2
    // (fw 2.05), the DSP only uses the gains, r/theta change nothing.
    // Also by ear: the Zik's gains are much weaker than their dB values suggest (±12 is strong but
    // not harsh, the unscaled presets were barely audible), hence the factor. Retune if it's too much.
    constexpr double ZikGainScale = 2.0;

    std::string ZikEqualizerCapability::ThumbEqualizerArg(const std::string &preset)
    {
        auto gains = AudioEffects::Preset(preset).value_or(std::array<double, 10>{});
        std::string arg;
        char band[16];
        for (int i = 0; i < 10; i += 2)
        {
            std::snprintf(band, sizeof(band), "%.1f,", std::clamp((gains[i] + gains[i + 1]) / 2 * ZikGainScale, -12.0, 12.0));
            arg += band;
        }
        return arg + "0,0";
    }

    nlohmann::json ZikEqualizerCapability::CreateJsonBody()
    {
        return {{"selected", preset}, {"options", AudioEffects::PresetNames()}};
    }

    void ZikEqualizerCapability::OnAnswer(const std::string &xml)
    {
        auto on = Zik::BoolAttr(xml, "equalizer", "enabled");
        if (!on)
            return;
        // the Zik only knows gains, the preset name is ours
        std::string newPreset = *on ? device.LoadSettingString("equalizer").value_or("Off") : "Off";
        if (!isAvailable || newPreset != preset)
        {
            isAvailable = true;
            preset = newPreset;
            _onChanged.FireEvent(*this);
        }
    }

    void ZikEqualizerCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name) || !json.at(name).contains("selected") || !json.at(name)["selected"].is_string())
            return;
        std::string selected = json.at(name)["selected"];
        if (!AudioEffects::Preset(selected))
        {
            Logger::Info("Error: ZikEqualizerCapability::SetFromJson unknown preset %s", selected.c_str());
            return;
        }
        device.SaveSettingString("equalizer", selected);
        if (selected != "Off")
            device.Set("/api/audio/thumb_equalizer/value", ThumbEqualizerArg(selected));
        device.Set("/api/audio/equalizer/enabled", selected == "Off" ? "false" : "true");
    }

    // --- plain settings ---

    ZikSettingCapability::ZikSettingCapability(ZikDevice &device, ZikSetting setting)
        : ZikCapability(setting.name, false, device), setting(std::move(setting)) {}

    nlohmann::json ZikSettingCapability::CreateJsonBody()
    {
        if (IsSwitch())
            return {{"selected", value == "true" || value == "invalid_on"}};
        auto it = std::find(setting.values.begin(), setting.values.end(), value);
        return {{"selected", it == setting.values.end() ? -1 : static_cast<int>(it - setting.values.begin())}, {"options", setting.values}};
    }

    void ZikSettingCapability::OnAnswer(const std::string &xml)
    {
        auto v = Zik::Attr(xml, setting.element, setting.attr);
        if (!v || (isAvailable && *v == value))
            return;
        isAvailable = true;
        value = *v;
        Logger::Debug("Zik %s: %s", name.c_str(), value.c_str());
        _onChanged.FireEvent(*this);
    }

    void ZikSettingCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name) || !json.at(name).contains("selected"))
            return;
        const auto &selected = json.at(name)["selected"];
        std::string arg;
        if (IsSwitch() && selected.is_boolean())
            arg = selected.get<bool>() ? "true" : "false";
        else if (!IsSwitch() && selected.is_number_integer() && selected.get<int>() >= 0 && selected.get<int>() < static_cast<int>(setting.values.size()))
            arg = setting.values[selected.get<int>()];
        else
        {
            Logger::Info("Error: Zik %s got unexpected value %s", name.c_str(), selected.dump().c_str());
            return;
        }

        if (setting.enableDisable)
        {
            device.Query(setting.path + (arg == "true" ? "/enable" : "/disable"));
            device.Get(setting.path);
        }
        else
            device.Set(setting.path, arg);
    }
}
