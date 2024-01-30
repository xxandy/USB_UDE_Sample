/*++
Copyright (c) Microsoft Corporation

Module Name:

misc.cpp

Abstract:


--*/

#include "Misc.h"
#include "Driver.h"
#include "Device.h"
#include "usbdevice.h"

#include "Misc.tmh"


static VOID
_WQQCancelRequest(
    IN WDFQUEUE Queue,
    IN WDFREQUEST  Request
)
{
    UNREFERENCED_PARAMETER(Queue);
    WdfRequestComplete(Request, STATUS_CANCELLED);
}


static VOID
_WQQCancelUSBRequest(
    IN WDFQUEUE Queue,
    IN WDFREQUEST  Request
)
{
    UNREFERENCED_PARAMETER(Queue);
    LogInfo(TRACE_DEVICE, "Canceling request %p", Request);
    UdecxUrbCompleteWithNtStatus(Request, STATUS_CANCELLED);
}


NTSTATUS
WRQueueInit(
    _In_    WDFDEVICE parent,
    _Inout_ PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ,
    _In_    BOOLEAN bUSBReqQueue
)
{
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;

    memset(pQ, 0, sizeof(*pQ));

    status = STATUS_SUCCESS;
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

    // when a request gets canceled, this is where we want to do the completion
    queueConfig.EvtIoCanceledOnQueue = (bUSBReqQueue ? _WQQCancelUSBRequest : _WQQCancelRequest );


    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &(pQ->qsync));
    if (!NT_SUCCESS(status) )  {
        pQ->qsync = NULL;
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "Unable to create spinlock, err= %!STATUS!", status);
        goto Error;
    }

    InitializeListHead( &(pQ->WriteBufferQueue) );

    status = WdfIoQueueCreate(parent, 
        &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &(pQ->ReadBufferQueue) );

    if (!NT_SUCCESS(status))  {
        pQ->ReadBufferQueue = NULL;
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "Unable to create rd queue, err= %!STATUS!", status );
        goto Error;
    }

    goto Exit;

Error: // free anything done half-way
    WRQueueDestroy(pQ);

Exit:
    return status;
}

VOID
WRQueueDestroy(
    _Inout_ PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ
)
{
    PLIST_ENTRY e;
    if (pQ->qsync == NULL)  {
        return; // init has not even started
    }

    // clean up the entire list
    WdfSpinLockAcquire( pQ->qsync );
    while ((e = RemoveHeadList(&(pQ->WriteBufferQueue))) != (&(pQ->WriteBufferQueue)) ) {

        PBUFFER_CONTENT pWriteEntry = CONTAINING_RECORD(e, BUFFER_CONTENT, BufferLink);
        ExFreePool(pWriteEntry);

    }
    WdfSpinLockRelease(pQ->qsync);

    WdfObjectDelete(pQ->ReadBufferQueue);
    pQ->ReadBufferQueue = NULL;
 
    WdfObjectDelete(pQ->qsync);
    pQ->qsync = NULL;
}


NTSTATUS
WRQueuePushWrite(
    _In_ PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ,
    _In_ PVOID wbuffer,
    _In_ SIZE_T wlen,
    _Out_ WDFREQUEST *rqReadToComplete
)
{
    NTSTATUS status;
    WDFREQUEST firstPendingRead;

    if (rqReadToComplete == NULL) {
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    (*rqReadToComplete) = NULL;


    status = WdfIoQueueRetrieveNextRequest(pQ->ReadBufferQueue, &firstPendingRead);
    if ( NT_SUCCESS(status) )  {
        (*rqReadToComplete) = firstPendingRead;
        goto Exit;
    } else {
        PBUFFER_CONTENT pNewEntry;
        status = STATUS_SUCCESS; // til proven otherwise

        // allocate
        pNewEntry = ExAllocatePool2(POOL_FLAG_PAGED, sizeof(BUFFER_CONTENT) + wlen, UDEFX_POOL_TAG);
        if (pNewEntry == NULL) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "Not enough memory to queue write, err= %!STATUS!", status);
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }

        // copy
        memcpy(&(pNewEntry->BufferStart), wbuffer, wlen);
        pNewEntry->BufferLength = wlen;

        // enqueue
        WdfSpinLockAcquire(pQ->qsync);
        InsertTailList(
            &(pQ->WriteBufferQueue),
            &(pNewEntry->BufferLink) );
        WdfSpinLockRelease(pQ->qsync);
    }

Exit:
    return status;
}


NTSTATUS
WRQueuePullRead(
    _In_  PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ,
    _In_  WDFREQUEST rqRead,
    _Out_ PVOID rbuffer,
    _In_  SIZE_T rlen,
    _Out_ PBOOLEAN pbReadyToComplete,
    _Out_ PSIZE_T completedBytes
)
{
    NTSTATUS status;
    PLIST_ENTRY firstPendingWrite;
    PBUFFER_CONTENT pWriteEntry = NULL;

    if (pbReadyToComplete == NULL) {
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }
    if (completedBytes == NULL) {
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }
    if (rbuffer == NULL) {
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // defaults
    (*pbReadyToComplete) = FALSE;
    (*completedBytes) = 0;

    WdfSpinLockAcquire(pQ->qsync);
    firstPendingWrite = RemoveHeadList( &(pQ->WriteBufferQueue) );
    WdfSpinLockRelease(pQ->qsync);

    if (firstPendingWrite == &(pQ->WriteBufferQueue) ) {

        // no dangling writes found, must pend this read
        status = WdfRequestForwardToIoQueue(rqRead, pQ->ReadBufferQueue);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_QUEUE,
                "Unable to foward pending read, err= %!STATUS!", status);
        }

    } else {
        size_t minlen;
        pWriteEntry = CONTAINING_RECORD(firstPendingWrite, BUFFER_CONTENT, BufferLink);

        minlen = MINLEN(pWriteEntry->BufferLength, rlen);
        memcpy(rbuffer, &(pWriteEntry->BufferStart), minlen);

        (*pbReadyToComplete) = TRUE;
        (*completedBytes) = minlen;
        ExFreePool(pWriteEntry);
        status = STATUS_SUCCESS;
    }

Exit:
    return status;
}



