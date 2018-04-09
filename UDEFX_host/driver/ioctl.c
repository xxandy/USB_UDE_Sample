/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Ioctl.c

Abstract:

    USB device driver for OSR USB-FX2 Learning Kit

Environment:

    Kernel mode only

--*/

#include <hostude.h>

#include "ioctl.tmh"

#pragma alloc_text(PAGE, OsrFxEvtIoDeviceControl)
#pragma alloc_text(PAGE, ResetPipe)
#pragma alloc_text(PAGE, ResetDevice)

VOID
OsrFxEvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.
Return Value:

    VOID

--*/
{
    WDFDEVICE           device;
    PDEVICE_CONTEXT     pDevContext;
    size_t              bytesReturned = 0;
    BOOLEAN             defaultCompletionNeeded = TRUE;
    NTSTATUS            status = STATUS_INVALID_DEVICE_REQUEST;

    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    //
    // If your driver is at the top of its driver stack, EvtIoDeviceControl is called
    // at IRQL = PASSIVE_LEVEL.
    //
    _IRQL_limited_to_(PASSIVE_LEVEL);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> OsrFxEvtIoDeviceControl\n");
    //
    // initialize variables
    //
    device = WdfIoQueueGetDevice(Queue);
    pDevContext = GetDeviceContext(device);

    switch(IoControlCode) {

    case IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR: {

        PUSB_CONFIGURATION_DESCRIPTOR   configurationDescriptor = NULL;
        USHORT                          requiredSize = 0;

        //
        // First get the size of the config descriptor
        //
        status = WdfUsbTargetDeviceRetrieveConfigDescriptor(
                                    pDevContext->UsbDevice,
                                    NULL,
                                    &requiredSize);

        if (status != STATUS_BUFFER_TOO_SMALL) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
                "WdfUsbTargetDeviceRetrieveConfigDescriptor failed 0x%x\n", status);
            break;
        }

        //
        // Get the buffer - make sure the buffer is big enough
        //
        status = WdfRequestRetrieveOutputBuffer(Request,
                                        (size_t)requiredSize,  // MinimumRequired
                                        &configurationDescriptor,
                                        NULL);
        if(!NT_SUCCESS(status)){
            TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
                "WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
            break;
        }

        status = WdfUsbTargetDeviceRetrieveConfigDescriptor(
                                        pDevContext->UsbDevice,
                                        configurationDescriptor,
                                        &requiredSize);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
                "WdfUsbTargetDeviceRetrieveConfigDescriptor failed 0x%x\n", status);
            break;
        }

        bytesReturned = requiredSize;

        }
        break;

    case IOCTL_OSRUSBFX2_RESET_DEVICE:

        status = ResetDevice(device);
        break;


    default :
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    if (defaultCompletionNeeded) {
        WdfRequestCompleteWithInformation(Request, status, bytesReturned);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "<-- OsrFxEvtIoDeviceControl\n");

    return;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetPipe(
    _In_ WDFUSBPIPE             Pipe
    )
/*++

Routine Description:

    This routine resets the pipe.

Arguments:

    Pipe - framework pipe handle

Return Value:

    NT status value

--*/
{
    NTSTATUS          status;

    PAGED_CODE();

    //
    //  This routine synchronously submits a URB_FUNCTION_RESET_PIPE
    // request down the stack.
    //
    status = WdfUsbTargetPipeResetSynchronously(Pipe,
                                WDF_NO_HANDLE, // WDFREQUEST
                                NULL // PWDF_REQUEST_SEND_OPTIONS
                                );

    if (NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "ResetPipe - success\n");
        status = STATUS_SUCCESS;
    }
    else {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "ResetPipe - failed\n");
    }

    return status;
}

VOID
StopAllPipes(
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkWritePipe),
                                 WdfIoTargetCancelSentIo);
}

NTSTATUS
StartAllPipes(
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    NTSTATUS status;

    status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkWritePipe));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetDevice(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    This routine calls WdfUsbTargetDeviceResetPortSynchronously to reset the device if it's still
    connected.

Arguments:

    Device - Handle to a framework device

Return Value:

    NT status value

--*/
{
    PDEVICE_CONTEXT pDeviceContext;
    NTSTATUS status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> ResetDevice\n");

    pDeviceContext = GetDeviceContext(Device);

    //
    // A NULL timeout indicates an infinite wake
    //
    status = WdfWaitLockAcquire(pDeviceContext->ResetDeviceWaitLock, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "ResetDevice - could not acquire lock\n");
        return status;
    }

    StopAllPipes(pDeviceContext);

    status = WdfUsbTargetDeviceResetPortSynchronously(pDeviceContext->UsbDevice);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "ResetDevice failed - 0x%x\n", status);
    }

    status = StartAllPipes(pDeviceContext);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "Failed to start all pipes - 0x%x\n", status);
    }

    WdfWaitLockRelease(pDeviceContext->ResetDeviceWaitLock);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "<-- ResetDevice\n");
    return status;
}

