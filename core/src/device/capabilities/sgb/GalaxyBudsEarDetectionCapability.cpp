// MyPods
// License: GPL-3.0

#include "GalaxyBudsEarDetectionCapability.h"

namespace MagicPodsCore
{
    GalaxyBudsEarDetectionCapability::GalaxyBudsEarDetectionCapability(GalaxyBudsDevice &device)
        : GalaxyBudsCapability("earDetection", false, device), watcher(static_cast<GalaxyBudsModelIds>(device.GetProductId()))
    {
        option = device.LoadSettingInt("earDetection").value_or(1) != 0;
        watcherEventId = watcher.GetEarDetectionStateChangedEvent().Subscribe([this](size_t, const GalaxyBudsEarDetectionStateArgs &state)
        {
            int now = (state.Left == GalaxyBudsEarDetectionState::Wearing) + (state.Right == GalaxyBudsEarDetectionState::Wearing);
            int before = inEar;
            if (isAvailable && now == before)
                return; // status updates repeat, only changes count
            inEar = now;
            isAvailable = true;
            _onChanged.FireEvent(*this);
            Logger::Info("Galaxy Buds ear detection: %d in ear", now);
            if (option)
                pause.Changed(before, now, this->device.KeepAlive());
        });
    }

    GalaxyBudsEarDetectionCapability::~GalaxyBudsEarDetectionCapability()
    {
        watcher.GetEarDetectionStateChangedEvent().Unsubscribe(watcherEventId);
    }

    nlohmann::json GalaxyBudsEarDetectionCapability::CreateJsonBody()
    {
        return {{"selected", option}};
    }

    void GalaxyBudsEarDetectionCapability::OnReceivedData(const GalaxyBudsResponseData &data)
    {
        watcher.ProcessResponse(data);
    }

    void GalaxyBudsEarDetectionCapability::Reset()
    {
        inEar = -1;
        pause.Reset();
        GalaxyBudsCapability::Reset();
    }

    void GalaxyBudsEarDetectionCapability::SetFromJson(const nlohmann::json &json)
    {
        if (!json.contains(name) || !json.at(name).contains("selected") || !json.at(name)["selected"].is_boolean())
            return;
        option = json.at(name)["selected"].get<bool>();
        device.SaveSettingInt("earDetection", option);
        _onChanged.FireEvent(*this);
    }
}
