/*++
Copyright (c) Microsoft Corporation

Module Name:

usbdevice.cpp

Abstract:


--*/

#include "Misc.h"
#include "Driver.h"
#include "Device.h"
#include "usbdevice.h"
#include "USBCom.h"
#include "ucx/1.4/ucxobjects.h"
#include "usbdevice.tmh"



#define UDECXMBIM_POOL_TAG 'UDEI'



// START ------------------ descriptor -------------------------------


DECLARE_CONST_UNICODE_STRING(g_ManufacturerStringEnUs, L"Microsoft");
DECLARE_CONST_UNICODE_STRING(g_ProductStringEnUs, L"UDE Client");


const USHORT AMERICAN_ENGLISH = 0x409;

const UCHAR g_LanguageDescriptor[] = { 4,3,9,4 };



const UCHAR g_UsbDeviceDescriptor[18] =
{
    0x12,                            // Descriptor size
    USB_DEVICE_DESCRIPTOR_TYPE,      // Device descriptor type
    0x00, 0x02,                      // USB 2.0
    0x00,                            // Device class (interface-class defined)
    0x00,                            // Device subclass
    0x00,                            // Device protocol
    0x40,                            // Maxpacket size for EP0
    UDEFX2_DEVICE_VENDOR_ID,         // Vendor ID
    UDEFX2_DEVICE_PROD_ID,           // Product ID
    0x00,                            // LSB of firmware revision
    0x01,                            // MSB of firmware revision
    0x01,                            // Manufacture string index
    0x02,                            // Product string index
    0x00,                            // Serial number string index
    0x01                             // Number of configurations
};

const UCHAR g_UsbConfigDescriptorSet[] =
{
    // Configuration Descriptor Type
    0x9,                              // Descriptor Size
    USB_CONFIGURATION_DESCRIPTOR_TYPE, // Configuration Descriptor Type
    0x27, 0x00,                        // Length of this descriptor and all sub descriptors
    0x1,                               // Number of interfaces
    0x01,                              // Configuration number
    0x00,                              // Configuration string index
    0xA0,                              // Config characteristics - bus powered
    0x32,                              // Max power consumption of device (in 2mA unit) : 0 ma

        // Interface  descriptor
        0x9,                                      // Descriptor size
        USB_INTERFACE_DESCRIPTOR_TYPE,             // Interface Association Descriptor Type
        0,                                        // bInterfaceNumber
        0,                                        // bAlternateSetting
        3,                                        // bNumEndpoints
        0xFF,                                     // bInterfaceClass
        0x00,                                     // bInterfaceSubClass
        0x00,                                     // bInterfaceProtocol
        0x00,                                     // iInterface

        // Bulk Out Endpoint descriptor
        0x07,                           // Descriptor size
        USB_ENDPOINT_DESCRIPTOR_TYPE,   // bDescriptorType
        g_BulkOutEndpointAddress,       // bEndpointAddress
        USB_ENDPOINT_TYPE_BULK,         // bmAttributes - bulk
        0x00, 0x2,                      // wMaxPacketSize
        0x00,                           // bInterval

        // Bulk IN endpoint descriptor
        0x07,                           // Descriptor size 
        USB_ENDPOINT_DESCRIPTOR_TYPE,   // Descriptor type
        g_BulkInEndpointAddress,        // Endpoint address and description
        USB_ENDPOINT_TYPE_BULK,         // bmAttributes - bulk
        0x00, 0x02,                     // Max packet size
        0x00,                           // Servicing interval for data transfers : NA for bulk

        // Interrupt IN endpoint descriptor
        0x07,                           // Descriptor size 
        USB_ENDPOINT_DESCRIPTOR_TYPE,   // Descriptor type
        g_InterruptEndpointAddress,     // Endpoint address and description
        USB_ENDPOINT_TYPE_INTERRUPT,    // bmAttributes - interrupt
        0x40, 0x0,                      // Max packet size = 64
        0x01                            // Servicing interval for interrupt (1ms/1 frame)
};



