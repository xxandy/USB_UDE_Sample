/*++
Copyright (c) Microsoft Corporation

Module Name:

misc.h

Abstract:


--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <wdfusb.h>
#include <usbdlib.h>
#include <ude/1.0/UdeCx.h>
#include <initguid.h>
#include <usbioctl.h>

#include "trace.h"




typedef struct _USB_CONTEXT {

	PUDECXUSBDEVICE_INIT  UdecxUsbDeviceInit;
	UDECXUSBDEVICE        UDEFX2Device;
	UDECXUSBENDPOINT      UDEFX2ControlEndpoint;
	UDECXUSBENDPOINT      UDEFX2BulkOutEndpoint;
	UDECXUSBENDPOINT      UDEFX2BulkInEndpoint;
	UDECXUSBENDPOINT      UDEFX2InterruptInEndpoint;
	ULONG                 MaxBulkInTransfer;
	USHORT                NTBFormatSetupValue;
} USB_CONTEXT, *PUSB_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(USB_CONTEXT, WdfDeviceGetUsbContext);


typedef struct _UDECX_USBDEVICE_CONTEXT {

	WDFDEVICE   WdfDevice;
} UDECX_USBDEVICE_CONTEXT, *PUDECX_USBDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(UDECX_USBDEVICE_CONTEXT, UdecxDeviceGetContext);







// ----- descriptor constants/strings/indexes




extern const UCHAR g_UsbDeviceDescriptor[];
extern const UCHAR g_UsbConfigDescriptorSet[];









EXTERN_C_START



NTSTATUS
Usb_Initialize(
	_In_
	WDFDEVICE WdfDevice
);

NTSTATUS
Usb_ReadDescriptorsAndPlugIn(
	_In_
	WDFDEVICE WdfDevice
);

NTSTATUS
Usb_Disconnect(
	_In_
	WDFDEVICE WdfDevice
);

VOID
Usb_Destroy(
	_In_
	WDFDEVICE WdfDevice
);

//
// Private functions
//
NTSTATUS
UsbCreateControlEndpoint(
	_In_
	WDFDEVICE WdfDevice
);

NTSTATUS
UsbCreateInterruptEndpoint(
	_In_
	WDFDEVICE WdfDevice
);
NTSTATUS
UsbCreateBulkInEndpoint(
	_In_
	WDFDEVICE WdfDevice
);

NTSTATUS
UsbCreateBulkOutEndpoint(
	_In_
	WDFDEVICE WdfDevice
);


EVT_UDECX_USB_DEVICE_ENDPOINTS_CONFIGURE              UsbDevice_EvtUsbDeviceEndpointsConfigure;
EVT_UDECX_USB_DEVICE_D0_ENTRY                         UsbDevice_EvtUsbDeviceLinkPowerEntry;
EVT_UDECX_USB_DEVICE_D0_EXIT                          UsbDevice_EvtUsbDeviceLinkPowerExit;
EVT_UDECX_USB_DEVICE_SET_FUNCTION_SUSPEND_AND_WAKE    UsbDevice_EvtUsbDeviceSetFunctionSuspendAndWake;
EVT_UDECX_USB_ENDPOINT_RESET                          UsbEndpointReset;


EXTERN_C_END


