/*++
Copyright (c) Microsoft Corporation

Module Name:

USBCom.c

Abstract:
    Implementation of functions defined in USBCom.h

--*/

#include "Misc.h"
#include "Driver.h"
#include "Device.h"
#include "usbdevice.h"
#include "USBCom.h"
#include "BackChannel.h"
#include "ucx/1.4/ucxobjects.h"
#include "USBCom.tmh"



typedef struct _ENDPOINTQUEUE_CONTEXT {
    UDECXUSBDEVICE usbDeviceObj;
    WDFDEVICE      backChannelDevice;
} ENDPOINTQUEUE_CONTEXT, *PENDPOINTQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(ENDPOINTQUEUE_CONTEXT, GetEndpointQueueContext);


NTSTATUS
Io_AllocateContext(
    _In_ UDECXUSBDEVICE Object
)
/*++

Routine Description:

Object context allocation helper

Arguments:

Object - WDF object upon which to allocate the new context

Return value:

NTSTATUS. Could fail on allocation failure or the same context type already exists on the object

--*/
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, IO_CONTEXT);

    NTSTATUS status = WdfObjectAllocateContext(Object, &attributes, NULL);

    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE, "Unable to allocate new context for WDF object %p", Object);
        goto exit;
    }

exit:

    return status;
}






static VOID
IoEvtControlUrb(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    //NT_VERIFY(IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB);

    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        // These are on the control pipe.
        // We don't do anything special with these requests,
        // but this is where we would add processing for vendor-specific commands.
        WDF_USB_CONTROL_SETUP_PACKET setupPacket;
        NTSTATUS status = UdecxUrbRetrieveControlSetupPacket(Request, &setupPacket);

        if (!NT_SUCCESS(status))
        {
            LogError(TRACE_DEVICE, "WdfRequest %p is not a control URB? UdecxUrbRetrieveControlSetupPacket %!STATUS!",
                Request, status);
            UdecxUrbCompleteWithNtStatus(Request, status);
            goto exit;
        }


        LogInfo(TRACE_DEVICE, "v44 control CODE %x, [type=%x dir=%x recip=%x] req=%x [wv = %x wi = %x wlen = %x]",
            IoControlCode,
            (int)(setupPacket.Packet.bm.Request.Type),
            (int)(setupPacket.Packet.bm.Request.Dir),
            (int)(setupPacket.Packet.bm.Request.Recipient),
            (int)(setupPacket.Packet.bRequest),
            (int)(setupPacket.Packet.wValue.Value),
            (int)(setupPacket.Packet.wIndex.Value),
            (int)(setupPacket.Packet.wLength)
        );




        UdecxUrbCompleteWithNtStatus(Request, STATUS_SUCCESS);
    }
    else
    {
        LogError(TRACE_DEVICE, "control NO submit code is %x", IoControlCode);
    }
exit:
    return;
}


static int _Test_rebound = 0;


