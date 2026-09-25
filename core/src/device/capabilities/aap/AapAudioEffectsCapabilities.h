// MyPods
// License: GPL-3.0

#pragma once
#include "AapCapability.h"
#include "audio/AudioEffects.h"
#include <chrono>

namespace MagicPodsCore
{
    // Spatial audio: off, fixed (virtual speakers in front), head tracked (speakers stay put when you turn).
    // Rendering happens on this computer (AudioEffects), head orientation streams from the AirPods (AAP 0x17).
    class AapSpatialAudioCapability : public AapCapability
    {
    private:
        int mode = 0;
        const bool headTracking;
        bool tracking = false;
        int samples = 0;
        double neutral2 = 0, neutral3 = 0, center = 0;
        std::chrono::steady_clock::time_point lastSample{};

        void UpdateTracking();

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;
        void Reset() override;

    public:
        explicit AapSpatialAudioCapability(AapDevice &device);
        void SetFromJson(const nlohmann::json &json) override;

        // AirPods 1/2 and older Beats have no motion sensors, so no head tracking
        static bool HasHeadTracking(unsigned short model);

        // Yaw in degrees from a head-tracking packet relative to the given neutral orientation values
        static double Yaw(const std::vector<unsigned char> &packet, double neutral2, double neutral3);
    };

    // Equalizer with Apple Music's presets, applied on this computer in front of the headphones, plus the measured
    // headphone correction ("correction", only for models with a measurement) and crossfeed ("crossfeed").
    class AapEqualizerCapability : public AapCapability
    {
    private:
        std::string preset;
        const bool hasCorrection;
        bool corrected;
        bool crossfeed;

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;

    public:
        explicit AapEqualizerCapability(AapDevice &device);
        void SetFromJson(const nlohmann::json &json) override;

        // AutoEQ correction to the Harman target for the model, empty if nobody measured it
        static std::vector<Biquad> Correction(unsigned short model);
    };
}
