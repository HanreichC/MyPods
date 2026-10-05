// MyPods
// License: GPL-3.0

// Windows effects through Equalizer APO (AudioEffects.h). The MSI ships its APO in <install>\EqualizerAPO and runs
// `magicpodscore --apo-install`, which registers it with Windows and puts it on every headphone endpoint there is.
// Headphones that come later are put on with `--apo-register`, elevated, the first time they get effects.
// The daemon writes MyPods.txt into Equalizer APO's config folder, included from config.txt; Equalizer APO rereads
// it on every change. An Equalizer APO the user installed before keeps its install, its config and its devices.
//
// Putting the APO on an endpoint is what Equalizer APO's Device Selector does (DeviceAPOInfo::install, the mode
// it picks in DeviceAPOInfo::load), in the registry format it reads back (version 2), so either can undo it.

#include "AudioEffects.h"
#include "Logger.h"

#include <windows.h>
#include <aclapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <winsvc.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <thread>

namespace MagicPodsCore
{
    static const std::wstring RENDER = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render";
    static const std::wstring APP = L"SOFTWARE\\EqualizerAPO";
    static const std::wstring CHILD = APP + L"\\Child APOs";
    static const std::wstring PRE_MIX = L"{EACD2258-FCAC-4FF4-B36D-419E924A6D79}";
    static const std::wstring POST_MIX = L"{EC1CC9CE-FAED-4822-828A-82A81A6F018F}";
    // FxProperties APO slots, in Device Selector's order, and what the driver may have instead of them
    enum { LFX, GFX, SFX, MFX, EFX };
    static const wchar_t *FX[] = {L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},1", L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},2",
                                  L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5", L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},6",
                                  L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7"};
    static const wchar_t *MULTI_FX[] = {L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},13", L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},14",
                                        L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},15"};
    // processing modes of SFX, MFX, EFX
    static const wchar_t *MODES[] = {nullptr, nullptr, L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},5", L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},6",
                                     L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},7"};
    static const wchar_t *DEFAULT_MODE = L"{C18E2F7E-933D-4965-B7D1-1EEF228D2AF3}";
    static const wchar_t *TITLE = L"{b725f130-47ef-101a-a5f1-02608c9eebac},10";
    static const wchar_t *DISABLE_ENHANCEMENTS = L"{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5";
    static const wchar_t *FORM_FACTOR = L"{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},0";
    static const wchar_t *COMBINED = L"{b3f8fa53-0004-438e-9003-51a46e139bfc},41"; // Windows 11 joins a Bluetooth pair's endpoints; EFX doesn't run there
    static const std::wstring NO_KEY = L"!KEY", NO_VALUE = L"!VALUE";

    static std::wstring Wide(const std::string &text) { return std::wstring(text.begin(), text.end()); } // GUIDs and IDs are ASCII

    static bool Same(const std::wstring &a, const std::wstring &b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }

    static std::optional<std::wstring> Read(const std::wstring &key, const wchar_t *name)
    {
        wchar_t buf[2048];
        DWORD size = sizeof buf;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), name, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
            return std::nullopt;
        return std::wstring(buf);
    }

    static std::optional<DWORD> ReadDword(const std::wstring &key, const wchar_t *name)
    {
        DWORD value = 0, size = sizeof value;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS)
            return std::nullopt;
        return value;
    }

    static bool Exists(const std::wstring &key, const wchar_t *name = nullptr)
    {
        if (name)
            return RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), name, RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        HKEY handle;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ, &handle) != ERROR_SUCCESS)
            return false;
        RegCloseKey(handle);
        return true;
    }

    static void Write(const std::wstring &key, const wchar_t *name, const void *data, DWORD bytes, DWORD type)
    {
        HKEY handle;
        LSTATUS status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
        if (status == ERROR_SUCCESS)
        {
            status = RegSetValueExW(handle, name, 0, type, static_cast<const BYTE *>(data), bytes);
            RegCloseKey(handle);
        }
        if (status != ERROR_SUCCESS)
            throw std::runtime_error("registry write failed (" + std::to_string(status) + ")");
    }

    static void Write(const std::wstring &key, const wchar_t *name, const std::wstring &value)
    {
        Write(key, name, value.c_str(), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)), REG_SZ);
    }

    static void Delete(const std::wstring &key, const wchar_t *name)
    {
        RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, key.c_str(), name);
    }

    // The endpoint keys belong to the audio service: an administrator may write their values but not add the
    // FxProperties key a driver without effects lacks. Take the key over and grant administrators full control.
    static void MakeWritable(const std::wstring &key)
    {
        HANDLE token;
        TOKEN_PRIVILEGES privilege{1};
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
            throw std::runtime_error("no process token");
        LookupPrivilegeValueW(nullptr, L"SeTakeOwnershipPrivilege", &privilege.Privileges[0].Luid); // SE_TAKE_OWNERSHIP_NAME is narrow without UNICODE
        privilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token, FALSE, &privilege, sizeof privilege, nullptr, nullptr);
        CloseHandle(token);

        BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
        DWORD sidSize = sizeof sidBuffer;
        PSID admins = sidBuffer;
        CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admins, &sidSize);
        std::wstring path = L"MACHINE\\" + key;
        if (SetNamedSecurityInfoW(path.data(), SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION, admins, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            throw std::runtime_error("could not take over the endpoint key");

        PACL dacl = nullptr, granted = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        EXPLICIT_ACCESSW access{KEY_ALL_ACCESS, SET_ACCESS, SUB_CONTAINERS_AND_OBJECTS_INHERIT};
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
        access.Trustee.ptstrName = static_cast<LPWSTR>(admins);
        bool ok = GetNamedSecurityInfoW(path.c_str(), SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr, &descriptor) == ERROR_SUCCESS &&
                  SetEntriesInAclW(1, &access, dacl, &granted) == ERROR_SUCCESS &&
                  SetNamedSecurityInfoW(path.data(), SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr, granted, nullptr) == ERROR_SUCCESS;
        LocalFree(granted);
        LocalFree(descriptor);
        if (!ok)
            throw std::runtime_error("could not make the endpoint key writable");
    }

    static bool Installed(const std::wstring &fx)
    {
        for (auto *slot : FX)
            if (auto value = Read(fx, slot); value && (Same(*value, PRE_MIX) || Same(*value, POST_MIX)))
                return true;
        return false;
    }

    // Equalizer APO on the endpoint, the driver's own effects running inside it. False if it was there already.
    static bool RegisterEndpoint(const std::wstring &guid)
    {
        auto device = RENDER + L"\\" + guid, fx = device + L"\\FxProperties", child = CHILD + L"\\" + guid;
        if (Installed(fx))
            return false;
        enum { LfxGfx, SfxMfx, SfxEfx } mode = LfxGfx;
        std::wstring original[5];
        if (!Exists(fx))
        {
            HKEY handle;
            if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, fx.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr) != ERROR_SUCCESS)
            {
                MakeWritable(device);
                if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, fx.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr) != ERROR_SUCCESS)
                    throw std::runtime_error("could not add FxProperties");
            }
            RegCloseKey(handle);
            Write(fx, TITLE, L"Equalizer APO");
            for (auto &o : original)
                o = NO_KEY;
        }
        else
        {
            for (int i = 0; i < 5; i++)
                original[i] = Read(fx, FX[i]).value_or(NO_VALUE);
            // LFX/GFX only if the driver brings nothing else
            bool modern = Exists(fx, FX[SFX]) || Exists(fx, FX[MFX]) || Exists(fx, FX[EFX]);
            for (auto *slot : MULTI_FX)
                modern = modern || Exists(fx, slot);
            if (modern || !(Exists(fx, FX[LFX]) || Exists(fx, FX[GFX])))
                mode = Exists(device + L"\\Properties", COMBINED) ? SfxMfx : SfxEfx;
        }
        for (int i = 0; i < 5; i++)
            Write(child, FX[i], original[i]);

        int pre = mode == LfxGfx ? LFX : SFX, post = mode == LfxGfx ? GFX : mode == SfxMfx ? MFX : EFX;
        // a driver with the effects in the other pair of slots keeps them
        int preChild = pre, postChild = post;
        if (original[pre] == NO_VALUE && original[post] == NO_VALUE)
            preChild = mode == LfxGfx ? SFX : LFX, postChild = mode == LfxGfx ? MFX : GFX;
        auto childGuid = [&](int slot) { return original[slot] == NO_KEY || original[slot] == NO_VALUE ? std::wstring{} : original[slot]; };
        Write(child, L"PreMixChild", childGuid(preChild));
        Write(child, L"PostMixChild", childGuid(postChild));
        Write(child, L"AllowSilentBufferModification", L"false");
        Write(child, L"Version", L"2");

        for (int slot : {LFX, GFX, SFX, MFX, EFX})
            if (slot != pre && slot != post && (mode == LfxGfx || slot == LFX || slot == GFX))
                Delete(fx, FX[slot]); // LFX/GFX mode clears SFX, MFX and EFX; the others clear only LFX and GFX
        Write(fx, FX[pre], PRE_MIX);
        Write(fx, FX[post], POST_MIX);
        for (int slot : {pre, post})
            if (MODES[slot] && !Exists(fx, MODES[slot]))
            {
                std::wstring modes = std::wstring(DEFAULT_MODE) + L'\0';
                Write(fx, MODES[slot], modes.c_str(), static_cast<DWORD>((modes.size() + 1) * sizeof(wchar_t)), REG_MULTI_SZ);
            }
        Delete(fx, DISABLE_ENHANCEMENTS); // "Disable all enhancements" would skip the APO
        return true;
    }

    // Back to the driver's own APOs (DeviceAPOInfo::uninstall). False if Equalizer APO wasn't on it.
    static bool UnregisterEndpoint(const std::wstring &guid)
    {
        auto fx = RENDER + L"\\" + guid + L"\\FxProperties", child = CHILD + L"\\" + guid;
        if (!Exists(child))
            return false;
        bool installed = Installed(fx);
        if (installed && Read(child, FX[LFX]) == NO_KEY)
            RegDeleteTreeW(HKEY_LOCAL_MACHINE, fx.c_str());
        else if (installed)
            for (auto *slot : FX)
            {
                auto original = Read(child, slot);
                if (original == NO_VALUE)
                    Delete(fx, slot);
                else if (original && !original->empty())
                    Write(fx, slot, *original);
            }
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, child.c_str());
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, CHILD.c_str()); // only goes when empty
        return installed;
    }

    // Every playback endpoint Windows remembers, with its form factor. Also the absent ones, unlike Device Selector:
    // paired Bluetooth headphones are "not present" while they're off, and must get the APO and lose it like the rest.
    static std::vector<std::pair<std::wstring, DWORD>> Endpoints()
    {
        std::vector<std::pair<std::wstring, DWORD>> endpoints;
        HKEY render;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, RENDER.c_str(), 0, KEY_READ, &render) != ERROR_SUCCESS)
            return endpoints;
        wchar_t name[64];
        for (DWORD i = 0, size = 64; RegEnumKeyExW(render, i, name, &size, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; i++, size = 64)
            endpoints.push_back({name, ReadDword(RENDER + L"\\" + name + L"\\Properties", FORM_FACTOR).value_or(0)});
        RegCloseKey(render);
        return endpoints;
    }

    static bool IsHeadphones(DWORD formFactor) { return formFactor == 3 || formFactor == 5; } // headphones, headset

    static std::filesystem::path ApoDir()
    {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        return std::filesystem::path(exe).parent_path().parent_path() / L"EqualizerAPO"; // <install>\modules\magicpodscore.exe
    }

    // Stops the audio service and what depends on it, then starts them again, so the endpoints load their new APOs
    static void RestartAudio()
    {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm)
            return;
        std::vector<std::wstring> services{L"Audiosrv"};
        if (SC_HANDLE audio = OpenServiceW(scm, L"Audiosrv", SERVICE_ENUMERATE_DEPENDENTS))
        {
            DWORD bytes = 0, count = 0;
            EnumDependentServicesW(audio, SERVICE_ACTIVE, nullptr, 0, &bytes, &count);
            std::vector<BYTE> buffer(bytes);
            auto *dependents = reinterpret_cast<ENUM_SERVICE_STATUSW *>(buffer.data());
            if (bytes && EnumDependentServicesW(audio, SERVICE_ACTIVE, dependents, bytes, &bytes, &count))
                for (DWORD i = 0; i < count; i++)
                    services.insert(services.end() - 1, dependents[i].lpServiceName); // in stop order, the audio service last
            CloseServiceHandle(audio);
        }
        auto wait = [](SC_HANDLE service, DWORD state)
        {
            SERVICE_STATUS status{};
            for (int i = 0; i < 300 && QueryServiceStatus(service, &status) && status.dwCurrentState != state; i++)
                Sleep(100);
        };
        for (auto &name : services)
            if (SC_HANDLE service = OpenServiceW(scm, name.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS))
            {
                SERVICE_STATUS status{};
                ControlService(service, SERVICE_CONTROL_STOP, &status);
                wait(service, SERVICE_STOPPED);
                CloseServiceHandle(service);
            }
        for (auto it = services.rbegin(); it != services.rend(); it++)
            if (SC_HANDLE service = OpenServiceW(scm, it->c_str(), SERVICE_START | SERVICE_QUERY_STATUS))
            {
                StartServiceW(service, 0, nullptr);
                wait(service, SERVICE_RUNNING);
                CloseServiceHandle(service);
            }
        CloseServiceHandle(scm);
    }

    // Unsigned APOs only run with protected audio off, as Equalizer APO's installer sets it
    static bool AllowApo()
    {
        static const std::wstring AUDIO = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio";
        if (ReadDword(AUDIO, L"DisableProtectedAudioDG") == 1u)
            return false;
        DWORD one = 1;
        Write(AUDIO, L"DisableProtectedAudioDG", &one, sizeof one, REG_DWORD);
        return true;
    }

    static bool CallDll(const std::filesystem::path &dll, const char *function)
    {
        HMODULE module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH); // fftw and sndfile lie next to it
        auto call = module ? reinterpret_cast<HRESULT(STDAPICALLTYPE *)()>(GetProcAddress(module, function)) : nullptr;
        bool ok = call && SUCCEEDED(call());
        if (module)
            FreeLibrary(module);
        return ok;
    }

    int AudioEffects::SetupApo(const std::string &command, const std::string &endpoint)
    {
        try
        {
            auto dir = ApoDir();
            auto installPath = Read(APP, L"InstallPath");
            bool ours = !installPath || std::filesystem::path(*installPath) == dir;
            bool changed = false;
            if (command == "--apo-install")
            {
                if (ours)
                {
                    changed = !Exists(L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\" + PRE_MIX);
                    if (!CallDll(dir / L"EqualizerAPO.dll", "DllRegisterServer"))
                        throw std::runtime_error("EqualizerAPO.dll did not register");
                    Write(APP, L"InstallPath", dir.wstring());
                    Write(APP, L"ConfigPath", (dir / L"config").wstring());
                    if (!Exists(APP, L"EnableTrace"))
                        Write(APP, L"EnableTrace", L"false");
                }
                changed |= AllowApo();
                for (auto &[guid, formFactor] : Endpoints())
                    if (IsHeadphones(formFactor))
                        changed |= RegisterEndpoint(guid);
            }
            else if (command == "--apo-register")
            {
                // from the daemon's command line, but elevated: only a GUID gets into a registry path
                if (!std::regex_match(endpoint, std::regex(R"(\{[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\})")))
                    throw std::runtime_error("not an endpoint GUID: " + endpoint);
                if (!Exists(RENDER + L"\\" + Wide(endpoint)))
                    throw std::runtime_error("no such endpoint: " + endpoint);
                changed = AllowApo();
                changed |= RegisterEndpoint(Wide(endpoint));
            }
            else if (command == "--apo-uninstall")
            {
                if (!ours)
                    return 0; // the user's own Equalizer APO stays as it is
                changed = Exists(L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\" + PRE_MIX);
                for (auto &[guid, formFactor] : Endpoints())
                    UnregisterEndpoint(guid);
                CallDll(dir / L"EqualizerAPO.dll", "DllUnregisterServer");
                RegDeleteTreeW(HKEY_LOCAL_MACHINE, APP.c_str());
            }
            else
                return 2;
            if (changed)
                RestartAudio();
            Logger::Info("Equalizer APO %s: done%s", command.c_str(), changed ? ", audio restarted" : "");
            return 0;
        }
        catch (const std::exception &e)
        {
            Logger::Error("Equalizer APO %s: %s", command.c_str(), e.what());
            return 1;
        }
    }

    AudioEffects &AudioEffects::Instance()
    {
        static AudioEffects instance;
        return instance;
    }

    AudioEffects::AudioEffects() = default;
    AudioEffects::~AudioEffects() = default;

    static std::string Guid(const std::string &endpoint)
    {
        auto brace = endpoint.rfind('{');
        return brace == std::string::npos ? endpoint : endpoint.substr(brace);
    }

    // Asks for the elevation (UAC) to put Equalizer APO on headphones that came after the install. ShellExecuteEx
    // waits for the answer, so it runs on its own thread.
    static void OfferToEqualizerApo(const std::string &guid)
    {
        std::thread([guid]()
        {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            wchar_t exe[MAX_PATH];
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            std::wstring parameters = L"--apo-register " + Wide(guid);
            SHELLEXECUTEINFOW info{sizeof info};
            info.lpVerb = L"runas";
            info.lpFile = exe;
            info.lpParameters = parameters.c_str();
            info.nShow = SW_HIDE;
            if (!ShellExecuteExW(&info))
                Logger::Info("AudioEffects: Equalizer APO not put on %s (%lu)", guid.c_str(), GetLastError());
            CoUninitialize();
        }).detach();
    }

    std::string AudioEffects::Apply(const std::string &sink, const std::string &description, EffectsConfig config)
    {
        std::lock_guard lock{_lock};
        if (config.IsNeutral())
            _sections.erase(sink);
        else
            _sections[sink] = ApoSection(sink, description, config);

        auto configPath = Read(APP, L"ConfigPath");
        if (!configPath)
        {
            Logger::Error("AudioEffects: Equalizer APO is not installed, the effects stay off");
            return sink;
        }
        std::filesystem::path dir(*configPath);
        std::string text = "# Written by MyPods for the headphones it controls; changes here are overwritten\n";
        for (auto &[endpoint, section] : _sections)
            text += "\n" + section;
        // whole files only: Equalizer APO rereads the folder on every change
        {
            std::ofstream out(dir / L"MyPods.txt.tmp", std::ios::binary | std::ios::trunc);
            out << text;
        }
        if (!MoveFileExW((dir / L"MyPods.txt.tmp").c_str(), (dir / L"MyPods.txt").c_str(), MOVEFILE_REPLACE_EXISTING))
            Logger::Error("AudioEffects: could not write %s (%lu)", (dir / L"MyPods.txt").string().c_str(), GetLastError());
        std::ifstream in(dir / L"config.txt", std::ios::binary);
        std::string includes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        in.close();
        if (includes.find("Include: MyPods.txt") == std::string::npos)
            std::ofstream(dir / L"config.txt", std::ios::binary | std::ios::app) << (includes.empty() || includes.ends_with("\n") ? "" : "\n") << "Include: MyPods.txt\n";

        auto guid = Guid(sink);
        if (!config.IsNeutral() && !Installed(RENDER + L"\\" + Wide(guid) + L"\\FxProperties") && _registering.insert(guid).second)
            OfferToEqualizerApo(guid);
        return sink;
    }

    // Each pair has its own section, picked by its endpoint; one that goes away takes its section along with its
    // endpoint, and the next Apply for it rewrites it. Head tracking needs spatial audio, which Windows hasn't got.
    void AudioEffects::Stop() {}
    void AudioEffects::StopFor(const std::string &) {}
    void AudioEffects::StopLocked() {}
    void AudioEffects::SetYaw(double) {}
    // ponytail: the loudness compensation follows the volume at the last Apply; a per-endpoint IAudioEndpointVolumeCallback would make it live
    void AudioEffects::SetVolume(double) {}
}
