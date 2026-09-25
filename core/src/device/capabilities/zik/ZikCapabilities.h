// Parrot Zik 2.0 capabilities. They all read the device's XML answers, so they live in one file.

#pragma once

#include "device/capabilities/Capability.h"
#include "device/ZikDevice.h"
#include "device/DeviceBattery.h"

#include <array>
#include <optional>

namespace MagicPodsCore
{
    class ZikCapability : public Capability
    {
    protected:
        ZikDevice &device;
        virtual void OnAnswer(const std::string &xml) = 0;

    public:
        ZikCapability(const std::string &name, bool isReadOnly, ZikDevice &device);
    };

    // /api/system/battery: single battery, state charging | charged | in_use
    class ZikBatteryCapability : public ZikCapability
    {
        DeviceBattery battery{false};
        std::string lastState; // state of the previous answer, empty after (re)connecting

    protected:
        nlohmann::json CreateJsonBody() override { return battery.CreateJsonBody(); }
        void OnAnswer(const std::string &xml) override;
        void Reset() override;

    public:
        explicit ZikBatteryCapability(ZikDevice &device);

        // The percentage the UI shows. The Zik estimates it from the battery voltage, which recovers
        // after load, so in use the reading creeps back up (seen: 76 -> 81 % in 20 min, fw 2.05).
        // While in use it only goes down; charging, charged or a fresh connection take any value.
        static short SmoothedPercent(const std::string &previousState, short previous, const std::string &state, short reported);
    };

    // /api/audio/noise_control (type off | anc | aoc "street mode", value 1 | 2 = strength)
    // plus the master switch /api/audio/noise_control/enabled. JSON: selected/options like
    // every "anc" capability, and "level" 1 | 2 for the strength.
    class ZikAncCapability : public ZikCapability
    {
        std::optional<bool> enabled;
        std::string type;
        int level = 1;

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnAnswer(const std::string &xml) override;
        void Reset() override;

    public:
        explicit ZikAncCapability(ZikDevice &device);
        void SetFromJson(const nlohmann::json &json) override;
    };

    // Same preset list as the software equalizer of the AirPods, but applied by the Zik's own
    // DSP through /api/audio/thumb_equalizer/value, "Off" switches /api/audio/equalizer/enabled off.
    // "Custom" sets the Zik's 5 bands by hand ("custom": 5 gains, -12 to 12), kept in the headphones like the presets.
    class ZikEqualizerCapability : public ZikCapability
    {
        std::string preset = "Off";
        std::array<double, 5> CustomGains();
        void Apply(const std::string &selected);

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnAnswer(const std::string &xml) override;

    public:
        explicit ZikEqualizerCapability(ZikDevice &device);
        void SetFromJson(const nlohmann::json &json) override;
        // A preset's 10 bands on the Zik's 5
        static std::array<double, 5> Gains(const std::string &preset);
        // "Custom" as saved ("3 2 0 -1 4"), nullopt if it isn't 5 gains from -12 to 12
        static std::optional<std::array<double, 5>> ParseCustom(const std::string &text);
        static std::string ThumbEqualizerArg(const std::array<double, 5> &gains);
    };

    // One switch or one list value of the Zik API. `values` {"false","true"} makes it a
    // switch (JSON selected: bool), anything else a list (JSON selected: index into values).
    struct ZikSetting
    {
        std::string name;    // JSON key for the UI
        std::string path;    // API path without /get or /set
        std::string element; // XML element in the answer
        std::string attr;    // attribute holding the value
        std::vector<std::string> values;
        bool enableDisable = false; // switched by path/enable and path/disable instead of path/set?arg=
    };

    class ZikSettingCapability : public ZikCapability
    {
        ZikSetting setting;
        std::string value;
        bool IsSwitch() const { return setting.values == std::vector<std::string>{"false", "true"}; }

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnAnswer(const std::string &xml) override;

    public:
        ZikSettingCapability(ZikDevice &device, ZikSetting setting);
        void SetFromJson(const nlohmann::json &json) override;
    };
}
