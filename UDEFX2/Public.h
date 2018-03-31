/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

DEFINE_GUID(GUID_DEVINTERFACE_UDEFX2,
	0x7f4eb2c7, 0x43b5, 0x417e, 0x9e, 0x7f, 0x22, 0xaa, 0x06, 0xaa, 0x14, 0xe8);