static VOID
IoEvtBulkOutUrb(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG transferBufferLength = 0;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    PENDPOINTQUEUE_CONTEXT pEpQContext = GetEndpointQueueContext(Queue);
    WDFDEVICE backchannel = pEpQContext->backChannelDevice;
    PUDECX_BACKCHANNEL_CONTEXT pBackChannelContext = GetBackChannelContext(backchannel);

    if (IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        LogError(TRACE_DEVICE, "WdfRequest BOUT %p Incorrect IOCTL %x, %!STATUS!",
            Request, IoControlCode, status);
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    PUCHAR transferBuffer;
    status = UdecxUrbRetrieveBuffer(Request, &transferBuffer, &transferBufferLength);
    if (!NT_SUCCESS(status))
    {
        LogError(TRACE_DEVICE, "WdfRequest BOUT %p unable to retrieve buffer %!STATUS!",
            Request, status);
        goto exit;
    }

    // try to get us information about a request that may be waiting for this info
    WDFREQUEST matchingRead;
    status = WRQueuePushWrite(
        &(pBackChannelContext->missionRequest),
        transferBuffer,
        transferBufferLength,
        &matchingRead);

    if (matchingRead != NULL)
    {
        PVOID rbuffer;
        SIZE_T rlen;

        // this is a back-channel read, not a USB read!
        status = WdfRequestRetrieveOutputBuffer(matchingRead, 1, &rbuffer, &rlen);
        SIZE_T completeBytes = 0;

        if (!NT_SUCCESS(status))  {
            LogError(TRACE_DEVICE, "WdfRequest %p cannot retrieve mission completion buffer %!STATUS!",
                matchingRead, status);
        } else  {
            completeBytes = min(rlen, transferBufferLength);
            memcpy(rbuffer, transferBuffer, completeBytes);
        }

        WdfRequestCompleteWithInformation(matchingRead, status, completeBytes);

        LogInfo(TRACE_DEVICE, "Mission request %p completed with matching read %p", Request, matchingRead);
    } else {
        LogInfo(TRACE_DEVICE, "Mission request %p enqueued", Request);
    }

exit:
    // writes never pended, always completed
    UdecxUrbSetBytesCompleted(Request, transferBufferLength);
    UdecxUrbCompleteWithNtStatus(Request, status);
    return;
}





static VOID
IoEvtBulkInUrb(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    PENDPOINTQUEUE_CONTEXT pEpQContext = GetEndpointQueueContext(Queue);
    WDFDEVICE backchannel = pEpQContext->backChannelDevice;
    PUDECX_BACKCHANNEL_CONTEXT pBackChannelContext = GetBackChannelContext(backchannel);

    if (IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        LogError(TRACE_DEVICE, "WdfRequest BIN %p Incorrect IOCTL %x, %!STATUS!",
            Request, IoControlCode, status);
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    PUCHAR transferBuffer;
    ULONG transferBufferLength;
    status = UdecxUrbRetrieveBuffer(Request, &transferBuffer, &transferBufferLength);
    if (!NT_SUCCESS(status))
    {
        LogError(TRACE_DEVICE, "WdfRequest BIN %p unable to retrieve buffer %!STATUS!",
            Request, status);
        goto exit;
    }

    // try to get us information about a request that may be waiting for this info
    SIZE_T completeBytes = 0;
    BOOLEAN bReady = FALSE;
    status = WRQueuePullRead(
        &(pBackChannelContext->missionCompletion),
        Request,
        transferBuffer,
        transferBufferLength,
        &bReady,
        &completeBytes);

    if (bReady)
    {
        UdecxUrbSetBytesCompleted(Request, (ULONG)completeBytes);
        UdecxUrbCompleteWithNtStatus(Request, status);
        LogInfo(TRACE_DEVICE, "Mission response %p completed with pre-existing data", Request);
    } else {
        LogInfo(TRACE_DEVICE, "Mission response %p pended", Request);
    }


exit:
    return;
}


static VOID
IoEvtCancelInterruptInUrb(
    IN WDFQUEUE Queue,
    IN WDFREQUEST  Request
)
{
    UNREFERENCED_PARAMETER(Queue);
    LogInfo(TRACE_DEVICE, "Canceling request %p", Request);
    UdecxUrbCompleteWithNtStatus(Request, STATUS_CANCELLED);
}


static VOID
IoCompletePendingRequest(
    _In_ WDFREQUEST request,
    _In_ DEVICE_INTR_FLAGS LatestStatus)
{
    PUCHAR transferBuffer;
    ULONG transferBufferLength;

    NTSTATUS status = UdecxUrbRetrieveBuffer(request, &transferBuffer, &transferBufferLength);
    if (!NT_SUCCESS(status))
    {
        LogError(TRACE_DEVICE, "WdfRequest  %p unable to retrieve buffer %!STATUS!",
            request, status);
        goto exit;
    }

    if (transferBufferLength != sizeof(LatestStatus))
    {
        LogError(TRACE_DEVICE, "Error: req %p Invalid interrupt buffer size, %d",
            request, transferBufferLength);
        status = STATUS_INVALID_BLOCK_LENGTH;
        goto exit;
    }

    memcpy(transferBuffer, &LatestStatus, sizeof(LatestStatus) );

    LogInfo(TRACE_DEVICE, "INTR completed req=%p, data=%x",
        request,
        LatestStatus
    );

    UdecxUrbSetBytesCompleted(request, transferBufferLength);

exit:
    UdecxUrbCompleteWithNtStatus(request, status);
    return;
}



NTSTATUS
Io_RaiseInterrupt(
    _In_ UDECXUSBDEVICE    Device,
    _In_ DEVICE_INTR_FLAGS LatestStatus )
{
    PIO_CONTEXT pIoContext = WdfDeviceGetIoContext(Device);

    WDFREQUEST request;
    NTSTATUS status = WdfIoQueueRetrieveNextRequest( pIoContext->IntrDeferredQueue, &request);

    // no items in the queue?  it is safe to assume the device is sleeping
    if (!NT_SUCCESS(status))    {
        LogInfo(TRACE_DEVICE, "Save update and wake device as queue status was %!STATUS!", status);

        WdfSpinLockAcquire(pIoContext->IntrState.sync);
        pIoContext->IntrState.latestStatus = LatestStatus;
        if ((pIoContext->IntrState.numUnreadUpdates) < INTR_STATE_MAX_CACHED_UPDATES)
        {
            ++(pIoContext->IntrState.numUnreadUpdates);
        }
        WdfSpinLockRelease(pIoContext->IntrState.sync);

        UdecxUsbDeviceSignalWake(Device);
        status = STATUS_SUCCESS;
    } else {
        IoCompletePendingRequest(request, LatestStatus);
    }

    return status;
}



static VOID
IoEvtInterruptInUrb(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;


    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    PENDPOINTQUEUE_CONTEXT pEpQContext = GetEndpointQueueContext(Queue);

    UDECXUSBDEVICE tgtDevice = pEpQContext->usbDeviceObj;

    PIO_CONTEXT pIoContext = WdfDeviceGetIoContext(tgtDevice);


    if (IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)   {
        LogError(TRACE_DEVICE, "Invalid Interrupt/IN out IOCTL code %x", IoControlCode);
        status = STATUS_ACCESS_DENIED;
        goto exit;
    }

    BOOLEAN bHasData = FALSE;
    DEVICE_INTR_FLAGS LatestStatus = 0;

    // gate cached data we may have and clear it
    WdfSpinLockAcquire(pIoContext->IntrState.sync);
    if( pIoContext->IntrState.numUnreadUpdates > 0)
    {
        bHasData = TRUE;
        LatestStatus = pIoContext->IntrState.latestStatus;
    }
    pIoContext->IntrState.latestStatus = 0;
    pIoContext->IntrState.numUnreadUpdates = 0;
    WdfSpinLockRelease(pIoContext->IntrState.sync);


    if (bHasData)  {

        IoCompletePendingRequest(Request, LatestStatus);

    } else {

        status = WdfRequestForwardToIoQueue(Request, pIoContext->IntrDeferredQueue);
        if (NT_SUCCESS(status)) {
            LogInfo(TRACE_DEVICE, "Request %p forwarded for later", Request);
        } else {
            LogError(TRACE_DEVICE, "ERROR: Unable to forward Request %p error %!STATUS!", Request, status);
            UdecxUrbCompleteWithNtStatus(Request, status);
        }

    }

exit:
    return;
}


static NTSTATUS
Io_CreateDeferredIntrQueue(
    _In_ WDFDEVICE   ControllerDevice,
    _In_ PIO_CONTEXT pIoContext )
{

    pIoContext->IntrState.latestStatus = 0;
    pIoContext->IntrState.numUnreadUpdates = 0;

    //
    // Register a manual I/O queue for handling Interrupt Message Read Requests.
    // This queue will be used for storing Requests that need to wait for an
    // interrupt to occur before they can be completed.
    //
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

    // when a request gets canceled, this is where we want to do the completion
    queueConfig.EvtIoCanceledOnQueue = IoEvtCancelInterruptInUrb;

    //
    // We shouldn't have to power-manage this queue, as we will manually 
    // purge it and de-queue from it whenever we get power indications.
    //
    queueConfig.PowerManaged = WdfFalse;

    NTSTATUS status = WdfIoQueueCreate(ControllerDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &(pIoContext->IntrDeferredQueue)
    );

    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE,
            "WdfIoQueueCreate failed 0x%x\n", status);
        goto Error;
    }


    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES,
        &(pIoContext->IntrState.sync));
    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE,
            "WdfSpinLockCreate failed  %!STATUS!\n", status);
        goto Error;
    }

