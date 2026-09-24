// MyPods AAP driver: gives user mode an L2CAP channel to AAP (PSM 0x1001) of paired AirPods.
// License: GPL-3.0
//
// KMDF Bluetooth profile driver. It loads on the device node Windows creates for MyPods' local service
// (see mypodsaap.h), takes the profile interface from the Bluetooth stack there and submits BRBs:
// BRB_L2CA_OPEN_CHANNEL on IOCTL_MYPODS_AAP_OPEN, BRB_L2CA_ACL_TRANSFER for ReadFile/WriteFile,
// BRB_L2CA_CLOSE_CHANNEL when the handle closes. One channel per handle.
//
// Trust boundary: any interactive user can open the device (the daemon runs as the user), so
//  * the PSM is fixed to AAP's; the driver is no general L2CAP gateway,
//  * channels need an encrypted link and never trigger pairing (CF_LINK_ENCRYPTED | CF_LINK_SUPPRESS_PIN),
//    which limits them to devices already paired with this computer,
//  * every length from user mode is checked against the MTU.

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <bthdef.h>
#include <bthguid.h>
#include <bthioctl.h>
#include <bthddi.h>
#include "mypodsaap.h"

#define AAP_PSM 0x1001
#define OPEN_TIMEOUT_MS 20000 // paging an idle device plus L2CAP config can take several seconds

typedef struct _DEVICE_CONTEXT {
    BTH_PROFILE_DRIVER_INTERFACE Profile;
    WDFIOTARGET Target;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

typedef struct _FILE_CONTEXT {
    BTH_ADDR Address;
    L2CAP_CHANNEL_HANDLE Channel;
    LONG Claimed;      // an open was started on this handle (only one per handle)
    LONG Open;         // Channel is valid
    LONG Disconnected; // the remote side closed the channel
} FILE_CONTEXT, *PFILE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILE_CONTEXT, FileGetContext)

// Every request carries room for the BRB it is forwarded with, freed together with the request
typedef struct _REQUEST_CONTEXT {
    BRB Brb;
} REQUEST_CONTEXT, *PREQUEST_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(REQUEST_CONTEXT, RequestGetContext)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDeviceCleanup;
EVT_WDF_FILE_CLEANUP EvtFileCleanup;
EVT_WDF_IO_QUEUE_IO_READ EvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE EvtIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;
EVT_WDF_REQUEST_COMPLETION_ROUTINE TransferComplete;

static NTSTATUS SubmitBrbSynchronously(PDEVICE_CONTEXT ctx, PBRB brb, ULONG timeoutMs)
{
    WDF_MEMORY_DESCRIPTOR desc;
    WDF_REQUEST_SEND_OPTIONS options;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&desc, brb, sizeof(*brb));
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, 0);
    if (timeoutMs != 0)
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options, WDF_REL_TIMEOUT_IN_MS(timeoutMs));
    // Waits for completion even on timeout (the request is cancelled first), so a stack BRB is safe
    return WdfIoTargetSendInternalIoctlOthersSynchronously(ctx->Target, NULL, IOCTL_INTERNAL_BTH_SUBMIT_BRB,
                                                            &desc, NULL, NULL, &options, NULL);
}

static void Indication(PVOID context, INDICATION_CODE indication, PINDICATION_PARAMETERS parameters)
{
    UNREFERENCED_PARAMETER(parameters);
    // Runs until the channel is closed in EvtFileCleanup, so the file context is still alive here.
    // Pending transfers are completed with an error by the stack; the handle close releases the channel.
    if (indication == IndicationRemoteDisconnect && context)
        InterlockedExchange(&((PFILE_CONTEXT)context)->Disconnected, 1);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);
    return WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS EvtDeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT init)
{
    UNREFERENCED_PARAMETER(driver);
    NTSTATUS status;
    WDFDEVICE device;
    WDFQUEUE queue;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_IO_QUEUE_CONFIG queueConfig;

    WdfDeviceInitSetIoType(init, WdfDeviceIoBuffered);

    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, WDF_NO_EVENT_CALLBACK, WDF_NO_EVENT_CALLBACK, EvtFileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FILE_CONTEXT);
    WdfDeviceInitSetFileObjectConfig(init, &fileConfig, &attributes);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, REQUEST_CONTEXT);
    WdfDeviceInitSetRequestAttributes(init, &attributes);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    attributes.ExecutionLevel = WdfExecutionLevelPassive; // synchronous BRBs in the I/O callbacks
    attributes.EvtCleanupCallback = EvtDeviceCleanup;
    status = WdfDeviceCreate(&init, &attributes, &device);
    if (!NT_SUCCESS(status))
        return status;

    PDEVICE_CONTEXT ctx = DeviceGetContext(device);
    ctx->Target = WdfDeviceGetIoTarget(device);
    status = WdfFdoQueryForInterface(device, &GUID_BTHDDI_PROFILE_DRIVER_INTERFACE, (PINTERFACE)&ctx->Profile,
                                     sizeof(ctx->Profile), BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI, NULL);
    if (!NT_SUCCESS(status))
        return status;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoRead = EvtIoRead;
    queueConfig.EvtIoWrite = EvtIoWrite;
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;
    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status))
        return status;

    return WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_MYPODS_AAP, NULL);
}

