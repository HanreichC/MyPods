// MyPods
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // Client lifecycle, send queue, settings file, AAP device info and rename. No hardware needed.
    class TestsCore
    {
    public:
        TestsCore();
        int failures = 0;

    private:
        void Test(const char *name, bool ok);
    };
}
