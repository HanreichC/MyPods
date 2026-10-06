// MyPods
// License: GPL-3.0

#include "CmnAudioEffectsCapabilities.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <utility>

namespace MagicPodsCore
{
    CmnSpatialAudioCapability::CmnSpatialAudioCapability(Device &device) : Capability("spatialAudio", false), device(device),
        surround(device.LoadSettingInt("surround").value_or(0) != 0), headTracking(device.HasHeadTracking())
    {
        mode = static_cast<int>(std::clamp<int64_t>(device.LoadSettingInt("spatialAudio").value_or(0), 0, headTracking ? 2 : 1));
        // the effects run on this computer, so they are there whenever the headphones are
        isAvailable = device.GetConnected();
        onConnectedId = device.GetConnectedPropertyChangedEvent().Subscribe([this](size_t, bool connected)
        {
            if (!connected)
                return Reset();
            isAvailable = true;
            _onChanged.FireEvent(*this);
        });
    }

    CmnSpatialAudioCapability::~CmnSpatialAudioCapability()
    {
        device.GetConnectedPropertyChangedEvent().Unsubscribe(onConnectedId);
    }

    nlohmann::json CmnSpatialAudioCapability::CreateJsonBody()
    {
        return {{"selected", mode}, {"headTracking", headTracking}, {"surround", surround}};
    }

