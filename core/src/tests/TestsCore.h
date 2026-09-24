// MyPods
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // Client lifecycle, send queue and settings file. No hardware needed.
    class TestsCore
    {
    public:
        TestsCore();
        int failures = 0;

    private:
        void Test(const char *name, bool ok);
    };
}
