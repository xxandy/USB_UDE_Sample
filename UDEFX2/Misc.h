/*++
Copyright (c) Microsoft Corporation

Module Name:

misc.h

Abstract:


--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include "trace.h"



#define MINLEN(__a, __b)  ( ((__a) < (__b)) ? (__a) : (__b) )


EXTERN_C_START


typedef struct _BUFFER_CONTENT
{
    LIST_ENTRY  BufferLink;
    SIZE_T      BufferLength;
    UCHAR       BufferStart; // variable-size structure, first byte of last field
} BUFFER_CONTENT, *PBUFFER_CONTENT;


typedef struct _WRITE_BUFFER_TO_READ_REQUEST_QUEUE
{
    LIST_ENTRY WriteBufferQueue; // write data comes in, keep it here to complete a read request
    WDFQUEUE   ReadBufferQueue; // read request comes in, stays here til a matching write buffer arrives
    WDFSPINLOCK qsync;
} WRITE_BUFFER_TO_READ_REQUEST_QUEUE, *PWRITE_BUFFER_TO_READ_REQUEST_QUEUE;

NTSTATUS
WRQueueInit(
    _In_    WDFDEVICE parent,
    _Inout_ PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ,
    _In_    BOOLEAN bUSBReqQueue
);

VOID
WRQueueDestroy(
    _Inout_ PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ
);



NTSTATUS
WRQueuePushWrite(
    _In_ PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ,
    _In_ PVOID wbuffer,
    _In_ SIZE_T wlen,
    _Out_ WDFREQUEST *rqReadToComplete
);

NTSTATUS
WRQueuePullRead(
    _In_  PWRITE_BUFFER_TO_READ_REQUEST_QUEUE pQ,
    _In_  WDFREQUEST rqRead,
    _Out_ PVOID rbuffer,
    _In_  SIZE_T rlen,
    _Out_ PBOOLEAN pbReadyToComplete,
    _Out_ PSIZE_T completedBytes
);


EXTERN_C_END
