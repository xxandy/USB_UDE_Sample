/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Interrupt.c

Abstract:

    This modules has routines configure a continuous reader on an
    interrupt pipe to asynchronously read toggle switch states.

Environment:

    Kernel mode

--*/

#include <hostude.h>

#include "interrupt.tmh"


_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
OsrFxConfigContReaderForInterruptEndPoint(
    _In_ PDEVICE_CONTEXT DeviceContext
    )
/*++

Routine Description:

    This routine configures a continuous reader on the
    interrupt endpoint. It's called from the PrepareHarware event.

Arguments:


Return Value:

    NT status value

--*/
{
    WDF_USB_CONTINUOUS_READER_CONFIG contReaderConfig;
    NTSTATUS status;

    WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&contReaderConfig,
                                          OsrFxEvtUsbInterruptPipeReadComplete,
                                          DeviceContext,    // Context
                                          sizeof(DEVICE_INTR_FLAGS) );   // TransferLength

    contReaderConfig.EvtUsbTargetPipeReadersFailed = OsrFxEvtUsbInterruptReadersFailed;

    //
    // Reader requests are not posted to the target automatically.
    // Driver must explictly call WdfIoTargetStart to kick start the
    // reader.  In this sample, it's done in D0Entry.
    // By defaut, framework queues two requests to the target
    // endpoint. Driver can configure up to 10 requests with CONFIG macro.
    //
    status = WdfUsbTargetPipeConfigContinuousReader(DeviceContext->InterruptPipe,
                                                    &contReaderConfig);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "OsrFxConfigContReaderForInterruptEndPoint failed %x\n",
                    status);
        return status;
    }

    return status;
}

VOID
OsrFxEvtUsbInterruptPipeReadComplete(
    WDFUSBPIPE  Pipe,
    WDFMEMORY   Buffer,
    size_t      NumBytesTransferred,
    WDFCONTEXT  Context
    )
/*++

Routine Description:

    This the completion routine of the continour reader. This can
    called concurrently on multiprocessor system if there are
    more than one readers configured. So make sure to protect
    access to global resources.

Arguments:

    Buffer - This buffer is freed when this call returns.
             If the driver wants to delay processing of the buffer, it
             can take an additional referrence.

    Context - Provided in the WDF_USB_CONTINUOUS_READER_CONFIG_INIT macro

Return Value:

    NT status value

--*/
{
    PDEVICE_INTR_FLAGS  intrFlags = NULL;
    DEVICE_INTR_FLAGS   newFlags;
    WDFDEVICE           device;
    PDEVICE_CONTEXT     pDeviceContext = Context;

    UNREFERENCED_PARAMETER(Pipe);

    device = WdfObjectContextGetObject(pDeviceContext);

    //
    // Make sure that there is data in the read packet.  Depending on the device
    // specification, it is possible for it to return a 0 length read in
    // certain conditions.
    //

    if (NumBytesTransferred == 0) {
        TraceEvents(TRACE_LEVEL_WARNING, DBG_INIT,
                    "OsrFxEvtUsbInterruptPipeReadComplete Zero length read "
                    "occured on the Interrupt Pipe's Continuous Reader\n"
                    );
        return;
    }


    NT_ASSERT(NumBytesTransferred == sizeof(DEVICE_INTR_FLAGS));

    intrFlags = WdfMemoryGetBuffer(Buffer, NULL);

    // deal with possible memory alignment issues
    memcpy(&newFlags, intrFlags, sizeof(newFlags));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT,
                "OsrFxEvtUsbInterruptPipeReadComplete flags %x\n",
                newFlags);

    //
    // Handle any pending Interrupt Message IOCTLs. 
    //
    OsrUsbIoctlGetInterruptMessage(device, STATUS_SUCCESS, newFlags);

}

BOOLEAN
OsrFxEvtUsbInterruptReadersFailed(
    _In_ WDFUSBPIPE Pipe,
    _In_ NTSTATUS Status,
    _In_ USBD_STATUS UsbdStatus
    )
{
    WDFDEVICE device = WdfIoTargetGetDevice(WdfUsbTargetPipeGetIoTarget(Pipe));

    UNREFERENCED_PARAMETER(UsbdStatus);

    //
    // Service the pending interrupt switch change request
    //
    OsrUsbIoctlGetInterruptMessage(device, Status, 0 /*irrelevant*/);

    return TRUE;
}

