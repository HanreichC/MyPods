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

    std::puts("DesktopCheck: OK");
    return 0;
}
