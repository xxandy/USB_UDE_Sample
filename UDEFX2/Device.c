/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "usbdevice.h"
#include "Misc.h"
#include "USBCom.h"
#include "BackChannel.h"

#include <ntstrsafe.h>
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, UDEFX2CreateDevice)
#endif

NTSTATUS
UDEFX2CreateDevice(
    _Inout_ PWDFDEVICE_INIT WdfDeviceInit
    )
/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
{
	NTSTATUS                            status;
	WDFDEVICE                           wdfDevice;
	WDF_PNPPOWER_EVENT_CALLBACKS        wdfPnpPowerCallbacks;
	WDF_OBJECT_ATTRIBUTES               wdfDeviceAttributes;
	WDF_OBJECT_ATTRIBUTES               wdfRequestAttributes;
	UDECX_WDF_DEVICE_CONFIG             controllerConfig;
	WDF_FILEOBJECT_CONFIG               fileConfig;
    PUDECX_USBCONTROLLER_CONTEXT        pControllerContext;
	WDF_IO_QUEUE_CONFIG                 defaultQueueConfig;
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS
		idleSettings;
	UNICODE_STRING                      refString;

	FuncEntry(TRACE_DEVICE);

	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&wdfPnpPowerCallbacks);
	wdfPnpPowerCallbacks.EvtDevicePrepareHardware = ControllerWdfEvtDevicePrepareHardware;
	wdfPnpPowerCallbacks.EvtDeviceReleaseHardware = ControllerWdfEvtDeviceReleaseHardware;
	wdfPnpPowerCallbacks.EvtDeviceD0Entry = ControllerWdfEvtDeviceD0Entry;
	wdfPnpPowerCallbacks.EvtDeviceD0Exit = ControllerWdfEvtDeviceD0Exit;
	wdfPnpPowerCallbacks.EvtDeviceD0EntryPostInterruptsEnabled =
		ControllerWdfEvtDeviceD0EntryPostInterruptsEnabled;
	wdfPnpPowerCallbacks.EvtDeviceD0ExitPreInterruptsDisabled =
		ControllerWdfEvtDeviceD0ExitPreInterruptsDisabled;

	WdfDeviceInitSetPnpPowerEventCallbacks(WdfDeviceInit, &wdfPnpPowerCallbacks);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&wdfRequestAttributes, REQUEST_CONTEXT);
	WdfDeviceInitSetRequestAttributes(WdfDeviceInit, &wdfRequestAttributes);

	//
	// To distinguish I/O sent to GUID_DEVINTERFACE_USB_HOST_CONTROLLER, we will enable
	// interface reference strings. This requires calling WdfDeviceInitSetFileObjectConfig
	// with FileObjectClass WdfFileObjectWdfXxx.
	//
	WDF_FILEOBJECT_CONFIG_INIT(&fileConfig,
		WDF_NO_EVENT_CALLBACK,
		WDF_NO_EVENT_CALLBACK,
		WDF_NO_EVENT_CALLBACK // No cleanup callback function
	);

	//
	// Safest value forces WDF to track handles separately. If the driver stack allows it, then
	// for performance, we should change this to a different option.
	//
	fileConfig.FileObjectClass = WdfFileObjectWdfCannotUseFsContexts;

	WdfDeviceInitSetFileObjectConfig(WdfDeviceInit,
		&fileConfig,
		WDF_NO_OBJECT_ATTRIBUTES);

	//
	// Set the security descriptor for the device.
	//
	status = WdfDeviceInitAssignSDDLString(WdfDeviceInit, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R);

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DEVICE, "WdfDeviceInitAssignSDDLString Failed %!STATUS!", status);
		goto exit;
	}

	//
	// Do additional setup required for USB controllers.
	//
	status = UdecxInitializeWdfDeviceInit(WdfDeviceInit);

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DEVICE, "UdecxInitializeDeviceInit failed %!STATUS!", status);
		goto exit;
	}

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&wdfDeviceAttributes, UDECX_USBCONTROLLER_CONTEXT);
	wdfDeviceAttributes.EvtCleanupCallback = ControllerWdfEvtCleanupCallback;

	//
	// Call WdfDeviceCreate with a few extra compatibility steps to ensure this device looks
	// exactly like other USB host controllers.
	//
	status = ControllerCreateWdfDeviceWithNameAndSymLink(&WdfDeviceInit,
		&wdfDeviceAttributes,
		&wdfDevice);

	if (!NT_SUCCESS(status)) {

		goto exit;
	}

	//
	// Create the device interface.
	//
	RtlInitUnicodeString(&refString,
		USB_HOST_DEVINTERFACE_REF_STRING);

	status = WdfDeviceCreateDeviceInterface(wdfDevice,
		(LPGUID)&GUID_DEVINTERFACE_USB_HOST_CONTROLLER,
		&refString);

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DEVICE, "WdfDeviceCreateDeviceInterface Failed %!STATUS!", status);
		goto exit;
	}


    // create a 2nd interface for back-channel interaction with the controller
    status = WdfDeviceCreateDeviceInterface(wdfDevice,
        (LPGUID)&GUID_DEVINTERFACE_UDE_BACKCHANNEL,
        NULL);

    if (!NT_SUCCESS(status)) {

        LogError(TRACE_DEVICE, "WdfDeviceCreateDeviceInterface (backchannel) Failed %!STATUS!", status);
        goto exit;
    }

	UDECX_WDF_DEVICE_CONFIG_INIT(&controllerConfig, ControllerEvtUdecxWdfDeviceQueryUsbCapability);

	status = UdecxWdfDeviceAddUsbDeviceEmulation(wdfDevice,
		&controllerConfig);
    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE, "Unable to add USB device emulation, err= %!STATUS!", status);
        goto exit;
    }
	//
	// Initialize controller data members.

	pControllerContext = GetUsbControllerContext(wdfDevice);

    status = BackChannelInit(wdfDevice);
    if (!NT_SUCCESS(status))
    {
        LogError(TRACE_DEVICE, "Unable to initialize backchannel err=%!STATUS!", status);
        goto exit;
    }


	KeInitializeEvent(&pControllerContext->ResetCompleteEvent,
		NotificationEvent,
		FALSE /* initial state: not signaled */);

	//
	// Create default queue. It only supports USB controller IOCTLs. (USB I/O will come through
	// in separate USB device queues.)
	//
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&defaultQueueConfig, WdfIoQueueDispatchSequential);
	defaultQueueConfig.EvtIoDeviceControl = ControllerEvtIoDeviceControl;
    defaultQueueConfig.EvtIoRead = BackChannelEvtRead;
    defaultQueueConfig.EvtIoWrite = BackChannelEvtWrite;
    defaultQueueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(wdfDevice,
		&defaultQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&pControllerContext->DefaultQueue);

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DEVICE, "Default queue creation failed %!STATUS!", status);
		goto exit;
	}



	//
	// Initialize virtual USB device software objects.
	//
	status = Usb_Initialize(wdfDevice);

	if (!NT_SUCCESS(status)) {

		goto exit;
	}

	//
	// Setup the S0 Idle settings just so that we get registered with power
	// framework as some SOC platforms depend on it. 
	//
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCannotWakeFromS0); // TODO:can

																					 //idleSettings.IdleTimeoutType = SystemManagedIdleTimeoutWithHint;
																					 //idleSettings.Enabled = WdfFalse;

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DEVICE, "WdfDeviceAssignS0IdleSettings failed %!STATUS!", status);
		goto exit;
	}

