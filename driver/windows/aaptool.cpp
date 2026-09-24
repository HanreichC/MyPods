// MyPods AAP driver helper (runs in the test VM, as administrator for register/unregister)
// License: GPL-3.0
//
//   aaptool register            registers MyPods' local Bluetooth service; Windows then creates the
//                               device node mypodsaap.sys loads on
//   aaptool unregister          removes it again
//   aaptool test AA:BB:CC:DD:EE:FF
//                               the D2 check: opens AAP to the paired AirPods through the driver, sends the
//                               handshake and prints what comes back for 5 s. Exit code 0 = a reply arrived.

#include <windows.h>
#include <initguid.h>
#include <bluetoothapis.h>
#include <cfgmgr32.h>
#include <cstdio>
#include <string>
#include <vector>
#include "mypodsaap.h"

static bool EnablePrivilege(const wchar_t *name)
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;
    TOKEN_PRIVILEGES tp{1};
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueW(nullptr, name, &tp.Privileges[0].Luid) &&
              AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr) && GetLastError() == ERROR_SUCCESS;
    CloseHandle(token);
    return ok;
}

static int SetService(bool enabled)
{
    // BluetoothSetLocalServiceInfo needs SeLoadDriverPrivilege: it makes Windows load a driver
    if (!EnablePrivilege(L"SeLoadDriverPrivilege")) {
        std::printf("needs to run as administrator\n");
        return 1;
    }
    BLUETOOTH_LOCAL_SERVICE_INFO info{};
    info.Enabled = enabled;
    wcscpy_s(info.szName, L"MyPods AAP");
    wcscpy_s(info.szDeviceString, L"MyPods AirPods (AAP)");
    DWORD error = BluetoothSetLocalServiceInfo(nullptr, &GUID_MYPODS_AAP_SERVICE, 0, &info);
    std::printf("%s: %lu\n", enabled ? "register" : "unregister", error);
    return error == ERROR_SUCCESS ? 0 : 1;
}

static std::wstring FindInterface()
{
    ULONG size = 0;
    if (CM_Get_Device_Interface_List_SizeW(&size, const_cast<GUID *>(&GUID_DEVINTERFACE_MYPODS_AAP), nullptr,
                                           CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS || size <= 1)
        return {};
    std::vector<wchar_t> list(size);
    if (CM_Get_Device_Interface_ListW(const_cast<GUID *>(&GUID_DEVINTERFACE_MYPODS_AAP), nullptr, list.data(), size,
                                      CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS)
        return {};
    return list.data(); // first one
}

static int Test(const char *text)
{
    unsigned int b[6];
    if (sscanf_s(text, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
        std::printf("address like AA:BB:CC:DD:EE:FF expected\n");
        return 2;
    }
    ULONGLONG address = 0;
    for (unsigned int byte : b)
        address = (address << 8) | (byte & 0xFF);

    std::wstring path = FindInterface();
    if (path.empty()) {
        std::printf("driver interface not found (driver installed? service registered?)\n");
        return 1;
    }
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        std::printf("open failed: %lu\n", GetLastError());
        return 1;
    }
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD bytes = 0;
    if (!DeviceIoControl(h, IOCTL_MYPODS_AAP_OPEN, &address, sizeof(address), nullptr, 0, nullptr, &ov) &&
        (GetLastError() != ERROR_IO_PENDING || !GetOverlappedResult(h, &ov, &bytes, TRUE))) {
        std::printf("L2CAP open failed: %lu\n", GetLastError());
        return 1;
    }
    std::printf("L2CAP channel to %s open\n", text);

    // AAP handshake (core/src/sdk/aap/setters/AapInit.cpp)
    const unsigned char handshake[] = {0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ResetEvent(ov.hEvent);
    if (!WriteFile(h, handshake, sizeof(handshake), nullptr, &ov) &&
        (GetLastError() != ERROR_IO_PENDING || !GetOverlappedResult(h, &ov, &bytes, TRUE))) {
        std::printf("write failed: %lu\n", GetLastError());
        return 1;
    }

    int packets = 0;
    const ULONGLONG until = GetTickCount64() + 5000;
    unsigned char buffer[MYPODS_AAP_MTU];
    while (GetTickCount64() < until) {
        ResetEvent(ov.hEvent);
        if (!ReadFile(h, buffer, sizeof(buffer), nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) {
            std::printf("read failed: %lu\n", GetLastError());
            break;
        }
        if (WaitForSingleObject(ov.hEvent, static_cast<DWORD>(until - GetTickCount64())) != WAIT_OBJECT_0) {
            CancelIoEx(h, &ov);
            GetOverlappedResult(h, &ov, &bytes, TRUE);
            break;
        }
        if (!GetOverlappedResult(h, &ov, &bytes, FALSE)) {
            std::printf("read failed: %lu\n", GetLastError());
            break;
        }
        packets++;
        std::printf("r:");
        for (DWORD i = 0; i < bytes; i++)
            std::printf("%02x", buffer[i]);
        std::printf("\n");
    }
    CloseHandle(h);
    std::printf("%d packet(s) received\n", packets);
    return packets > 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    std::string command = argc > 1 ? argv[1] : "";
    if (command == "register")
        return SetService(true);
    if (command == "unregister")
        return SetService(false);
    if (command == "test" && argc > 2)
        return Test(argv[2]);
    std::printf("usage: aaptool register | unregister | test AA:BB:CC:DD:EE:FF\n");
    return 2;
}