//
// Generic descriptor asserts
//
static
FORCEINLINE
VOID
UsbValidateConstants(
)
{
    //
    // C_ASSERT doesn't treat these expressions as constant, so use NT_ASSERT
    //
    NT_ASSERT(((PUSB_STRING_DESCRIPTOR)g_LanguageDescriptor)->bString[0] == AMERICAN_ENGLISH);
    //NT_ASSERT(((PUSB_STRING_DESCRIPTOR)g_LanguageDescriptor)->bString[1] == PRC_CHINESE);
    NT_ASSERT(((PUSB_DEVICE_DESCRIPTOR)g_UsbDeviceDescriptor)->iManufacturer == g_ManufacturerIndex);
    NT_ASSERT(((PUSB_DEVICE_DESCRIPTOR)g_UsbDeviceDescriptor)->iProduct == g_ProductIndex);

    NT_ASSERT(((PUSB_DEVICE_DESCRIPTOR)g_UsbDeviceDescriptor)->bLength ==
        sizeof(USB_DEVICE_DESCRIPTOR));
    NT_ASSERT(sizeof(g_UsbDeviceDescriptor) == sizeof(USB_DEVICE_DESCRIPTOR));
    NT_ASSERT(((PUSB_CONFIGURATION_DESCRIPTOR)g_UsbConfigDescriptorSet)->wTotalLength ==
        sizeof(g_UsbConfigDescriptorSet));
    NT_ASSERT(((PUSB_STRING_DESCRIPTOR)g_LanguageDescriptor)->bLength ==
        sizeof(g_LanguageDescriptor));

    NT_ASSERT(((PUSB_DEVICE_DESCRIPTOR)g_UsbDeviceDescriptor)->bDescriptorType ==
        USB_DEVICE_DESCRIPTOR_TYPE);
    NT_ASSERT(((PUSB_CONFIGURATION_DESCRIPTOR)g_UsbConfigDescriptorSet)->bDescriptorType ==
        USB_CONFIGURATION_DESCRIPTOR_TYPE);
    NT_ASSERT(((PUSB_STRING_DESCRIPTOR)g_LanguageDescriptor)->bDescriptorType ==
        USB_STRING_DESCRIPTOR_TYPE);
}


// END ------------------ descriptor -------------------------------





NTSTATUS
Usb_Initialize(
    _In_ WDFDEVICE WdfDevice
)
{
    NTSTATUS                                status;
    PUDECX_USBCONTROLLER_CONTEXT            controllerContext;
    UDECX_USB_DEVICE_STATE_CHANGE_CALLBACKS   callbacks;



    //
    // Allocate per-controller private contexts used by other source code modules (I/O,
    // etc.)
    //


    controllerContext = GetUsbControllerContext(WdfDevice);

    UsbValidateConstants();

    controllerContext->ChildDeviceInit = UdecxUsbDeviceInitAllocate(WdfDevice);

    if (controllerContext->ChildDeviceInit == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate UDECXUSBDEVICE_INIT %!STATUS!", status);
        goto exit;
    }

    //
    // State changed callbacks
    //
    UDECX_USB_DEVICE_CALLBACKS_INIT(&callbacks);

    callbacks.EvtUsbDeviceLinkPowerEntry = UsbDevice_EvtUsbDeviceLinkPowerEntry;
    callbacks.EvtUsbDeviceLinkPowerExit = UsbDevice_EvtUsbDeviceLinkPowerExit;
    callbacks.EvtUsbDeviceSetFunctionSuspendAndWake = UsbDevice_EvtUsbDeviceSetFunctionSuspendAndWake;

    UdecxUsbDeviceInitSetStateChangeCallbacks(controllerContext->ChildDeviceInit, &callbacks);

    //
    // Set required attributes.
    //
    UdecxUsbDeviceInitSetSpeed(controllerContext->ChildDeviceInit, UdecxUsbHighSpeed);

    UdecxUsbDeviceInitSetEndpointsType(controllerContext->ChildDeviceInit, UdecxEndpointTypeSimple);

    //
    // Device descriptor
    //
    status = UdecxUsbDeviceInitAddDescriptor(controllerContext->ChildDeviceInit,
        (PUCHAR)g_UsbDeviceDescriptor,
        sizeof(g_UsbDeviceDescriptor));

    if (!NT_SUCCESS(status)) {

        goto exit;
    }


    //
    // String descriptors
    //
    status = UdecxUsbDeviceInitAddDescriptorWithIndex(controllerContext->ChildDeviceInit,
        (PUCHAR)g_LanguageDescriptor,
        sizeof(g_LanguageDescriptor),
        0);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    status = UdecxUsbDeviceInitAddStringDescriptor(controllerContext->ChildDeviceInit,
        &g_ManufacturerStringEnUs,
        g_ManufacturerIndex,
        AMERICAN_ENGLISH);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    status = UdecxUsbDeviceInitAddStringDescriptor(controllerContext->ChildDeviceInit,
        &g_ProductStringEnUs,
        g_ProductIndex,
        AMERICAN_ENGLISH);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    //
    // Remaining init requires lower edge interaction.  Postpone to Usb_ReadDescriptorsAndPlugIn.
    //

exit:

    //
    // On failure in this function (or later but still before creating the UDECXUSBDEVICE),
    // UdecxUsbDeviceInit will be freed by Usb_Destroy.
    //

    return status;
}





