// MyPods
// License: GPL-3.0

// Windows side of DBusDeviceInfo: a paired classic Bluetooth device through WinRT.
//  * VID/PID and the HFP battery live on the device's PnP nodes (same container id), not on BluetoothDevice.
//  * The UUIDs are the RFCOMM services from the SDP record Windows cached at pairing, which covers every
//    UUID the daemon looks for (Hands-Free, Parrot Zik, Galaxy Buds).
//  * Windows has no API to connect a paired audio device; the Bluetooth audio driver's
//    KSPROPERTY_ONESHOT_RECONNECT/DISCONNECT (what the Sound settings use) does it.

#include "DBusDeviceInfo.h"
#include "Logger.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Rfcomm.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <windows.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <ks.h>
#include <ksmedia.h>

#include <algorithm>
#include <thread>

namespace MagicPodsCore {

    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Enumeration;

    // DEVPKEY_Bluetooth_Battery, the level Settings > Bluetooth shows (HFP AT+IPHONEACCEV / +BIEV)
    static constexpr const wchar_t* BATTERY_KEY = L"{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2";
    static constexpr const wchar_t* HARDWARE_IDS_KEY = L"System.Devices.HardwareIds";
    static constexpr const wchar_t* CONTAINER_KEY = L"System.Devices.Aep.ContainerId";

    struct DBusDeviceInfo::Native {
        BluetoothDevice device{nullptr};
        winrt::event_token connectionToken{};
        DeviceWatcher batteryWatcher{nullptr};
    };

