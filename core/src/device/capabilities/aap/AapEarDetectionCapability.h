// MyPods
// License: GPL-3.0

#pragma once
#include "AapCapability.h"
#include <mutex>

namespace MagicPodsCore
{
    // Automatic ear detection like on a Mac: taking a pod out pauses, putting it back resumes.
    // AAP: 04 00 04 00 06 00 <primary> <secondary> (00 in ear, 01 out, 02 in case); on/off is control command 0x0A.
    // Without AAP (Windows) the in-ear bits of the proximity advertisement drive it and on/off is a local setting.
    class AapEarDetectionCapability : public AapCapability
    {
    private:
        bool option = true;
        int primary = -1;
        int secondary = -1;
        int inEarBeforePause = 0;
        std::vector<std::string> paused;
        std::mutex pausedLock;
        size_t leEventId = 0;
        void Update(int newPrimary, int newSecondary);

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;
        void Reset() override;

    public:
        explicit AapEarDetectionCapability(AapDevice &device);
        ~AapEarDetectionCapability() override;
        void SetFromJson(const nlohmann::json &json) override;
    };
}
