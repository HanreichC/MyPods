// Standalone self-check for the low battery warning (not part of the UI build):
//   g++ -std=c++20 -fPIC -Iui/src/app/cpp ui/tests/LowBatteryCheck.cpp $(pkg-config --cflags --libs Qt6Core) -o /tmp/lowbat && /tmp/lowbat

#include <cassert>
#include <cstdio>
#include "LowBattery.h"

static QVariantMap Pod(int level, bool charging = false, int status = 2)
{
    return {{"battery", level}, {"charging", charging}, {"status", status}};
}

int main()
{
    QSet<QString> notified;
    const QString a = QStringLiteral("AA:BB");

    assert(LowBattery::newlyLow({{"left", Pod(50)}, {"right", Pod(40)}}, a, notified) == -1);
    // both pods cross at once: one warning with the lower level
    assert(LowBattery::newlyLow({{"left", Pod(10)}, {"right", Pod(8)}}, a, notified) == 8);
    // still low, hovering: no repeat
    assert(LowBattery::newlyLow({{"left", Pod(11)}, {"right", Pod(7)}}, a, notified) == -1);
    assert(LowBattery::newlyLow({{"left", Pod(9)}, {"right", Pod(7)}}, a, notified) == -1);
    // charging re-arms, the next drop warns again
    assert(LowBattery::newlyLow({{"left", Pod(9, true)}}, a, notified) == -1);
    assert(LowBattery::newlyLow({{"left", Pod(9)}}, a, notified) == 9);
    // cached (status 3) and the case never warn
    assert(LowBattery::newlyLow({{"single", Pod(5, false, 3)}, {"case", Pod(3)}}, QStringLiteral("CC:DD"), notified) == -1);
    // single-unit headphones (AirPods Max)
    assert(LowBattery::newlyLow({{"single", Pod(10)}}, QStringLiteral("EE:FF"), notified) == 10);

    std::puts("LowBatteryCheck OK");
}
