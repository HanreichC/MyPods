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

    struct EffectsConfig
    {
        SpatialMode spatial = SpatialMode::Off;
        std::array<double, 10> eq{}; // dB at 32, 64, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz

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

        static std::vector<std::string> PresetNames();
        static std::optional<std::array<double, 10>> Preset(const std::string &name);
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
