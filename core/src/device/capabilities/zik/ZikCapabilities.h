// Parrot Zik 2.0 capabilities. They all read the device's XML answers, so they live in one file.

#pragma once

#include "device/capabilities/Capability.h"
#include "device/ZikDevice.h"
#include "device/DeviceBattery.h"

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

    protected:
        nlohmann::json CreateJsonBody() override { return battery.CreateJsonBody(); }
        void OnAnswer(const std::string &xml) override;
        void Reset() override;

    public:
        explicit ZikBatteryCapability(ZikDevice &device);
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
    class ZikEqualizerCapability : public ZikCapability
    {
        std::string preset = "Off";

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnAnswer(const std::string &xml) override;

    public:
        explicit ZikEqualizerCapability(ZikDevice &device);
        void SetFromJson(const nlohmann::json &json) override;
        static std::string ThumbEqualizerArg(const std::string &preset);
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
