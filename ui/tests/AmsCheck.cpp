// Standalone self-check for the iPhone's now playing values (not part of the UI build):
//   g++ -std=c++20 -fPIC -Iui/src/app/cpp ui/tests/AmsCheck.cpp $(pkg-config --cflags --libs Qt6Core) -o /tmp/ams && /tmp/ams

#include <cassert>
#include <cstdio>
#include "Ams.h"

int main()
{
    assert(Ams::isPlaying("1,1.0,12.345"));
    assert(Ams::isPlaying("3,2.0,5.0"));   // fast forwarding
    assert(!Ams::isPlaying("0,0.0,12.345"));
    assert(!Ams::isPlaying(""));            // no player

    assert(Ams::volumePercent("0.5") == 50);
    assert(Ams::volumePercent("1.0") == 100);
    assert(Ams::volumePercent("") == -1);
    assert(Ams::volumePercent("0.07317692") == 6); // off the steps: the nearest one, 1 of 16
    assert(Ams::volumePercent("0.1356769") == 13); // 2 of 16

    assert(Ams::volumeSteps(50, 50) == 0);
    assert(Ams::volumeSteps(50, 100) == 8);
    assert(Ams::volumeSteps(13, 12) == 0);  // the slider's 12.5 against the stored 13: the same step
    assert(Ams::volumeSteps(6, 12) == 1);
    assert(Ams::volumeSteps(63, 60) == 0);
    assert(Ams::afterSteps(50, 1) == 56);
    assert(Ams::afterSteps(100, 1) == 100);
    assert(Ams::afterSteps(0, -1) == 0);

    // The popup's + and − (TrayPopup.qml neighbour(): the next step) walk through all 16 steps
    const auto neighbour = [](int value, int direction) {
        return qRound((qRound(value / Ams::VolumeStep) + direction) * Ams::VolumeStep);
    };
    int volume = 0, clicks = 0;
    while (volume < 100 && clicks < 50) {
        volume = Ams::afterSteps(volume, Ams::volumeSteps(volume, neighbour(volume, 1)));
        ++clicks;
    }
    assert(volume == 100 && clicks == 16);
    while (volume > 0 && clicks < 100) {
        volume = Ams::afterSteps(volume, Ams::volumeSteps(volume, neighbour(volume, -1)));
        ++clicks;
    }
    assert(volume == 0 && clicks == 32);

    // Dragging: every value the slider snaps to, sent as the int it arrives as, is exactly one step from
    // its neighbour and none from what the popup stores after sending it (no step back and forth)
    for (int k = 0; k < 16; ++k) {
        const int here = int(k * Ams::VolumeStep), next = int((k + 1) * Ams::VolumeStep);
        assert(Ams::volumeSteps(here, next) == 1 && Ams::volumeSteps(next, here) == -1);
        assert(Ams::volumeSteps(Ams::afterSteps(here, 1), next) == 0);
    }

    std::puts("AmsCheck: OK");
    return 0;
}
