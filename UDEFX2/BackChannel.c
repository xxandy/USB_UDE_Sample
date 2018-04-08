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
    NTSTATUS status = STATUS_SUCCESS;
    PUDECX_USBCONTROLLER_CONTEXT pControllerContext;

    pControllerContext = GetUsbControllerContext(ctrdevice);

    status = WRQueueInit(ctrdevice, &(pControllerContext->missionRequest), FALSE);
    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE, "Unable to initialize mission completion, err= %!STATUS!", status);
        goto exit;
    }

    status = WRQueueInit(ctrdevice, &(pControllerContext->missionCompletion), TRUE);
    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE, "Unable to initialize mission request, err= %!STATUS!", status);
        goto exit;
    }

exit:
    return status;
}


VOID
BackChannelDestroy(
    _In_ WDFDEVICE ctrdevice
)
{
    PUDECX_USBCONTROLLER_CONTEXT pControllerContext;

    pControllerContext = GetUsbControllerContext(ctrdevice);

    WRQueueDestroy(&(pControllerContext->missionCompletion));
    WRQueueDestroy(&(pControllerContext->missionRequest));
}

VOID
BackChannelEvtRead(
    WDFQUEUE   Queue,
    WDFREQUEST Request,
    size_t     Length
)
{
    WDFDEVICE controller;
    PUDECX_USBCONTROLLER_CONTEXT pControllerContext;
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN bReady = FALSE;
    PVOID transferBuffer;
    SIZE_T transferBufferLength;
    SIZE_T completeBytes = 0;

    UNREFERENCED_PARAMETER(Length);

    controller = WdfIoQueueGetDevice(Queue); /// WdfIoQueueGetDevice
    pControllerContext = GetUsbControllerContext(controller);

    status = WdfRequestRetrieveOutputBuffer(Request, 1, &transferBuffer, &transferBufferLength);
    if (!NT_SUCCESS(status))
    {
        LogError(TRACE_DEVICE, "BCHAN WdfRequest read %p unable to retrieve buffer %!STATUS!",
            Request, status);
        goto exit;
    }

    // try to get us information about a request that may be waiting for this info
    status = WRQueuePullRead(
        &(pControllerContext->missionRequest),
        Request,
        transferBuffer,
        transferBufferLength,
        &bReady,
        &completeBytes);

    if (bReady)
    {
        WdfRequestCompleteWithInformation(Request, status, completeBytes);
        LogInfo(TRACE_DEVICE, "BCHAN Mission request %p filed with pre-existing data", Request);
    }
    else {
        LogInfo(TRACE_DEVICE, "BCHAN Mission request %p pended", Request);
    }


exit:
    return;

}

VOID
BackChannelEvtWrite(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t Length
)
{
    WDFDEVICE controller;
    WDFREQUEST matchingRead;
    PUDECX_USBCONTROLLER_CONTEXT pControllerContext;
    NTSTATUS status = STATUS_SUCCESS;
    PVOID transferBuffer;
    SIZE_T transferBufferLength;
    SIZE_T completeBytes = 0;

    UNREFERENCED_PARAMETER(Length);

    controller = WdfIoQueueGetDevice(Queue); /// WdfIoQueueGetDevice
    pControllerContext = GetUsbControllerContext(controller);

    status = WdfRequestRetrieveInputBuffer(Request, 1, &transferBuffer, &transferBufferLength);
    if (!NT_SUCCESS(status))
    {
        LogError(TRACE_DEVICE, "BCHAN WdfRequest write %p unable to retrieve buffer %!STATUS!",
            Request, status);
        goto exit;
    }

    // try to get us information about a request that may be waiting for this info
    status = WRQueuePushWrite(
        &(pControllerContext->missionCompletion),
        transferBuffer,
        transferBufferLength,
        &matchingRead);

    if (matchingRead != NULL)
    {
        PUCHAR rbuffer;
        ULONG rlen;

        // this is a USB read!
        status = UdecxUrbRetrieveBuffer(matchingRead, &rbuffer, &rlen);

        if (!NT_SUCCESS(status)) {

            LogError(TRACE_DEVICE, "BCHAN WdfRequest %p cannot retrieve mission completion buffer %!STATUS!",
                matchingRead, status);
        }
        else {
            completeBytes = MINLEN(rlen, transferBufferLength);
            memcpy(rbuffer, transferBuffer, completeBytes);
        }

        UdecxUrbSetBytesCompleted(matchingRead, (ULONG)completeBytes);
        UdecxUrbCompleteWithNtStatus(matchingRead, status);

        LogInfo(TRACE_DEVICE, "BCHAN Mission completion %p delivered with matching USB read %p", Request, matchingRead);
    }
    else {
        LogInfo(TRACE_DEVICE, "BCHAN Mission completion %p enqueued", Request);
    }

exit:
    // writes never pended, always completed
    WdfRequestCompleteWithInformation(Request, status, transferBufferLength);
    return;
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
