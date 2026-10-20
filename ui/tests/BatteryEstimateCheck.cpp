// Self-check for the tray tooltip's time left (BatteryEstimate.h). Run by tools/run_checks.sh.
#include "BatteryEstimate.h"

#include <cassert>
#include <cstdio>

int main()
{
    constexpr qint64 minute = 60000;
    BatteryEstimate::Track track;

    assert(BatteryEstimate::update(track, 100, false, 0) == -1);            // the start of a discharge
    assert(BatteryEstimate::update(track, 98, false, 30 * minute) == -1);    // 2 %: too little to go by
    assert(BatteryEstimate::update(track, 90, false, 60 * minute) == 540);   // 10 % an hour: 9 h for the 90 % left
    assert(BatteryEstimate::update(track, 95, false, 61 * minute) == -1);    // back from the case, charged: anew
    assert(BatteryEstimate::update(track, 85, false, 91 * minute) == 255);   // 10 % in half an hour: 255 min for the 85 % left
    assert(BatteryEstimate::update(track, 80, true, 92 * minute) == -1);     // charging: no guess
    assert(track.from == -1);

    bool charging = true;
    const QVariantMap battery{
        {"left", QVariantMap{{"status", 2}, {"battery", 70}, {"charging", false}}},
        {"right", QVariantMap{{"status", 2}, {"battery", 60}, {"charging", false}}},
        {"case", QVariantMap{{"status", 2}, {"battery", 10}, {"charging", true}}},
        {"single", QVariantMap{{"status", 1}, {"battery", 5}}}, // cached, not live
    };
    assert(BatteryEstimate::lowestWorn(battery, charging) == 60 && !charging); // the case is no worn part

    assert(BatteryEstimate::format(42) == "40 min");
    assert(BatteryEstimate::format(1) == "5 min");
    assert(BatteryEstimate::format(150) == "3 h");
    std::puts("BatteryEstimate: OK");
    return 0;
}
