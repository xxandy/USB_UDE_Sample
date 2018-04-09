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


    case IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE:
        {
            BOOLEAN bDataReady = FALSE;
            DEVICE_INTR_FLAGS newData = 0;

            WdfSpinLockAcquire(pDevContext->InterruptStatus.sync);
            if (pDevContext->InterruptStatus.numUnreadUpdates > 0)
            {
                // data is ready ahead of time, grab it
                bDataReady = TRUE;
                newData = pDevContext->InterruptStatus.latestStatus;
                pDevContext->InterruptStatus.numUnreadUpdates = 0;
                pDevContext->InterruptStatus.latestStatus = 0;
            }
            WdfSpinLockRelease(pDevContext->InterruptStatus.sync);

            if (bDataReady)
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL,
                    "Completing pending request IMMEDIATELY %p\n", Request);

                // complete immediately - does not need to traverse the queue
                OsrCompleteInterruptRequest(Request, STATUS_SUCCESS, newData);
                defaultCompletionNeeded = FALSE;
            }
            else
            {
                // We haven't heard from the device since last update, so
                // forward the request to an interrupt message queue and dont complete
                // the request until an interrupt from the USB device occurs.
                // Note that WDK will automatically cancel this request if needed.
                //

                status = WdfRequestForwardToIoQueue(Request, pDevContext->InterruptMsgQueue);
                if (NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL,
                        "Deferred pending request %p\n", Request);
                    defaultCompletionNeeded = FALSE;
                } else {
                    TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
                        "ERROR: Unable to defer request %p, error %x\n", Request, status);
                }
            }
        }

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
    WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DeviceContext->InterruptPipe),
                                 WdfIoTargetCancelSentIo);
    WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkReadPipe),
                                 WdfIoTargetCancelSentIo);
    WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkWritePipe),
                                 WdfIoTargetCancelSentIo);
}

NTSTATUS
StartAllPipes(
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    NTSTATUS status;

    status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(DeviceContext->InterruptPipe));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkReadPipe));
    if (!NT_SUCCESS(status)) {
        return status;
    }

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


VOID
OsrCompleteInterruptRequest(
    _In_ WDFREQUEST request,
    _In_ NTSTATUS  ReaderStatus,
    _In_ DEVICE_INTR_FLAGS NewDeviceFlags
)
/*++

Routine Description

Completes one pending interrupt request and resets pending updates to zero.

Arguments:

request - Handle to the request to complete
ReaderStatus - status of read operation
NewDeviceFlags - new hardware data obtained with this interrupt, if status is success

Return Value:

None.

--*/
{
    NTSTATUS            status;
    size_t              bytesReturned = 0;
    PDEVICE_INTR_FLAGS  intrFlags = NULL;

    status = WdfRequestRetrieveOutputBuffer(request,
        sizeof(DEVICE_INTR_FLAGS),
        &intrFlags,
        NULL);// BufferLength

    if (!NT_SUCCESS(status)) {

        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
            "User's output buffer is too small for this IOCTL, expecting a DEVICE_INTR_FLAGS\n");
        bytesReturned = sizeof(DEVICE_INTR_FLAGS);

    }
    else {

        //
        // Copy the state information saved by the continuous reader.
        //
        if (NT_SUCCESS(ReaderStatus)) {
            memcpy(intrFlags, &NewDeviceFlags, sizeof(NewDeviceFlags));
            bytesReturned = sizeof(DEVICE_INTR_FLAGS);
        }
        else {
            bytesReturned = 0;
        }
    }

    //
    // Complete the request.  If we failed to get the output buffer then
    // complete with that status.  Otherwise complete with the status from the reader.
    //
    WdfRequestCompleteWithInformation(request,
        NT_SUCCESS(status) ? ReaderStatus : status,
        bytesReturned);

    return;
}


VOID
OsrUsbIoctlGetInterruptMessage(
    _In_ WDFDEVICE Device,
    _In_ NTSTATUS  ReaderStatus,
    _In_ DEVICE_INTR_FLAGS NewDeviceFlags
)
/*++

Routine Description

    This method handles the completion of the pended request for the IOCTL
    IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE.

Arguments:

    Device - Handle to a framework device.
    ReaderStatus - status of read operation
    NewDeviceFlags - new hardware data obtained with this interrupt, if status is success

Return Value:

    None.

--*/
{
    NTSTATUS            status;
    WDFREQUEST          request;
    PDEVICE_CONTEXT     pDevContext;

    pDevContext = GetDeviceContext(Device);

    //
    // Check if there are any pending requests in the Interrupt Message Queue.
    status = WdfIoQueueRetrieveNextRequest(pDevContext->InterruptMsgQueue, &request);

    WdfSpinLockAcquire(pDevContext->InterruptStatus.sync);
    if (NT_SUCCESS(ReaderStatus)) { // new data produced
        if (NT_SUCCESS(status)) { // and there's at least one waiter
            // clear the data, as it will be consumed by this waiter right down below
            pDevContext->InterruptStatus.latestStatus = 0;
            pDevContext->InterruptStatus.numUnreadUpdates = 0;
        }
        else { // overwrite any existing status and bump the count
            pDevContext->InterruptStatus.latestStatus = NewDeviceFlags;
            if ((pDevContext->InterruptStatus.numUnreadUpdates) < MAX_CACHED_INTR_UPDATES) // prevent wrap-around
            {
                ++(pDevContext->InterruptStatus.numUnreadUpdates);
            }
        }
    }
    else if( ReaderStatus != STATUS_CANCELLED ) { // error or termination
        // erase any prior data
        pDevContext->InterruptStatus.latestStatus = 0;
        pDevContext->InterruptStatus.numUnreadUpdates = 0;
    }
    WdfSpinLockRelease(pDevContext->InterruptStatus.sync);

    // we will unconditionally empty the queue
    while( NT_SUCCESS(status) )  {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL,
            "Completing previously pending request %p\n", request);

        OsrCompleteInterruptRequest(request, ReaderStatus, NewDeviceFlags);
        request = NULL;

        status = WdfIoQueueRetrieveNextRequest(pDevContext->InterruptMsgQueue, &request);

    } while (status == STATUS_SUCCESS);

    if (status != STATUS_NO_MORE_ENTRIES) {
        KdPrint(("Interrupt queue draining ended with bad status %08x\n", status));
    }

    return;

}