exit:
	FuncExit(TRACE_DEVICE, status);
	return status;


}


NTSTATUS
ControllerCreateWdfDeviceWithNameAndSymLink(
	_Inout_
	PWDFDEVICE_INIT * WdfDeviceInit,
	_In_
	PWDF_OBJECT_ATTRIBUTES WdfDeviceAttributes,
	_Out_
	WDFDEVICE * WdfDevice
)
/*++

Routine Description:

Create the WDFDEVICE with a few extra compatibility steps, to ensure this device looks exactly
like other USB host controllers. This function should be called from EvtDriverDeviceAdd.

The extra compatibility steps are:

1) Assign a USB host controller FDO name to the device.
2) Create a USB host controller symbolic link to the device.

Arguments:

WdfDeviceInit - Initialization parameters for the new device. This function does not modify the
parameters, but sets the pointer to NULL to indicate the structure has been
used up in creating the device.

WdfDeviceAttributes - Attributes of the device (cleanup callback, context type, etc.)

WdfDevice - The created WdfDevice. Note, when failure is returned this may be NULL or non-NULL.
Non-NULL indicates the function encountered a failure after successfully calling
WdfDeviceCreate. WDF will delete the WdfDevice itself if EvtDriverDeviceAdd returns
a failure.

Return Value:

NTSTATUS

--*/
{
	NTSTATUS status;
	UINT32 instanceNumber;
	BOOLEAN isCreated;

	DECLARE_UNICODE_STRING_SIZE(uniDeviceName, DeviceNameSize);
	DECLARE_UNICODE_STRING_SIZE(uniSymLinkName, SymLinkNameSize);
	FuncEntry(TRACE_DEVICE);

	*WdfDevice = NULL;

	//
	// Generate a unique static device name in order to provide compatibility to look like with
	// existing USB host controller driver implementations.
	//
	isCreated = FALSE;

	for (instanceNumber = 0; instanceNumber < MAXUINT32; instanceNumber++) {

		status = RtlUnicodeStringPrintf(&uniDeviceName,
			L"%ws%d",
			BASE_DEVICE_NAME,
			instanceNumber);

		if (!NT_SUCCESS(status)) {

			LogError(TRACE_DEVICE, "RtlUnicodeStringPrintf (uniDeviceName) failed %!STATUS!", status);
			goto exit;
		}

		status = WdfDeviceInitAssignName(*WdfDeviceInit, &uniDeviceName);

		if (!NT_SUCCESS(status)) {

			LogError(TRACE_DEVICE, "WdfDeviceInitAssignName Failed %!STATUS!", status);
			goto exit;
		}

		status = WdfDeviceCreate(WdfDeviceInit, WdfDeviceAttributes, WdfDevice);

		if (status == STATUS_OBJECT_NAME_COLLISION) {

			//
			// This is expected to happen at least once when another USB host controller
			// already exists on the system.
			//
			LogVerbose(TRACE_DEVICE, "WdfDeviceCreate Object Name Collision %d", instanceNumber);

		}
		else if (!NT_SUCCESS(status)) {

			LogError(TRACE_DEVICE, "WdfDeviceCreate Failed %!STATUS!", status);
			goto exit;

		}
		else {

			isCreated = TRUE;
			break;
		}
	}

	if (!isCreated) {

		status = STATUS_OBJECT_NAME_COLLISION;
		LogError(TRACE_DEVICE, "All instance numbers of USB host controller are already used %!STATUS!",
			status);
		goto exit;
	}

	//
	// Create the symbolic link (also for compatibility).
	//
	status = RtlUnicodeStringPrintf(&uniSymLinkName,
		L"%ws%d",
		BASE_SYMBOLIC_LINK_NAME,
		instanceNumber);

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DEVICE, "RtlUnicodeStringPrintf (SymLinkName) Failed %!STATUS!", status);
		goto exit;
	}

	status = WdfDeviceCreateSymbolicLink(*WdfDevice, &uniSymLinkName);

	if (!NT_SUCCESS(status)) {

		LogError(TRACE_DEVICE, "WdfDeviceCreateSymbolicLink Failed %!STATUS!", status);
		goto exit;
	}

