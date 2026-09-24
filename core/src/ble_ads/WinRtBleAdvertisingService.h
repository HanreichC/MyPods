// MyPods
// License: GPL-3.0

#pragma once

#include "BleAdvertisingService.h"
#include "../dbus/DBusService.h"
#include <memory>

namespace MagicPodsCore
{
    // Windows counterpart of DBusBasedBleAdvertisingService: BluetoothLEAdvertisementWatcher. Windows shares
    // the radio between scanning and paired LE devices itself, so there is no discovery filter to be careful with.
    class WinRtBleAdvertisingService : public BleAdvertisingService
    {
    private:
        struct Native;
        std::unique_ptr<Native> _native;
        DBusService &_dbusService;
        size_t _adapterPoweredSubscriptionId = 0;

    public:
        explicit WinRtBleAdvertisingService(DBusService &dbusService);
        ~WinRtBleAdvertisingService() override;

        void StartScan(bool isPassive) override;
        void StopScan() override;
        void StartListening() override {}
    };
}
