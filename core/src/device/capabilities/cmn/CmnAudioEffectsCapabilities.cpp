// MyPods
// License: GPL-3.0

#include "CmnAudioEffectsCapabilities.h"
#include "Logger.h"
#include <algorithm>
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
        nlohmann::json body{{"selected", preset}, {"options", AudioEffects::PresetNames()}, {"crossfeed", crossfeed},
                            {"loudness", loudness.load()}, {"hearing", hearing}, {"audiogramLeft", audiograms[0]},
                            {"audiogramRight", audiograms[1]}, {"bypass", device.effectsBypass.load()}};
        if (!device.Correction().empty())
            body["correction"] = corrected;
        return body;
    }

    void CmnEqualizerCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;
        const auto &capability = json.at(name);
        if (capability.contains("selected"))
        {
            if (!capability["selected"].is_string() || !AudioEffects::Preset(capability["selected"].get<std::string>()))
            {
                Logger::Error("CmnEqualizerCapability::SetFromJson: unknown preset");
                return;
            }
            preset = capability["selected"].get<std::string>();
            device.SaveSettingString("equalizer", preset);
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
            Logger::Error("CmnEqualizerCapability::SetFromJson: expected selected, correction, crossfeed, loudness, hearing, audiogramLeft/Right or bypass");
            return;
        }
        _onChanged.FireEvent(*this);
        device.RouteAudioAsync();
    }
}