exit:
	FuncExit(TRACE_DEVICE, status);
	return status;
}



NTSTATUS
ControllerWdfEvtDevicePrepareHardware(
	_In_
	WDFDEVICE       WdfDevice,
	_In_
	WDFCMRESLIST    WdfResourcesRaw,
	_In_
	WDFCMRESLIST    WdfResourcesTranslated
)
{
	FuncEntry(TRACE_DEVICE);
	UNREFERENCED_PARAMETER(WdfDevice);
	UNREFERENCED_PARAMETER(WdfResourcesRaw);
	UNREFERENCED_PARAMETER(WdfResourcesTranslated);
	FuncExit(TRACE_DEVICE, 0);
	return STATUS_SUCCESS;
}





NTSTATUS
ControllerWdfEvtDeviceD0Entry(
	_In_
	WDFDEVICE              WdfDevice,
	_In_
	WDF_POWER_DEVICE_STATE PreviousState
)
{
	NTSTATUS status = STATUS_SUCCESS;
    PUDECX_USBCONTROLLER_CONTEXT pControllerContext;

	FuncEntry(TRACE_DEVICE);
	pControllerContext = GetUsbControllerContext(WdfDevice);

	if (PreviousState == WdfPowerDeviceD3Final) {

		NT_ASSERT(!pControllerContext->AllowOnlyResetInterrupts);
		pControllerContext->AllowOnlyResetInterrupts = TRUE;

		status = Usb_ReadDescriptorsAndPlugIn(WdfDevice);

		if (!NT_SUCCESS(status)) {

			goto exit;
		}
	}

exit:
	FuncExit(TRACE_DEVICE, status);
	return status;
}


