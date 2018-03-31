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