Error:
    return status;
}


NTSTATUS
Io_DeviceSlept(
    _In_ UDECXUSBDEVICE  Device
)
{
    PIO_CONTEXT pIoContext = WdfDeviceGetIoContext(Device);

    // thi will result in all current requests being canceled
    LogInfo(TRACE_DEVICE, "About to purge deferred request queue" );
    WdfIoQueuePurge(pIoContext->IntrDeferredQueue, NULL, NULL);

    return STATUS_SUCCESS;
}


NTSTATUS
Io_DeviceWokeUp(
    _In_ UDECXUSBDEVICE  Device
)
{
    PIO_CONTEXT pIoContext = WdfDeviceGetIoContext(Device);

    // thi will result in all current requests being canceled
    LogInfo(TRACE_DEVICE, "About to re-start paused deferred queue");
    WdfIoQueueStart(pIoContext->IntrDeferredQueue);

    return STATUS_SUCCESS;
}


NTSTATUS
Io_RetrieveEpQueue(
    _In_ UDECXUSBDEVICE  Device,
    _In_ UCHAR           EpAddr,
    _Out_ WDFQUEUE     * Queue
)
{
    WDFQUEUE *pQueueRecord = NULL;
    PFN_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL pIoCallback = NULL;

    NTSTATUS status = STATUS_SUCCESS;
    PIO_CONTEXT pIoContext = WdfDeviceGetIoContext(Device);
    PUSB_CONTEXT pUsbContext = GetUsbDeviceContext(Device);

    WDFDEVICE wdfController = pUsbContext->ControllerDevice;

    switch (EpAddr)
    {
    case USB_DEFAULT_ENDPOINT_ADDRESS:
        pQueueRecord = &(pIoContext->ControlQueue);
        pIoCallback = IoEvtControlUrb;
        break;

    case g_BulkOutEndpointAddress:
        pQueueRecord = &(pIoContext->BulkOutQueue);
        pIoCallback = IoEvtBulkOutUrb;
        break;

    case g_BulkInEndpointAddress:
        pQueueRecord = &(pIoContext->BulkInQueue);
        pIoCallback = IoEvtBulkInUrb;
        break;

    case g_InterruptEndpointAddress:
        status = Io_CreateDeferredIntrQueue(wdfController, pIoContext);
        pQueueRecord = &(pIoContext->InterruptUrbQueue);
        pIoCallback = IoEvtInterruptInUrb;
        break;

    default:
        LogError(TRACE_DEVICE, "Io_RetrieveEpQueue received unrecognized ep %x", EpAddr);
        status = STATUS_ILLEGAL_FUNCTION;
        goto exit;
    }


    *Queue = NULL;
    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    if ( (*pQueueRecord)  == NULL) {
        WDF_IO_QUEUE_CONFIG queueConfig;
        WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);

        //Sequential must specify this callback
        queueConfig.EvtIoInternalDeviceControl = pIoCallback;
        WDF_OBJECT_ATTRIBUTES  attributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, ENDPOINTQUEUE_CONTEXT);

        status = WdfIoQueueCreate(wdfController,
            &queueConfig,
            &attributes,
            pQueueRecord);

        PENDPOINTQUEUE_CONTEXT pEPQContext;
        pEPQContext = GetEndpointQueueContext(*pQueueRecord);
        pEPQContext->usbDeviceObj      = Device;
        pEPQContext->backChannelDevice = wdfController; // this is a dirty little secret, so we contain it.

        if (!NT_SUCCESS(status)) {

            LogError(TRACE_DEVICE, "WdfIoQueueCreate failed for queue of ep %x %!STATUS!", EpAddr, status);
            goto exit;
        }
    }

    *Queue = (*pQueueRecord);

