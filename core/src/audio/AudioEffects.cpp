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
    // Virtual speakers; SOFA azimuth runs counter-clockwise, 30 = front left. Stereo is a screen in front of you,
    // 7.1 the ITU layout with the sides at 90 and the rears at 135 degrees. LFE has no place, it goes to both ears.
    struct Speaker
    {
        const char *id, *position;
        double azimuth;
        bool Lfe() const { return position == std::string("LFE"); }
    };
    static const std::vector<Speaker> STEREO{{"L", "FL", 30}, {"R", "FR", -30}};
    static const std::vector<Speaker> SURROUND{{"L", "FL", 30}, {"R", "FR", -30}, {"C", "FC", 0}, {"LFE", "LFE", 0},
                                               {"RL", "RL", 135}, {"RR", "RR", -135}, {"SL", "SL", 90}, {"SR", "SR", -90}};
    static constexpr double SPEAKER_GAIN = 0.7; // per virtual speaker and ear: -3 dB keeps a phantom center near the original loudness
    static constexpr double LFE_GAIN = 1.0;     // ponytail: LFE at 0 dB per ear; cinema plays it +10 dB, ITU downmixes drop it
    // The KEMAR set is measured at the eardrum and includes the measurement loudspeaker: through the +-30 degree pair a
    // centered voice came out 12 dB thin at 100 Hz, -35 dB at 31 Hz, and 7 dB sharp at 2-4 kHz. Below the crossover
    // both ears hear both speakers alike, so the bass skips the HRTF as (L+R)/2 (Linkwitz-Riley, 4th order); above it
    // these flatten the phantom center. Fitted offline to MIT_KEMAR_normal_pinna.sofa after libmysofa's loudness
    // normalization, 1/3-octave within 0.6 dB from 20 Hz to 8 kHz, and measured through PipeWire the same.
    // ponytail: bass localization cues under 250 Hz are dropped with the HRTF's broken low end; a better-measured SOFA could take a lower crossover
    // ponytail: the 7.1 center, sides and rears borrow the compensation fitted for the front pair
    static constexpr double KEMAR_CROSSOVER = 250;
    static const std::vector<Biquad> KEMAR_COMPENSATION{
        {Biquad::LowShelf, 345, 14.0, 0.30}, {Biquad::Peaking, 457, 3.4, 1.41}, {Biquad::Peaking, 1453, 10.1, 1.02},
        {Biquad::Peaking, 2204, -10.5, 4.00}, {Biquad::Peaking, 4213, -7.8, 0.49}, {Biquad::HighShelf, 4419, 4.2, 2.59},
        {Biquad::Peaking, 6853, 2.7, 3.33}, {Biquad::Peaking, 8426, 6.0, 4.00},
    };
    // Peak of crossover, compensation, HRTF and mix for any stereo input (same analysis): looking at the screen, and
    // with the head turned up to 45 degrees, where the ear facing a speaker gets louder. A custom SOFA gets neither
    // crossover nor compensation and borrows these numbers.
    static constexpr double SPATIAL_PEAK_FIXED_DB = 10.4;
    static constexpr double SPATIAL_PEAK_TRACKED_DB = 12.2;
    // With the lookahead limiter after the mix the pre-gain only takes off what typical material reaches; the
    // limiter catches the rare peaks up to the worst case above, and head turns past 45 degrees.
    // ponytail: 3 dB is a judgment, not a measurement over a music corpus; a louder master makes the limiter work more
    static constexpr double SPATIAL_PEAK_LIMITED_DB = 3.0;
    // ponytail: 7.1 input gets 3 dB more than the stereo peak; films keep lots of headroom, the limiter covers the rest
    static constexpr double SURROUND_EXTRA_DB = 3.0;
    static constexpr const char *LIMITER_PLUGIN = "fast_lookahead_limiter_1913.so";
    static constexpr double LIMITER_CEILING_DB = -1.0; // AAC encoding overshoots a little
    // bs2b's default crossfeed level (700 Hz, 4.5 dB): L' = L + g * lowpass(R - L). The other channel's bass arrives
    // 4.5 dB below the own one, and mono passes untouched (a shelf on the direct path instead dips it 2.7 dB at 580 Hz).
    // ponytail: no extra interaural delay, bs2b doesn't add one either
    static const double CROSSFEED_FEED = std::pow(10.0, -4.5 / 20);
    static const double CROSSFEED_GAIN = CROSSFEED_FEED / (1 + CROSSFEED_FEED);
    static const Biquad CROSSFEED_LOWPASS{Biquad::LowPass, 700, 0, 0.5};
    // Loudness: music is mixed at about 80 phon. Quieter, the ear loses bass faster than mids (ISO 226), and this low
    // shelf gives it back: 1.3 x the contour difference at 63 Hz, fitted offline to the ISO 226 table, within 1.9 dB
    // from 25 Hz to 1.6 kHz between 30 and 70 phon. Above 1 kHz the contours move by 2 dB at most up to 10 kHz, so there
    // is no treble shelf. The volume maps to phon like PulseAudio's cubic volume (60 dB per decade).
    // ponytail: phon at 100 % volume (`loudnessReference`) is a guess per model; a sound level meter calibrates it
    static constexpr double LOUDNESS_MIX_PHON = 80;
    static constexpr double LOUDNESS_MAX_DB = 15; // what earbuds and the pre-gain sensibly take
    static constexpr std::array<int, 6> HEARING_FREQS{250, 500, 1000, 2000, 4000, 8000};
    // ponytail: half-gain rule (Lybarger), capped at 20 dB; NAL-R would weigh the frequencies against each other
    static constexpr double HEARING_MAX_DB = 20;

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
        return spatial == SpatialMode::Off && eq == std::array<double, 10>{} && !(corrected && !correction.empty()) && !crossfeed &&
               !loudness && hearing == std::array<std::array<double, 6>, 2>{};
    }

    static bool Spatial(const EffectsConfig &config)
    {
        return config.spatial != SpatialMode::Off;
    }

    static const std::vector<Speaker> &Speakers(const EffectsConfig &config)
    {
        return Spatial(config) && config.surround ? SURROUND : STEREO;
    }

    static constexpr const char *EARS[] = {"L", "R"};

    // Biquads of one ear (0 = left) in signal order with the gains in effect: the 10 EQ bands ("eq"), the correction
    // ("co"), the hearing profile ("hl") and the loudness shelf ("ld"). They run after spatial audio and crossfeed, at the ear.
    static std::vector<std::pair<std::string, Biquad>> Filters(const EffectsConfig &config, int ear)
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
        for (size_t i = 0; i < HEARING_FREQS.size(); i++)
            filters.push_back({"hl" + std::to_string(i), {Biquad::Peaking, static_cast<double>(HEARING_FREQS[i]), config.hearing[ear][i], 1.41}});
        filters.push_back({"ld0", AudioEffects::LoudnessShelf(config)});
        if (config.bypass)
            for (auto &[name, bq] : filters)
                bq.gain = 0;
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

    // With the KEMAR the bass of every speaker bypasses the HRTF, and 7.1 always has an LFE: both take the bass path
    static bool HasBass(const EffectsConfig &config)
    {
        return Spatial(config) && (DefaultSofa(config) || config.surround);
    }

    // Gains of an ear's final mixer: "In 1" the HRTF speakers, "In 2" the bass path, "In 3" the unprocessed input (A/B)
    static std::array<double, 3> MixGains(const EffectsConfig &config)
    {
        if (config.bypass)
            return {0, 0, 1};
        return {1, HasBass(config) ? 1.0 : 0.0, 0};
    }

    // ITU downmix of a 7.1 channel into one ear, for the A/B comparison
    static double Downmix(const Speaker &s, int ear)
    {
        if (s.Lfe())
            return 0;
        if (s.azimuth == 0)
            return std::numbers::sqrt2 / 2;
        if ((s.azimuth > 0) != (ear == 0))
            return 0;
        return s.id == std::string("L") || s.id == std::string("R") ? 1 : std::numbers::sqrt2 / 2;
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

    double AudioEffects::GainDb(const Biquad &bq, double freq)
    {
        return 20 * std::log10(std::abs(Response(bq, freq, 48000)));
    }

    double AudioEffects::HeadroomDb(const EffectsConfig &config)
    {
        // the A/B side keeps the effects' pre-gain, that's what makes it level-matched
        EffectsConfig effects = config;
        effects.bypass = false;
        // adjacent bands add up (Dance peaks 1.7 dB above its largest band), so this measures the summed curve
        bool crossfeed = config.crossfeed && !Spatial(config);
        double peak = 1;
        for (int ear : {0, 1})
        {
            auto filters = Filters(effects, ear);
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
        }
        // ponytail: EQ and spatial peaks are added although they rarely sit at the same frequency; costs level, never clips
        double spatial = 0;
        if (Spatial(config))
        {
            spatial = !config.limiter.empty() ? SPATIAL_PEAK_LIMITED_DB : config.spatial == SpatialMode::Fixed ? SPATIAL_PEAK_FIXED_DB : SPATIAL_PEAK_TRACKED_DB;
            if (config.surround)
                spatial += SURROUND_EXTRA_DB;
        }
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

    std::optional<std::vector<Biquad>> AudioEffects::ParseParametricEq(const std::string &text)
    {
        static const std::map<std::string, Biquad::Type> TYPES{
            {"PK", Biquad::Peaking}, {"PEQ", Biquad::Peaking}, {"LSC", Biquad::LowShelf}, {"LS", Biquad::LowShelf},
            {"HSC", Biquad::HighShelf}, {"HS", Biquad::HighShelf}, {"LP", Biquad::LowPass}, {"LPQ", Biquad::LowPass},
            {"HP", Biquad::HighPass}, {"HPQ", Biquad::HighPass}};
        std::vector<Biquad> filters;
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line))
        {
            std::istringstream words(line);
            std::string key;
            words >> key;
            // one curve for both ears; a file with per-channel sections would come out wrong, so it is refused
            if (key.starts_with("Channel") && line.find("all") == std::string::npos)
                return std::nullopt;
            if (!key.starts_with("Filter"))
                continue; // Preamp (the headroom is computed), comments, blank lines
            auto colon = line.find(':');
            if (colon == std::string::npos)
                return std::nullopt;
            words = std::istringstream(line.substr(colon + 1));
            std::string state, type, word;
            words >> state >> type;
            if (state == "OFF")
                continue;
            auto it = TYPES.find(type);
            if (state != "ON" || it == TYPES.end())
                return std::nullopt;
            Biquad bq{it->second, 0, 0, 0.71};
            while (words >> word)
            {
                double *field = word == "Fc" ? &bq.freq : word == "Gain" ? &bq.gain : word == "Q" ? &bq.q : nullptr;
                if (field && !(words >> *field))
                    return std::nullopt;
            }
            if (!(bq.freq >= 10 && bq.freq <= 22000 && std::abs(bq.gain) <= 30 && bq.q >= 0.05 && bq.q <= 20))
                return std::nullopt;
            filters.push_back(bq);
        }
        if (filters.empty() || filters.size() > 32)
            return std::nullopt;
        return filters;
    }

    std::optional<std::array<double, 6>> AudioEffects::ParseAudiogram(const std::string &text)
    {
        std::string spaced = text;
        std::replace_if(spaced.begin(), spaced.end(), [](char ch) { return ch == ',' || ch == ';'; }, ' ');
        std::istringstream in(spaced);
        std::array<double, 6> thresholds{};
        for (double &t : thresholds)
            if (!(in >> t) || t < -10 || t > 120)
                return std::nullopt;
        std::string rest;
        if (in >> rest)
            return std::nullopt;
        return thresholds;
    }

    std::array<double, 6> AudioEffects::HearingGains(const std::array<double, 6> &thresholds)
    {
        std::array<double, 6> gains{};
        for (size_t i = 0; i < gains.size(); i++)
            gains[i] = std::clamp(thresholds[i] / 2, 0.0, HEARING_MAX_DB);
        return gains;
    }

    double AudioEffects::Iso226(double freq, double phon)
    {
        // ISO 226:2003 table 1: frequency, alpha_f, L_U, T_f
        static constexpr double TABLE[][4]{
            {20, 0.532, -31.6, 78.5}, {25, 0.506, -27.2, 68.7}, {31.5, 0.480, -23.0, 59.5}, {40, 0.455, -19.1, 51.1},
            {50, 0.432, -15.9, 44.0}, {63, 0.409, -13.0, 37.5}, {80, 0.387, -10.3, 31.5}, {100, 0.367, -8.1, 26.5},
            {125, 0.349, -6.2, 22.1}, {160, 0.330, -4.5, 17.9}, {200, 0.315, -3.1, 14.4}, {250, 0.301, -2.0, 11.4},
            {315, 0.288, -1.1, 8.6}, {400, 0.276, -0.4, 6.2}, {500, 0.267, 0.0, 4.4}, {630, 0.259, 0.3, 3.0},
            {800, 0.253, 0.5, 2.2}, {1000, 0.250, 0.0, 2.4}, {1250, 0.246, -2.7, 3.5}, {1600, 0.244, -4.1, 1.7},
            {2000, 0.243, -1.0, -1.3}, {2500, 0.243, 1.7, -4.2}, {3150, 0.243, 2.5, -6.0}, {4000, 0.242, 1.2, -5.4},
            {5000, 0.242, -2.1, -1.5}, {6300, 0.245, -7.1, 6.0}, {8000, 0.254, -11.2, 12.6}, {10000, 0.271, -10.7, 13.9},
            {12500, 0.301, -3.1, 12.3}};
        // nearest table row on a log scale
        auto row = std::min_element(std::begin(TABLE), std::end(TABLE), [&](auto &a, auto &b)
                                    { return std::abs(std::log(a[0] / freq)) < std::abs(std::log(b[0] / freq)); });
        double af = (*row)[1], lu = (*row)[2], tf = (*row)[3];
        double a = 4.47e-3 * (std::pow(10.0, 0.025 * phon) - 1.15) + std::pow(0.4 * std::pow(10.0, (tf + lu) / 10 - 9), af);
        return 10 / af * std::log10(a) - lu + 94;
    }

    Biquad AudioEffects::LoudnessShelf(const EffectsConfig &config)
    {
        double gain = 0;
        if (config.loudness)
        {
            double phon = std::clamp(config.loudnessReference + 60 * std::log10(std::max(config.volume, 1e-3)), 30.0, LOUDNESS_MIX_PHON);
            double contour = (Iso226(63, phon) - Iso226(63, LOUDNESS_MIX_PHON)) - (phon - LOUDNESS_MIX_PHON);
            gain = std::min(LOUDNESS_MAX_DB, 1.3 * contour);
        }
        return {Biquad::LowShelf, 140, gain, 0.55};
    }

    std::string AudioEffects::FindLimiter()
    {
        const char *env = std::getenv("LADSPA_PATH");
        std::istringstream dirs(std::string(env ? env : "") + ":/usr/lib/ladspa:/usr/lib64/ladspa:/usr/local/lib/ladspa");
        std::string dir;
        std::error_code ec;
        while (std::getline(dirs, dir, ':'))
            if (!dir.empty() && std::filesystem::exists(dir + "/" + LIMITER_PLUGIN, ec))
                return dir + "/" + LIMITER_PLUGIN;
        return "";
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

    // pw-cli parameters that turn the virtual speakers by the head's yaw
    static std::string AzimuthParams(const EffectsConfig &config, double yaw)
    {
        std::string params;
        if (Spatial(config))
            for (auto &s : Speakers(config))
                if (!s.Lfe())
                    params += std::string(" \"sp") + s.id + ":Azimuth\" " + Num(Azimuth(s.azimuth, yaw));
        return params;
    }

    std::string AudioEffects::BuildConfig(const std::string &sink, const std::string &rawDescription, const EffectsConfig &config, double yaw)
    {
        // the name comes from the headphones and the paths from the config; keep them from breaking out of a quoted config string
        auto clean = [](const std::string &raw)
        {
            std::string out;
            for (char ch : raw)
                if (ch != '"' && ch != '\\' && ch != '\n')
                    out += ch;
            return out;
        };
        std::string description = clean(rawDescription);
        // Filter, crossfeed and mixer nodes are always there (0 dB is transparent), so changing a setting is a live
        // control update, not a restart. Signal flow: pre-gain per input channel, spatial audio or crossfeed, the
        // filters per ear, the limiter.
        bool spatial = Spatial(config), kemar = DefaultSofa(config);
        const auto &speakers = Speakers(config);
        static const char *LABELS[] = {"bq_peaking", "bq_lowshelf", "bq_highshelf", "bq_lowpass", "bq_highpass"};
        std::ostringstream nodes, links;
        auto biquad = [&](const std::string &name, const Biquad &bq)
        {
            nodes << "      { type = builtin label = " << LABELS[bq.type] << " name = " << name << " control = { Freq = " << Num(bq.freq)
                  << " Q = " << Num(bq.q) << " Gain = " << Num(bq.gain) << " } }\n";
        };
        auto mixer = [&](const std::string &name, const std::vector<std::pair<std::string, double>> &ins)
        {
            nodes << "      { type = builtin label = mixer name = " << name << " control = {";
            for (size_t i = 0; i < ins.size(); i++)
                nodes << " \"Gain " << i + 1 << "\" = " << Num(ins[i].second);
            nodes << " } }\n";
            for (size_t i = 0; i < ins.size(); i++)
                links << "      { output = \"" << ins[i].first << "\" input = \"" << name << ":In " << i + 1 << "\" }\n";
        };
        auto link = [&](const std::string &output, const std::string &input)
        {
            links << "      { output = \"" << output << "\" input = \"" << input << "\" }\n";
        };

        double headroom = HeadroomDb(config);
        std::string inputs, positions;
        for (auto &s : speakers)
        {
            // broadband gain via a 0 Hz high shelf, so boosts don't clip
            biquad(std::string("pre") + s.id, {Biquad::HighShelf, 0, -headroom, 1});
            inputs += std::string(" \"pre") + s.id + ":In\"";
            positions += std::string(" ") + s.position;
        }

        std::string heads[2]; // where each ear's filter chain starts
        if (!spatial)
        {
            // crossfeed nodes are always there too; off mutes the low-passed paths, which the mixer then skips
            double g = config.crossfeed && !config.bypass ? CROSSFEED_GAIN : 0;
            for (std::string c : {"L", "R"})
            {
                biquad("xl" + c, CROSSFEED_LOWPASS);
                link("pre" + c + ":Out", "xl" + c + ":In");
            }
            for (int ear : {0, 1})
            {
                std::string c = EARS[ear], other = EARS[1 - ear];
                mixer("xm" + c, {{"pre" + c + ":Out", 1.0}, {"xl" + c + ":Out", g ? -g : 0.0}, {"xl" + other + ":Out", g}});
                heads[ear] = "xm" + c;
            }
        }
        else
        {
            for (auto &s : speakers)
            {
                if (s.Lfe())
                    continue;
                std::vector<std::string> chain{std::string("pre") + s.id};
                if (kemar)
                {
                    std::vector<Biquad> highs{2, {Biquad::HighPass, KEMAR_CROSSOVER, 0, 0.71}};
                    highs.insert(highs.end(), KEMAR_COMPENSATION.begin(), KEMAR_COMPENSATION.end());
                    for (size_t i = 0; i < highs.size(); i++)
                    {
                        chain.push_back(std::string("sc") + s.id + std::to_string(i));
                        biquad(chain.back(), highs[i]);
                    }
                }
                chain.push_back(std::string("sp") + s.id);
                nodes << "      { type = sofa label = spatializer name = sp" << s.id << " config = { filename = \"" << clean(kemar ? SOFA_FILE : config.sofa)
                      << "\" } control = { Azimuth = " << Num(Azimuth(s.azimuth, yaw)) << " Elevation = 0.0 Radius = 1.0 } }\n";
                for (size_t i = 1; i < chain.size(); i++)
                    link(chain[i - 1] + ":Out", chain[i] + ":In");
            }
            // with the KEMAR every speaker's bass reaches both ears as the mean of the channels, and the LFE joins it
            if (HasBass(config))
            {
                std::vector<std::pair<std::string, double>> bass;
                for (auto &s : speakers)
                    if (s.Lfe() || kemar)
                        bass.push_back({std::string("pre") + s.id + ":Out", s.Lfe() ? LFE_GAIN : 0.5});
                mixer("bass", bass);
                biquad("lp0", {Biquad::LowPass, KEMAR_CROSSOVER, 0, 0.71});
                biquad("lp1", {Biquad::LowPass, KEMAR_CROSSOVER, 0, 0.71});
                link("bass:Out", "lp0:In");
                link("lp0:Out", "lp1:In");
            }
            auto gains = MixGains(config);
            for (int ear : {0, 1})
            {
                std::string e = EARS[ear];
                std::vector<std::pair<std::string, double>> hrtf, downmix;
                for (auto &s : speakers)
                {
                    if (!s.Lfe())
                        hrtf.push_back({std::string("sp") + s.id + ":Out " + e, SPEAKER_GAIN});
                    if (Downmix(s, ear))
                        downmix.push_back({std::string("pre") + s.id + ":Out", Downmix(s, ear)});
                }
                mixer("hrtf" + e, hrtf);
                // A/B: the unprocessed input, for 7.1 downmixed
                std::string direct = "pre" + e + ":Out";
                if (config.surround)
                {
                    mixer("dm" + e, downmix);
                    direct = "dm" + e + ":Out";
                }
                std::vector<std::pair<std::string, double>> ins{{"hrtf" + e + ":Out", gains[0]}, {"lp1:Out", gains[1]}, {direct, gains[2]}};
                if (!HasBass(config))
                    ins[1] = {"hrtf" + e + ":Out", 0}; // placeholder keeps "In 3" the direct path
                mixer("mix" + e, ins);
                heads[ear] = "mix" + e;
            }
        }

        std::string outputs;
        for (int ear : {0, 1})
        {
            std::string e = EARS[ear], previous = heads[ear];
            for (auto &[filter, bq] : Filters(config, ear))
            {
                biquad(NodeName(filter, e), bq);
                link(previous + ":Out", NodeName(filter, e) + ":In");
                previous = NodeName(filter, e);
            }
            if (!config.limiter.empty())
                link(previous + ":Out", "lim:Input " + std::to_string(ear + 1));
            outputs += " \"" + (config.limiter.empty() ? previous + ":Out" : "lim:Output " + std::to_string(ear + 1)) + "\"";
        }
        if (!config.limiter.empty())
            nodes << "      { type = ladspa name = lim plugin = \"" << clean(config.limiter) << "\" label = fastLookaheadLimiter"
                  << " control = { \"Input gain (dB)\" = 0.0 \"Limit (dB)\" = " << Num(LIMITER_CEILING_DB) << " \"Release time (s)\" = 0.10 } }\n";

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
             << "      capture.props = { node.name = \"" << SINK_NAME << "\" media.class = Audio/Sink audio.channels = " << speakers.size()
             << " audio.position = [" << positions << " ] }\n"
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
        for (auto &s : Speakers(config))
            params += std::string(" \"pre") + s.id + ":Gain\" " + headroom;
        double g = config.crossfeed && !config.bypass ? CROSSFEED_GAIN : 0;
        auto gains = MixGains(config);
        for (int ear : {0, 1})
        {
            std::string c = EARS[ear];
            for (auto &[filter, bq] : Filters(config, ear))
                params += " \"" + NodeName(filter, c) + ":Gain\" " + Num(bq.gain);
            if (!Spatial(config))
                params += " \"xm" + c + ":Gain 2\" " + Num(g ? -g : 0.0) + " \"xm" + c + ":Gain 3\" " + Num(g);
            else
                for (int i = 0; i < 3; i++)
                    params += " \"mix" + c + ":Gain " + std::to_string(i + 1) + "\" " + Num(gains[i]);
        }
        params += AzimuthParams(config, yaw);
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
    void AudioEffects::SetVolume(double) {}
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
        if (config.spatial == SpatialMode::Off)
            config.surround = false; // 7.1 only feeds the virtual speakers
        config.limiter = FindLimiter();
        bool alive = _chain > 0 && waitpid(_chain, nullptr, WNOHANG) == 0;
        if (!alive)
            _chain = -1; // exited (and now reaped), so it must not be signalled again
        // Same graph (spatial on/off, 7.1, the HRTF, the limiter and the correction's filter count change it): update
        // the running chain, a restart would drop the stream and pause the player.
        // ponytail: EQ "Off" then leaves a flat chain running until the headphones go away; costs a few biquads, keeps playback going
        auto graph = [](const EffectsConfig &c)
        { return std::tuple(c.spatial != SpatialMode::Off, c.surround, c.correction.size(), DefaultSofa(c) ? "" : c.sofa, c.limiter); };
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
        Logger::Info("AudioEffects: chain started in front of %s (%s)", sink.c_str(),
                     config.limiter.empty() ? "no limiter, install swh-plugins for louder spatial audio" : "with limiter");
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
        std::string cmd = std::string("s ") + SINK_NAME + " Props { params = [" + AzimuthParams(_config, degrees) + " ] }\n";
        if (write(_ctlFd, cmd.data(), cmd.size()) < 0)
            Logger::Debug("AudioEffects: pw-cli not accepting commands");
    }

    void AudioEffects::SetVolume(double volume)
    {
        std::lock_guard lock{_lock};
        if (_ctlFd < 0 || !_config.loudness || std::abs(volume - _config.volume) < 0.005)
            return;
        _config.volume = volume;
        // the shelf and the pre-gain move together, so a louder bass never clips
        std::string cmd = ControlCommand(_config, _yaw);
        if (write(_ctlFd, cmd.data(), cmd.size()) < 0)
            Logger::Debug("AudioEffects: pw-cli not accepting commands");
    }
#endif
}
