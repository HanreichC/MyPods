// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "AapBatteryCapability.h"
#include "AppAnimationCapability.h"

namespace MagicPodsCore
{

    nlohmann::json AapBatteryCapability::CreateJsonBody()
    {
        return battery.CreateJsonBody();
    }

    void AapBatteryCapability::OnReceivedData(const std::vector<unsigned char> &data)
    {
        watcher.ProcessResponse(data);
    }

    void AapBatteryCapability::Reset()
    {
        battery.ClearBattery();
        AapCapability::Reset();
    }

    AapBatteryCapability::AapBatteryCapability(AapDevice& device) : AapCapability("battery", true, device),
                                                                    battery(true)
    {
        batteryChangedEventId = battery.GetBatteryChangedEvent().Subscribe([this](size_t id, const std::vector<DeviceBatteryData> &b){
            if (!isAvailable)
                isAvailable = true;

            _onChanged.FireEvent(*this);
        });

        watcherBatteryChangedEventId = watcher.GetEvent().Subscribe([this](size_t id, const std::vector<DeviceBatteryData> &b){
            battery.UpdateBattery(b);

        });

        // No AAP battery without L2CAP: the advertisement's stands in (1 % steps with the ENC key, 10 % without)
        if (!Client::SupportsL2CAP())
            leEventId = this->device.GetLeDataReceived().Subscribe([this](size_t, const BleAdertisingData &ad){
                if (!this->device.GetConnected())
                    return;
                if (auto message = this->device.OwnProximityMessage(ad))
                    if (auto data = AppAnimationCapability::ParseBle(*message, this->device.GetProductId(), this->device.LoadSettingString("enc").value_or("")))
                        battery.UpdateBattery(data->batteryData);
            });
    }

    AapBatteryCapability::~AapBatteryCapability()
    {
        battery.GetBatteryChangedEvent().Unsubscribe(batteryChangedEventId);
        watcher.GetEvent().Unsubscribe(watcherBatteryChangedEventId);
        if (leEventId != 0)
            device.GetLeDataReceived().Unsubscribe(leEventId);
    }

}