    static std::string FormatAddress(uint64_t address) {
        char buf[18];
        std::snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X",
                      unsigned(address >> 40) & 0xFF, unsigned(address >> 32) & 0xFF, unsigned(address >> 24) & 0xFF,
                      unsigned(address >> 16) & 0xFF, unsigned(address >> 8) & 0xFF, unsigned(address) & 0xFF);
        return buf;
    }

    // "{0000111E-0000-1000-8000-00805F9B34FB}" -> "0000111e-0000-1000-8000-00805f9b34fb", the BlueZ spelling
    static std::string UuidString(const winrt::guid& guid) {
        std::string s = winrt::to_string(winrt::to_hstring(guid));
        s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return c == '{' || c == '}'; }), s.end());
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static std::optional<uint8_t> ReadBattery(const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Windows::Foundation::IInspectable>& properties) {
        if (!properties.HasKey(BATTERY_KEY))
            return std::nullopt;
        auto value = properties.Lookup(BATTERY_KEY).try_as<winrt::Windows::Foundation::IPropertyValue>();
        if (!value)
            return std::nullopt;
        return value.GetUInt8();
    }

    DBusDeviceInfo::DBusDeviceInfo(uint64_t address) : _native{std::make_unique<Native>()} {
        auto& device = _native->device = BluetoothDevice::FromBluetoothAddressAsync(address).get();
        if (!device)
            throw std::runtime_error("Bluetooth device not found: " + FormatAddress(address));

        _address = FormatAddress(address);
        _name = device.Name().empty() ? _address : winrt::to_string(device.Name());
        _clazz = device.ClassOfDevice().RawValue();
        _pairedStatus.SetValue(true);
        _servicesResolved.SetValue(true);
        _connectionStatus.SetValue(device.ConnectionStatus() == BluetoothConnectionStatus::Connected);

        try {
            auto services = device.GetRfcommServicesAsync(BluetoothCacheMode::Cached).get();
            for (const auto& service : services.Services())
                _uuids.push_back(UuidString(service.ServiceId().Uuid()));
        }
        catch (const winrt::hresult_error& e) {
            Logger::Error("%s: no service list: %s", _address.c_str(), winrt::to_string(e.message()).c_str());
        }

        // The device's PnP nodes share its container id: the service nodes carry the profile UUID and
        // VID/PID in their hardware ids ("BTHENUM\{0000110b-...}_VID&0001004c_PID&200a"), the hands-free
        // node the battery. Their UUIDs complete the RFCOMM list to what BlueZ reports (A2DP, AVRCP, ...).
        try {
            static const std::regex serviceUuid(R"(BTHENUM\\\{([0-9a-fA-F-]{36})\})");
            auto aep = DeviceInformation::CreateFromIdAsync(device.DeviceId(), {CONTAINER_KEY}, DeviceInformationKind::AssociationEndpoint).get();
            auto container = aep.Properties().TryLookup(CONTAINER_KEY);
            if (container) {
                auto aqs = L"System.Devices.ContainerId:=\"" + winrt::to_hstring(winrt::unbox_value<winrt::guid>(container)) + L"\"";
                for (const auto& node : DeviceInformation::FindAllAsync(aqs, {HARDWARE_IDS_KEY, BATTERY_KEY}, DeviceInformationKind::Device).get()) {
                    if (auto ids = node.Properties().TryLookup(HARDWARE_IDS_KEY).try_as<winrt::Windows::Foundation::IPropertyValue>()) {
                        winrt::com_array<winrt::hstring> list;
                        ids.GetStringArray(list);
                        for (const auto& hstringId : list) {
                            const auto id = winrt::to_string(hstringId);
                            std::smatch match;
                            if (std::regex_search(id, match, serviceUuid)) {
                                auto uuid = StringUtils::ToLowerCase(match[1]);
                                if (std::find(_uuids.begin(), _uuids.end(), uuid) == _uuids.end())
                                    _uuids.push_back(uuid);
                            }
                            const auto vidPid = ParseVidPid(id);
                            if (_vendorId == 0 && vidPid[0] != 0) {
                                _vendorId = vidPid[0];
                                _productId = vidPid[1];
                            }
                        }
                    }
                    if (auto battery = ReadBattery(node.Properties()))
                        _handsFreeBatteryStatus.SetValue(*battery);
                }

                // No change notification for the battery property; a watcher on the same nodes reports updates.
                _native->batteryWatcher = DeviceInformation::CreateWatcher(aqs, {BATTERY_KEY}, DeviceInformationKind::Device);
                _native->batteryWatcher.Updated([this](const DeviceWatcher&, const DeviceInformationUpdate& update) {
                    if (auto battery = ReadBattery(update.Properties()))
                        _handsFreeBatteryStatus.SetValue(*battery);
                });
                _native->batteryWatcher.Start();
            }
        }
        catch (const winrt::hresult_error& e) {
            Logger::Error("%s: no device properties: %s", _address.c_str(), winrt::to_string(e.message()).c_str());
        }

        _native->connectionToken = device.ConnectionStatusChanged([this](const BluetoothDevice& sender, const auto&) {
            _connectionStatus.SetValue(sender.ConnectionStatus() == BluetoothConnectionStatus::Connected);
        });
    }

    DBusDeviceInfo::~DBusDeviceInfo() {
        if (_native->batteryWatcher && _native->batteryWatcher.Status() == DeviceWatcherStatus::Started)
            _native->batteryWatcher.Stop();
        _native->device.ConnectionStatusChanged(_native->connectionToken);
    }

    // Sends KSPROPERTY_ONESHOT_(RE|DIS)CONNECT to the Bluetooth audio filters of this device. The filters
    // are found through the audio endpoints' topology; their ids contain the device address
    // ("...bthhfenum#...&0&a4c6f0123456_c00000000#...").
    static bool SendBtAudioOneShot(const std::string& address, ULONG property) {
        std::wstring needle;
        for (char c : address)
            if (c != ':')
                needle += static_cast<wchar_t>(std::tolower(static_cast<unsigned char>(c)));

        bool sent = false;
        winrt::com_ptr<IMMDeviceEnumerator> enumerator;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(enumerator.put()))))
            return false;
        winrt::com_ptr<IMMDeviceCollection> endpoints;
        if (FAILED(enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, endpoints.put())))
            return false;
        UINT count = 0;
        endpoints->GetCount(&count);
        std::vector<std::wstring> done;
        for (UINT i = 0; i < count; i++) {
            winrt::com_ptr<IMMDevice> endpoint;
            winrt::com_ptr<IDeviceTopology> topology;
            winrt::com_ptr<IConnector> connector, connectedTo;
            winrt::com_ptr<IDeviceTopology> filterTopology;
            LPWSTR filterId = nullptr;
            if (FAILED(endpoints->Item(i, endpoint.put())) ||
                FAILED(endpoint->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr, topology.put_void())) ||
                FAILED(topology->GetConnector(0, connector.put())) ||
                FAILED(connector->GetConnectedTo(connectedTo.put())))
                continue;
            auto part = connectedTo.try_as<IPart>();
            if (!part || FAILED(part->GetTopologyObject(filterTopology.put())) || FAILED(filterTopology->GetDeviceId(&filterId)))
                continue;
            std::wstring id{filterId};
            CoTaskMemFree(filterId);
            std::wstring lower = id;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (lower.find(needle) == std::wstring::npos || std::find(done.begin(), done.end(), lower) != done.end())
                continue;
            done.push_back(lower);

            winrt::com_ptr<IMMDevice> filter;
            winrt::com_ptr<IKsControl> ks;
            if (FAILED(enumerator->GetDevice(id.c_str(), filter.put())) ||
                FAILED(filter->Activate(__uuidof(IKsControl), CLSCTX_ALL, nullptr, ks.put_void())))
                continue;
            KSPROPERTY request{};
            request.Set = KSPROPSETID_BtAudio;
            request.Id = property;
            request.Flags = KSPROPERTY_TYPE_GET;
            ULONG returned = 0;
            if (SUCCEEDED(ks->KsProperty(&request, sizeof(request), nullptr, 0, &returned)))
                sent = true;
        }
        return sent;
    }

    // COM for the audio calls: the caller's thread may belong to no apartment, so run on our own MTA thread
    static std::optional<std::string> OneShot(const std::string& address, ULONG property) {
        std::optional<std::string> error;
        std::thread([&] {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            if (!SendBtAudioOneShot(address, property))
                error = "no Bluetooth audio endpoint for " + address;
            winrt::uninit_apartment();
        }).join();
        return error;
    }

    void DBusDeviceInfo::Connect() {
        if (auto error = OneShot(_address, KSPROPERTY_ONESHOT_RECONNECT))
            throw std::runtime_error(*error);
    }

    // ponytail: the worker holds a raw this, like the D-Bus reply handler does; a device unpaired during
    // the up to 10 s wait would be gone under it. A weak_ptr handle would close that gap.
    void DBusDeviceInfo::ConnectAsync(BtCallback&& callback) {
        std::thread([this, callback = std::move(callback)] {
            auto error = OneShot(_address, KSPROPERTY_ONESHOT_RECONNECT);
            // the reconnect request returns at once; give the link time like BlueZ's Connect reply does
            for (int i = 0; !error && i < 100 && !_connectionStatus.GetValue(); i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (callback)
                callback(error ? &*error : nullptr);
        }).detach();
    }

    void DBusDeviceInfo::Disconnect() {
        if (auto error = OneShot(_address, KSPROPERTY_ONESHOT_DISCONNECT))
            throw std::runtime_error(*error);
    }

    void DBusDeviceInfo::DisconnectAsync(BtCallback&& callback) {
        std::thread([this, callback = std::move(callback)] {
            auto error = OneShot(_address, KSPROPERTY_ONESHOT_DISCONNECT);
            for (int i = 0; !error && i < 100 && _connectionStatus.GetValue(); i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (callback)
                callback(error ? &*error : nullptr);
        }).detach();
    }

    std::array<unsigned short, 2> DBusDeviceInfo::ParseVidPid(const std::string& hardwareId) {
        std::smatch match;
        // VID&<source 4><vendor 4>_PID&<product 4>
        static const std::regex pattern("VID&[0-9A-Fa-f]{4}([0-9A-Fa-f]{4})_PID&([0-9A-Fa-f]{4})", std::regex::icase);
        if (!std::regex_search(hardwareId, match, pattern))
            return {0, 0};
        return {static_cast<unsigned short>(std::stoul(match[1], nullptr, 16)),
                static_cast<unsigned short>(std::stoul(match[2], nullptr, 16))};
    }
}
