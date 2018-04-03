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
#include "ucx/1.4/ucxobjects.h"
#include "USBCom.tmh"






NTSTATUS
Io_AllocateContext(
    _In_
    WDFDEVICE Object
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
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, IO_CONTEXT);

    status = WdfObjectAllocateContext(Object, &attributes, NULL);

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
    WDF_USB_CONTROL_SETUP_PACKET setupPacket;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    //NT_VERIFY(IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB);

    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        // These are on the control pipe.
        // We don't do anything special with these requests,
        // but this is where we would add processing for vendor-specific commands.

        status = UdecxUrbRetrieveControlSetupPacket(Request, &setupPacket);

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
    PUCHAR transferBuffer;
    ULONG transferBufferLength;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        PULONG pCheck;
        ULONG numLongs;
        ULONG j;
        status = UdecxUrbRetrieveBuffer(Request, &transferBuffer, &transferBufferLength);
        if (!NT_SUCCESS(status))
        {
            LogError(TRACE_DEVICE, "WdfRequest %p unable to retrieve buffer %!STATUS!",
                Request, status);
            goto exit;
        }

        LogInfo(TRACE_DEVICE, "ubx CODE %x, [outbuf=%d inbuf=%d], trlen=%d",
            IoControlCode,
            (ULONG)(OutputBufferLength),
            (ULONG)(InputBufferLength),
            transferBufferLength
        );


        pCheck = (PULONG)transferBuffer;
        numLongs = transferBufferLength / sizeof(ULONG);

        // check the data with the algorithm used by the test app
        for (j = 0; j < numLongs; j++, pCheck++)
        {
            if ((*(pCheck)) == j)
            {
                LogInfo(TRACE_DEVICE, "v44 Long %d OK", j);
            }
            else
            {
                LogInfo(TRACE_DEVICE, "Long %d MISMATCH, was %x", j, (*(pCheck)));
            }
        }

        UdecxUrbSetBytesCompleted(Request, transferBufferLength);
    }
    else
    {
        LogError(TRACE_DEVICE, "Invalid Bulk out IOCTL code %x", IoControlCode);
        status = STATUS_ACCESS_DENIED;
    }

exit:
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
    PUCHAR transferBuffer;
    ULONG transferBufferLength;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        PULONG pCheck;
        ULONG numLongs;
        ULONG j;
        status = UdecxUrbRetrieveBuffer(Request, &transferBuffer, &transferBufferLength);
        if (!NT_SUCCESS(status))
        {
            LogError(TRACE_DEVICE, "WdfRequest %p unable to retrieve buffer %!STATUS!",
                Request, status);
            goto exit;
        }

        LogInfo(TRACE_DEVICE, "IN ubx CODE %x, [outbuf=%d inbuf=%d], trlen=%d",
            IoControlCode,
            (ULONG)(OutputBufferLength),
            (ULONG)(InputBufferLength),
            transferBufferLength
        );


        pCheck = (PULONG)transferBuffer;
        numLongs = transferBufferLength / sizeof(ULONG);

        // check the data with the algorithm used by the test app
        for (j = 0; j < numLongs; j++, pCheck++)
        {
            (*(pCheck)) = (j + 42);
            LogInfo(TRACE_DEVICE, "v44 Produced %d OK", (j + 42));
        }

        UdecxUrbSetBytesCompleted(Request, transferBufferLength);
    }
    else
    {
        LogError(TRACE_DEVICE, "Invalid Bulk out IOCTL code %x", IoControlCode);
        status = STATUS_ACCESS_DENIED;
    }

exit:
    UdecxUrbCompleteWithNtStatus(Request, status);
    return;
}


