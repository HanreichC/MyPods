// MyPods
// License: GPL-3.0

#pragma once

#include <array>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/types.h>
#endif

namespace MagicPodsCore
{
#ifdef _WIN32
    using pid_t = int; // members only, the chain never runs on Windows
#endif

    enum class SpatialMode : int
    {
        Off = 0,
        Fixed = 1,
        HeadTracked = 2,
    };

    // One of PipeWire's builtin biquads (RBJ cookbook, same Q meaning as AutoEQ's ParametricEQ.txt)
    struct Biquad
    {
        enum Type { Peaking, LowShelf, HighShelf, LowPass, HighPass } type;
        double freq, gain, q; // Hz, dB (ignored by the passes)

        bool operator==(const Biquad &) const = default;
    };

    struct EffectsConfig
    {
        SpatialMode spatial = SpatialMode::Off;
        bool surround = false;       // spatial audio takes 7.1 (films, games); PipeWire upmixes stereo players into it
        std::array<double, 10> eq{}; // dB at 32, 64, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
        std::vector<Biquad> correction; // headphone correction: the user's ParametricEQ.txt or the model's AutoEQ one, empty if there is none
        bool corrected = false;         // correction on; its nodes stay in the chain either way, so toggling is a live update
        bool crossfeed = false;         // only without spatial audio, which mixes the channels itself
        bool loudness = false;          // ISO 226 bass compensation that follows `volume`
        double volume = 1;              // listening volume, 1 = 100 % (headphones and chain sink together)
        double loudnessReference = 90;  // phon at 100 % volume, the calibration of the loudness compensation
        std::array<std::array<double, 6>, 2> hearing{}; // left, right: gains (dB) at HEARING_FREQS from the audiogram
        bool bypass = false;            // A/B: every effect off, the pre-gain stays so both sides play equally loud
        std::string sofa;               // HRTF file, empty = the default KEMAR
        std::string limiter;            // LADSPA limiter plugin, empty if it isn't installed (Apply fills it in)

        bool IsNeutral() const;
        bool operator==(const EffectsConfig &) const = default;
    };

    // Spatial audio and equalizer as a PipeWire filter-chain sink in front of the headphones
    // (runs `pipewire -c <generated conf>` as a child process, the documented way to host a filter-chain).
    // Windows has no user-space equivalent (it would take an APO driver): Apply/Stop/SetYaw do nothing there
    // and the capabilities are not offered, while the presets still serve the Parrot Zik's on-device EQ.
    class AudioEffects
    {
    public:
        static constexpr const char *SINK_NAME = "mypods_fx";

        static AudioEffects &Instance();

        // Puts the chain in front of `sink`, or removes it when the config is neutral.
        // Returns the sink applications should play to.
        std::string Apply(const std::string &sink, const std::string &description, EffectsConfig config);
        void Stop();

        // Head tracking: rotates the virtual speakers against the head so the sound stays anchored to the screen.
        void SetYaw(double degrees);
        // Listening volume changed: moves the loudness compensation along, a live update like SetYaw
        void SetVolume(double volume);

        static std::vector<std::string> PresetNames();
        static std::optional<std::array<double, 10>> Preset(const std::string &name);
        // AutoEQ / Equalizer APO ParametricEQ.txt ("Filter 1: ON PK Fc 105 Hz Gain -3.0 dB Q 0.70"), nullopt if it isn't one
        static std::optional<std::vector<Biquad>> ParseParametricEq(const std::string &text);
        // Audiogram thresholds in dB HL at 250, 500, 1k, 2k, 4k and 8k Hz ("20 25 30 40 55 60"), nullopt if it isn't one
        static std::optional<std::array<double, 6>> ParseAudiogram(const std::string &text);
        // Per-ear gains for an audiogram (half-gain rule)
        static std::array<double, 6> HearingGains(const std::array<double, 6> &thresholds);
        // Loudness compensation low shelf for the config's volume, 0 dB at and above the mixing level
        static Biquad LoudnessShelf(const EffectsConfig &config);
        // ISO 226:2003 sound pressure level (dB SPL) of the equal-loudness contour at a table frequency (20 Hz - 12.5 kHz)
        static double Iso226(double freq, double phon);
        // The installed LADSPA lookahead limiter (swh-plugins), empty if there is none
        static std::string FindLimiter();
        // How far the chain can lift any input above full scale (dB, >= 0); the pre-gain takes this off so nothing clips
        static double HeadroomDb(const EffectsConfig &config);
        // Gain of one biquad at `freq` Hz (48 kHz), as PipeWire computes it
        static double GainDb(const Biquad &bq, double freq);
        static std::string BuildConfig(const std::string &sink, const std::string &description, const EffectsConfig &config, double yaw);
        // pw-cli line that sets every gain (and the speaker angles) of a running chain
        static std::string ControlCommand(const EffectsConfig &config, double yaw);

    private:
        AudioEffects();
        ~AudioEffects();
        void StopLocked();

        std::mutex _lock;
        pid_t _chain = -1;
        pid_t _ctl = -1;
        int _ctlFd = -1;
        std::string _sink;
        EffectsConfig _config;
        double _yaw = 0;
        std::chrono::steady_clock::time_point _yawSentAt{};
    };
}