NTSTATUS
Usb_ReadDescriptorsAndPlugIn(
    _In_ WDFDEVICE WdfControllerDevice
)
{
    NTSTATUS                          status;
    PUSB_CONFIGURATION_DESCRIPTOR     pComputedConfigDescSet;
    WDF_OBJECT_ATTRIBUTES             attributes;
    PUDECX_USBCONTROLLER_CONTEXT      controllerContext;
    PUSB_CONTEXT                      deviceContext = NULL;
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS  pluginOptions;

    controllerContext = GetUsbControllerContext(WdfControllerDevice);
    pComputedConfigDescSet = NULL;

    //
    // Compute configuration descriptor dynamically.
    //
    pComputedConfigDescSet = (PUSB_CONFIGURATION_DESCRIPTOR)
        ExAllocatePoolWithTag(NonPagedPoolNx, sizeof(g_UsbConfigDescriptorSet), UDECXMBIM_POOL_TAG);

    if (pComputedConfigDescSet == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate %d bytes for temporary config descriptor %!STATUS!",
            sizeof(g_UsbConfigDescriptorSet), status);
        goto exit;
    }

    RtlCopyMemory(pComputedConfigDescSet,
        g_UsbConfigDescriptorSet,
        sizeof(g_UsbConfigDescriptorSet));

    status = UdecxUsbDeviceInitAddDescriptor(controllerContext->ChildDeviceInit,
        (PUCHAR)pComputedConfigDescSet,
        sizeof(g_UsbConfigDescriptorSet));

    if (!NT_SUCCESS(status)) {

        goto exit;
    }


    //
    // Create emulated USB device
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, USB_CONTEXT);

    status = UdecxUsbDeviceCreate(&controllerContext->ChildDeviceInit,
        &attributes,
        &(controllerContext->ChildDevice) );

    if (!NT_SUCCESS(status)) {

        goto exit;
    }


    status = Io_AllocateContext(controllerContext->ChildDevice);
    if (!NT_SUCCESS(status)) {

        goto exit;
    }


    deviceContext = GetUsbDeviceContext(controllerContext->ChildDevice);

    // create link to parent
    deviceContext->ControllerDevice = WdfControllerDevice;


    LogInfo(TRACE_DEVICE, "USB device created, controller=%p, UsbDevice=%p",
        WdfControllerDevice, controllerContext->ChildDevice);

    deviceContext->IsAwake = TRUE;  // for some strange reason, it starts out awake!

    //
    // Create static endpoints.
    //
    status = UsbCreateEndpointObj(controllerContext->ChildDevice,
        USB_DEFAULT_ENDPOINT_ADDRESS,
        &(deviceContext->UDEFX2ControlEndpoint) );

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    status = UsbCreateEndpointObj(controllerContext->ChildDevice,
        g_BulkOutEndpointAddress, 
        &(deviceContext->UDEFX2BulkOutEndpoint) );

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    status = UsbCreateEndpointObj(controllerContext->ChildDevice,
        g_BulkInEndpointAddress,
        &(deviceContext->UDEFX2BulkInEndpoint));

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    status = UsbCreateEndpointObj(controllerContext->ChildDevice,
        g_InterruptEndpointAddress,
        &(deviceContext->UDEFX2InterruptInEndpoint));

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    //
    // This begins USB communication and prevents us from modifying descriptors and simple endpoints.
    //
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS_INIT(&pluginOptions);
    pluginOptions.Usb20PortNumber = 1;
    status = UdecxUsbDevicePlugIn(controllerContext->ChildDevice, &pluginOptions);


    LogInfo(TRACE_DEVICE, "Usb_ReadDescriptorsAndPlugIn ends successfully");