static VOID 
IoFakeTimerIntr(
    _In_ WDFTIMER Timer )
{

    PIO_CONTEXT pIoContext;
    DEVICE_INTR_FLAGS n;

    WDFDEVICE device = WdfTimerGetParentObject(Timer);
    pIoContext = WdfDeviceGetIoContext(device);


    LogInfo(TRACE_DEVICE, "v3 Timer fired to wake up device!");
    n = pIoContext->FakeNextValue;
    pIoContext->FakeNextValue = 0;
    Io_RaiseInterrupt(device, n);
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
    NTSTATUS status = STATUS_SUCCESS;
    PUCHAR transferBuffer;
    ULONG transferBufferLength;

    status = UdecxUrbRetrieveBuffer(request, &transferBuffer, &transferBufferLength);
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
    _In_ WDFDEVICE         Device,
    _In_ DEVICE_INTR_FLAGS LatestStatus )
{
    PUSB_CONTEXT pUsbContext;
    PIO_CONTEXT pIoContext;
    WDFREQUEST request;
    NTSTATUS status = STATUS_SUCCESS;

    pIoContext = WdfDeviceGetIoContext(Device);

    status = WdfIoQueueRetrieveNextRequest( pIoContext->IntrDeferredQueue, &request);

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

        pUsbContext = WdfDeviceGetUsbContext(Device); // TODO: shouldn't need this soon enough, when the context moves to the USB device
        UdecxUsbDeviceSignalWake(pUsbContext->UDEFX2Device);
    } else {
        IoCompletePendingRequest(request, LatestStatus);
        if (((++_Test_rebound) % 2) != 0)
        {
            pIoContext->FakeNextValue = 0x3311;
            WdfTimerStart(pIoContext->FakeIntrTimer, WDF_REL_TIMEOUT_IN_SEC(4));
        }
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
    PIO_CONTEXT pIoContext;
    WDFDEVICE tgtDevice;
    NTSTATUS status = STATUS_SUCCESS;
    DEVICE_INTR_FLAGS LatestStatus = 0;
    BOOLEAN bHasData = FALSE;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    tgtDevice = WdfIoQueueGetDevice(Queue);
    pIoContext = WdfDeviceGetIoContext(tgtDevice);


    if (IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)   {
        LogError(TRACE_DEVICE, "Invalid Interrupt/IN out IOCTL code %x", IoControlCode);
        status = STATUS_ACCESS_DENIED;
        goto exit;
    }

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
        if (((++_Test_rebound) % 2) != 0)
        {
            pIoContext->FakeNextValue = 0x3323;
            WdfTimerStart(pIoContext->FakeIntrTimer, WDF_REL_TIMEOUT_IN_SEC(4));
        }

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
    _In_ WDFDEVICE  Device,
    _In_ PIO_CONTEXT pIoContext )
{
    NTSTATUS status;

    WDF_TIMER_CONFIG  timerConfig;
    WDF_OBJECT_ATTRIBUTES  timerAttributes;

    WDF_IO_QUEUE_CONFIG queueConfig;

    pIoContext->IntrState.latestStatus = 0;
    pIoContext->IntrState.numUnreadUpdates = 0;

    //
    // Register a manual I/O queue for handling Interrupt Message Read Requests.
    // This queue will be used for storing Requests that need to wait for an
    // interrupt to occur before they can be completed.
    //
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

    // when a request gets canceled, this is where we want to do the completion
    queueConfig.EvtIoCanceledOnQueue = IoEvtCancelInterruptInUrb;

    //
    // We shouldn't have to power-manage this queue, as we will manually 
    // purge it and de-queue from it whenever we get power indications.
    //
    queueConfig.PowerManaged = WdfFalse;

    status = WdfIoQueueCreate(Device,
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

    WDF_TIMER_CONFIG_INIT(
        &timerConfig,
        IoFakeTimerIntr
    );

    timerConfig.AutomaticSerialization = TRUE;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = Device;

    status = WdfTimerCreate(
        &timerConfig,
        &timerAttributes,
        &(pIoContext->FakeIntrTimer)
    );
    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE,
            "WdfTimerCreate failed 0x%x\n", status);
        goto Error;
    }




Error:
    return status;
}


NTSTATUS
Io_DeviceSlept(
    _In_ WDFDEVICE  Device
)
{
    PIO_CONTEXT pIoContext;
    pIoContext = WdfDeviceGetIoContext(Device);

    // thi will result in all current requests being canceled
    LogInfo(TRACE_DEVICE, "About to purge deferred request queue" );
    WdfIoQueuePurge(pIoContext->IntrDeferredQueue, NULL, NULL);

    pIoContext->FakeNextValue = 0x8891;
    WdfTimerStart(pIoContext->FakeIntrTimer, WDF_REL_TIMEOUT_IN_SEC(15));

    return STATUS_SUCCESS;
}


NTSTATUS
Io_DeviceWokeUp(
    _In_ WDFDEVICE  Device
)
{
    PIO_CONTEXT pIoContext;
    pIoContext = WdfDeviceGetIoContext(Device);

    // thi will result in all current requests being canceled
    LogInfo(TRACE_DEVICE, "About to re-start paused deferred queue");
    WdfIoQueueStart(pIoContext->IntrDeferredQueue);

    return STATUS_SUCCESS;
}


NTSTATUS
Io_RetrieveEpQueue(
    _In_ WDFDEVICE  Device,
    _In_ UCHAR      EpAddr,
    _Out_ WDFQUEUE * Queue
)
{
    NTSTATUS status;
    PIO_CONTEXT pIoContext;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE *pQueueRecord = NULL;
    PFN_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL pIoCallback = NULL;

    status = STATUS_SUCCESS;
    pIoContext = WdfDeviceGetIoContext(Device);

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
        status = Io_CreateDeferredIntrQueue(Device, pIoContext);
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

        WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);

        //Sequential must specify this callback
        queueConfig.EvtIoInternalDeviceControl = pIoCallback;

        status = WdfIoQueueCreate(Device,
            &queueConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            pQueueRecord);

        if (!NT_SUCCESS(status)) {

            LogError(TRACE_DEVICE, "WdfIoQueueCreate failed for queue of ep %x %!STATUS!", EpAddr, status);
            goto exit;
        }
    }

    *Queue = (*pQueueRecord);

exit:

    return status;
}




