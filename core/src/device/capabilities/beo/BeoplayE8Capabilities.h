// MyPods
// License: GPL-3.0

#pragma once

#include "../Capability.h"
#include "device/BeoplayE8.h"
#include "device/enums/DeviceAncModes.h"

#include <atomic>
#include <mutex>

namespace MagicPodsCore
{
    // What the Beoplay E8 tells over LE (BeoplayE8.h), read-only: fed by its link while connected, cleared after

    // Transparency on or off, as noise control the user sees but can't set yet (tapping the left earbud still does)
    class BeoplayE8AncCapability : public Capability
    {
    private:
        std::atomic<unsigned char> mode{static_cast<unsigned char>(DeviceAncModes::Off)};

    protected:
        nlohmann::json CreateJsonBody() override;

    public:
        BeoplayE8AncCapability() : Capability("anc", true) {}
        void Update(bool transparency);
        void Clear();
    };

    // Model, serial and firmware from the Device Information service; the name is the Bluetooth one
    class BeoplayE8DeviceInfoCapability : public Capability
    {
    private:
        std::mutex lock;
        std::string deviceName;
        BeoplayE8::Info info;

    protected:
        nlohmann::json CreateJsonBody() override;

    public:
        BeoplayE8DeviceInfoCapability() : Capability("deviceInfo", true) {}
        void Update(const std::string &name, const BeoplayE8::Info &newInfo);
        void Clear();
    };
}
