// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "AapConversationAwarenessStateCapability.h"
#include "sdk/aap/enums/AapConversationAwarenessState.h"
#include <algorithm>
#include <thread>

namespace MagicPodsCore
{
    bool AapConversationAwarenessStateCapability::AapConversationAwarenessStateToBoolean(AapConversationAwarenessState mode)
    {
        if (mode == AapConversationAwarenessState::StartSpeaking){
            return true;
        }
        return false;
    }

    std::optional<bool> AapConversationAwarenessStateCapability::SpeakingFromLevel(int level)
    {
        if (level == 1 || level == 2)
            return true;
        if (level == 6 || level == 8 || level == 9)
            return false;
        return std::nullopt;
    }

    nlohmann::json AapConversationAwarenessStateCapability::CreateJsonBody()
    {
        auto bodyJson = nlohmann::json::object();
        bodyJson["selected"] = option;
        return bodyJson;
    }

    void AapConversationAwarenessStateCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        watcher.ProcessResponse(data);

        if (data.size() >= 10 && data[4] == 0x4B && data[6] == 0x02)
            if (auto speaking = SpeakingFromLevel(data[9]))
                Duck(*speaking);
    }

    void AapConversationAwarenessStateCapability::Duck(bool speaking)
    {
        if (speaking && !device.ownsAudio)
            return; // the AirPods play another device's audio, nothing of ours to lower

        // PulseAudio round trips and a short ramp: off the AAP reader
        std::thread([this, speaking, generation = ++duckGeneration, keep = device.KeepAlive()]()
        {
            auto pac = device.GetAudioClient();
            std::string mac = device.GetAddress();
            std::replace(mac.begin(), mac.end(), ':', '_');
            auto sink = pac->FindSink("bluez_output." + mac);
            if (!sink)
                return;

            std::lock_guard lock{duckLock};
            if (generation != duckGeneration)
                return; // a newer start/stop already came in
            auto current = pac->GetSinkVolume(*sink);
            if (!current)
                return;

            double target;
            if (speaking)
            {
                if (volumeBeforeDuck)
                    return; // already lowered
                volumeBeforeDuck = *current;
                target = *current * DUCKED_VOLUME;
            }
            else
            {
                if (!volumeBeforeDuck)
                    return;
                target = *volumeBeforeDuck;
                volumeBeforeDuck.reset();
            }
            Logger::Info("Conversation Awareness: volume %.0f%% -> %.0f%%", *current * 100, target * 100);

            // a quick ramp instead of a jump, like iOS
            constexpr int STEPS = 6;
            for (int i = 1; i <= STEPS && generation == duckGeneration; i++)
            {
                pac->SetSinkVolume(*sink, *current + (target - *current) * i / STEPS);
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
        }).detach();
    }

    void AapConversationAwarenessStateCapability::Reset()
    {
        {
            std::lock_guard lock{duckLock};
            volumeBeforeDuck.reset(); // the sink went away with the connection
        }
        ++duckGeneration;
        AapCapability::Reset();
    }

    AapConversationAwarenessStateCapability::AapConversationAwarenessStateCapability(AapDevice& device) : AapCapability("conversationAwarenessSpeaking", true, device)
    {
        watcherEventId = watcher.GetEvent().Subscribe([this](size_t id, AapConversationAwarenessState mode){
            bool newOption = AapConversationAwarenessStateToBoolean(mode);

            if (!isAvailable){
                    //When we get ConversationAwareness mode for the first time, we must notify. But on the second and subsequent times, we should notify only if the option has changed.
                    isAvailable = true;
                    option = newOption;
                    Logger::Info("ConversationAwarenessState updated: %s", AapConversationAwarenessStateToString(mode).c_str());
                    _onChanged.FireEvent(*this);
                }
                else if (option != newOption){
                    option = newOption;
                    Logger::Info("ConversationAwarenessState updated: %s", AapConversationAwarenessStateToString(mode).c_str());
                    _onChanged.FireEvent(*this);
                }
        });

    }

    AapConversationAwarenessStateCapability::~AapConversationAwarenessStateCapability()
    {
        watcher.GetEvent().Unsubscribe(watcherEventId);
    }
}
