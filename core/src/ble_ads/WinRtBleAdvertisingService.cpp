// MyPods
// License: GPL-3.0

#include "WinRtBleAdvertisingService.h"
#include "Logger.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Storage.Streams.h>

#include <atomic>

namespace MagicPodsCore
{
    using namespace winrt::Windows::Devices::Bluetooth::Advertisement;

    struct WinRtBleAdvertisingService::Native
    {
        BluetoothLEAdvertisementWatcher watcher{};
        std::atomic<bool> scanDesired{false};
    };

    WinRtBleAdvertisingService::WinRtBleAdvertisingService(DBusService &dbusService)
        : _native{std::make_unique<Native>()}, _dbusService{dbusService}
    {
        _native->watcher.Received([this](const BluetoothLEAdvertisementWatcher &, const BluetoothLEAdvertisementReceivedEventArgs &args)
        {
            std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
            for (const auto &section : args.Advertisement().ManufacturerData())
            {
                auto buffer = section.Data();
                std::vector<uint8_t> bytes(buffer.data(), buffer.data() + buffer.Length());
                manufacturerData[section.CompanyId()] = std::move(bytes);
            }
            if (manufacturerData.empty())
                return;

            const uint64_t a = args.BluetoothAddress();
            char address[18];
            std::snprintf(address, sizeof address, "%02X:%02X:%02X:%02X:%02X:%02X",
                          unsigned(a >> 40) & 0xFF, unsigned(a >> 32) & 0xFF, unsigned(a >> 24) & 0xFF,
                          unsigned(a >> 16) & 0xFF, unsigned(a >> 8) & 0xFF, unsigned(a) & 0xFF);
            std::string addressString{address};
            _onAdReceivedEvent.FireEvent(BleAdertisingData(addressString, static_cast<int8_t>(args.RawSignalStrengthInDBm()), std::move(manufacturerData)));
        });

        // The watcher aborts when the radio goes off and does not come back on its own
        _adapterPoweredSubscriptionId = _dbusService.IsBluetoothAdapterPowered().GetEvent().Subscribe([this](size_t, bool isPowered)
        {
            if (isPowered && _native->scanDesired)
            {
                Logger::Info("Bluetooth adapter powered on, restarting scan");
                StartScan(true);
            }
        });
    }

    WinRtBleAdvertisingService::~WinRtBleAdvertisingService()
    {
        _dbusService.IsBluetoothAdapterPowered().GetEvent().Unsubscribe(_adapterPoweredSubscriptionId);
        StopScan();
    }

    void WinRtBleAdvertisingService::StartScan(bool isPassive)
    {
        _native->scanDesired = true;
        try
        {
            if (_native->watcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started)
                return;
            // the proximity message is in the advertisement itself, a passive scan sees it without scan requests
            _native->watcher.ScanningMode(isPassive ? BluetoothLEScanningMode::Passive : BluetoothLEScanningMode::Active);
            _native->watcher.Start();
        }
        catch (const winrt::hresult_error &e)
        {
            Logger::Error("BLE scan: %s", winrt::to_string(e.message()).c_str());
        }
    }

    void WinRtBleAdvertisingService::StopScan()
    {
        _native->scanDesired = false;
        try
        {
            if (_native->watcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started)
                _native->watcher.Stop();
        }
        catch (const winrt::hresult_error &e)
        {
            Logger::Error("BLE scan: %s", winrt::to_string(e.message()).c_str());
        }
    }
}
