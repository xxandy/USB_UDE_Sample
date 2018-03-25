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
#include "ucx/1.4/ucxobjects.h"
#include "usbdevice.tmh"



#define UDECXMBIM_POOL_TAG 'UDEI'



// ------------------ descriptor -------------------------------
#define g_ManufacturerIndex   1
#define g_ProductIndex        2
#define g_BulkOutEndpointAddress 2
#define g_BulkInEndpointAddress    0x84
#define g_InterruptEndpointAddress 0x86

#define UDEFX2_DEVICE_VENDOR_ID  0x47, 0x05 // little endian
#define UDEFX2_DEVICE_PROD_ID    0x2, 0x10 // little endian


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
    0x19, 0x00,                        // Length of this descriptor and all sub descriptors
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
        1,                                        // bNumEndpoints
        0xFF,                                     // bInterfaceClass
        0x00,                                     // bInterfaceSubClass
        0x00,                                     // bInterfaceProtocol
        0x00,                                     // iInterface

            // Bulk Out Endpoint descriptor
            0x07,                           // Descriptor size
            USB_ENDPOINT_DESCRIPTOR_TYPE,   // bDescriptorType
            g_BulkOutEndpointAddress,       // bEndpointAddress
            0x02,                           // bmAttributes - bulk
            0x00, 0x2,                      // wMaxPacketSize
            0x00                            // bInterval
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


// ------------------ descriptor -------------------------------





// IO --------------------------------------

static VOID
IoEvtControlUrb(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    WDF_USB_CONTROL_SETUP_PACKET setupPacket;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    //NT_VERIFY(IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB);

    if (IoControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
    {
        status = UdecxUrbRetrieveControlSetupPacket(Request, &setupPacket);

        if (!NT_SUCCESS(status))
        {
            LogError(TRACE_DEVICE, "WdfRequest %p is not a control URB? UdecxUrbRetrieveControlSetupPacket %!STATUS!",
                Request, status);
            UdecxUrbCompleteWithNtStatus(Request, status);
            goto exit;
        }


        LogInfo(TRACE_DEVICE, "control CODE %x, [type=%x dir=%x recip=%x] req=%x [wv = %x wi = %x wlen = %x]",
            IoControlCode,
            (int)(setupPacket.Packet.bm.Request.Type),
            (int)(setupPacket.Packet.bm.Request.Dir),
            (int)(setupPacket.Packet.bm.Request.Recipient),
            (int)(setupPacket.Packet.bRequest),
            (int)(setupPacket.Packet.wValue.Value),
            (int)(setupPacket.Packet.wIndex.Value),
            (int)(setupPacket.Packet.wLength )
        );




        UdecxUrbCompleteWithNtStatus(Request, STATUS_SUCCESS);
    }
    else
    {
        LogError(TRACE_DEVICE, "control NO submit code is %x", IoControlCode);
    }
exit:
    return;
}


static VOID
IoEvtBulkOutUrb(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    LogError(TRACE_DEVICE, "bulk out code is %x", IoControlCode);
    return;
}





static NTSTATUS
Io_RetrieveControlQueue(
    _In_
    WDFDEVICE  Device,
    _Out_
    WDFQUEUE * Queue
)
{
    NTSTATUS status;
    PIO_CONTEXT pIoContext;
    WDF_IO_QUEUE_CONFIG queueConfig;

    pIoContext = WdfDeviceGetIoContext(Device);

    status = STATUS_SUCCESS;
    *Queue = NULL;

    if (pIoContext->ControlQueue == NULL) {

        WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);

        //Sequential must specify this callback
        queueConfig.EvtIoInternalDeviceControl = IoEvtControlUrb;

        status = WdfIoQueueCreate(Device,
            &queueConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &pIoContext->ControlQueue);

        if (!NT_SUCCESS(status)) {

            LogError(TRACE_DEVICE, "WdfIoQueueCreate failed for ct queue %!STATUS!", status);
            goto exit;
        }
    }

    *Queue = pIoContext->ControlQueue;

exit:

    return status;
}







static NTSTATUS
Io_RetrieveBulkOutQueue(
    _In_
    WDFDEVICE  Device,
    _Out_
    WDFQUEUE * Queue
)
{
    NTSTATUS status;
    PIO_CONTEXT pIoContext;
    WDF_IO_QUEUE_CONFIG queueConfig;

    pIoContext = WdfDeviceGetIoContext(Device);

    status = STATUS_SUCCESS;
    *Queue = NULL;

    if (pIoContext->BulkOutQueue == NULL) {

        WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);

        //Sequential must specify this callback
        queueConfig.EvtIoInternalDeviceControl = IoEvtBulkOutUrb;

        status = WdfIoQueueCreate(Device,
            &queueConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &pIoContext->BulkOutQueue);

        if (!NT_SUCCESS(status)) {

            LogError(TRACE_DEVICE, "WdfIoQueueCreate failed for bulk2 queue %!STATUS!", status);
            goto exit;
        }
    }

    *Queue = pIoContext->BulkOutQueue;