VOID EvtDeviceCleanup(WDFOBJECT object)
{
    PDEVICE_CONTEXT ctx = DeviceGetContext((WDFDEVICE)object);
    if (ctx->Profile.Interface.InterfaceDereference)
        ctx->Profile.Interface.InterfaceDereference(ctx->Profile.Interface.Context);
}

static NTSTATUS OpenChannel(WDFDEVICE device, PFILE_CONTEXT file, BTH_ADDR address)
{
    PDEVICE_CONTEXT ctx = DeviceGetContext(device);
    BRB brb = {0};
    struct _BRB_L2CA_OPEN_CHANNEL *open = &brb.BrbL2caOpenChannel;

    ctx->Profile.BthInitializeBrb(&brb, BRB_L2CA_OPEN_CHANNEL);
    open->BtAddress = address;
    open->Psm = AAP_PSM;
    open->ChannelFlags = CF_ROLE_EITHER | CF_LINK_ENCRYPTED | CF_LINK_SUPPRESS_PIN;
    open->ConfigOut.Flags = 0;
    open->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
    open->ConfigOut.Mtu.Preferred = MYPODS_AAP_MTU;
    open->ConfigOut.Mtu.Max = MYPODS_AAP_MTU;
    open->ConfigIn.Flags = 0;
    open->ConfigIn.Mtu.Min = L2CAP_MIN_MTU;
    open->ConfigIn.Mtu.Preferred = MYPODS_AAP_MTU;
    open->ConfigIn.Mtu.Max = MYPODS_AAP_MTU;
    open->IncomingQueueDepth = 50; // AAP bursts (battery, noise control, ...) right after the handshake
    open->CallbackFlags = CALLBACK_DISCONNECT;
    open->Callback = Indication;
    open->CallbackContext = file;
    open->ReferenceObject = WdfDeviceWdmGetDeviceObject(device); // keeps the driver loaded while callbacks can come

    NTSTATUS status = SubmitBrbSynchronously(ctx, &brb, OPEN_TIMEOUT_MS);
    if (NT_SUCCESS(status)) {
        file->Address = address;
        file->Channel = open->ChannelHandle;
        InterlockedExchange(&file->Open, 1);
    }
    return status;
}

