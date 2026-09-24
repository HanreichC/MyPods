// MyPods
// License: GPL-3.0

#include "AudioEffects.h"
#include "Logger.h"

#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iterator>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace MagicPodsCore
{
    static constexpr std::array<int, 10> EQ_FREQS{32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    // ponytail: generic KEMAR HRTF shipped with libmysofa; Apple personalizes it from an ear scan. Swap the file for a personal SOFA to upgrade.
    static constexpr const char *SOFA_FILE = "/usr/share/libmysofa/default.sofa";
    // Virtual stereo speakers like a screen in front of you; SOFA azimuth runs counter-clockwise, 30 = front left.
    static constexpr double SPEAKER_AZIMUTH = 30.0;

    // Apple Music equalizer presets (values as circulated for iTunes, not an official Apple table).
    static const std::vector<std::pair<std::string, std::array<double, 10>>> PRESETS{
        {"Off", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {"Acoustic", {5, 4.9, 3.95, 1.05, 1.75, 1.75, 3.5, 4.1, 3.55, 2.15}},
        {"Bass Booster", {5.5, 4.25, 3.5, 2.5, 1.25, 0, 0, 0, 0, 0}},
        {"Bass Reducer", {-5.5, -4.25, -3.5, -2.5, -1.25, 0, 0, 0, 0, 0}},
        {"Classical", {4.75, 3.75, 3, 2.5, -1.5, -1.5, 0, 2.25, 3.25, 3.75}},
        {"Dance", {3.57, 6.55, 4.99, 0, 1.92, 3.65, 5.15, 4.54, 3.59, 0}},
        {"Deep", {4.95, 3.55, 1.75, 1, 2.85, 2.5, 1.45, -2.15, -3.55, -4.6}},
        {"Electronic", {4.25, 3.8, 1.2, 0, -2.15, 2.2, 0.85, 1.25, 3.95, 4.8}},
        {"Hip-Hop", {5, 4.25, 1.5, 3, -1, -1, 1.5, -0.5, 2, 3}},
        {"Jazz", {4, 3, 1.5, 2.25, -1.5, -1.5, 0, 1.5, 3, 3.75}},
        {"Loudness", {6, 4, 0, 0, -2, 0, -1, -5, 5, 1}},
        {"Lounge", {-3, -1.5, -0.5, 1.5, 4, 2.5, 0, -1.5, 2, 1}},
        {"Piano", {3, 2, 0, 2.5, 3, 1.5, 3.5, 4.5, 3, 3.5}},
        {"Pop", {-1.5, -1, 0, 2, 4, 4, 2, 0, -1, -1.5}},
        {"R&B", {2.62, 6.92, 5.65, 1.33, -2.19, -1.5, 2.32, 2.65, 3, 3.75}},
        {"Rock", {5, 4, 3, 1.5, -0.5, -1, 0.5, 2.5, 3.5, 4.5}},
        {"Spoken Word", {-3.46, -0.47, 0, 0.69, 3.46, 4.61, 4.84, 4.28, 2.54, 0}},
        {"Treble Booster", {0, 0, 0, 0, 0, 1.25, 2.5, 3.5, 4.25, 5.5}},
        {"Treble Reducer", {0, 0, 0, 0, 0, -1.25, -2.5, -3.5, -4.25, -5.5}},
        {"Vocal Booster", {-1.5, -3, -3, 1.5, 3.5, 3.5, 3, 1.5, 0, -1.5}},
    };

    bool EffectsConfig::IsNeutral() const
    {
        return spatial == SpatialMode::Off && eq == std::array<double, 10>{};
    }

    std::vector<std::string> AudioEffects::PresetNames()
    {
        std::vector<std::string> names;
        for (auto &[name, gains] : PRESETS)
            names.push_back(name);
        return names;
    }

    std::optional<std::array<double, 10>> AudioEffects::Preset(const std::string &name)
    {
        for (auto &[presetName, gains] : PRESETS)
            if (presetName == name)
                return gains;
        return std::nullopt;
    }

    static double Azimuth(double base, double yaw)
    {
        return std::fmod(std::fmod(base + yaw, 360.0) + 360.0, 360.0);
    }

    static std::string Num(double v)
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.2f", v); // daemon never calls setlocale, so '.' is the separator
        return buf;
    }

    std::string AudioEffects::BuildConfig(const std::string &sink, const std::string &rawDescription, const EffectsConfig &config, double yaw)
    {
        // the name comes from the headphones; keep it from breaking out of the quoted config string
        std::string description;
        for (char ch : rawDescription)
            if (ch != '"' && ch != '\\' && ch != '\n')
                description += ch;
        // EQ nodes are always there (0 dB is transparent), so a preset change is a live control update, not a restart
        bool spatial = config.spatial != SpatialMode::Off;
        double peak = 0;
        for (double g : config.eq)
            peak = std::max(peak, g);

        std::ostringstream nodes, links;
        std::string inputs, outputs;
        for (std::string c : {"L", "R"})
        {
            std::vector<std::string> chain; // node names in signal order, each with an In and an Out port
            // broadband gain via a 0 Hz high shelf, so boosts don't clip
            nodes << "      { type = builtin label = bq_highshelf name = pre" << c << " control = { Freq = 0.0 Q = 1.0 Gain = " << Num(-peak) << " } }\n";
            chain.push_back("pre" + c);
            for (size_t b = 0; b < EQ_FREQS.size(); b++)
            {
                chain.push_back("eq" + c + std::to_string(b));
                nodes << "      { type = builtin label = bq_peaking name = " << chain.back() << " control = { Freq = " << EQ_FREQS[b]
                      << ".0 Q = 1.41 Gain = " << Num(config.eq[b]) << " } }\n";
            }
            if (spatial)
            {
                chain.push_back("sp" + c);
                nodes << "      { type = sofa label = spatializer name = sp" << c << " config = { filename = \"" << SOFA_FILE
                      << "\" } control = { Azimuth = " << Num(Azimuth(c == "L" ? SPEAKER_AZIMUTH : -SPEAKER_AZIMUTH, yaw)) << " Elevation = 0.0 Radius = 1.0 } }\n";
            }
            for (size_t i = 1; i < chain.size(); i++)
                links << "      { output = \"" << chain[i - 1] << ":Out\" input = \"" << chain[i] << ":In\" }\n";
            inputs += " \"" + chain.front() + ":In\"";
            outputs += " \"" + chain.back() + ":Out\"";
        }
        if (spatial)
        {
            // each ear hears both virtual speakers; -3 dB per path keeps the sum near the original loudness
            nodes << "      { type = builtin label = mixer name = mixL control = { \"Gain 1\" = 0.7 \"Gain 2\" = 0.7 } }\n"
                  << "      { type = builtin label = mixer name = mixR control = { \"Gain 1\" = 0.7 \"Gain 2\" = 0.7 } }\n";
            links << "      { output = \"spL:Out L\" input = \"mixL:In 1\" }\n"
                  << "      { output = \"spR:Out L\" input = \"mixL:In 2\" }\n"
                  << "      { output = \"spL:Out R\" input = \"mixR:In 1\" }\n"
                  << "      { output = \"spR:Out R\" input = \"mixR:In 2\" }\n";
            outputs = " \"mixL:Out\" \"mixR:Out\"";
        }

        std::ostringstream conf;
        conf << "context.properties = { log.level = 0 }\n"
             << "context.spa-libs = { audio.convert.* = audioconvert/libspa-audioconvert support.* = support/libspa-support }\n"
             << "context.modules = [\n"
             << "  { name = libpipewire-module-rt args = {} flags = [ ifexists nofail ] }\n"
             << "  { name = libpipewire-module-protocol-native }\n"
             << "  { name = libpipewire-module-client-node }\n"
             << "  { name = libpipewire-module-adapter }\n"
             << "  { name = libpipewire-module-filter-chain\n"
             << "    args = {\n"
             << "      node.description = \"" << description << "\"\n"
             << "      media.name = \"" << description << "\"\n"
             << "      filter.graph = {\n"
             << "        nodes = [\n" << nodes.str() << "        ]\n"
             << "        links = [\n" << links.str() << "        ]\n"
             << "        inputs = [" << inputs << " ]\n"
             << "        outputs = [" << outputs << " ]\n"
             << "      }\n"
             << "      capture.props = { node.name = \"" << SINK_NAME << "\" media.class = Audio/Sink audio.channels = 2 audio.position = [ FL FR ] }\n"
             // no fallback: if the headphones go away the chain must not start playing through the speakers
             << "      playback.props = { node.name = \"" << SINK_NAME << ".out\" node.passive = true audio.channels = 2 audio.position = [ FL FR ]"
             << " target.object = \"" << sink << "\" node.dont-reconnect = true node.dont-fallback = true }\n"
             << "    }\n"
             << "  }\n"
             << "]\n";
        return conf.str();
    }

    std::string AudioEffects::ControlCommand(const EffectsConfig &config, double yaw)
    {
        double peak = 0;
        for (double g : config.eq)
            peak = std::max(peak, g);
        std::string params;
        for (std::string c : {"L", "R"})
        {
            params += " \"pre" + c + ":Gain\" " + Num(-peak);
            for (size_t b = 0; b < EQ_FREQS.size(); b++)
                params += " \"eq" + c + std::to_string(b) + ":Gain\" " + Num(config.eq[b]);
        }
        if (config.spatial != SpatialMode::Off)
            params += " \"spL:Azimuth\" " + Num(Azimuth(SPEAKER_AZIMUTH, yaw)) + " \"spR:Azimuth\" " + Num(Azimuth(-SPEAKER_AZIMUTH, yaw));
        return std::string("s ") + SINK_NAME + " Props { params = [" + params + " ] }\n";
    }

#ifdef _WIN32
    AudioEffects &AudioEffects::Instance()
    {
        static AudioEffects instance;
        return instance;
    }

    AudioEffects::AudioEffects() = default;
    AudioEffects::~AudioEffects() = default;
    std::string AudioEffects::Apply(const std::string &sink, const std::string &, EffectsConfig) { return sink; }
    void AudioEffects::Stop() {}
    void AudioEffects::StopLocked() {}
    void AudioEffects::SetYaw(double) {}
#else
    static pid_t Spawn(const std::vector<const char *> &argv, int *stdinFd)
    {
        int fds[2] = {-1, -1};
        if (stdinFd && pipe(fds) != 0)
            return -1;
        pid_t pid = fork();
        if (pid == 0)
        {
            if (stdinFd)
            {
                // a command pipe means pw-cli, which echoes every graph change; keep that out of the daemon log
                dup2(fds[0], STDIN_FILENO);
                int null = open("/dev/null", O_WRONLY);
                dup2(null, STDOUT_FILENO);
                dup2(null, STDERR_FILENO);
            }
            // don't inherit the daemon's sockets (WebSocket port, L2CAP): an orphan would keep port 2020 busy
            close_range(3, ~0U, 0);
            execvp(argv[0], const_cast<char *const *>(argv.data()));
            _exit(127);
        }
        if (stdinFd)
        {
            close(fds[0]);
            *stdinFd = fds[1];
        }
        return pid;
    }

    static std::string RuntimePath(const std::string &name)
    {
        const char *runtime = std::getenv("XDG_RUNTIME_DIR");
        return std::string(runtime ? runtime : "/tmp") + "/" + name;
    }

    static void Kill(pid_t &pid)
    {
        if (pid <= 0)
            return;
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        pid = -1;
    }

    AudioEffects &AudioEffects::Instance()
    {
        static AudioEffects instance;
        return instance;
    }

    AudioEffects::AudioEffects()
    {
        signal(SIGPIPE, SIG_IGN); // a dead pw-cli must not take the daemon down on the next write
        // The UI stops the daemon with SIGTERM, which leaves the chain and pw-cli (it ignores EOF) running.
        // PR_SET_PDEATHSIG can't help: it follows the spawning worker thread, not the process.
        std::ifstream pids(RuntimePath("mypods-fx.pid"));
        pid_t pid;
        while (pids >> pid)
        {
            std::ifstream cmd("/proc/" + std::to_string(pid) + "/cmdline");
            std::string cmdline((std::istreambuf_iterator<char>(cmd)), std::istreambuf_iterator<char>());
            if (cmdline.find("mypods-fx.conf") != std::string::npos || cmdline.starts_with("pw-cli"))
                kill(pid, SIGTERM);
        }
    }

    AudioEffects::~AudioEffects()
    {
        Stop();
    }

    std::string AudioEffects::Apply(const std::string &sink, const std::string &description, EffectsConfig config)
    {
        std::lock_guard lock{_lock};
        // without the HRTF there is no spatializer; checked before IsNeutral so an empty chain is never built
        if (config.spatial != SpatialMode::Off && !std::filesystem::exists(SOFA_FILE))
        {
            Logger::Error("AudioEffects: %s missing (install libmysofa), spatial audio disabled", SOFA_FILE);
            config.spatial = SpatialMode::Off;
        }
        bool alive = _chain > 0 && waitpid(_chain, nullptr, WNOHANG) == 0;
        if (!alive)
            _chain = -1; // exited (and now reaped), so it must not be signalled again
        // Same graph (only spatial on/off changes it): update the running chain, a restart would drop the stream and pause the player.
        // ponytail: EQ "Off" then leaves a flat chain running until the headphones go away; costs a few biquads, keeps playback going
        if (alive && _ctlFd >= 0 && sink == _sink && (config.spatial != SpatialMode::Off) == (_config.spatial != SpatialMode::Off))
        {
            std::string cmd = ControlCommand(config, 0);
            if (config == _config || write(_ctlFd, cmd.data(), cmd.size()) == static_cast<ssize_t>(cmd.size()))
            {
                _config = config;
                _yaw = 0;
                return SINK_NAME;
            }
            Logger::Error("AudioEffects: pw-cli not accepting commands, restarting the chain");
        }
        if (config.IsNeutral())
        {
            StopLocked();
            return sink;
        }

        StopLocked();
        std::string path = RuntimePath("mypods-fx.conf");
        std::ofstream(path) << BuildConfig(sink, description, config, 0);

        _chain = Spawn({"pipewire", "-c", path.c_str(), nullptr}, nullptr);
        _ctl = Spawn({"env", "LC_ALL=C", "pw-cli", nullptr}, &_ctlFd);
        if (_chain <= 0)
        {
            Logger::Error("AudioEffects: could not start pipewire filter-chain");
            StopLocked();
            return sink;
        }
        std::ofstream(RuntimePath("mypods-fx.pid")) << _chain << " " << _ctl << "\n";
        _sink = sink;
        _config = config;
        _yaw = 0;
        Logger::Info("AudioEffects: chain started in front of %s", sink.c_str());
        return SINK_NAME;
    }

    void AudioEffects::Stop()
    {
        std::lock_guard lock{_lock};
        StopLocked();
    }

    void AudioEffects::StopLocked()
    {
        if (_ctlFd >= 0)
            close(_ctlFd);
        _ctlFd = -1;
        Kill(_ctl);
        if (_chain > 0)
            Logger::Info("AudioEffects: chain stopped");
        Kill(_chain);
        _sink.clear();
        _config = {};
    }

    void AudioEffects::SetYaw(double degrees)
    {
        std::lock_guard lock{_lock};
        auto now = std::chrono::steady_clock::now();
        // ponytail: one pw-cli line per update, rate-limited to 20 Hz with a 1.5 degree deadband; a native pw_stream control would be smoother
        if (_ctlFd < 0 || _config.spatial != SpatialMode::HeadTracked || std::abs(degrees - _yaw) < 1.5 || now - _yawSentAt < std::chrono::milliseconds(50))
            return;
        _yaw = degrees;
        _yawSentAt = now;
        std::string cmd = std::string("s ") + SINK_NAME + " Props { params = [ \"spL:Azimuth\" " + Num(Azimuth(SPEAKER_AZIMUTH, degrees)) +
                          " \"spR:Azimuth\" " + Num(Azimuth(-SPEAKER_AZIMUTH, degrees)) + " ] }\n";
        if (write(_ctlFd, cmd.data(), cmd.size()) < 0)
            Logger::Debug("AudioEffects: pw-cli not accepting commands");
    }
#endif
}