exit:

    return status;
}



VOID
Io_StopDeferredProcessing(
    _In_ UDECXUSBDEVICE  Device,
    _Out_ PIO_CONTEXT   pIoContextCopy
)
{
    PIO_CONTEXT pIoContext = WdfDeviceGetIoContext(Device);

    pIoContext->bStopping = TRUE;
    // plus this queue will no longer accept incoming requests
    WdfIoQueuePurgeSynchronously( pIoContext->IntrDeferredQueue);

    (*pIoContextCopy) = (*pIoContext);
}


VOID
Io_FreeEndpointQueues(
    _In_ PIO_CONTEXT   pIoContext
)
{

    WdfObjectDelete(pIoContext->IntrDeferredQueue);

    WdfIoQueuePurgeSynchronously(pIoContext->ControlQueue);
    WdfObjectDelete(pIoContext->ControlQueue);

    WdfIoQueuePurgeSynchronously(pIoContext->InterruptUrbQueue);
    WdfObjectDelete(pIoContext->InterruptUrbQueue);

    WdfIoQueuePurgeSynchronously(pIoContext->BulkInQueue);
    WdfObjectDelete(pIoContext->BulkInQueue);

    WdfIoQueuePurgeSynchronously(pIoContext->BulkOutQueue);
    WdfObjectDelete(pIoContext->BulkOutQueue);

}
