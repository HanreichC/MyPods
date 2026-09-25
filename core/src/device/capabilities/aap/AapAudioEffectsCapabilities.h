// MyPods
// License: GPL-3.0

#pragma once
#include "device/capabilities/cmn/CmnAudioEffectsCapabilities.h"
#include "device/AapDevice.h"
#include <chrono>

namespace MagicPodsCore
{
    // Spatial audio with head tracking: the head orientation streams from the AirPods (AAP 0x17).
    class AapSpatialAudioCapability : public CmnSpatialAudioCapability
    {
    private:
        AapDevice &aap;
        size_t responseDataReceivedId = 0;
        bool talking = false; // an AAP packet arrived this session
        bool tracking = false;
        int samples = 0;
        double neutral2 = 0, neutral3 = 0, center = 0;
        std::chrono::steady_clock::time_point lastSample{};

        void OnReceivedData(const std::vector<unsigned char> &data);

    protected:
        void UpdateTracking() override;
        void Reset() override;

    public:
        explicit AapSpatialAudioCapability(AapDevice &device);
        ~AapSpatialAudioCapability() override;

        // AirPods 1/2 and older Beats have no motion sensors, so no head tracking
        static bool HasHeadTracking(unsigned short model);

        // Yaw in degrees from a head-tracking packet relative to the given neutral orientation values
        static double Yaw(const std::vector<unsigned char> &packet, double neutral2, double neutral3);
    };
}
