// MyPods
// License: GPL-3.0

#pragma once
#include "AapCapability.h"
#include <atomic>

namespace MagicPodsCore
{
    // A setting that is a single AAP control command: 04 00 04 00 09 00 <id> <data1> <data2> 00 00.
    // The AirPods report each of their settings once notifications are on, so a setting only shows up
    // on models that have it. Identifiers and values: LibrePods, docs/control_commands.md.
    class AapControlCapability : public AapCapability
    {
    public:
        enum class Kind
        {
            Toggle,         // 0x01 on, 0x02 off; JSON bool
            Choice,         // one of `choices`; JSON int
            ListeningModes, // bitmask of the modes a stem press-and-hold cycles through; JSON int
            HearingAid,     // data1 enrolled, data2 enabled (0x01/0x02); only offered when enrolled
        };

        // Bits of ListeningModes
        static constexpr int MODE_OFF = 0x01, MODE_ANC = 0x02, MODE_TRANSPARENCY = 0x04, MODE_ADAPTIVE = 0x08;

        AapControlCapability(const std::string &name, unsigned char id, Kind kind, AapDevice &device, std::vector<int> choices = {});

        void SetFromJson(const nlohmann::json &json) override;

        static std::vector<unsigned char> Packet(unsigned char id, unsigned char data1, unsigned char data2 = 0x00);
        // At least two modes, like on an iPhone: a press-and-hold has to switch between something
        static bool IsValidListeningModes(int mask);

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;

    private:
        unsigned char id;
        Kind kind;
        std::vector<int> choices;
        std::atomic<int> value{0};
        std::atomic<int> enrolled{0}; // HearingAid only

        void Update(int newValue);
    };
}
