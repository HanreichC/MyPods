// MyPods
// License: GPL-3.0

#pragma once
#include "AapCapability.h"
#include <mutex>
#include <optional>

namespace MagicPodsCore
{
    struct AapDeviceInfo
    {
        std::string name, model, manufacturer, serial, firmware;
    };

    // Name, model number (A2096 …), serial and firmware from the AirPods' information packet (0x1D),
    // and renaming them (0x1A), which the AirPods keep and show on every paired device.
    class AapDeviceInfoCapability : public AapCapability
    {
    private:
        std::mutex lock{};
        AapDeviceInfo info{};

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;

    public:
        static constexpr size_t MAX_NAME_BYTES = 32;

        static std::optional<AapDeviceInfo> Parse(const std::vector<unsigned char> &data);
        // empty when the name can't be sent (empty, too long, control characters)
        static std::vector<unsigned char> RenamePacket(const std::string &name);

        explicit AapDeviceInfoCapability(AapDevice &device) : AapCapability("deviceInfo", false, device) {}
        void SetFromJson(const nlohmann::json &json) override;
    };
}