    void CmnSpatialAudioCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (capability.contains("surround") && capability["surround"].is_boolean())
        {
            surround = capability["surround"].get<bool>();
            device.SaveSettingInt("surround", surround);
            _onChanged.FireEvent(*this);
            device.RouteAudioAsync();
            return;
        }
        if (!capability.contains("selected") || !capability["selected"].is_number_integer() ||
            capability["selected"].get<int>() < 0 || capability["selected"].get<int>() > (headTracking ? 2 : 1))
        {
            Logger::Error("CmnSpatialAudioCapability::SetFromJson: selected must be 0, 1 or (with head tracking) 2");
            return;
        }
        mode = capability["selected"].get<int>();
        device.SaveSettingInt("spatialAudio", mode);
        UpdateTracking();
        _onChanged.FireEvent(*this);
        device.RouteAudioAsync();
    }

    int CmnSignalPathCapability::Bits(const std::string &format)
    {
        // "s16le", "s24le", "s24-32le" (24 bits in 32), "s32le", "u8", "float32le"
        if (format.starts_with("float"))
            return std::atoi(format.c_str() + 5);
        if (format.starts_with("s") || format.starts_with("u"))
            return std::atoi(format.c_str() + 1);
        return 0;
    }

    nlohmann::json CmnSignalPathCapability::Describe(const std::vector<StreamInfo> &streams, const SinkDetails &output, bool processed)
    {
        auto spec = [](uint32_t rate, const std::string &format, int channels)
        {
            return nlohmann::json{{"rate", rate}, {"bits", Bits(format)}, {"float", format.starts_with("float")}, {"channels", channels}};
        };
        bool resampled = false, reduced = false, volume = false;
        int outputBits = Bits(output.format);
        const StreamInfo *source = nullptr;
        for (const auto &stream : streams)
        {
            // the richest stream is the one listened for (a notification sound plays next to the music)
            if (!source || std::pair(stream.rate, Bits(stream.format)) > std::pair(source->rate, Bits(source->format)))
                source = &stream;
            resampled = resampled || stream.rate != output.rate;
            // a float source carries 24 bits of resolution; into 24 or 32 bit integers it loses nothing
            int sourceBits = stream.format.starts_with("float") ? 24 : Bits(stream.format);
            reduced = reduced || sourceBits > outputBits;
            // ponytail: an application's volume is scaled in software; the output's own volume is left out, ALSA
            // usually sets it in the hardware mixer
            volume = volume || std::abs(stream.volume - 1) > 0.001;
        }
        std::vector<std::string> reasons;
        for (auto [reason, applies] : {std::pair{"processed", processed}, {"encoded", !output.codec.empty()}, {"resampled", resampled},
                                       {"reduced", reduced}, {"volume", volume}})
            if (applies)
                reasons.push_back(reason);

        auto out = spec(output.rate, output.format, output.channels);
        out["codec"] = output.codec;
        return {{"playing", !streams.empty()}, {"bitPerfect", !streams.empty() && reasons.empty()}, {"reasons", reasons},
                {"source", source ? spec(source->rate, source->format, source->channels) : nlohmann::json()}, {"output", out}};
    }

    CmnSignalPathCapability::CmnSignalPathCapability(Device &device) : Capability("signalPath", true), device(device)
    {
        auto pac = device.GetAudioClient();
        // PulseAudio thread: the look runs on a worker
        streamEventId = pac->GetStreamChangedEvent().Subscribe([this](size_t, const uint32_t &) { UpdateSoon(); });
        sinkEventId = pac->GetSinkChangedEvent().Subscribe([this](size_t, const uint32_t &) { UpdateSoon(); });
        onConnectedId = device.GetConnectedPropertyChangedEvent().Subscribe([this](size_t, bool connected)
        {
            if (!connected)
                return Reset();
            UpdateSoon();
        });
        worker = std::thread([this]() { Work(); });
        UpdateSoon();
    }

    CmnSignalPathCapability::~CmnSignalPathCapability()
    {
        device.GetAudioClient()->GetStreamChangedEvent().Unsubscribe(streamEventId);
        device.GetAudioClient()->GetSinkChangedEvent().Unsubscribe(sinkEventId);
        device.GetConnectedPropertyChangedEvent().Unsubscribe(onConnectedId);
        {
            std::lock_guard guard{lock};
            exiting = true;
        }
        wake.notify_one();
        worker.join();
    }

    nlohmann::json CmnSignalPathCapability::CreateJsonBody()
    {
        std::lock_guard guard{lock};
        return body;
    }

    void CmnSignalPathCapability::Reset()
    {
        {
            std::lock_guard guard{lock};
            body = nullptr;
        }
        Capability::Reset();
    }

    void CmnSignalPathCapability::UpdateSoon()
    {
        if (!device.GetConnected())
            return;
        {
            std::lock_guard guard{lock};
            requested = true;
        }
        wake.notify_one();
    }

    void CmnSignalPathCapability::Work()
    {
        std::unique_lock guard{lock};
        while (true)
        {
            wake.wait(guard, [this]() { return requested || exiting; });
            // the rest of the burst arrives meanwhile
            if (wake.wait_for(guard, std::chrono::milliseconds(300), [this]() { return exiting; }))
                return;
            requested = false;
            guard.unlock();
            Update();
            guard.lock();
        }
    }

    void CmnSignalPathCapability::Update()
    {
        auto pac = device.GetAudioClient();
        auto sink = device.HeadphonesSink();
        auto output = sink ? pac->GetSinkDetails(*sink) : std::nullopt;
        nlohmann::json next;
        if (output)
        {
            // with the chain in front, the applications play into its sink
            bool processed = AudioEffects::Instance().PlaysInto(device.SinkPart());
            uint32_t target = output->index;
            if (auto chain = processed ? pac->GetSinkDetails(AudioEffects::SINK_NAME) : std::nullopt)
                target = chain->index;
            std::vector<StreamInfo> playing;
            for (const auto &stream : pac->GetStreams())
                if (stream.sink == target && !stream.paused)
                    playing.push_back(stream);
            next = Describe(playing, *output, processed);
        }
        {
            std::lock_guard guard{lock};
            // disconnected meanwhile (Reset ran), or nothing new
            if (!device.GetConnected() || (next == body && isAvailable == !next.is_null()))
                return;
            body = next;
        }
        isAvailable = !next.is_null();
        _onChanged.FireEvent(*this);
    }

    CmnEqualizerCapability::CmnEqualizerCapability(Device &device) : Capability("equalizer", false), device(device)
    {
        preset = device.LoadSettingString("equalizer").value_or("Off");
        tilt = static_cast<int>(std::clamp<int64_t>(device.LoadSettingInt("tilt").value_or(0), -6, 6));
        corrected = device.LoadSettingInt("headphoneCorrection").value_or(0) != 0;
        crossfeed = device.LoadSettingInt("crossfeed").value_or(0) != 0;
        loudness = device.LoadSettingInt("loudness").value_or(0) != 0;
        hearing = device.LoadSettingInt("hearingProfile").value_or(0) != 0;
        audiograms[0] = device.LoadSettingString("audiogramLeft").value_or("");
        audiograms[1] = device.LoadSettingString("audiogramRight").value_or("");

        isAvailable = device.GetConnected();
        onConnectedId = device.GetConnectedPropertyChangedEvent().Subscribe([this](size_t, bool connected)
        {
            if (!connected)
                return Reset();
            isAvailable = true;
            _onChanged.FireEvent(*this);
        });

        // The loudness compensation follows the volume. PulseAudio thread: the volume is read on a worker, and a
        // burst of changes (a dragged slider) makes one read.
        sinkEventId = device.GetAudioClient()->GetSinkChangedEvent().Subscribe([this](size_t, const uint32_t &)
        {
            if (!loudness || !this->device.ownsAudio || volumePending.exchange(true))
                return;
            std::thread([this, keep = this->device.KeepAlive()]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                volumePending = false;
                AudioEffects::Instance().SetVolume(this->device.SinkPart(), this->device.ListeningVolume());
            }).detach();
        });
    }

    CmnEqualizerCapability::~CmnEqualizerCapability()
    {
        device.GetAudioClient()->GetSinkChangedEvent().Unsubscribe(sinkEventId);
        device.GetConnectedPropertyChangedEvent().Unsubscribe(onConnectedId);
    }

    nlohmann::json CmnEqualizerCapability::CreateJsonBody()
    {
        auto options = AudioEffects::PresetNames();
        options.push_back("Custom");
        // ponytail: the curve shows the loudness compensation at full volume; the live one would need a blocking volume read here
        auto config = device.LoadEffectsConfig();
        nlohmann::json body{{"selected", preset}, {"options", options}, {"bands", config.eq}, {"tilt", tilt}, {"crossfeed", crossfeed},
                            {"loudness", loudness.load()}, {"hearing", hearing}, {"audiogramLeft", audiograms[0]},
                            {"audiogramRight", audiograms[1]}, {"bypass", device.effectsBypass.load()}};
        if (!config.correction.empty())
            body["correction"] = corrected;

        // 40 points from 20 Hz to 20 kHz, a third of an octave apart at 0.1 dB, enough to draw
        std::vector<double> freqs;
        for (int i = 0; i < 40; i++)
            freqs.push_back(20 * std::pow(1000.0, i / 39.0));
        auto round = [](std::vector<double> db) { for (double &d : db) d = std::round(d * 10) / 10; return db; };
        for (double &f : freqs)
            f = std::round(f);
        body["response"] = {{"frequencies", freqs}, {"left", round(AudioEffects::ResponseDb(config, 0, freqs))},
                            {"right", round(AudioEffects::ResponseDb(config, 1, freqs))}};
        return body;
    }

    void CmnEqualizerCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (capability.contains("selected"))
        {
            if (!capability["selected"].is_string() ||
                (capability["selected"] != "Custom" && !AudioEffects::Preset(capability["selected"].get<std::string>())))
            {
                Logger::Error("CmnEqualizerCapability::SetFromJson: unknown preset");
                return;
            }
            // the first "Custom" starts from the preset that was on, to edit it from there
            if (capability["selected"] == "Custom" && !AudioEffects::ParseBands(device.LoadSettingString("customEq").value_or("")))
            {
                std::string text;
                for (double g : device.LoadEffectsConfig().eq)
                    text += std::to_string(g) + " ";
                device.SaveSettingString("customEq", text);
            }
            preset = capability["selected"].get<std::string>();
            device.SaveSettingString("equalizer", preset);
        }
        else if (capability.contains("custom"))
        {
            // the 10 bands as numbers; stored as text, the way ParseBands reads them back
            const auto &bands = capability["custom"];
            std::string text;
            if (bands.is_array() && bands.size() == 10)
                for (const auto &b : bands)
                    text += (b.is_number() ? std::to_string(b.get<double>()) : "x") + " ";
            if (!AudioEffects::ParseBands(text))
            {
                Logger::Error("CmnEqualizerCapability::SetFromJson: custom is 10 gains from -12 to 12 dB");
                return;
            }
            device.SaveSettingString("customEq", text);
            preset = "Custom";
            device.SaveSettingString("equalizer", preset);
        }
        else if (capability.contains("tilt"))
        {
            if (!capability["tilt"].is_number_integer() || std::abs(capability["tilt"].get<int64_t>()) > 6)
            {
                Logger::Error("CmnEqualizerCapability::SetFromJson: tilt is -6 to 6 dB");
                return;
            }
            tilt = capability["tilt"].get<int>();
            device.SaveSettingInt("tilt", tilt);
        }
        else if (capability.contains("correction") && capability["correction"].is_boolean() && !device.Correction().empty())
        {
            corrected = capability["correction"].get<bool>();
            device.SaveSettingInt("headphoneCorrection", corrected);
        }
        else if (capability.contains("crossfeed") && capability["crossfeed"].is_boolean())
        {
            crossfeed = capability["crossfeed"].get<bool>();
            device.SaveSettingInt("crossfeed", crossfeed);
        }
        else if (capability.contains("loudness") && capability["loudness"].is_boolean())
        {
            loudness = capability["loudness"].get<bool>();
            device.SaveSettingInt("loudness", loudness);
        }
        else if (capability.contains("hearing") && capability["hearing"].is_boolean())
        {
            hearing = capability["hearing"].get<bool>();
            device.SaveSettingInt("hearingProfile", hearing);
        }
        else if (capability.contains("bypass") && capability["bypass"].is_boolean())
            device.effectsBypass = capability["bypass"].get<bool>();
        else if (auto key = capability.contains("audiogramLeft") ? "audiogramLeft" : capability.contains("audiogramRight") ? "audiogramRight" : nullptr)
        {
            // empty clears the ear
            const auto &value = capability[key];
            if (!value.is_string() || (!value.get<std::string>().empty() && !AudioEffects::ParseAudiogram(value.get<std::string>())))
            {
                Logger::Error("CmnEqualizerCapability::SetFromJson: an audiogram is six thresholds in dB HL (250 Hz to 8 kHz)");
                return;
            }
            audiograms[key == std::string("audiogramRight")] = value.get<std::string>();
            device.SaveSettingString(key, value.get<std::string>());
        }
        else
        {
            Logger::Error("CmnEqualizerCapability::SetFromJson: expected selected, custom, tilt, correction, crossfeed, loudness, hearing, audiogramLeft/Right or bypass");
            return;
        }
        _onChanged.FireEvent(*this);
        device.RouteAudioAsync();
    }
}
