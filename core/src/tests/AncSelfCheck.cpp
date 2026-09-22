// Standalone self-check for the ANC wire path (not part of the magicpodscore build):
//   g++ -std=c++20 -Icore/src -Icore/dependencies/json/include core/src/tests/AncSelfCheck.cpp \
//       core/src/sdk/aap/setters/AapSetAnc.cpp core/src/sdk/aap/setters/AapRequest.cpp \
//       core/src/sdk/aap/setters/AapInitExt.cpp core/src/sdk/aap/watchers/AapAncWatcher.cpp \
//       core/src/sdk/aap/watchers/AapWatcher.cpp -o /tmp/anc && /tmp/anc

#include <cassert>
#include <cstdio>
#include "device/enums/DeviceAncModes.h"
#include "sdk/aap/setters/AapSetAnc.h"
#include "sdk/aap/watchers/AapAncWatcher.h"

using namespace MagicPodsCore;

int main()
{
    // UI (BatteryPage.qml ancModes / TrayIconManager kAnc*) sends these values
    assert(static_cast<int>(DeviceAncModes::Off) == 1);
    assert(static_cast<int>(DeviceAncModes::Transparency) == 2);
    assert(static_cast<int>(DeviceAncModes::Adaptive) == 4);
    assert(static_cast<int>(DeviceAncModes::NoiseCancellation) == 16);
    assert(isValidDeviceAncModesType(16) && !isValidDeviceAncModesType(3));

    // 04 00 04 00 09 00 0d <mode> 00 00 00, and the headphones echo the same packet
    for (auto mode : {AapAncMode::Off, AapAncMode::Anc, AapAncMode::Transparency, AapAncMode::Adaptive})
    {
        auto packet = AapSetAnc(mode).Request();
        assert((packet == std::vector<unsigned char>{0x04, 0x00, 0x04, 0x00, 0x09, 0x00, 0x0d,
                                                      static_cast<unsigned char>(mode), 0x00, 0x00, 0x00}));

        AapAncWatcher watcher;
        AapAncMode seen{};
        watcher.GetEvent().Subscribe([&](size_t, AapAncMode m) { seen = m; });
        watcher.ProcessResponse(packet);
        assert(seen == mode);
    }

    std::puts("ANC self-check OK");
}
