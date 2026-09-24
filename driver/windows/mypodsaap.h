// MyPods AAP driver: the interface between magicpodscore (user mode) and mypodsaap.sys.
// License: GPL-3.0
//
// Usage from user mode:
//   h = CreateFile(<an interface of GUID_DEVINTERFACE_MYPODS_AAP>, GENERIC_READ | GENERIC_WRITE, ...)
//   DeviceIoControl(h, IOCTL_MYPODS_AAP_OPEN, &address, sizeof(ULONGLONG), ...)   // paired device only
//   WriteFile(h, packet)  -> one AAP packet out
//   ReadFile(h, buffer)   -> one AAP packet in (blocks until one arrives)
//   CloseHandle(h)        -> closes the L2CAP channel
// The PSM is fixed to AAP's 0x1001; the driver opens nothing else.

#pragma once

#include <guiddef.h>

// Local Bluetooth service MyPods registers (BluetoothSetLocalServiceInfo); Windows creates the device
// node for it that mypodsaap.sys loads on. {d4c9e179-5e77-4fab-b53e-3ed22c52b087}
DEFINE_GUID(GUID_MYPODS_AAP_SERVICE,
    0xd4c9e179, 0x5e77, 0x4fab, 0xb5, 0x3e, 0x3e, 0xd2, 0x2c, 0x52, 0xb0, 0x87);

// Device interface the daemon opens. {d53e9c2d-b93d-45b0-882e-f593926892a8}
DEFINE_GUID(GUID_DEVINTERFACE_MYPODS_AAP,
    0xd53e9c2d, 0xb93d, 0x45b0, 0x88, 0x2e, 0xf5, 0x93, 0x92, 0x68, 0x92, 0xa8);

// Input: ULONGLONG Bluetooth address (0x0000AABBCCDDEEFF for AA:BB:CC:DD:EE:FF). No output.
#define IOCTL_MYPODS_AAP_OPEN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

// Largest packet in either direction (L2CAP default MTU)
#define MYPODS_AAP_MTU 672
