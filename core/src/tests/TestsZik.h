// MyPods
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // Parrot Zik framing and answer parsing. No hardware needed.
    class TestsZik
    {
    public:
        TestsZik();
        int failures = 0;

    private:
        void Test(const char *name, bool ok);
    };
}