NTSTATUS
ControllerWdfEvtDeviceD0EntryPostInterruptsEnabled(
	_In_
	WDFDEVICE              WdfDevice,
	_In_
	WDF_POWER_DEVICE_STATE PreviousState
)
{
	FuncEntry(TRACE_DEVICE);
	UNREFERENCED_PARAMETER(WdfDevice);
	UNREFERENCED_PARAMETER(PreviousState);
	FuncExit(TRACE_DEVICE, 0);
	return STATUS_SUCCESS;
}


NTSTATUS
ControllerWdfEvtDeviceD0ExitPreInterruptsDisabled(
	_In_
	WDFDEVICE              WdfDevice,
	_In_
	WDF_POWER_DEVICE_STATE TargetState
)
{
	FuncEntry(TRACE_DEVICE);
	UNREFERENCED_PARAMETER(WdfDevice);
	UNREFERENCED_PARAMETER(TargetState);
	FuncExit(TRACE_DEVICE, 0);
	return STATUS_SUCCESS;
}


NTSTATUS
ControllerWdfEvtDeviceD0Exit(
	_In_ WDFDEVICE              WdfDevice,
	_In_ WDF_POWER_DEVICE_STATE TargetState
)
{
	FuncEntry(TRACE_DEVICE);

	if (TargetState == WdfPowerDeviceD3Final)
	{
		Usb_Disconnect(WdfDevice);
	}

	FuncExit(TRACE_DEVICE, 0);
	return STATUS_SUCCESS;
}



NTSTATUS
ControllerWdfEvtDeviceReleaseHardware(
	_In_
	WDFDEVICE       WdfDevice,
	_In_
	WDFCMRESLIST    WdfResourcesTranslated
)
{
	FuncEntry(TRACE_DEVICE);
	UNREFERENCED_PARAMETER(WdfDevice);
	UNREFERENCED_PARAMETER(WdfResourcesTranslated);
	FuncExit(TRACE_DEVICE, 0);
	return STATUS_SUCCESS;
}


VOID
ControllerWdfEvtCleanupCallback(
	_In_ WDFOBJECT   WdfDevice
)
{
	FuncEntry(TRACE_DEVICE);

	Usb_Destroy((WDFDEVICE)WdfDevice);

    BackChannelDestroy((WDFDEVICE)WdfDevice);

	FuncExit(TRACE_DEVICE, 0);
}



VOID
ControllerEvtIoDeviceControl(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ size_t OutputBufferLength,
	_In_ size_t InputBufferLength,
	_In_ ULONG IoControlCode
)
{
	BOOLEAN handled;
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE ctrdevice = WdfIoQueueGetDevice(Queue);

    
    UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	handled = UdecxWdfDeviceTryHandleUserIoctl(ctrdevice,
		Request);

	if (handled) {
		goto exit;
	}

    handled = BackChannelIoctl(IoControlCode, ctrdevice, Request);


    if (handled) {
        goto exit;
    }


	status = STATUS_INVALID_DEVICE_REQUEST;
	LogError(TRACE_DEVICE, "Unexpected I/O control code 0x%x %!STATUS!", IoControlCode, status);
	NT_ASSERTMSG("Unexpected I/O", FALSE);
	WdfRequestComplete(Request, status);

exit:

	return;
}



NTSTATUS
ControllerEvtUdecxWdfDeviceQueryUsbCapability(
	_In_ WDFDEVICE     UdecxWdfDevice,
	_In_ PGUID         CapabilityType,
	_In_ ULONG         OutputBufferLength,
	_Out_writes_to_opt_(OutputBufferLength, *ResultLength)
	PVOID OutputBuffer,
	_Out_ PULONG        ResultLength
)
{
	UNREFERENCED_PARAMETER(UdecxWdfDevice);
	UNREFERENCED_PARAMETER(CapabilityType);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(ResultLength);

	if (RtlCompareMemory(
		CapabilityType,
		&GUID_USB_CAPABILITY_DEVICE_CONNECTION_HIGH_SPEED_COMPATIBLE,
		sizeof(GUID)
	) == sizeof(GUID))
	{
		return STATUS_SUCCESS;
	}
	return STATUS_UNSUCCESSFUL;
}