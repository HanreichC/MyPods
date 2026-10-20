// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Event.h"
#include "BleAdvertisingData.h"

namespace MyPods
{

    class BleAdvertisingService
    {
    protected:
        Event<BleAdvertisingData> _onAdReceivedEvent{};

    public:
        BleAdvertisingService() = default;
        virtual ~BleAdvertisingService() = default;

        Event<BleAdvertisingData> &GetOnAdReceivedEvent()
        {
            return _onAdReceivedEvent;
        }

        virtual void StartScan(bool isPassive) = 0;
        virtual void StopScan() = 0;
        virtual void StartListening() = 0;
    };

}
