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



typedef struct _ENDPOINTQUEUE_CONTEXT {
    UDECXUSBDEVICE usbDeviceObj;
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
    PENDPOINTQUEUE_CONTEXT pEpQContext;
    NTSTATUS status = STATUS_SUCCESS;
    PUCHAR transferBuffer;
    ULONG transferBufferLength = 0;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    pEpQContext = GetEndpointQueueContext(Queue);

    if (IoControlCode != IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        LogError(TRACE_DEVICE, "WdfRequest BOUT %p Incorrect IOCTL %x, %!STATUS!",
            Request, IoControlCode, status);
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    status = UdecxUrbRetrieveBuffer(Request, &transferBuffer, &transferBufferLength);
    if (!NT_SUCCESS(status))
    {
        LogError(TRACE_DEVICE, "WdfRequest BOUT %p unable to retrieve buffer %!STATUS!",
            Request, status);
        goto exit;
    }

    LogError(TRACE_DEVICE, "Successfully received %d bytes from device %p",
        transferBufferLength, pEpQContext->usbDeviceObj );
    // dump to trace


exit:
    // writes never pended, always completed
    UdecxUrbSetBytesCompleted(Request, transferBufferLength);
    UdecxUrbCompleteWithNtStatus(Request, status);
    return;
}






NTSTATUS
Io_DeviceSlept(
    _In_ UDECXUSBDEVICE  Device
)
{
    UNREFERENCED_PARAMETER(Device);//for now
    return STATUS_SUCCESS;
}


NTSTATUS
Io_DeviceWokeUp(
    _In_ UDECXUSBDEVICE  Device
)
{
    UNREFERENCED_PARAMETER(Device);//for now
    return STATUS_SUCCESS;
}


NTSTATUS
Io_RetrieveEpQueue(
    _In_ UDECXUSBDEVICE  Device,
    _In_ UCHAR           EpAddr,
    _Out_ WDFQUEUE     * Queue
)
{
    NTSTATUS status;
    PIO_CONTEXT pIoContext;
    PUSB_CONTEXT pUsbContext;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFDEVICE wdfController;
    WDFQUEUE *pQueueRecord = NULL;
    WDF_OBJECT_ATTRIBUTES  attributes;
    PFN_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL pIoCallback = NULL;

    status = STATUS_SUCCESS;
    pIoContext = WdfDeviceGetIoContext(Device);
    pUsbContext = GetUsbDeviceContext(Device);

    wdfController = pUsbContext->ControllerDevice;

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


    *Queue = NULL;
    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    if ( (*pQueueRecord)  == NULL) {
        PENDPOINTQUEUE_CONTEXT pEPQContext;
        WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);

        //Sequential must specify this callback
        queueConfig.EvtIoInternalDeviceControl = pIoCallback;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, ENDPOINTQUEUE_CONTEXT);

        status = WdfIoQueueCreate(wdfController,
            &queueConfig,
            &attributes,
            pQueueRecord);

        pEPQContext = GetEndpointQueueContext(*pQueueRecord);
        pEPQContext->usbDeviceObj      = Device;

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

    (*pIoContextCopy) = (*pIoContext);
}


VOID
Io_FreeEndpointQueues(
    _In_ PIO_CONTEXT   pIoContext
)
{

    WdfIoQueuePurgeSynchronously(pIoContext->ControlQueue);
    WdfObjectDelete(pIoContext->ControlQueue);

    WdfIoQueuePurgeSynchronously(pIoContext->BulkOutQueue);
    WdfObjectDelete(pIoContext->BulkOutQueue);

}



