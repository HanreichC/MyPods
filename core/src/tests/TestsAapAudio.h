// MyPods
// License: GPL-3.0

#pragma once

namespace MagicPodsCore
{
    // Byte-level checks for audio switching, head tracking and the effect chain. No hardware needed.
    class TestsAapAudio
    {
    public:
        TestsAapAudio();
        int failures = 0;

    private:
        void Test(const char *name, bool ok);
    };
}
