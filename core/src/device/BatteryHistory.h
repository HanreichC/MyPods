// MyPods
// License: GPL-3.0

#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace MagicPodsCore
{
    // The battery level over time, per device: the level BatteryProvider hands to BlueZ (the emptier bud).
    // Appended to <config>/history/<address>.csv as "unix seconds,level", "-" when disconnected.
    // From it come the charge cycles (every 100 % drained is one, the way Apple counts them) and the runtime
    // of a full charge, now and over the first cycles recorded: the battery wearing out shows as the difference.
    class BatteryHistory
    {
    public:
        static BatteryHistory &Instance();

        // nullopt: disconnected
        void Record(const std::string &address, std::optional<uint8_t> level);
        // null until something was recorded for the device
        nlohmann::json GetAsJson(const std::string &address);

        // One device's numbers, without files (the self-check feeds it directly)
        struct Stats
        {
            void Add(int64_t time, std::optional<uint8_t> level);
            nlohmann::json ToJson(int64_t now) const;

            std::optional<uint8_t> last; // kept across disconnects: what was drained elsewhere counts as well
            bool connected = false;
            int64_t lastTime = 0;
            double drained = 0; // percent points, in total
            std::vector<std::pair<uint8_t, uint32_t>> drains; // (percent points, seconds) measured while connected
            std::deque<std::pair<int64_t, int>> points; // the last week, level or -1 for disconnected
        };

    private:
        std::mutex _lock;
        std::map<std::string, Stats> _devices;
        Stats &Load(const std::string &address); // with _lock held
    };
}
