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
        bool surround; // 7.1 input ("surround")
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

    // Equalizer with Apple Music's presets, applied on this computer in front of the headphones, plus the headphone
    // correction ("correction", with a measurement or an `eqFile`), crossfeed ("crossfeed"), loudness compensation
    // ("loudness"), the hearing profile from an audiogram per ear ("hearing", "audiogramLeft", "audiogramRight")
    // and the level-matched A/B comparison ("bypass").
    class AapEqualizerCapability : public AapCapability
    {
    private:
        std::string preset;
        bool corrected;
        bool crossfeed;
        std::atomic<bool> loudness; // read on the PulseAudio thread
        bool hearing;
        std::string audiograms[2];
        size_t sinkEventId = 0;
        std::atomic<bool> volumePending{false};

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnReceivedData(const std::vector<unsigned char> &data) override;

    public:
        explicit AapEqualizerCapability(AapDevice &device);
        ~AapEqualizerCapability() override;
        void SetFromJson(const nlohmann::json &json) override;

        // AutoEQ correction to the Harman target for the model, empty if nobody measured it
        static std::vector<Biquad> Correction(unsigned short model);
    };
}