exit:

    return status;
}








// IO -----------------------------













NTSTATUS
Usb_Initialize(
    _In_
    WDFDEVICE WdfDevice
)
{
    NTSTATUS                                status;
    PUSB_CONTEXT                            usbContext;
    UDECX_USB_DEVICE_STATE_CHANGE_CALLBACKS   callbacks;


    usbContext = WdfDeviceGetUsbContext(WdfDevice);

    UsbValidateConstants();

    usbContext->UdecxUsbDeviceInit = UdecxUsbDeviceInitAllocate(WdfDevice);

    if (usbContext->UdecxUsbDeviceInit == NULL) {

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

    UdecxUsbDeviceInitSetStateChangeCallbacks(usbContext->UdecxUsbDeviceInit, &callbacks);

    //
    // Set required attributes.
    //
    UdecxUsbDeviceInitSetSpeed(usbContext->UdecxUsbDeviceInit, UdecxUsbHighSpeed);

    UdecxUsbDeviceInitSetEndpointsType(usbContext->UdecxUsbDeviceInit, UdecxEndpointTypeSimple);

    //
    // Device descriptor
    //
    status = UdecxUsbDeviceInitAddDescriptor(usbContext->UdecxUsbDeviceInit,
        (PUCHAR)g_UsbDeviceDescriptor,
        sizeof(g_UsbDeviceDescriptor));

    if (!NT_SUCCESS(status)) {

        goto exit;
    }


    //
    // String descriptors
    //
    status = UdecxUsbDeviceInitAddDescriptorWithIndex(usbContext->UdecxUsbDeviceInit,
        (PUCHAR)g_LanguageDescriptor,
        sizeof(g_LanguageDescriptor),
        0);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    status = UdecxUsbDeviceInitAddStringDescriptor(usbContext->UdecxUsbDeviceInit,
        &g_ManufacturerStringEnUs,
        g_ManufacturerIndex,
        AMERICAN_ENGLISH);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    status = UdecxUsbDeviceInitAddStringDescriptor(usbContext->UdecxUsbDeviceInit,
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
    _In_
    WDFDEVICE WdfDevice
)
{
    NTSTATUS                        status;
    PUSB_CONTEXT                    usbContext;
    PUSB_CONFIGURATION_DESCRIPTOR   pComputedConfigDescSet;
    WDF_OBJECT_ATTRIBUTES           attributes;
    PUDECX_USBDEVICE_CONTEXT          deviceContext;
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS  pluginOptions;
    usbContext = WdfDeviceGetUsbContext(WdfDevice);
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

    status = UdecxUsbDeviceInitAddDescriptor(usbContext->UdecxUsbDeviceInit,
        (PUCHAR)pComputedConfigDescSet,
        sizeof(g_UsbConfigDescriptorSet));

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    //
    // Create emulated USB device
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, UDECX_USBDEVICE_CONTEXT);

    status = UdecxUsbDeviceCreate(&usbContext->UdecxUsbDeviceInit,
        &attributes,
        &usbContext->UDEFX2Device);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    deviceContext = UdecxDeviceGetContext(usbContext->UDEFX2Device);
    deviceContext->WdfDevice = WdfDevice;

    //
    // Create static endpoints.
    //
    status = UsbCreateControlEndpoint(WdfDevice);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    /*
    // TNEXT
    status = UsbCreateInterruptEndpoint(WdfDevice);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }
    status = UsbCreateBulkInEndpoint(WdfDevice);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }
    */

    status = UsbCreateBulkOutEndpoint(WdfDevice);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    //
    // This begins USB communication and prevents us from modifying descriptors and simple endpoints.
    //
    UDECX_USB_DEVICE_PLUG_IN_OPTIONS_INIT(&pluginOptions);
    pluginOptions.Usb20PortNumber = 1;
    status = UdecxUsbDevicePlugIn(usbContext->UDEFX2Device, &pluginOptions);

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
    _In_
    WDFDEVICE WdfDevice
)
{
    NTSTATUS status;
    PUSB_CONTEXT pUsbContext;


    pUsbContext = WdfDeviceGetUsbContext(WdfDevice);

    status = UdecxUsbDevicePlugOutAndDelete(pUsbContext->UDEFX2Device);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

exit:

    return status;
}


VOID
Usb_Destroy(
    _In_
    WDFDEVICE WdfDevice
)
{
    PUSB_CONTEXT pUsbContext;

    pUsbContext = WdfDeviceGetUsbContext(WdfDevice);

    //
    // Free device init in case we didn't successfully create the device.
    //
    if (pUsbContext != NULL && pUsbContext->UdecxUsbDeviceInit != NULL) {

        UdecxUsbDeviceInitFree(pUsbContext->UdecxUsbDeviceInit);
        pUsbContext->UdecxUsbDeviceInit = NULL;
    }

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
UsbCreateControlEndpoint(
    _In_
    WDFDEVICE WdfDevice
)
{
    NTSTATUS                    status;
    PUSB_CONTEXT                pUsbContext;
    WDFQUEUE                    controlQueue;
    UDECX_USB_ENDPOINT_CALLBACKS  callbacks;
    PUDECXUSBENDPOINT_INIT        endpointInit;

    UNREFERENCED_PARAMETER(callbacks);

    pUsbContext = WdfDeviceGetUsbContext(WdfDevice);
    endpointInit = NULL;

    status = Io_RetrieveControlQueue(WdfDevice, &controlQueue);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    endpointInit = UdecxUsbSimpleEndpointInitAllocate(pUsbContext->UDEFX2Device);

    if (endpointInit == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate endpoint init %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointInitSetEndpointAddress(endpointInit, USB_DEFAULT_ENDPOINT_ADDRESS);

    UDECX_USB_ENDPOINT_CALLBACKS_INIT(&callbacks, UsbEndpointReset);
    UdecxUsbEndpointInitSetCallbacks(endpointInit, &callbacks);

    status = UdecxUsbEndpointCreate(&endpointInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pUsbContext->UDEFX2ControlEndpoint );

    if (!NT_SUCCESS(status)) {

        LogError(TRACE_DEVICE, "UdecxUsbEndpointCreate failed for control endpoint %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointSetWdfIoQueue(pUsbContext->UDEFX2ControlEndpoint,
        controlQueue);

exit:

    if (endpointInit != NULL) {

        NT_ASSERT(!NT_SUCCESS(status));
        UdecxUsbEndpointInitFree(endpointInit);
        endpointInit = NULL;
    }

    return status;
}











NTSTATUS
UsbCreateBulkOutEndpoint(
    _In_
    WDFDEVICE WdfDevice
)
{
    NTSTATUS                    status;
    PUSB_CONTEXT                pUsbContext;
    WDFQUEUE                    bulkOutQueue;
    UDECX_USB_ENDPOINT_CALLBACKS  callbacks;
    PUDECXUSBENDPOINT_INIT        endpointInit;

    UNREFERENCED_PARAMETER(callbacks);

    pUsbContext = WdfDeviceGetUsbContext(WdfDevice);
    endpointInit = NULL;

    status = Io_RetrieveBulkOutQueue(WdfDevice, &bulkOutQueue);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    endpointInit = UdecxUsbSimpleEndpointInitAllocate(pUsbContext->UDEFX2Device);

    if (endpointInit == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate endpoint init %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointInitSetEndpointAddress(endpointInit, g_BulkOutEndpointAddress);

    UDECX_USB_ENDPOINT_CALLBACKS_INIT(&callbacks, UsbEndpointReset);
    UdecxUsbEndpointInitSetCallbacks(endpointInit, &callbacks);

    status = UdecxUsbEndpointCreate(&endpointInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pUsbContext->UDEFX2BulkOutEndpoint );

    if (!NT_SUCCESS(status)) {

        LogError(TRACE_DEVICE, "UdecxUsbEndpointCreate failed for control endpoint %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointSetWdfIoQueue(pUsbContext->UDEFX2BulkOutEndpoint,
        bulkOutQueue);

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
    _In_
    UDECXUSBENDPOINT UdecxUsbEndpoint,
    _In_
    WDFREQUEST     Request
)
{
    UNREFERENCED_PARAMETER(UdecxUsbEndpoint);
    UNREFERENCED_PARAMETER(Request);
}



VOID
UsbDevice_EvtUsbDeviceEndpointsConfigure(
    _In_
    UDECXUSBDEVICE                    UdecxUsbDevice,
    _In_
    WDFREQUEST                      Request,
    _In_
    PUDECX_ENDPOINTS_CONFIGURE_PARAMS Params
)
{
    UNREFERENCED_PARAMETER(UdecxUsbDevice);
    UNREFERENCED_PARAMETER(Params);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

NTSTATUS
UsbDevice_EvtUsbDeviceLinkPowerEntry(
    _In_
    WDFDEVICE       UdecxWdfDevice,
    _In_
    UDECXUSBDEVICE    UdecxUsbDevice
)
{
    UNREFERENCED_PARAMETER(UdecxWdfDevice);
    UNREFERENCED_PARAMETER(UdecxUsbDevice);

    return STATUS_SUCCESS;
}

NTSTATUS
UsbDevice_EvtUsbDeviceLinkPowerExit(
    _In_
    WDFDEVICE                   UdecxWdfDevice,
    _In_
    UDECXUSBDEVICE                UdecxUsbDevice,
    _In_
    UDECX_USB_DEVICE_WAKE_SETTING WakeSetting
)
{
    UNREFERENCED_PARAMETER(UdecxWdfDevice);
    UNREFERENCED_PARAMETER(UdecxUsbDevice);
    UNREFERENCED_PARAMETER(WakeSetting);

    return STATUS_SUCCESS;
}

NTSTATUS
UsbDevice_EvtUsbDeviceSetFunctionSuspendAndWake(
    _In_
    WDFDEVICE                      UdecxWdfDevice,
    _In_
    UDECXUSBDEVICE                   UdecxUsbDevice,
    _In_
    ULONG                          Interface,
    _In_
    UDECX_USB_DEVICE_FUNCTION_POWER  FunctionPower
)
{
    UNREFERENCED_PARAMETER(UdecxWdfDevice);
    UNREFERENCED_PARAMETER(UdecxUsbDevice);
    UNREFERENCED_PARAMETER(Interface);
    UNREFERENCED_PARAMETER(FunctionPower);

    return STATUS_SUCCESS;
}











/// not used YET

/*

NTSTATUS
UsbCreateInterruptEndpoint(
    _In_
    WDFDEVICE WdfDevice
)
{
    NTSTATUS                    status;
    PUSB_CONTEXT                pUsbContext;
    PUDECXUSBENDPOINT_INIT        endpointInit;
    WDFQUEUE                    interruptQueue;
    UDECX_USB_ENDPOINT_CALLBACKS  callbacks;

    UNREFERENCED_PARAMETER(callbacks);

    pUsbContext = WdfDeviceGetUsbContext(WdfDevice);
    endpointInit = NULL;

    status = Io_RetrieveInterruptQueue(WdfDevice, &interruptQueue);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    endpointInit = UdecxUsbSimpleEndpointInitAllocate(pUsbContext->UDEFX2Device);

    if (endpointInit == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate endpoint init %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointInitSetEndpointAddress(endpointInit, g_InterruptEndpointAddress);

    UDECX_USB_ENDPOINT_CALLBACKS_INIT(&callbacks, UsbEndpointReset);
    UdecxUsbEndpointInitSetCallbacks(endpointInit, &callbacks);

    status = UdecxUsbEndpointCreate(&endpointInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pUsbContext->UDEFX2InterruptInEndpoint);

    if (!NT_SUCCESS(status)) {

        LogError(TRACE_DEVICE, "UdecxUsbEndpointCreate failed for interrupt endpoint %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointSetWdfIoQueue(pUsbContext->UDEFX2InterruptInEndpoint,
        interruptQueue);

exit:

    if (endpointInit != NULL) {

        NT_ASSERT(!NT_SUCCESS(status));
        UdecxUsbEndpointInitFree(endpointInit);
        endpointInit = NULL;
    }

    return status;
}


NTSTATUS
UsbCreateBulkInEndpoint(
    _In_
    WDFDEVICE WdfDevice
)
{
    NTSTATUS                    status;
    PUSB_CONTEXT                pUsbContext;
    WDFQUEUE                    bulkInQueue;
    UDECX_USB_ENDPOINT_CALLBACKS  callbacks;
    PUDECXUSBENDPOINT_INIT        endpointInit;

    UNREFERENCED_PARAMETER(callbacks);

    pUsbContext = WdfDeviceGetUsbContext(WdfDevice);
    endpointInit = NULL;

    status = Io_RetrieveBulkInQueue(WdfDevice, &bulkInQueue);

    if (!NT_SUCCESS(status)) {

        goto exit;
    }

    endpointInit = UdecxUsbSimpleEndpointInitAllocate(pUsbContext->UDEFX2Device);

    if (endpointInit == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        LogError(TRACE_DEVICE, "Failed to allocate endpoint init %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointInitSetEndpointAddress(endpointInit, g_BulkInEndpointAddress);

    UDECX_USB_ENDPOINT_CALLBACKS_INIT(&callbacks, UsbEndpointReset);
    UdecxUsbEndpointInitSetCallbacks(endpointInit, &callbacks);

    status = UdecxUsbEndpointCreate(&endpointInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pUsbContext->UDEFX2BulkInEndpoint);

    if (!NT_SUCCESS(status)) {

        LogError(TRACE_DEVICE, "UdecxUsbEndpointCreate failed for control endpoint %!STATUS!", status);
        goto exit;
    }

    UdecxUsbEndpointSetWdfIoQueue(pUsbContext->UDEFX2BulkInEndpoint,
        bulkInQueue);

exit:

    if (endpointInit != NULL) {

        NT_ASSERT(!NT_SUCCESS(status));
        UdecxUsbEndpointInitFree(endpointInit);
        endpointInit = NULL;
    }

    return status;
}

*/
