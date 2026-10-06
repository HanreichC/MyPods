// MyPods
// License: GPL-3.0

// Windows: Core Audio stands in for the sound server. Playback endpoints are the sinks, named by their ID;
// there are no card profiles or codecs to pick, and Windows routes to connected headphones itself, so those
// queries answer "not there" and the codec capability stays hidden.

#include "PulseAudioClient.h"
#include "Logger.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <winrt/base.h>

#include <algorithm>
#include <cctype>

namespace MagicPodsCore
{
    // Endpoint properties (the same keys the endpoint's registry "Properties" holds); no initguid.h needed
    static constexpr PROPERTYKEY FORM_FACTOR_KEY{{0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}}, 0};
    static constexpr PROPERTYKEY ENUMERATOR_KEY{{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 24};
    static constexpr PROPERTYKEY NAME_KEY{{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 2};
    static constexpr PROPERTYKEY INSTANCE_KEY{{0xb3f8fa53, 0x0004, 0x438e, {0x90, 0x03, 0x51, 0xa4, 0x6e, 0x13, 0x9b, 0xfc}}, 2};
    static constexpr PROPERTYKEY ADAPTER_KEY{{0xb3f8fa53, 0x0004, 0x438e, {0x90, 0x03, 0x51, 0xa4, 0x6e, 0x13, 0x9b, 0xfc}}, 6};
    static constexpr PROPERTYKEY BLUETOOTH_KEY{{0xb3f8fa53, 0x0004, 0x438e, {0x90, 0x03, 0x51, 0xa4, 0x6e, 0x13, 0x9b, 0xfc}}, 39};
    static constexpr unsigned HEADPHONES = 3, HEADSET = 5;

    static std::string Utf8(const wchar_t *text)
    {
        if (!text)
            return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        std::string out(size > 0 ? size - 1 : 0, '\0');
        if (size > 1)
            WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
        return out;
    }

    static std::string Upper(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return text;
    }

    // Plugged in, unplugged, added, removed: what decides which headphones there are. Volume and property
    // changes are left out, they come in bursts and change nothing here.
    struct PulseAudioClient::Native : IMMNotificationClient
    {
        PulseAudioClient *owner;
        winrt::com_ptr<IMMDeviceEnumerator> enumerator;

        explicit Native(PulseAudioClient *owner) : owner(owner) {}

        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return Changed(); }
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return Changed(); }
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return Changed(); }
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
        // owned by the client, which unregisters it before it goes
        ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override { return 1; }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override
        {
            if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient))
            {
                *out = static_cast<IMMNotificationClient *>(this);
                return S_OK;
            }
            *out = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT Changed()
        {
            owner->_onSinkChangedEvent.FireEvent(0);
            return S_OK;
        }

        std::vector<AudioEndpoint> ActiveEndpoints()
        {
            std::vector<AudioEndpoint> result;
            winrt::com_ptr<IMMDeviceCollection> endpoints;
            UINT count = 0;
            if (!enumerator || FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, endpoints.put())) || FAILED(endpoints->GetCount(&count)))
                return result;
            for (UINT i = 0; i < count; i++)
            {
                winrt::com_ptr<IMMDevice> device;
                winrt::com_ptr<IPropertyStore> properties;
                LPWSTR id = nullptr;
                if (FAILED(endpoints->Item(i, device.put())) || FAILED(device->GetId(&id)))
                    continue;
                AudioEndpoint endpoint{Utf8(id)};
                CoTaskMemFree(id);
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, properties.put())))
                {
                    auto read = [&](const PROPERTYKEY &key, std::string &text, unsigned *number = nullptr)
                    {
                        PROPVARIANT value;
                        PropVariantInit(&value);
                        if (SUCCEEDED(properties->GetValue(key, &value)))
                        {
                            if (value.vt == VT_LPWSTR)
                                text = Utf8(value.pwszVal);
                            else if (value.vt == VT_UI4 && number)
                                *number = value.ulVal;
                        }
                        PropVariantClear(&value);
                    };
                    std::string unused;
                    read(FORM_FACTOR_KEY, unused, &endpoint.formFactor);
                    read(ENUMERATOR_KEY, endpoint.enumerator);
                    read(INSTANCE_KEY, endpoint.instance);
                    read(BLUETOOTH_KEY, endpoint.bluetooth);
                    read(NAME_KEY, endpoint.name);
                    read(ADAPTER_KEY, endpoint.adapter);
                }
                result.push_back(std::move(endpoint));
            }
            return result;
        }
    };

    PulseAudioClient::PulseAudioClient() : _native{std::make_unique<Native>(this)}
    {
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(_native->enumerator.put()))) ||
            FAILED(_native->enumerator->RegisterEndpointNotificationCallback(_native.get())))
            Logger::Error("PulseAudioClient: no Core Audio, wired headphones and effects stay off");
    }

    PulseAudioClient::~PulseAudioClient()
    {
        if (_native->enumerator)
            _native->enumerator->UnregisterEndpointNotificationCallback(_native.get());
    }

    // Bluetooth: its own driver, or Intel's offload, which names the Bluetooth device behind it
    static bool Bluetooth(const AudioEndpoint &endpoint)
    {
        return Upper(endpoint.enumerator).starts_with("BTH") || Upper(endpoint.bluetooth).find("BTH") != std::string::npos;
    }

    // The jack or monitor by its endpoint ("Kopfhörer", "OMEN 27i IPS"), USB by its product
    static WiredOutput Output(const AudioEndpoint &endpoint)
    {
        bool usb = Upper(endpoint.enumerator) == "USB";
        std::string name = usb && !endpoint.adapter.empty() ? endpoint.adapter : endpoint.name;
        return WiredOutput{endpoint.id, name.empty() ? endpoint.id : name, usb};
    }

    std::optional<WiredOutput> PulseAudioClient::WiredHeadphones(const AudioEndpoint &endpoint)
    {
        // A jack that shares the speakers' endpoint, or one on a monitor (sound over DisplayPort/HDMI), tells
        // Windows nothing about a plug: the user marks such outputs as headphones (GetOutputs)
        if ((endpoint.formFactor != HEADPHONES && endpoint.formFactor != HEADSET) || Bluetooth(endpoint))
            return std::nullopt;
        return Output(endpoint);
    }

    std::vector<WiredOutput> PulseAudioClient::GetOutputs()
    {
        std::vector<WiredOutput> outputs;
        for (const auto &endpoint : _native->ActiveEndpoints())
            if (!Bluetooth(endpoint))
                outputs.push_back(Output(endpoint));
        return outputs;
    }

    std::vector<WiredOutput> PulseAudioClient::GetWiredHeadphones()
    {
        std::vector<WiredOutput> outputs;
        for (const auto &endpoint : _native->ActiveEndpoints())
            if (auto output = WiredHeadphones(endpoint))
                outputs.push_back(*output);
        return outputs;
    }

    std::optional<std::string> PulseAudioClient::FindSink(const std::string &part)
    {
        // `part` is an endpoint ID, or a Bluetooth MAC without colons, which the device instance carries.
        // A headset's hands-free endpoint shares the MAC; the music goes to the stereo one.
        std::optional<std::string> found;
        auto needle = Upper(part);
        for (const auto &endpoint : _native->ActiveEndpoints())
        {
            if (Upper(endpoint.id + " " + endpoint.instance + " " + endpoint.bluetooth).find(needle) == std::string::npos)
                continue;
            if (endpoint.formFactor != HEADSET)
                return endpoint.id;
            found = endpoint.id;
        }
        return found;
    }

    std::optional<double> PulseAudioClient::GetSinkVolume(const std::string &name)
    {
        std::wstring id(name.begin(), name.end()); // endpoint IDs are ASCII
        winrt::com_ptr<IMMDevice> device;
        winrt::com_ptr<IAudioEndpointVolume> volume;
        float level = 0;
        if (!_native->enumerator || FAILED(_native->enumerator->GetDevice(id.c_str(), device.put())) ||
            FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, volume.put_void())) ||
            FAILED(volume->GetMasterVolumeLevelScalar(&level)))
            return std::nullopt;
        return level;
    }

    // What Windows' own sound settings call to change the default output (EarTrumpet and SoundSwitch do the same).
    // ponytail: undocumented COM; should a Windows release change it, switching fails and the output stays where it was
    struct __declspec(uuid("f8679f50-850a-41cf-9c72-430f290290c8")) IPolicyConfig : IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, void **) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, void **) = 0;
        virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, void *, void *) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, INT64 *, INT64 *) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, INT64 *) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void *) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void *) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    };
    static constexpr CLSID POLICY_CONFIG_CLIENT{0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};

    bool PulseAudioClient::SetDefaultSink(const std::string &name)
    {
        std::wstring id(name.begin(), name.end()); // endpoint IDs are ASCII
        winrt::com_ptr<IPolicyConfig> policy;
        if (FAILED(CoCreateInstance(POLICY_CONFIG_CLIENT, nullptr, CLSCTX_ALL, __uuidof(IPolicyConfig), policy.put_void())))
            return false;
        // all three roles, as the sound settings do: music, calls and system sounds go there together
        for (ERole role : {eConsole, eMultimedia, eCommunications})
            if (FAILED(policy->SetDefaultEndpoint(id.c_str(), role)))
                return false;
        return true;
    }

    bool PulseAudioClient::SetCardProfile(const std::string &, const std::string &) { return false; }
    std::optional<CardInfo> PulseAudioClient::GetCardInfoByName(const std::string &) { return std::nullopt; }
    std::string PulseAudioClient::GetNameFromMac(const std::string &mac) { return mac; }
    bool PulseAudioClient::SetSinkVolume(const std::string &, double) { return false; }
    std::optional<SinkDetails> PulseAudioClient::GetSinkDetails(const std::string &) { return std::nullopt; }
    std::vector<StreamInfo> PulseAudioClient::GetStreams() { return {}; }
}
