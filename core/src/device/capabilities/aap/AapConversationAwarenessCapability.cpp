// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "AapConversationAwarenessCapability.h"
#include "AapConversationAwarenessStateCapability.h"
#include "sdk/aap/enums/AapConversationAwarenessMode.h"
#include "sdk/aap/setters/AapSetConversationAwarenessStatus.h"

namespace MagicPodsCore
{
    nlohmann::json AapConversationAwarenessCapability::CreateJsonBody()
    {    
        auto bodyJson = nlohmann::json::object();
        bodyJson["selected"] = option;
        // how loud media stays while you speak (MyPods, AapConversationAwarenessStateCapability)
        bodyJson["duckVolume"] = device.LoadSettingInt("caVolume").value_or(AapConversationAwarenessStateCapability::DEFAULT_DUCK_PERCENT);
        return bodyJson;
    }

    void AapConversationAwarenessCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        watcher.ProcessResponse(data);
    }

    AapConversationAwarenessCapability::AapConversationAwarenessCapability(AapDevice& device) : AapCapability("conversationAwareness", false, device)
    {
        watcherEventId = watcher.GetEvent().Subscribe([this](size_t id, AapConversationAwarenessMode mode){
            bool newOption = mode == AapConversationAwarenessMode::On;            
            
            if (!isAvailable){
                    //When we get ConversationAwareness mode for the first time, we must notify. But on the second and subsequent times, we should notify only if the option has changed.
                    isAvailable = true;
                    option = newOption;
                    Logger::Info("ConversationAwareness updated: %s", AapConversationAwarenessModeToString(mode).c_str());
                    _onChanged.FireEvent(*this);
                }
                else if (option != newOption){
                    option = newOption;
                    Logger::Info("ConversationAwareness updated: %s", AapConversationAwarenessModeToString(mode).c_str());
                    _onChanged.FireEvent(*this);
                }
        });

    }

    AapConversationAwarenessCapability::~AapConversationAwarenessCapability()
    {
        watcher.GetEvent().Unsubscribe(watcherEventId);
    }

    void AapConversationAwarenessCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;

        const auto& capability = json.at(name);

        if (capability.contains("duckVolume"))
        {
            if (capability["duckVolume"].is_number_integer() && capability["duckVolume"].get<int>() >= 0 && capability["duckVolume"].get<int>() <= 100)
            {
                device.SaveSettingInt("caVolume", capability["duckVolume"].get<int>());
                _onChanged.FireEvent(*this);
            }
            else
                Logger::Error("AapConversationAwarenessCapability::SetFromJson: duckVolume must be 0-100");
            if (!capability.contains("selected"))
                return;
        }

        if (capability.contains("selected") && capability["selected"].is_boolean())
        {
            bool selected = capability["selected"].get<bool>();
            AapConversationAwarenessMode nativeMode = selected ? AapConversationAwarenessMode::On : AapConversationAwarenessMode::Off;    
            SendData(AapSetConversationAwarenessStatus(nativeMode));
            
            // Manually notify because AirPods will not send a notification that the option has been changed.
            if (option != selected){
                    option = selected;
                    Logger::Info("ConversationAwareness updated: %s", AapConversationAwarenessModeToString(nativeMode).c_str());
                    _onChanged.FireEvent(*this);
            }

            Logger::Debug("AapConversationAwarenessCapability::SetFromJson set option to %s", AapConversationAwarenessModeToString(nativeMode).c_str());            
        }
        else
        {
            Logger::Error("Error: AapConversationAwarenessCapability::SetFromJson got no value or value is not an boolean");
        }
    }
}
