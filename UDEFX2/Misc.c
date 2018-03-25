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




NTSTATUS
Misc_WdfDeviceAllocateIoContext(
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

		LogError(TRACE_DRIVER, "Unable to allocate new context for WDF object %p", Object);
		goto exit;
	}

exit:

	return status;
}

NTSTATUS
Misc_WdfDeviceAllocateUsbContext(
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

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, USB_CONTEXT);

	status = WdfObjectAllocateContext(Object, &attributes, NULL);

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DRIVER, "Unable to allocate new context for WDF object %p", Object);
		goto exit;
	}

exit:

	return status;
}

VOID FreeBufferContent(PLIST_ENTRY BufferEntry)
{
	if (BufferEntry != NULL)
	{
		FREE_POOL(((PBUFFER_CONTENT)BufferEntry)->Buffer);
		FREE_POOL(BufferEntry);
	}
}


VOID MbbCleanupBufferQueue(_In_ LIST_ENTRY* BufferQueue)
{
	PLIST_ENTRY listEntry = BufferQueue->Flink;
	while (listEntry != BufferQueue)
	{
		RemoveEntryList(listEntry);
		FreeBufferContent(listEntry);
		listEntry = BufferQueue->Flink;
	}
}
