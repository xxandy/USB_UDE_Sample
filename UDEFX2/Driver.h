/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <wdfusb.h>
#include <usbdlib.h>
#include <ude/1.0/UdeCx.h>
#include <initguid.h>
#include <usbioctl.h>

#include "device.h"
#include "trace.h"


#define UDEFX_POOL_TAG 'UDEX'


EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD UDEFX2EvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP UDEFX2EvtDriverContextCleanup;

EXTERN_C_END
