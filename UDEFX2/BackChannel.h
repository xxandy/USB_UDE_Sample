#pragma once
/*++

Module Name:

BackChannel.h


Abstract:

The interface exposed here is used by the test application to feed information
to the virtual USB device, so it knows what/when to answer to queries from the hist.
All the code in here is for testing purpose, and it is NOT related to UDE directly.


Environment:

Kernel-mode Driver Framework

--*/

#pragma once

#include "public.h"
#include "Misc.h"

EXTERN_C_START

// magic to re-use the controller context without creating
// explict dependencies on the controller where we only want to use
// the back-channel
typedef UDECX_USBCONTROLLER_CONTEXT UDECX_BACKCHANNEL_CONTEXT, *PUDECX_BACKCHANNEL_CONTEXT;
#define GetBackChannelContext GetUsbControllerContext

NTSTATUS
BackChannelInit(
    _In_ WDFDEVICE ctrdevice
);

VOID
BackChannelDestroy(
    _In_ WDFDEVICE ctrdevice
);

BOOLEAN
BackChannelIoctl(
    _In_ ULONG IoControlCode,
    _In_ WDFDEVICE ctrdevice,
    _In_ WDFREQUEST Request
);



EVT_WDF_IO_QUEUE_IO_READ                        BackChannelEvtRead;
EVT_WDF_IO_QUEUE_IO_WRITE                       BackChannelEvtWrite;

EXTERN_C_END

