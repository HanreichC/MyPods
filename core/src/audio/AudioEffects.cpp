// MyPods
// License: GPL-3.0

#include "AudioEffects.h"
#include "Logger.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <numbers>
#include <sstream>
#include <iterator>
#include <tuple>
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
    // The KEMAR set is measured at the eardrum and includes the measurement loudspeaker: through the +-30 degree pair a
    // centered voice came out 12 dB thin at 100 Hz, -35 dB at 31 Hz, and 7 dB sharp at 2-4 kHz. Below the crossover
    // both ears hear both speakers alike, so the bass skips the HRTF as (L+R)/2 (Linkwitz-Riley, 4th order); above it
    // these flatten the phantom center. Fitted offline to MIT_KEMAR_normal_pinna.sofa after libmysofa's loudness
    // normalization, 1/3-octave within 0.6 dB from 20 Hz to 8 kHz, and measured through PipeWire the same.
    // ponytail: bass localization cues under 250 Hz are dropped with the HRTF's broken low end; a better-measured SOFA could take a lower crossover
    static constexpr double KEMAR_CROSSOVER = 250;
    static const std::vector<Biquad> KEMAR_COMPENSATION{
        {Biquad::LowShelf, 345, 14.0, 0.30}, {Biquad::Peaking, 457, 3.4, 1.41}, {Biquad::Peaking, 1453, 10.1, 1.02},
        {Biquad::Peaking, 2204, -10.5, 4.00}, {Biquad::Peaking, 4213, -7.8, 0.49}, {Biquad::HighShelf, 4419, 4.2, 2.59},
        {Biquad::Peaking, 6853, 2.7, 3.33}, {Biquad::Peaking, 8426, 6.0, 4.00},
    };
    // Peak of crossover, compensation, HRTF and mix for any stereo input (same analysis): looking at the screen, and
    // with the head turned up to 45 degrees, where the ear facing a speaker gets louder.
    // ponytail: past 45 degrees of head turn up to +15 dB can still clip; a limiter after the mix would cover it. A custom SOFA gets neither crossover nor compensation and borrows these numbers.
    static constexpr double SPATIAL_PEAK_FIXED_DB = 10.4;
    static constexpr double SPATIAL_PEAK_TRACKED_DB = 12.2;
    // bs2b's default crossfeed level (700 Hz, 4.5 dB): L' = L + g * lowpass(R - L). The other channel's bass arrives
    // 4.5 dB below the own one, and mono passes untouched (a shelf on the direct path instead dips it 2.7 dB at 580 Hz).
    // ponytail: no extra interaural delay, bs2b doesn't add one either
    static const double CROSSFEED_FEED = std::pow(10.0, -4.5 / 20);
    static const double CROSSFEED_GAIN = CROSSFEED_FEED / (1 + CROSSFEED_FEED);
    static const Biquad CROSSFEED_LOWPASS{Biquad::LowPass, 700, 0, 0.5};

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
        return spatial == SpatialMode::Off && eq == std::array<double, 10>{} && !(corrected && !correction.empty()) && !crossfeed;
    }

    // Per-channel biquads in signal order with the gains in effect: the 10 EQ bands ("eq"), then the correction ("co")
    static std::vector<std::pair<std::string, Biquad>> Filters(const EffectsConfig &config)
    {
        std::vector<std::pair<std::string, Biquad>> filters;
        for (size_t b = 0; b < EQ_FREQS.size(); b++)
            filters.push_back({"eq" + std::to_string(b), {Biquad::Peaking, static_cast<double>(EQ_FREQS[b]), config.eq[b], 1.41}});
        for (size_t i = 0; i < config.correction.size(); i++)
        {
            Biquad bq = config.correction[i];
            if (!config.corrected)
                bq.gain = 0;
            filters.push_back({"co" + std::to_string(i), bq});
        }
        return filters;
    }

    // Node name for channel "L"/"R": "eq3" -> "eqL3"
    static std::string NodeName(const std::string &filter, const std::string &c)
    {
        return filter.substr(0, 2) + c + filter.substr(2);
    }

    static bool DefaultSofa(const EffectsConfig &config)
    {
        return config.sofa.empty() || config.sofa == SOFA_FILE;
    }

    // Frequency response of a biquad at f Hz, with the coefficients of PipeWire's spa/plugins/audioconvert/biquad.c
    static std::complex<double> Response(const Biquad &bq, double f, double rate)
    {
        double A = std::pow(10.0, bq.gain / 40), w0 = 2 * std::numbers::pi * bq.freq / rate;
        double alpha = std::sin(w0) / (2 * bq.q), k = std::cos(w0), k2 = 2 * std::sqrt(A) * alpha;
        double b0, b1, b2, a0, a1, a2;
        switch (bq.type)
        {
        case Biquad::Peaking:
            b0 = 1 + alpha * A, b1 = -2 * k, b2 = 1 - alpha * A, a0 = 1 + alpha / A, a1 = -2 * k, a2 = 1 - alpha / A;
            break;
        case Biquad::LowShelf:
            b0 = A * ((A + 1) - (A - 1) * k + k2), b1 = 2 * A * ((A - 1) - (A + 1) * k), b2 = A * ((A + 1) - (A - 1) * k - k2);
            a0 = (A + 1) + (A - 1) * k + k2, a1 = -2 * ((A - 1) + (A + 1) * k), a2 = (A + 1) + (A - 1) * k - k2;
            break;
        case Biquad::HighShelf:
            b0 = A * ((A + 1) + (A - 1) * k + k2), b1 = -2 * A * ((A - 1) + (A + 1) * k), b2 = A * ((A + 1) + (A - 1) * k - k2);
            a0 = (A + 1) - (A - 1) * k + k2, a1 = 2 * ((A - 1) - (A + 1) * k), a2 = (A + 1) - (A - 1) * k - k2;
            break;
        case Biquad::LowPass:
            b0 = (1 - k) / 2, b1 = 1 - k, b2 = (1 - k) / 2, a0 = 1 + alpha, a1 = -2 * k, a2 = 1 - alpha;
            break;
        default: // HighPass
            b0 = (1 + k) / 2, b1 = -(1 + k), b2 = (1 + k) / 2, a0 = 1 + alpha, a1 = -2 * k, a2 = 1 - alpha;
        }
        auto z = std::polar(1.0, -2 * std::numbers::pi * f / rate);
        return (b0 + b1 * z + b2 * z * z) / (a0 + a1 * z + a2 * z * z);
    }

    double AudioEffects::HeadroomDb(const EffectsConfig &config)
    {
        // adjacent bands add up (Dance peaks 1.7 dB above its largest band), so this measures the summed curve
        auto filters = Filters(config);
        bool crossfeed = config.crossfeed && config.spatial == SpatialMode::Off;
        double peak = 1;
        for (double rate : {44100.0, 48000.0})
            for (double f = 10; f < 20000; f *= 1.005)
            {
                std::complex<double> h = 1;
                for (auto &[name, bq] : filters)
                    h *= Response(bq, f, rate);
                double gain = std::abs(h);
                // an ear gets (1 - g*lowpass) of its own channel and g*lowpass of the other: worst case they add up
                if (crossfeed)
                {
                    auto lowpass = CROSSFEED_GAIN * Response(CROSSFEED_LOWPASS, f, rate);
                    gain *= std::abs(1.0 - lowpass) + std::abs(lowpass);
                }
                peak = std::max(peak, gain);
            }
        // ponytail: EQ and spatial peaks are added although they rarely sit at the same frequency; costs level, never clips
        double spatial = config.spatial == SpatialMode::Fixed ? SPATIAL_PEAK_FIXED_DB : config.spatial == SpatialMode::HeadTracked ? SPATIAL_PEAK_TRACKED_DB : 0;
        return 20 * std::log10(peak) + spatial;
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
        // the name comes from the headphones and the SOFA path from the config; keep both from breaking out of a quoted config string
        auto clean = [](const std::string &raw)
        {
            std::string out;
            for (char ch : raw)
                if (ch != '"' && ch != '\\' && ch != '\n')
                    out += ch;
            return out;
        };
        std::string description = clean(rawDescription);
        // EQ and correction nodes are always there (0 dB is transparent), so a preset change is a live control update, not a restart
        bool spatial = config.spatial != SpatialMode::Off, kemar = DefaultSofa(config);
        static const char *LABELS[] = {"bq_peaking", "bq_lowshelf", "bq_highshelf", "bq_lowpass", "bq_highpass"};
        std::ostringstream nodes, links;
        auto biquad = [&](const std::string &name, const Biquad &bq)
        {
            nodes << "      { type = builtin label = " << LABELS[bq.type] << " name = " << name << " control = { Freq = " << Num(bq.freq)
                  << " Q = " << Num(bq.q) << " Gain = " << Num(bq.gain) << " } }\n";
        };
        auto link = [&](const std::string &output, const std::string &input)
        {
            links << "      { output = \"" << output << "\" input = \"" << input << "\" }\n";
        };

        double headroom = HeadroomDb(config);
        std::string inputs, outputs;
        std::map<std::string, std::string> tails; // last node of each channel before the channels mix
        for (std::string c : {"L", "R"})
        {
            std::vector<std::string> chain; // node names in signal order, each with an In and an Out port
            // broadband gain via a 0 Hz high shelf, so boosts don't clip
            chain.push_back("pre" + c);
            biquad(chain.back(), {Biquad::HighShelf, 0, -headroom, 1});
            for (auto &[filter, bq] : Filters(config))
            {
                chain.push_back(NodeName(filter, c));
                biquad(chain.back(), bq);
            }
            tails[c] = chain.back();
            if (spatial)
            {
                if (kemar)
                {
                    std::vector<Biquad> highs{2, {Biquad::HighPass, KEMAR_CROSSOVER, 0, 0.71}};
                    highs.insert(highs.end(), KEMAR_COMPENSATION.begin(), KEMAR_COMPENSATION.end());
                    for (size_t i = 0; i < highs.size(); i++)
                    {
                        chain.push_back("sc" + c + std::to_string(i));
                        biquad(chain.back(), highs[i]);
                    }
                    biquad("lp" + c + "0", {Biquad::LowPass, KEMAR_CROSSOVER, 0, 0.71});
                    biquad("lp" + c + "1", {Biquad::LowPass, KEMAR_CROSSOVER, 0, 0.71});
                    link(tails[c] + ":Out", "lp" + c + "0:In");
                    link("lp" + c + "0:Out", "lp" + c + "1:In");
                }
                chain.push_back("sp" + c);
                nodes << "      { type = sofa label = spatializer name = sp" << c << " config = { filename = \"" << clean(kemar ? SOFA_FILE : config.sofa)
                      << "\" } control = { Azimuth = " << Num(Azimuth(c == "L" ? SPEAKER_AZIMUTH : -SPEAKER_AZIMUTH, yaw)) << " Elevation = 0.0 Radius = 1.0 } }\n";
            }
            for (size_t i = 1; i < chain.size(); i++)
                link(chain[i - 1] + ":Out", chain[i] + ":In");
            inputs += " \"" + chain.front() + ":In\"";
            outputs += " \"" + chain.back() + ":Out\"";
        }
        if (!spatial)
        {
            // crossfeed nodes are always there too; off mutes the low-passed paths, which the mixer then skips
            outputs.clear();
            double g = config.crossfeed ? CROSSFEED_GAIN : 0;
            for (std::string c : {"L", "R"})
            {
                biquad("xl" + c, CROSSFEED_LOWPASS);
                link(tails[c] + ":Out", "xl" + c + ":In");
            }
            for (std::string c : {"L", "R"})
            {
                std::string other = c == "L" ? "R" : "L";
                nodes << "      { type = builtin label = mixer name = xm" << c << " control = { \"Gain 1\" = 1.0 \"Gain 2\" = " << Num(g ? -g : 0.0)
                      << " \"Gain 3\" = " << Num(g) << " } }\n";
                link(tails[c] + ":Out", "xm" + c + ":In 1");
                link("xl" + c + ":Out", "xm" + c + ":In 2");
                link("xl" + other + ":Out", "xm" + c + ":In 3");
                outputs += " \"xm" + c + ":Out\"";
            }
        }
        else
        {
            // each ear hears both virtual speakers; -3 dB per path keeps the sum near the original loudness,
            // and with the KEMAR both ears get the bypassed bass as (L+R)/2
            outputs.clear();
            for (std::string c : {"L", "R"})
            {
                nodes << "      { type = builtin label = mixer name = mix" << c << " control = { \"Gain 1\" = 0.7 \"Gain 2\" = 0.7"
                      << (kemar ? " \"Gain 3\" = 0.5 \"Gain 4\" = 0.5" : "") << " } }\n";
                link("spL:Out " + c, "mix" + c + ":In 1");
                link("spR:Out " + c, "mix" + c + ":In 2");
                if (kemar)
                {
                    link("lpL1:Out", "mix" + c + ":In 3");
                    link("lpR1:Out", "mix" + c + ":In 4");
                }
                outputs += " \"mix" + c + ":Out\"";
            }
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
        std::string params, headroom = Num(-HeadroomDb(config));
        auto filters = Filters(config);
        for (std::string c : {"L", "R"})
        {
            params += " \"pre" + c + ":Gain\" " + headroom;
            for (auto &[filter, bq] : filters)
                params += " \"" + NodeName(filter, c) + ":Gain\" " + Num(bq.gain);
            if (config.spatial == SpatialMode::Off)
                params += " \"xm" + c + ":Gain 2\" " + Num(config.crossfeed ? -CROSSFEED_GAIN : 0) +
                          " \"xm" + c + ":Gain 3\" " + Num(config.crossfeed ? CROSSFEED_GAIN : 0);
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
            // the daemon blocks SIGTERM for its signal thread (main.cpp); exec keeps the mask, and Kill() relies on it
            sigset_t none;
            sigemptyset(&none);
            sigprocmask(SIG_SETMASK, &none, nullptr);
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
        // SIGTERM stops the chain (main.cpp), but a crash or SIGKILL leaves it and pw-cli (it ignores EOF) running.
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
        if (config.spatial != SpatialMode::Off && !DefaultSofa(config) && !std::filesystem::exists(config.sofa))
        {
            Logger::Error("AudioEffects: %s missing, using the default HRTF", config.sofa.c_str());
            config.sofa.clear();
        }
        // without the HRTF there is no spatializer; checked before IsNeutral so an empty chain is never built
        if (config.spatial != SpatialMode::Off && DefaultSofa(config) && !std::filesystem::exists(SOFA_FILE))
        {
            Logger::Error("AudioEffects: %s missing (install libmysofa), spatial audio disabled", SOFA_FILE);
            config.spatial = SpatialMode::Off;
        }
        bool alive = _chain > 0 && waitpid(_chain, nullptr, WNOHANG) == 0;
        if (!alive)
            _chain = -1; // exited (and now reaped), so it must not be signalled again
        // Same graph (spatial on/off, the HRTF and the correction's filter count change it): update the running chain,
        // a restart would drop the stream and pause the player.
        // ponytail: EQ "Off" then leaves a flat chain running until the headphones go away; costs a few biquads, keeps playback going
        auto graph = [](const EffectsConfig &c) { return std::tuple(c.spatial != SpatialMode::Off, c.correction.size(), DefaultSofa(c) ? "" : c.sofa); };
        if (alive && _ctlFd >= 0 && sink == _sink && graph(config) == graph(_config))
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