exit:

    //
    // Free temporary allocation always.
    //
    if (pComputedConfigDescSet != NULL) {

        ExFreePoolWithTag(pComputedConfigDescSet, UDECXMBIM_POOL_TAG);
        pComputedConfigDescSet = NULL;
    }

    return status;
}

NTSTATUS
Usb_Disconnect(
    _In_  WDFDEVICE WdfDevice
)
{
    NTSTATUS status;
    PUDECX_USBCONTROLLER_CONTEXT controllerCtx;
    IO_CONTEXT ioContextCopy;


    controllerCtx = GetUsbControllerContext(WdfDevice);

    Io_StopDeferredProcessing(controllerCtx->ChildDevice, &ioContextCopy);

    status = UdecxUsbDevicePlugOutAndDelete(controllerCtx->ChildDevice);
    // Not deleting the queues that belong to the controller, as this
    // happens only in the last disconnect.  But if we were to connect again,
    // we would need to do that as the queues would leak.

    if (!NT_SUCCESS(status)) {
        LogError(TRACE_DEVICE, "UdecxUsbDevicePlugOutAndDelete failed with %!STATUS!", status);
        goto exit;
    }

    Io_FreeEndpointQueues(&ioContextCopy);

    LogInfo(TRACE_DEVICE, "Usb_Disconnect ends successfully");

exit:

    return status;
}


VOID
Usb_Destroy(
    _In_ WDFDEVICE WdfDevice
)
{
    PUDECX_USBCONTROLLER_CONTEXT pControllerContext;

    pControllerContext = GetUsbControllerContext(WdfDevice);

    //
    // Free device init in case we didn't successfully create the device.
    //
    if (pControllerContext != NULL && pControllerContext->ChildDeviceInit != NULL) {

        UdecxUsbDeviceInitFree(pControllerContext->ChildDeviceInit);
        pControllerContext->ChildDeviceInit = NULL;
    }
    LogError(TRACE_DEVICE, "Usb_Destroy ends successfully");

    return;
}

VOID
Usb_UdecxUsbEndpointEvtReset(
    _In_ UCXCONTROLLER ctrl,
    _In_ UCXENDPOINT ep,
    _In_ WDFREQUEST r
)
{
    UNREFERENCED_PARAMETER(ctrl);
    UNREFERENCED_PARAMETER(ep);
    UNREFERENCED_PARAMETER(r);

    // TODO: endpoint reset. will require a different function prototype
}



