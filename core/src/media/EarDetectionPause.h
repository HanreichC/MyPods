// MyPods
// License: GPL-3.0

#pragma once

#include "media/MprisClient.h"
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace MagicPodsCore
{
    // Automatic ear detection like on a Mac: taking a bud out pauses what plays, putting it back resumes
    // exactly those players. Shared by AirPods and Galaxy Buds.
    class EarDetectionPause
    {
    private:
        std::mutex lock;
        std::vector<std::string> paused;
        int inEarBeforePause = 0;

    public:
        // What to do when the number of buds in the ears goes from `before` to `now`:
        // -1 pause, 1 resume, 0 nothing. Plain logic, see Changed for the effect.
        static int Decide(int before, int now, bool anythingPaused, int inEarBeforePause)
        {
            if (before < 0 || now == before)
                return 0; // first reading after connecting, or no change
            if (now < before && !anythingPaused)
                return -1;
            if (now >= inEarBeforePause && anythingPaused)
                return 1;
            return 0;
        }

        // `keep` holds the owner (Device::KeepAlive) for the thread: MPRIS calls go to other processes,
        // so they run off the Bluetooth reader
        void Changed(int before, int now, std::shared_ptr<void> keep)
        {
            std::thread([this, before, now, keep = std::move(keep)]()
            {
                std::lock_guard guard{lock};
                switch (Decide(before, now, !paused.empty(), inEarBeforePause))
                {
                case -1:
                    paused = MprisClient::Instance().PausePlaying();
                    inEarBeforePause = before;
                    break;
                case 1:
                    MprisClient::Instance().Play(paused);
                    paused.clear();
                    break;
                }
            }).detach();
        }

        // Disconnected: nothing to resume later
        void Reset()
        {
            std::lock_guard guard{lock};
            paused.clear();
        }
    };
}
