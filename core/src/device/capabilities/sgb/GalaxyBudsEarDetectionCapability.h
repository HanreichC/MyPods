// MyPods
// License: GPL-3.0

#pragma once
#include "GalaxyBudsCapability.h"
#include "sdk/sgb/watchers/GalaxyBudsEarDetectionWatcher.h"
#include "media/EarDetectionPause.h"

namespace MagicPodsCore
{
    // Taking a bud out pauses, putting it back resumes, as with AirPods. The Buds report where each bud is
    // (worn, idle, case) in their status updates; on/off is stored locally, the Buds have no such switch.
    class GalaxyBudsEarDetectionCapability : public GalaxyBudsCapability
    {
    private:
        bool option = true;
        int inEar = -1;
        GalaxyBudsEarDetectionWatcher watcher;
        size_t watcherEventId = 0;
        EarDetectionPause pause;

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const GalaxyBudsResponseData &data) override;
        void Reset() override;

    public:
        explicit GalaxyBudsEarDetectionCapability(GalaxyBudsDevice &device);
        ~GalaxyBudsEarDetectionCapability() override;
        void SetFromJson(const nlohmann::json &json) override;
    };
}
