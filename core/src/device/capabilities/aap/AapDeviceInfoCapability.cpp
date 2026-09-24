// MyPods
// License: GPL-3.0

#include "AapDeviceInfoCapability.h"

namespace MagicPodsCore
{
    std::optional<AapDeviceInfo> AapDeviceInfoCapability::Parse(const std::vector<unsigned char> &data)
    {
        // 04 00 04 00 1d, 6 bytes of unknown meaning, then null-terminated UTF-8 strings:
        // name, model number, manufacturer, serial, firmware, … (LibrePods, docs/AAP Definitions.md)
        static constexpr size_t STRINGS_AT = 11;
        if (data.size() <= STRINGS_AT || data[0] != 0x04 || data[1] != 0x00 || data[2] != 0x04 || data[3] != 0x00 || data[4] != 0x1D)
            return std::nullopt;

        std::vector<std::string> fields;
        for (size_t pos = STRINGS_AT; pos < data.size() && fields.size() < 5;)
        {
            size_t end = pos;
            while (end < data.size() && data[end] != 0x00)
                end++;
            if (end == data.size()) // unterminated: the packet was cut off
                break;
            fields.emplace_back(data.begin() + pos, data.begin() + end);
            pos = end + 1;
        }
        if (fields.size() < 5)
            return std::nullopt;
        return AapDeviceInfo{fields[0], fields[1], fields[2], fields[3], fields[4]};
    }

    std::vector<unsigned char> AapDeviceInfoCapability::RenamePacket(const std::string &name)
    {
        if (name.empty() || name.size() > MAX_NAME_BYTES)
            return {};
        for (unsigned char c : name)
            if (c < 0x20 || c == 0x7F)
                return {};
        // 04 00 04 00 1a 00 01 <length> 00 <UTF-8 name>
        std::vector<unsigned char> packet{0x04, 0x00, 0x04, 0x00, 0x1A, 0x00, 0x01, static_cast<unsigned char>(name.size()), 0x00};
        packet.insert(packet.end(), name.begin(), name.end());
        return packet;
    }

    nlohmann::json AapDeviceInfoCapability::CreateJsonBody()
    {
        std::lock_guard guard{lock};
        return {{"name", info.name}, {"model", info.model}, {"manufacturer", info.manufacturer},
                {"serial", info.serial}, {"firmware", info.firmware}, {"maxNameBytes", MAX_NAME_BYTES}};
    }

    void AapDeviceInfoCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        auto parsed = Parse(data);
        if (!parsed)
            return;
        {
            std::lock_guard guard{lock};
            info = *parsed;
        }
        isAvailable = true;
        Logger::Info("Device info: %s, model %s, firmware %s", parsed->name.c_str(), parsed->model.c_str(), parsed->firmware.c_str());
        _onChanged.FireEvent(*this);
    }

    void AapDeviceInfoCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (!capability.contains("name") || !capability["name"].is_string())
        {
            Logger::Error("AapDeviceInfoCapability::SetFromJson: name must be a string");
            return;
        }
        auto newName = capability["name"].get<std::string>();
        auto packet = RenamePacket(newName);
        if (packet.empty())
        {
            Logger::Error("AapDeviceInfoCapability::SetFromJson: name must be 1-%zu bytes without control characters", MAX_NAME_BYTES);
            return;
        }
        device.SendData(packet);
        {
            // BlueZ only reads the new name on the next connection, so show it from here right away
            std::lock_guard guard{lock};
            info.name = newName;
        }
        Logger::Info("Renamed to %s", newName.c_str());
        _onChanged.FireEvent(*this);
    }
}
