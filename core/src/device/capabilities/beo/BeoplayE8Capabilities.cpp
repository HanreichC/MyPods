// MyPods
// License: GPL-3.0

#include "BeoplayE8Capabilities.h"

namespace MagicPodsCore
{
    nlohmann::json BeoplayE8AncCapability::CreateJsonBody()
    {
        return {{"selected", mode.load()},
                {"options", static_cast<unsigned char>(DeviceAncModes::Off) | static_cast<unsigned char>(DeviceAncModes::Transparency)}};
    }

    void BeoplayE8AncCapability::Update(bool transparency)
    {
        mode = static_cast<unsigned char>(transparency ? DeviceAncModes::Transparency : DeviceAncModes::Off);
        isAvailable = true;
        _onChanged.FireEvent(*this);
    }

    void BeoplayE8AncCapability::Clear()
    {
        Reset();
        _onChanged.FireEvent(*this);
    }

    nlohmann::json BeoplayE8DeviceInfoCapability::CreateJsonBody()
    {
        std::lock_guard guard{lock};
        return {{"name", deviceName}, {"model", info.model}, {"serial", info.serial}, {"firmware", info.firmware}};
    }

    void BeoplayE8DeviceInfoCapability::Update(const std::string &name, const BeoplayE8::Info &newInfo)
    {
        {
            std::lock_guard guard{lock};
            deviceName = name;
            info = newInfo;
        }
        isAvailable = true;
        _onChanged.FireEvent(*this);
    }

    void BeoplayE8DeviceInfoCapability::Clear()
    {
        Reset();
        _onChanged.FireEvent(*this);
    }
}
