/*++

Module Name:

BackChannel.c - Offers interface for talking to the driver "offline", 
                for texst pourposes

Abstract:

    Implementation of interfaces declared in BackChannel.h.

    Notice that we are taking a shortcut here, by not explicitly creating
    a WDF object for the back-channel, and using the controller context
    as the back-channel context.
    This of course doesn't work if we want the back-channel to be per-device.
    But that's okay for the purposes of the demo. No point complicating
    the back-channel for this semantic issue, when it will likely
    be removed completely by any real product.

Environment:

Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "usbdevice.h"
#include "Misc.h"
#include "USBCom.h"
#include "BackChannel.h"

#include <ntstrsafe.h>
#include "BackChannel.tmh"



NTSTATUS
BackChannelInit(
    _In_ WDFDEVICE ctrdevice
)
{
    UNREFERENCED_PARAMETER(ctrdevice);
    return STATUS_SUCCESS;
}


VOID
BackChannelDestroy(
    _In_ WDFDEVICE ctrdevice
)
{
    UNREFERENCED_PARAMETER(ctrdevice);
}


BOOLEAN
BackChannelIoctl(
    _In_ ULONG IoControlCode,
    _In_ WDFDEVICE ctrdevice,
    _In_ WDFREQUEST Request
)
{
    BOOLEAN handled = FALSE;
    NTSTATUS status;
    PDEVICE_INTR_FLAGS pflags = 0;
    size_t pblen;
    PUDECX_USBCONTROLLER_CONTEXT pControllerContext;

    pControllerContext = GetUsbControllerContext(ctrdevice);


    switch (IoControlCode)
    {
    case IOCTL_UDEFX2_GENERATE_INTERRUPT:
        status = WdfRequestRetrieveInputBuffer(Request,
            sizeof(DEVICE_INTR_FLAGS),
            &pflags,
            &pblen);// BufferLength

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "%!FUNC! Unable to retrieve input buffer");
        }
        else if (pblen == sizeof(DEVICE_INTR_FLAGS) && (pflags != NULL)) {
            DEVICE_INTR_FLAGS flags;
            memcpy(&flags, pflags, sizeof(DEVICE_INTR_FLAGS));
            TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "%!FUNC! Will generate interrupt");
            status = Io_RaiseInterrupt(pControllerContext->ChildDevice, flags);

        }
        else {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "%!FUNC! Invalid buffer size");
            status = STATUS_INVALID_PARAMETER;
        }
        WdfRequestComplete(Request, status);
        handled = TRUE;
        break;
    }

    return handled;
}
