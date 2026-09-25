// MyPods
// License: GPL-3.0

#include "CmnAudioEffectsCapabilities.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>
#include <thread>

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
            {
                // a test from before would come back without its tone
                std::lock_guard lock{testLock};
                test.reset();
                return Reset();
            }
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
                AudioEffects::Instance().SetVolume(this->device.ListeningVolume());
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

        std::lock_guard lock{testLock};
        if (test)
            body["hearingTest"] = {{"ear", test->Ear()}, {"frequency", test->Frequency()}, {"step", test->Step()}, {"steps", HearingTest::STEPS},
                                   {"tooQuiet", tooQuiet}};
        return body;
    }

    void CmnEqualizerCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (capability.contains("hearingTest"))
        {
            if (capability["hearingTest"].is_string())
                HearingTestCommand(capability["hearingTest"].get<std::string>());
            return;
        }
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
            Logger::Error("CmnEqualizerCapability::SetFromJson: expected selected, custom, tilt, correction, crossfeed, loudness, hearing, audiogramLeft/Right, bypass or hearingTest");
            return;
        }
        _onChanged.FireEvent(*this);
        device.RouteAudioAsync();
    }

    void CmnEqualizerCapability::HearingTestCommand(const std::string &command)
    {
        bool finished = false;
        {
            std::lock_guard lock{testLock};
            if (command == "start")
            {
                test.emplace();
                GiveTonesHeadroom();
            }
            else if (!test)
                return;
            else if (command == "heard" || command == "missed")
            {
                if (tooQuiet)
                {
                    Logger::Error("CmnEqualizerCapability: the tone did not play, the volume is too low for it");
                    return;
                }
                test->Answer(command == "heard");
            }
            else if (command == "cancel")
                test.reset();
            else if (command != "repeat")
            {
                Logger::Error("CmnEqualizerCapability: hearingTest is start, heard, missed, repeat or cancel");
                return;
            }

            if (test && !test->Done())
                PlayTestTone();
            if (test && test->Done())
            {
                // the result goes straight into the hearing profile
                for (int ear : {0, 1})
                {
                    audiograms[ear] = test->Audiogram(ear);
                    device.SaveSettingString(ear ? "audiogramRight" : "audiogramLeft", audiograms[ear]);
                }
                hearing = true;
                device.SaveSettingInt("hearingProfile", hearing);
                test.reset();
                finished = true;
            }
        }
        _onChanged.FireEvent(*this);
        if (finished)
            device.RouteAudioAsync();
    }

    void CmnEqualizerCapability::GiveTonesHeadroom()
    {
        // Music plays through the chain at headphones x chain volume, so both change by the same factor and the music stays as
        // loud; the tones go straight to the headphones and get their full range. Without a chain the volume keys do it.
        auto pac = device.GetAudioClient();
        auto sink = device.HeadphonesSink();
        auto chain = pac->GetSinkVolume(AudioEffects::SINK_NAME);
        auto headphones = sink ? pac->GetSinkVolume(*sink) : std::nullopt;
        // chain down first, so nothing plays louder in between
        if (chain && headphones && *headphones < 1 && pac->SetSinkVolume(AudioEffects::SINK_NAME, *chain * *headphones))
            pac->SetSinkVolume(*sink, 1);
    }

    void CmnEqualizerCapability::PlayTestTone()
    {
        tooQuiet = false;
        auto sink = device.HeadphonesSink();
        if (!sink)
        {
            Logger::Error("CmnEqualizerCapability: no headphones sink to play the test tone on");
            return;
        }
        double volume = device.GetAudioClient()->GetSinkVolume(*sink).value_or(1);
        double reference = device.LoadEffectsConfig().loudnessReference;
        while (!test->Done())
        {
            double dbfs = AudioEffects::ToneDbfs(test->Level(), test->Frequency(), volume, reference);
            if (dbfs <= AudioEffects::TONE_MAX_DBFS)
                return AudioEffects::Instance().PlayTone(*sink, test->Ear(), test->Frequency(), dbfs);
            // Played cut down, a missed tone would count as missed at a level it never had. Turning the headphones up helps
            // (the UI asks for it, then "repeat"); at full volume the tone is past what they can play.
            if (volume < 0.99)
            {
                tooQuiet = true;
                return;
            }
            test->Unplayable();
        }
    }
}
