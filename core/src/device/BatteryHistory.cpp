// MyPods
// License: GPL-3.0

#include "BatteryHistory.h"
#include "settings/SettingsService.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace MagicPodsCore
{
    static constexpr int64_t WEEK = 7 * 24 * 3600;
    static constexpr double WINDOW = 300; // percent points the runtime is averaged over: three full charges
    static constexpr double MIN_MEASURED = 50; // below that the runtime is a guess

    static std::string FilePath(const std::string &address)
    {
        std::string name = address;
        std::replace(name.begin(), name.end(), ':', '_'); // no colons in Windows file names
        return SettingsService::GetConfigPath("history/" + name + ".csv");
    }

    // Hours a full charge lasts, from the drains in [first, last) until `WINDOW` percent points are covered
    template <typename It>
    static std::optional<double> Runtime(It first, It last)
    {
        double points = 0, seconds = 0;
        for (; first != last && points < WINDOW; ++first)
        {
            points += first->first;
            seconds += first->second;
        }
        if (points < MIN_MEASURED)
            return std::nullopt;
        return seconds / points * 100 / 3600;
    }

    void BatteryHistory::Stats::Add(int64_t time, std::optional<uint8_t> level)
    {
        if (!level)
        {
            if (connected)
                points.emplace_back(time, -1);
            connected = false;
            return;
        }

        if (last && *level < *last)
        {
            auto drop = static_cast<uint8_t>(*last - *level);
            drained += drop;
            // ponytail: the time between two levels counts as use. Lying idle while connected stretches it, so
            // drops slower than 1 % an hour are left out. Draining on the iPhone between two connections counts
            // towards the cycles (no time known); a charge in between hides it, so cycles are a lower bound.
            if (connected && time > lastTime && time - lastTime <= 3600 * drop)
                drains.emplace_back(drop, static_cast<uint32_t>(time - lastTime));
        }

        last = level;
        lastTime = time;
        connected = true;
        points.emplace_back(time, *level);
        while (points.front().first < time - WEEK)
            points.pop_front();
    }

    nlohmann::json BatteryHistory::Stats::ToJson(int64_t now) const
    {
        auto json = nlohmann::json::object();
        json["cycles"] = static_cast<int>(drained / 100);

        auto round1 = [](double hours) { return std::round(hours * 10) / 10; };
        auto recent = Runtime(drains.rbegin(), drains.rend());
        if (recent)
            json["runtime"] = round1(*recent);

        // Health only once the first and the latest window don't overlap
        double measured = 0;
        for (const auto &d : drains)
            measured += d.first;
        if (recent && measured >= 2 * WINDOW)
            if (auto baseline = Runtime(drains.begin(), drains.end()))
            {
                json["baselineRuntime"] = round1(*baseline);
                // above 100 is noise (louder listening back then), not a battery that got better
                json["health"] = std::min(100, static_cast<int>(std::lround(*recent / *baseline * 100)));
            }

        auto history = nlohmann::json::array();
        for (const auto &[time, level] : points)
            if (time >= now - WEEK)
                history.push_back({time, level < 0 ? nlohmann::json(nullptr) : nlohmann::json(level)});
        json["history"] = history;
        return json;
    }

    BatteryHistory &BatteryHistory::Instance()
    {
        static BatteryHistory instance;
        return instance;
    }

    BatteryHistory::Stats &BatteryHistory::Load(const std::string &address)
    {
        auto [it, added] = _devices.try_emplace(address);
        if (!added)
            return it->second;

        auto &stats = it->second;
        std::ifstream file(FilePath(address));
        for (std::string line; std::getline(file, line);)
        {
            auto comma = line.find(',');
            if (comma == std::string::npos)
                continue;
            try
            {
                auto value = line.substr(comma + 1);
                stats.Add(std::stoll(line), value == "-" ? std::nullopt
                                                         : std::optional<uint8_t>(std::clamp(std::stoi(value), 0, 100)));
            }
            catch (const std::exception &) {} // a line cut off by a crash
        }
        // The daemon ended while connected: the time it was gone isn't use
        if (stats.connected)
            stats.Add(stats.lastTime, std::nullopt);
        return stats;
    }

    void BatteryHistory::Record(const std::string &address, std::optional<uint8_t> level)
    {
        std::lock_guard lock{_lock};
        auto &stats = Load(address);
        if (level ? stats.connected && stats.last == level : !stats.connected)
            return;

        int64_t now = std::time(nullptr);
        stats.Add(now, level);

        auto path = FilePath(address);
        std::error_code ignored;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ignored);
        // ponytail: grows by about 100 lines per charge, a few hundred KB a year; compact it if that ever matters
        std::ofstream(path, std::ios::app) << now << ',' << (level ? std::to_string(*level) : "-") << '\n';
    }

    nlohmann::json BatteryHistory::GetAsJson(const std::string &address)
    {
        std::lock_guard lock{_lock};
        auto &stats = Load(address);
        if (!stats.last)
            return nullptr;
        return stats.ToJson(std::time(nullptr));
    }
}
