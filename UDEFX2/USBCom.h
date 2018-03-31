#pragma once


/*++
Copyright (c) Microsoft Corporation

Module Name:

USBCom.h

Abstract:
     This module contains the core USB-side transfer logic
     (business logic, just a bit above hardware interfaces)

--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include "trace.h"





typedef struct _IO_CONTEXT {
    WDFQUEUE ControlQueue;
    WDFQUEUE BulkOutQueue;
    WDFSPINLOCK InProgressLock;
} IO_CONTEXT, *PIO_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(IO_CONTEXT, WdfDeviceGetIoContext);




EXTERN_C_START


NTSTATUS
Io_AllocateContext(
    _In_ WDFDEVICE Object
);




NTSTATUS
Io_RetrieveEpQueue(
    _In_ WDFDEVICE  Device,
    _In_ UCHAR      EpAddr,
    _Out_ WDFQUEUE * Queue
);




EXTERN_C_END
