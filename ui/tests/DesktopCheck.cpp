// Standalone self-check for the menu entry's Exec argument (not part of the UI build):
//   g++ -std=c++20 -fPIC -Iui/src/app/cpp ui/tests/DesktopCheck.cpp $(pkg-config --cflags --libs Qt6Core) -o /tmp/desktop && /tmp/desktop

#include <cassert>
#include <cstdio>
#include "DesktopManager.h"

int main()
{
    // an AppImage in a folder with a space: one argument, not two
    assert(desktopExecArgument("/home/chris/My Apps/MyPods.AppImage") == R"("/home/chris/My Apps/MyPods.AppImage")");
    // quote, dollar and backtick escaped for the shell-like parsing; a backslash escaped twice (quoting, then the string escape)
    assert(desktopExecArgument(R"(/opt/a"b$c`d)") == R"("/opt/a\\"b\\$c\\`d")");
    assert(desktopExecArgument(R"(/opt/a\b)") == R"("/opt/a\\\\b")");
    // a literal percent is %% (field codes start with %)
    assert(desktopExecArgument("/opt/100%/app") == R"("/opt/100%%/app")");

    // the Apple DeviceID: under [General], uncommented, Apple's vendor
    assert(hasAppleDeviceId("[General]\nName = laptop\nDeviceID = bluetooth:004C:0000:0000\n"));
    assert(hasAppleDeviceId("[General]\n  DeviceID=bluetooth:004c:0000:0000"));
    assert(!hasAppleDeviceId("[General]\n#DeviceID = bluetooth:004C:0000:0000\n"));       // commented out
    assert(!hasAppleDeviceId("[General]\nDeviceID = bluetooth:1234:5678:abcd\n"));       // another vendor
    assert(!hasAppleDeviceId("[Policy]\nDeviceID = bluetooth:004C:0000:0000\n"));        // wrong section
    assert(!hasAppleDeviceId("[General]\nDeviceIDX = bluetooth:004C:0000:0000\n"));      // another key
    assert(!hasAppleDeviceId(""));

    std::puts("DesktopCheck: OK");
    return 0;
}
