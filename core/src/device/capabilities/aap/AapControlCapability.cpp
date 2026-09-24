// MyPods
// License: GPL-3.0

#include "AapControlCapability.h"
#include <algorithm>
#include <bit>

namespace MagicPodsCore
{
    static constexpr unsigned char HEARING_ASSIST = 0x33; // switched together with the hearing aid, as iOS does

    AapControlCapability::AapControlCapability(const std::string &name, unsigned char id, Kind kind, AapDevice &device, std::vector<int> choices)
        : AapCapability(name, false, device), id(id), kind(kind), choices(std::move(choices))
    {
    }

    std::vector<unsigned char> AapControlCapability::Packet(unsigned char id, unsigned char data1, unsigned char data2)
    {
        return {0x04, 0x00, 0x04, 0x00, 0x09, 0x00, id, data1, data2, 0x00, 0x00};
    }

    bool AapControlCapability::IsValidListeningModes(int mask)
    {
        return mask > 0 && mask <= 0x0F && std::popcount(static_cast<unsigned>(mask)) >= 2;
    }

    nlohmann::json AapControlCapability::CreateJsonBody()
    {
        switch (kind)
        {
        case Kind::Toggle:
        case Kind::HearingAid:
            return {{"selected", value == 0x01}};
        default:
            return {{"selected", value.load()}};
        }
    }

    void AapControlCapability::Update(int newValue)
    {
        if (isAvailable && value == newValue)
            return;
        value = newValue;
        isAvailable = true;
        Logger::Info("%s: %d", name.c_str(), newValue);
        _onChanged.FireEvent(*this);
    }

    void AapControlCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        if (data.size() < 9 || data[0] != 0x04 || data[1] != 0x00 || data[2] != 0x04 || data[3] != 0x00 ||
            data[4] != 0x09 || data[5] != 0x00 || data[6] != id)
            return;

        if (kind == Kind::HearingAid)
        {
            enrolled = data[7];
            if (enrolled != 0x01)
                return; // needs a hearing test set up from an iPhone first; without it there is nothing to switch on
            Update(data[8]);
            return;
        }
        if (kind == Kind::Choice && std::find(choices.begin(), choices.end(), data[7]) == choices.end())
            return;
        Update(data[7]);
    }

    void AapControlCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (!capability.contains("selected"))
            return;
        const auto &selected = capability["selected"];

        switch (kind)
        {
        case Kind::Toggle:
            if (!selected.is_boolean())
                break;
            device.SendData(Packet(id, selected.get<bool>() ? 0x01 : 0x02));
            Update(selected.get<bool>() ? 0x01 : 0x02); // not every setting is echoed back
            return;
        case Kind::HearingAid:
            if (!selected.is_boolean() || enrolled != 0x01)
                break;
            device.SendData(Packet(id, 0x01, selected.get<bool>() ? 0x01 : 0x02));
            device.SendData(Packet(HEARING_ASSIST, selected.get<bool>() ? 0x01 : 0x02));
            Update(selected.get<bool>() ? 0x01 : 0x02);
            return;
        case Kind::Choice:
            if (!selected.is_number_integer() || std::find(choices.begin(), choices.end(), selected.get<int>()) == choices.end())
                break;
            device.SendData(Packet(id, static_cast<unsigned char>(selected.get<int>())));
            Update(selected.get<int>());
            return;
        case Kind::ListeningModes:
            if (!selected.is_number_integer() || !IsValidListeningModes(selected.get<int>()))
                break;
            device.SendData(Packet(id, static_cast<unsigned char>(selected.get<int>())));
            Update(selected.get<int>());
            return;
        }
        Logger::Error("%s::SetFromJson: unexpected value %s", name.c_str(), selected.dump().c_str());
    }
}
