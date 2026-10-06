// MyPods
// License: GPL-3.0

#pragma once
#include "../Capability.h"
#include "device/Device.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

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

    // What happens to the sound on its way to the headphones (Linux): what plays (the source), what the output takes,
    // and why it isn't bit-perfect ("reasons"): "processed" (the effect chain), "encoded" (a Bluetooth codec, always
    // lossy), "resampled", "reduced" (fewer bits than the source) or "volume" (an application below 100 %).
    class CmnSignalPathCapability : public Capability
    {
    private:
        Device &device;
        size_t streamEventId = 0;
        size_t sinkEventId = 0;
        size_t onConnectedId = 0;
        std::mutex lock;
        nlohmann::json body; // under lock
        // Its own worker, joined in the destructor: a detached one could outlive the capability (KeepAlive is
        // empty while the device is still being created)
        std::thread worker;
        std::condition_variable wake;
        bool requested = false, exiting = false; // under lock
        // A burst of stream and sink changes (a track starts) makes one look, on the worker: the queries block
        void UpdateSoon();
        void Work();
        void Update();

    protected:
        nlohmann::json CreateJsonBody() override;
        void Reset() override;

    public:
        explicit CmnSignalPathCapability(Device &device);
        ~CmnSignalPathCapability() override;
        // The signal path for `streams` (those playing into the headphones or into the effect chain in front of them)
        // and the headphones' `output`; `processed`: the chain runs in front of them
        static nlohmann::json Describe(const std::vector<StreamInfo> &streams, const SinkDetails &output, bool processed);
        // Bits per sample of a PulseAudio format name ("s24le" 24, "float32le" 32), 0 if unknown
        static int Bits(const std::string &format);
    };

    // Equalizer with Apple Music's presets, applied on this computer in front of the headphones, plus the headphone
    // correction ("correction", with a measurement or an `eqFile`), crossfeed ("crossfeed"), loudness compensation
    // ("loudness"), the hearing profile from an audiogram per ear ("hearing", "audiogramLeft", "audiogramRight")
    // and the level-matched A/B comparison ("bypass"). "custom" sets the 10 bands by hand (preset "Custom"), "tilt" turns
    // the tone warmer or brighter, and "response" is the curve all of it makes.
    class CmnEqualizerCapability : public Capability
    {
    private:
        Device &device;
        std::string preset;
        int tilt;
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
