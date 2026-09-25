// MyPods
// License: GPL-3.0

#pragma once
#include "../Capability.h"
#include "device/Device.h"
#include <atomic>
#include <mutex>
#include <optional>

namespace MagicPodsCore
{
    // Spatial audio: off, fixed (virtual speakers in front), head tracked (speakers stay put when you turn).
    // Rendering happens on this computer (AudioEffects), so any headphones get it while connected;
    // head tracking needs motion sensors (AapSpatialAudioCapability).
    class CmnSpatialAudioCapability : public Capability
    {
    private:
        size_t onConnectedId = 0;

    protected:
        Device &device;
        int mode = 0;
        bool surround; // 7.1 input ("surround")
        const bool headTracking;
        nlohmann::json CreateJsonBody() override;
        // The mode or the audio ownership may have changed
        virtual void UpdateTracking() {}

    public:
        explicit CmnSpatialAudioCapability(Device &device);
        ~CmnSpatialAudioCapability() override;
        void SetFromJson(const nlohmann::json &json) override;
    };

    // Equalizer with Apple Music's presets, applied on this computer in front of the headphones, plus the headphone
    // correction ("correction", with a measurement or an `eqFile`), crossfeed ("crossfeed"), loudness compensation
    // ("loudness"), the hearing profile from an audiogram per ear ("hearing", "audiogramLeft", "audiogramRight")
    // and the level-matched A/B comparison ("bypass"). "custom" sets the 10 bands by hand (preset "Custom"), "tilt" turns
    // the tone warmer or brighter, "hearingTest" measures the audiograms, and "response" is the curve all of it makes.
    class CmnEqualizerCapability : public Capability
    {
    private:
        Device &device;
        std::string preset;
        int tilt;
        std::mutex testLock; // the test is driven from the WebSocket loop and ends on the D-Bus thread when the headphones go
        std::optional<HearingTest> test; // running hearing test
        bool tooQuiet = false; // the current tone is louder than the headphones' volume allows, so it didn't play
        // Plays the current tone, skipping those the headphones can't play even at full volume; under testLock
        void PlayTestTone();
        // With the effect chain in front the volume keys move the chain's sink: the headphones go to 100 % for the tones
        void GiveTonesHeadroom();
        // the hearing test's commands: start, heard, missed, repeat, cancel
        void HearingTestCommand(const std::string &command);
        bool corrected;
        bool crossfeed;
        std::atomic<bool> loudness; // read on the PulseAudio thread
        bool hearing;
        std::string audiograms[2];
        size_t sinkEventId = 0;
        size_t onConnectedId = 0;
        std::atomic<bool> volumePending{false};

    protected:
        nlohmann::json CreateJsonBody() override;

    public:
        explicit CmnEqualizerCapability(Device &device);
        ~CmnEqualizerCapability() override;
        void SetFromJson(const nlohmann::json &json) override;
    };
}
