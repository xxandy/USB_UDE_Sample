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
IoEvtCancelInterruptInUrb(
    IN WDFQUEUE Queue,
    IN WDFREQUEST  Request
)
{
    UNREFERENCED_PARAMETER(Queue);
    LogError(TRACE_DEVICE, "Canceling request A %p", Request);
    UdecxUrbCompleteWithNtStatus(Request, STATUS_CANCELLED);
}



static VOID
IoEvtCancelDirect(
    IN WDFREQUEST  Request
)
{
    LogError(TRACE_DEVICE, "Canceling request B %p", Request);
    UdecxUrbCompleteWithNtStatus(Request, STATUS_CANCELLED);
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
    PUCHAR transferBuffer;
    ULONG transferBufferLength;
    static int _s_serviceCtr = 0;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);


    tgtDevice = WdfIoQueueGetDevice(Queue);
    pIoContext = WdfDeviceGetIoContext(tgtDevice);



    if ( (++_s_serviceCtr) >= 4)
    {
        LogError(TRACE_DEVICE, "Interrupt: dev %p marking + hanging %p to prevent tight loop", tgtDevice, Request);

        status = WdfRequestForwardToIoQueue(Request, pIoContext->IntrDeferredQueue);
        if (NT_SUCCESS(status)) {
            LogInfo(TRACE_DEVICE, "Request %p forwarded for later", Request);
        }
        else {
            LogError(TRACE_DEVICE, "ERROR: Unable to forward Request %p error %!STATUS!", Request, status);
            WdfRequestMarkCancelable(Request, IoEvtCancelDirect);
        }
        return;
    }


    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        PULONG pCheck;
        ULONG numLongs;
        ULONG j;
        status = UdecxUrbRetrieveBuffer(Request, &transferBuffer, &transferBufferLength);
        if (!NT_SUCCESS(status))
        {
            LogError(TRACE_DEVICE, "WdfRequest tgtDevice %p %p unable to retrieve buffer %!STATUS!",
                tgtDevice, Request, status);
            goto exit;
        }

        LogInfo(TRACE_DEVICE, "INTR ubx tgtDevice %p Req %p CODE %x, [outbuf=%d inbuf=%d], trlen=%d",
            tgtDevice,
            Request,
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
            (*(pCheck)) = (j + 88);
            LogInfo(TRACE_DEVICE, "v44 INTR %d OK", (j + 42));
        }

        UdecxUrbSetBytesCompleted(Request, transferBufferLength);
    }
    else
    {
        LogError(TRACE_DEVICE, "Invalid Interrupt/IN out IOCTL code %x", IoControlCode);
        status = STATUS_ACCESS_DENIED;
    }

exit:
    UdecxUrbCompleteWithNtStatus(Request, status);
    return;
}


static NTSTATUS
Io_CreateDeferredIntrQueue(
    _In_ WDFDEVICE  Device,
    _In_ PIO_CONTEXT pIoContext )
{
    NTSTATUS status;

    WDF_IO_QUEUE_CONFIG queueConfig;
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




