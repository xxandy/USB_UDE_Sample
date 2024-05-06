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



// device context
typedef struct _USB_CONTEXT {
    WDFDEVICE             ControllerDevice;
    UDECXUSBENDPOINT      UDEFX2ControlEndpoint;
	UDECXUSBENDPOINT      UDEFX2BulkOutEndpoint;
    UDECXUSBENDPOINT      UDEFX2BulkInEndpoint;
    UDECXUSBENDPOINT      UDEFX2InterruptInEndpoint;
    BOOLEAN               IsAwake;
} USB_CONTEXT, *PUSB_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(USB_CONTEXT, GetUsbDeviceContext);










// ----- descriptor constants/strings/indexes
#define g_ManufacturerIndex   1
#define g_ProductIndex        2
#define g_BulkOutEndpointAddress 2
#define g_BulkInEndpointAddress    0x84
#define g_InterruptEndpointAddress 0x86


#define UDEFX2_DEVICE_VENDOR_ID  0x1209
#define UDEFX2_DEVICE_PROD_ID    0x0887

extern const UCHAR g_UsbConfigDescriptorSet[];

// ------------------------------------------------







EXTERN_C_START


NTSTATUS
Usb_Initialize(
	_In_ WDFDEVICE WdfControllerDevice
);


NTSTATUS
Usb_ReadDescriptorsAndPlugIn(
	_In_ WDFDEVICE WdfControllerDevice
);

NTSTATUS
Usb_Disconnect(
	_In_ WDFDEVICE WdfDevice
);

VOID
Usb_Destroy(
	_In_ WDFDEVICE WdfDevice
);

//
// Private functions
//
NTSTATUS
UsbCreateEndpointObj(
	_In_   UDECXUSBDEVICE    WdfUsbChildDevice,
    _In_   UCHAR             epAddr,
    _Out_  UDECXUSBENDPOINT *pNewEpObjAddr
);


EVT_UDECX_USB_DEVICE_ENDPOINTS_CONFIGURE              UsbDevice_EvtUsbDeviceEndpointsConfigure;
EVT_UDECX_USB_DEVICE_D0_ENTRY                         UsbDevice_EvtUsbDeviceLinkPowerEntry;
EVT_UDECX_USB_DEVICE_D0_EXIT                          UsbDevice_EvtUsbDeviceLinkPowerExit;
EVT_UDECX_USB_ENDPOINT_RESET                          UsbEndpointReset;


EXTERN_C_END


