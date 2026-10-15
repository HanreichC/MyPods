// MyPods
// License: GPL-3.0

#include "BeoplayE8.h"
#include "Logger.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace MagicPodsCore::BeoplayE8
{
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
    using namespace std::chrono_literals;

    // B&O's state characteristic in service 0xFE89: 4446cf5f-12f2-4c1e-afe1-b15797535ba8
    static constexpr winrt::guid STATE{0x4446cf5f, 0x12f2, 0x4c1e, {0xaf, 0xe1, 0xb1, 0x57, 0x97, 0x53, 0x5b, 0xa8}};

    struct Link::Native
    {
        std::atomic<bool> stop{false};
        std::mutex lock;
        std::condition_variable wake;
        std::thread worker;

        // waits for a WinRT operation in short steps, so stopping doesn't wait for a timeout of the Bluetooth stack.
        // Polls its status: wait_for sets a completion handler each time, and a second one throws
        template <typename Operation>
        auto Await(Operation operation)
        {
            while (operation.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
            {
                if (stop)
                {
                    operation.Cancel();
                    throw winrt::hresult_canceled();
                }
                std::this_thread::sleep_for(50ms);
            }
            return operation.GetResults();
        }

        // false once stopped
        bool Sleep(std::chrono::milliseconds duration)
        {
            std::unique_lock guard{lock};
            return !wake.wait_for(guard, duration, [this] { return stop.load(); });
        }

        static std::vector<unsigned char> Bytes(const winrt::Windows::Storage::Streams::IBuffer &buffer)
        {
            return {buffer.data(), buffer.data() + buffer.Length()};
        }

        std::string ReadString(const GattCharacteristic &characteristic)
        {
            auto result = Await(characteristic.ReadValueAsync(BluetoothCacheMode::Uncached));
            if (result.Status() != GattCommunicationStatus::Success)
                return {};
            auto bytes = Bytes(result.Value());
            return {bytes.begin(), bytes.end()};
        }

        // One LE session: device info, then the state until the link drops
        void Session(uint64_t address, const std::function<void(const Info &)> &onInfo, const std::function<void(bool)> &onTransparency)
        {
            auto device = Await(BluetoothLEDevice::FromBluetoothAddressAsync(address));
            if (!device)
                return;

            Info info;
            GattCharacteristic state{nullptr};
            // a discovery sometimes comes back without B&O's service (another app holding it, a stack still
            // settling): asked again a few times
            for (int attempt = 0; attempt < 5 && !state; ++attempt)
            {
                if (attempt > 0 && !Sleep(1500ms))
                    return;
                auto services = Await(device.GetGattServicesAsync(BluetoothCacheMode::Uncached));
                if (services.Status() != GattCommunicationStatus::Success)
                    continue;
                for (const auto &service : services.Services())
                {
                    auto characteristics = Await(service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached));
                    for (const auto &characteristic : characteristics.Characteristics())
                    {
                        auto uuid = characteristic.Uuid();
                        if (uuid == GattCharacteristicUuids::ModelNumberString())
                            info.model = ReadString(characteristic);
                        else if (uuid == GattCharacteristicUuids::SerialNumberString())
                            info.serial = ReadString(characteristic);
                        else if (uuid == GattCharacteristicUuids::FirmwareRevisionString())
                            info.firmware = ReadString(characteristic);
                        else if (uuid == STATE)
                            state = characteristic;
                    }
                }
            }
            if (!info.model.empty())
            {
                Logger::Info("Beoplay E8: %s, serial %s, firmware %s", info.model.c_str(), info.serial.c_str(), info.firmware.c_str());
                onInfo(info);
            }
            if (!state)
            {
                Logger::Debug("Beoplay E8: no state characteristic over LE");
                return;
            }

            auto report = [&onTransparency](const std::vector<unsigned char> &bytes)
            {
                if (auto transparency = Transparency(bytes))
                    onTransparency(*transparency);
            };
            auto token = state.ValueChanged([report](const GattCharacteristic &, const GattValueChangedEventArgs &args)
            {
                report(Bytes(args.CharacteristicValue()));
            });
            try
            {
                Await(state.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify));
                auto current = Await(state.ReadValueAsync(BluetoothCacheMode::Uncached));
                if (current.Status() == GattCommunicationStatus::Success)
                    report(Bytes(current.Value()));
                // the subscription keeps the LE link up; it ends when the earbuds go
                while (Sleep(2s) && device.ConnectionStatus() == BluetoothConnectionStatus::Connected) {}
            }
            catch (...)
            {
                state.ValueChanged(token);
                throw;
            }
            state.ValueChanged(token);
        }
    };

    Link::Link(const std::string &address, std::function<void(const Info &)> onInfo, std::function<void(bool)> onTransparency)
        : native{std::make_unique<Native>()}
    {
        uint64_t value = 0;
        for (char c : address)
            if (std::isxdigit(static_cast<unsigned char>(c)))
                value = value << 4 | std::stoul(std::string{c}, nullptr, 16);

        native->worker = std::thread([n = native.get(), value, onInfo = std::move(onInfo), onTransparency = std::move(onTransparency)]()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            while (!n->stop)
            {
                try
                {
                    n->Session(value, onInfo, onTransparency);
                }
                catch (const winrt::hresult_error &e)
                {
                    if (!n->stop)
                        Logger::Debug("Beoplay E8 over LE: %s", winrt::to_string(e.message()).c_str());
                }
                // again later: the earbuds may still be coming out of the case
                if (!n->Sleep(15s))
                    break;
            }
            winrt::uninit_apartment();
        });
    }

    Link::~Link()
    {
        {
            std::lock_guard guard{native->lock};
            native->stop = true;
        }
        native->wake.notify_all();
        if (native->worker.joinable())
            native->worker.join();
    }
}