VOID EvtIoDeviceControl(WDFQUEUE queue, WDFREQUEST request, size_t outputLength, size_t inputLength, ULONG ioControlCode)
{
    UNREFERENCED_PARAMETER(outputLength);
    UNREFERENCED_PARAMETER(inputLength);
    NTSTATUS status;
    ULONGLONG *address;

    if (ioControlCode != IOCTL_MYPODS_AAP_OPEN) {
        WdfRequestComplete(request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }
    status = WdfRequestRetrieveInputBuffer(request, sizeof(ULONGLONG), (PVOID *)&address, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(request, status);
        return;
    }
    // A Bluetooth address has 48 bits
    if (*address == 0 || (*address >> 48) != 0) {
        WdfRequestComplete(request, STATUS_INVALID_PARAMETER);
        return;
    }
    PFILE_CONTEXT file = FileGetContext(WdfRequestGetFileObject(request));
    if (InterlockedCompareExchange(&file->Claimed, 1, 0) != 0) {
        WdfRequestComplete(request, STATUS_DEVICE_BUSY);
        return;
    }
    status = OpenChannel(WdfIoQueueGetDevice(queue), file, *address);
    if (!NT_SUCCESS(status))
        InterlockedExchange(&file->Claimed, 0); // may be retried on the same handle
    WdfRequestComplete(request, status);
}

VOID TransferComplete(WDFREQUEST request, WDFIOTARGET target, PWDF_REQUEST_COMPLETION_PARAMS params, WDFCONTEXT context)
{
    UNREFERENCED_PARAMETER(target);
    UNREFERENCED_PARAMETER(context);
    struct _BRB_L2CA_ACL_TRANSFER *transfer = &RequestGetContext(request)->Brb.BrbL2caAclTransfer;
    NTSTATUS status = params->IoStatus.Status;
    ULONG_PTR bytes = 0;
    if (NT_SUCCESS(status) && transfer->RemainingBufferSize <= transfer->BufferSize)
        bytes = transfer->BufferSize - transfer->RemainingBufferSize;
    WdfRequestCompleteWithInformation(request, status, bytes);
}

// Forwards a ReadFile/WriteFile to the stack as one ACL transfer on the handle's channel
static VOID Transfer(WDFQUEUE queue, WDFREQUEST request, size_t length, BOOLEAN in)
{
    PDEVICE_CONTEXT ctx = DeviceGetContext(WdfIoQueueGetDevice(queue));
    PFILE_CONTEXT file = FileGetContext(WdfRequestGetFileObject(request));
    PREQUEST_CONTEXT rc = RequestGetContext(request);
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFMEMORY memory;
    PVOID buffer;
    NTSTATUS status;

    if (!ReadAcquire(&file->Open) || ReadAcquire(&file->Disconnected)) {
        WdfRequestComplete(request, STATUS_DEVICE_NOT_CONNECTED);
        return;
    }
    // One packet per request; a read shorter than the MTU still takes a whole short packet (ACL_SHORT_TRANSFER_OK)
    if (length == 0 || (!in && length > MYPODS_AAP_MTU)) {
        WdfRequestComplete(request, STATUS_INVALID_BUFFER_SIZE);
        return;
    }
    if (length > MYPODS_AAP_MTU)
        length = MYPODS_AAP_MTU;
    status = in ? WdfRequestRetrieveOutputBuffer(request, length, &buffer, NULL)
                : WdfRequestRetrieveInputBuffer(request, length, &buffer, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(request, status);
        return;
    }

    ctx->Profile.BthInitializeBrb(&rc->Brb, BRB_L2CA_ACL_TRANSFER);
    struct _BRB_L2CA_ACL_TRANSFER *transfer = &rc->Brb.BrbL2caAclTransfer;
    transfer->BtAddress = file->Address;
    transfer->ChannelHandle = file->Channel;
    transfer->TransferFlags = in ? (ACL_TRANSFER_DIRECTION_IN | ACL_SHORT_TRANSFER_OK) : ACL_TRANSFER_DIRECTION_OUT;
    transfer->BufferSize = (ULONG)length;
    transfer->Buffer = buffer;
    transfer->BufferMDL = NULL;
    transfer->Timeout = 0; // a read waits for the next packet; closing the handle cancels it

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = request;
    status = WdfMemoryCreatePreallocated(&attributes, &rc->Brb, sizeof(rc->Brb), &memory);
    if (NT_SUCCESS(status))
        status = WdfIoTargetFormatRequestForInternalIoctlOthers(ctx->Target, request, IOCTL_INTERNAL_BTH_SUBMIT_BRB,
                                                                 memory, NULL, NULL, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(request, status);
        return;
    }
    WdfRequestSetCompletionRoutine(request, TransferComplete, NULL);
    if (!WdfRequestSend(request, ctx->Target, WDF_NO_SEND_OPTIONS))
        WdfRequestComplete(request, WdfRequestGetStatus(request));
}

VOID EvtIoRead(WDFQUEUE queue, WDFREQUEST request, size_t length)
{
    Transfer(queue, request, length, TRUE);
}

VOID EvtIoWrite(WDFQUEUE queue, WDFREQUEST request, size_t length)
{
    Transfer(queue, request, length, FALSE);
}

VOID EvtFileCleanup(WDFFILEOBJECT fileObject)
{
    PFILE_CONTEXT file = FileGetContext(fileObject);
    if (!InterlockedExchange(&file->Open, 0))
        return;
    PDEVICE_CONTEXT ctx = DeviceGetContext(WdfFileObjectGetDevice(fileObject));
    BRB brb = {0};
    ctx->Profile.BthInitializeBrb(&brb, BRB_L2CA_CLOSE_CHANNEL);
    brb.BrbL2caCloseChannel.BtAddress = file->Address;
    brb.BrbL2caCloseChannel.ChannelHandle = file->Channel;
    // Also completes the handle's pending reads; after it returns no Indication for this file comes
    SubmitBrbSynchronously(ctx, &brb, 0);
}
