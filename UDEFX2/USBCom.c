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


        LogInfo(TRACE_DEVICE, "v2 control CODE %x, [type=%x dir=%x recip=%x] req=%x [wv = %x wi = %x wlen = %x]",
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
                LogInfo(TRACE_DEVICE, "v2 Long %d OK", j);
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

    default:
        LogError(TRACE_DEVICE, "Io_RetrieveEpQueue received unrecognized ep %x", EpAddr);
        status = STATUS_ILLEGAL_FUNCTION;
        goto exit;
    }


    status = STATUS_SUCCESS;
    *Queue = NULL;

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




