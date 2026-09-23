// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "CmnBluetoothCodecCapability.h"
#include "Logger.h"
#include <chrono>
#include <thread>


namespace MagicPodsCore
{

    nlohmann::json CmnBluetoothCodecCapability::CreateJsonBody()
    {
        auto bodyJson = nlohmann::json::object();
        std::lock_guard lock{infoLock};
        bodyJson["selected"] = info.activeProfile;

        auto options =  nlohmann::json::array();
        for(auto &profile: info.profiles){
            options.push_back({profile.first, profile.second});
        }        
        bodyJson["options"] = options;
        
        return bodyJson;
    }

    void CmnBluetoothCodecCapability::Reset()
    {
        {
            std::lock_guard lock{infoLock};
            info = {};
        }
        Capability::Reset();
    }

    void CmnBluetoothCodecCapability::UpdateCodecInfo()
    {
        if (!device.GetConnected())
            return;

        auto pac = device.GetAudioClient();
        
        if(auto info = pac->GetCardInfoByName(pac->GetNameFromMac(device.GetAddress()))){
            UpdateCardInfo(*info);
        }
    }

    void CmnBluetoothCodecCapability::UpdateCardInfo(const CardInfo &newinfo)
    {
        {
            std::lock_guard lock{infoLock};
            if (info == newinfo)
                return;
            info = newinfo;
        }
        isAvailable = true;

        Logger::Debug("%s, (%s)",newinfo.name.c_str(),newinfo.activeProfile.c_str());
        for(auto &profile: newinfo.profiles){
            Logger::Debug("    %s: %s", profile.first.c_str(), profile.second.c_str());
        }

        _onChanged.FireEvent(*this);
    }

    bool CmnBluetoothCodecCapability::IsValidSelected(const std::string &selected)
    {
        std::lock_guard lock{infoLock};
        for(auto &profile: info.profiles){
            if (profile.first == selected)
                return true;            
        } 
        return false;
    }

    CmnBluetoothCodecCapability::CmnBluetoothCodecCapability(Device& device) : Capability("bluetoothCodec", false),
                                                                    device(device)
    {
        onAudioCardPropertyChangedId = device.GetAudioClient()->GatAudioCardPropertyChangedEvent().Subscribe([this](size_t id, const CardInfo& info){
            if (info.name == this->device.GetAudioClient()->GetNameFromMac(this->device.GetAddress())){
                this->UpdateCardInfo(info);
            }
        });
        onConnectedPropertyChangedId = device.GetConnectedPropertyChangedEvent().Subscribe([this](size_t id, const bool b){
            if (!b) {
                Reset();
            }
        });

        if (device.GetConnected()){
            UpdateCodecInfo();
        }
    }

void CmnBluetoothCodecCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name))
            return;

        const auto& capability = json.at(name);

        if (capability.contains("selected") && capability["selected"].is_string())
        {
            std::string selected = capability["selected"].get<std::string>();
            if (IsValidSelected(selected))
            {
                Logger::Debug("CmnBluetoothCodecCapability::SetFromJson set option to %s", selected.c_str());
                // Switching waits for PipeWire, so keep it off the API thread; the newest pick wins.
                // ponytail: detached like AapAudioSwitch, a device removed mid-switch would outlive `this`.
                int id = ++request;
                std::thread([this, selected, id]()
                {
                    std::lock_guard lock{switching};
                    if (id != request)
                        return;
                    auto pac = device.GetAudioClient();
                    if (!pac->SetCardProfile(pac->GetNameFromMac(device.GetAddress()), selected))
                    {
                        Logger::Error("CmnBluetoothCodecCapability: %s was not applied", selected.c_str());
                        // no card event follows, so the UI still shows the pick: resend the active codec
                        _onChanged.FireEvent(*this);
                    }
                }).detach();
            }
            else
            {
                Logger::Error("Error: CmnBluetoothCodecCapability::SetFromJson got unexpected option: %s", selected.c_str());
            }
        }
        else
        {
            Logger::Error("Error: CmnBluetoothCodecCapability::SetFromJson got no value or value is not an string");
        }
    }

    CmnBluetoothCodecCapability::~CmnBluetoothCodecCapability()
    {
        device.GetAudioClient()->GatAudioCardPropertyChangedEvent().Unsubscribe(onAudioCardPropertyChangedId);
        device.GetConnectedPropertyChangedEvent().Unsubscribe(onConnectedPropertyChangedId);
    }
}
