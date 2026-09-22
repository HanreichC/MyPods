// Standalone self-check for the AAP battery path (not part of the magicpodscore build):
//   g++ -std=c++20 -Icore/src -Icore/dependencies/json/include core/src/tests/BatterySelfCheck.cpp \
//       core/src/sdk/aap/watchers/AapBatteryWatcher.cpp core/src/sdk/aap/watchers/AapWatcher.cpp \
//       core/src/device/DeviceBattery.cpp -o /tmp/battery && /tmp/battery

#include <cassert>
#include <cstdio>
#include "device/DeviceBattery.h"
#include "sdk/aap/watchers/AapBatteryWatcher.h"

using namespace MagicPodsCore;

int main()
{
    AapBatteryWatcher watcher;
    DeviceBattery battery(true);
    watcher.GetEvent().Subscribe([&](size_t, const std::vector<DeviceBatteryData> &b) { battery.UpdateBattery(b); });

    auto json = [&] { return battery.CreateJsonBody(); };

    // 04 00 04 00 04 00 <count> then per entry: <type> 01 <level> <status> 01
    // Right 80% not charging, Left 75% charging, Case 42% not charging
    watcher.ProcessResponse({0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x03,
                             0x02, 0x01, 80, 0x02, 0x01,
                             0x04, 0x01, 75, 0x01, 0x01,
                             0x08, 0x01, 42, 0x02, 0x01});
    assert(json()["right"]["battery"] == 80 && json()["right"]["status"] == 2 && !json()["right"]["charging"]);
    assert(json()["left"]["battery"] == 75 && json()["left"]["charging"]);
    assert(json()["case"]["battery"] == 42);
    assert(json()["single"]["status"] == 0);

    // Level changes are picked up
    watcher.ProcessResponse({0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 79, 0x02, 0x01});
    assert(json()["right"]["battery"] == 79);

    // Earbud gone: keep last level, mark it cached (status 3)
    watcher.ProcessResponse({0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x04, 0x01, 0x00, 0x04, 0x01});
    assert(json()["left"]["battery"] == 75 && json()["left"]["status"] == 3);

    // An unknown entry must not shift the following ones
    watcher.ProcessResponse({0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x02,
                             0x10, 0x01, 11, 0x02, 0x01,
                             0x08, 0x01, 55, 0x01, 0x01});
    assert(json()["case"]["battery"] == 55 && json()["case"]["charging"]);

    // Count larger than the packet must not read past the end
    watcher.ProcessResponse({0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x09, 0x02, 0x01, 50, 0x02, 0x01});
    assert(json()["right"]["battery"] == 50);

    std::puts("Battery self-check OK");
}
