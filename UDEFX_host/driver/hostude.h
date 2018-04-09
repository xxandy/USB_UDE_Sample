/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    private.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#include <initguid.h>
#include <ntddk.h>
#include "usbdi.h"
#include "usbdlib.h"
#include "public.h"
#include "driverspecs.h"
#include <wdf.h>
#include <wdfusb.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "trace.h"



#ifndef _PRIVATE_H
#define _PRIVATE_H

#define POOL_TAG (ULONG) 'FRSO'

#define TEST_BOARD_TRANSFER_BUFFER_SIZE (64*1024)



//
// Data that the device reports via interrupts
//

// if we decide to queue incoming updates, this will be a limit
#define MAX_CACHED_INTR_UPDATES 100

typedef struct _DEVICE_INTR_STATE {
    DEVICE_INTR_FLAGS latestStatus;
    ULONG             numUnreadUpdates;
    WDFSPINLOCK       sync;
} DEVICE_INTR_STATE, *PDEVICE_INTR_STATE;


//
// A structure representing the instance information associated with
// this particular device.
//

typedef struct _DEVICE_CONTEXT {

    WDFUSBDEVICE                    UsbDevice;

    WDFUSBINTERFACE                 UsbInterface;

    WDFUSBPIPE                      BulkReadPipe;

    WDFUSBPIPE                      BulkWritePipe;

    WDFUSBPIPE                      InterruptPipe;

    WDFWAITLOCK                     ResetDeviceWaitLock;

    WDFQUEUE                        InterruptMsgQueue;

    DEVICE_INTR_STATE               InterruptStatus;

    ULONG                           UsbDeviceTraits;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

extern ULONG DebugLevel;

typedef
NTSTATUS
(*PFN_IO_GET_ACTIVITY_ID_IRP) (
    _In_     PIRP   Irp,
    _Out_    LPGUID Guid
    );

typedef
NTSTATUS
(*PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA) (
    _In_ PUNICODE_STRING    SymbolicLinkName,
    _In_ CONST DEVPROPKEY   *PropertyKey,
    _In_ LCID               Lcid,
    _In_ ULONG              Flags,
    _In_ DEVPROPTYPE        Type,
    _In_ ULONG              Size,
    _In_opt_ PVOID          Data
    );

// some O.S.'s have this, some don't - so store
// the API pointer conditionally
extern PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA g_pIoSetDeviceInterfacePropertyData;

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_OBJECT_CONTEXT_CLEANUP OsrFxEvtDriverContextCleanup;

EVT_WDF_DRIVER_DEVICE_ADD OsrFxEvtDeviceAdd;

EVT_WDF_DEVICE_PREPARE_HARDWARE OsrFxEvtDevicePrepareHardware;

EVT_WDF_IO_QUEUE_IO_READ OsrFxEvtIoRead;

EVT_WDF_IO_QUEUE_IO_WRITE OsrFxEvtIoWrite;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL OsrFxEvtIoDeviceControl;

EVT_WDF_REQUEST_COMPLETION_ROUTINE EvtRequestReadCompletionRoutine;

EVT_WDF_REQUEST_COMPLETION_ROUTINE EvtRequestWriteCompletionRoutine;

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetPipe(
    _In_ WDFUSBPIPE             Pipe
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetDevice(
    _In_ WDFDEVICE Device
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SelectInterfaces(
    _In_ WDFDEVICE Device
    );


VOID
OsrCompleteInterruptRequest(
    _In_ WDFREQUEST request,
    _In_ NTSTATUS  ReaderStatus,
    _In_ DEVICE_INTR_FLAGS NewDeviceFlags
    );

VOID
OsrUsbIoctlGetInterruptMessage(
    _In_ WDFDEVICE Device,
    _In_ NTSTATUS ReaderStatus,
    _In_ DEVICE_INTR_FLAGS NewDeviceFlags
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
OsrFxSetPowerPolicy(
        _In_ WDFDEVICE Device
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
OsrFxConfigContReaderForInterruptEndPoint(
    _In_ PDEVICE_CONTEXT DeviceContext
    );

EVT_WDF_USB_READER_COMPLETION_ROUTINE OsrFxEvtUsbInterruptPipeReadComplete;

EVT_WDF_USB_READERS_FAILED OsrFxEvtUsbInterruptReadersFailed;

EVT_WDF_IO_QUEUE_IO_STOP OsrFxEvtIoStop;

EVT_WDF_DEVICE_D0_ENTRY OsrFxEvtDeviceD0Entry;

EVT_WDF_DEVICE_D0_EXIT OsrFxEvtDeviceD0Exit;

EVT_WDF_DEVICE_SELF_MANAGED_IO_FLUSH OsrFxEvtDeviceSelfManagedIoFlush;




_IRQL_requires_(PASSIVE_LEVEL)
PCHAR
DbgDevicePowerString(
    _In_ WDF_POWER_DEVICE_STATE Type
    );


_IRQL_requires_(PASSIVE_LEVEL)
USBD_STATUS
OsrFxValidateConfigurationDescriptor(  
    _In_reads_bytes_(BufferLength) PUSB_CONFIGURATION_DESCRIPTOR ConfigDesc,
    _In_ ULONG BufferLength,
    _Inout_ PUCHAR *Offset
    );



#endif


