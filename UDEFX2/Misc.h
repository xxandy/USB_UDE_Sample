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

#define FREE_POOL(_x) {ExFreePool(_x);_x=NULL;};

EXTERN_C_START

NTSTATUS
Misc_WdfDeviceAllocateIoContext(
	_In_ WDFDEVICE Object
);

NTSTATUS
Misc_WdfDeviceAllocateUsbContext(
	_In_ WDFDEVICE Object
);


VOID
MbbCleanupBufferQueue(
	_In_ LIST_ENTRY* BufferQueue
);


VOID
FreeBufferContent(
	PLIST_ENTRY BufferEntry
);


EXTERN_C_END