NTSTATUS
UsbCreateEndpointObj(
    _In_   UDECXUSBDEVICE    WdfUsbChildDevice,
    _In_   UCHAR             epAddr,
    _Out_  UDECXUSBENDPOINT *pNewEpObjAddr
)
{
    NTSTATUS                      status;
    PUSB_CONTEXT                  pUsbContext;
    WDFQUEUE                      epQueue;
    UDECX_USB_ENDPOINT_CALLBACKS  callbacks;
    PUDECXUSBENDPOINT_INIT        endpointInit;


    pUsbContext = GetUsbDeviceContext(WdfUsbChildDevice);
    endpointInit = NULL;

    status = Io_RetrieveEpQueue(WdfUsbChildDevice, epAddr, &epQueue);

    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    endpointInit = UdecxUsbSimpleEndpointInitAllocate(WdfUsbChildDevice);

    if (endpointInit == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate endpoint init %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointInitSetEndpointAddress(endpointInit, epAddr);

    UDECX_USB_ENDPOINT_CALLBACKS_INIT(&callbacks, UsbEndpointReset);
    UdecxUsbEndpointInitSetCallbacks(endpointInit, &callbacks);

    status = UdecxUsbEndpointCreate(&endpointInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        pNewEpObjAddr );

    if (!NT_SUCCESS(status)) {

        LogError(TRACE_DEVICE, "UdecxUsbEndpointCreate failed for endpoint %x, %!STATUS!", epAddr, status);
        goto exit;
    }

    UdecxUsbEndpointSetWdfIoQueue( *pNewEpObjAddr,  epQueue);

exit:

    if (endpointInit != NULL) {

        NT_ASSERT(!NT_SUCCESS(status));
        UdecxUsbEndpointInitFree(endpointInit);
        endpointInit = NULL;
    }

    return status;
}





VOID
UsbEndpointReset(
    _In_ UDECXUSBENDPOINT UdecxUsbEndpoint,
    _In_ WDFREQUEST     Request
)
{
    UNREFERENCED_PARAMETER(UdecxUsbEndpoint);
    UNREFERENCED_PARAMETER(Request);
}



VOID
UsbDevice_EvtUsbDeviceEndpointsConfigure(
    _In_ UDECXUSBDEVICE                    UdecxUsbDevice,
    _In_ WDFREQUEST                        Request,
    _In_ PUDECX_ENDPOINTS_CONFIGURE_PARAMS Params
)
{
    UNREFERENCED_PARAMETER(UdecxUsbDevice);
    UNREFERENCED_PARAMETER(Params);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

NTSTATUS
UsbDevice_EvtUsbDeviceLinkPowerEntry(
    _In_ WDFDEVICE       UdecxWdfDevice,
    _In_ UDECXUSBDEVICE    UdecxUsbDevice )
{
    PUSB_CONTEXT pUsbContext;
    UNREFERENCED_PARAMETER(UdecxWdfDevice);

    pUsbContext = GetUsbDeviceContext(UdecxUsbDevice);
    Io_DeviceWokeUp(UdecxUsbDevice);
    pUsbContext->IsAwake = TRUE;
    LogInfo(TRACE_DEVICE, "USB Device power ENTRY");

    return STATUS_SUCCESS;
}

NTSTATUS
UsbDevice_EvtUsbDeviceLinkPowerExit(
    _In_ WDFDEVICE UdecxWdfDevice,
    _In_ UDECXUSBDEVICE UdecxUsbDevice,
    _In_ UDECX_USB_DEVICE_WAKE_SETTING WakeSetting )
{
    PUSB_CONTEXT pUsbContext;
    UNREFERENCED_PARAMETER(UdecxWdfDevice);

    pUsbContext = GetUsbDeviceContext(UdecxUsbDevice);
    pUsbContext->IsAwake = FALSE;

    Io_DeviceSlept(UdecxUsbDevice);

    LogInfo(TRACE_DEVICE, "USB Device power EXIT [wdfDev=%p, usbDev=%p], WakeSetting=%x", UdecxWdfDevice, UdecxUsbDevice, WakeSetting);
    return STATUS_SUCCESS;
}

NTSTATUS
UsbDevice_EvtUsbDeviceSetFunctionSuspendAndWake(
    _In_ WDFDEVICE                        UdecxWdfDevice,
    _In_ UDECXUSBDEVICE                   UdecxUsbDevice,
    _In_ ULONG                            Interface,
    _In_ UDECX_USB_DEVICE_FUNCTION_POWER  FunctionPower
)
{
    UNREFERENCED_PARAMETER(UdecxWdfDevice);
    UNREFERENCED_PARAMETER(UdecxUsbDevice);
    UNREFERENCED_PARAMETER(Interface);
    UNREFERENCED_PARAMETER(FunctionPower);

    // this never gets printed!
    LogInfo(TRACE_DEVICE, "USB Device SuspendAwakeState=%x", FunctionPower );

    return STATUS_SUCCESS;
}





