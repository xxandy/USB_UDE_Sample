/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    public.h

Abstract:

Environment:

    User & Kernel mode

--*/

#ifndef _PUBLIC_H
#define _PUBLIC_H

#include <initguid.h>

// {573E8C73-0CB4-4471-A1BF-FAB26C31D384}
DEFINE_GUID(GUID_DEVINTERFACE_HOSTUDE, 
            0x5ad9d323, 0xc478, 0x4ad2, 0xb6, 0x39, 0x9d, 0x2e, 0xdd, 0xbd, 0x3b, 0x2c );


#pragma warning(push)
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

//
// Define the structures that will be used by the IOCTL 
//  interface to the driver
//


#pragma warning(pop)


typedef ULONG DEVICE_INTR_FLAGS;
typedef DEVICE_INTR_FLAGS* PDEVICE_INTR_FLAGS;



#define IOCTL_INDEX             0x800
#define FILE_DEVICE_OSRUSBFX2   65500U

#define IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR CTL_CODE(FILE_DEVICE_OSRUSBFX2,     \
                                                     IOCTL_INDEX,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_READ_ACCESS)
                                                   
#define IOCTL_OSRUSBFX2_RESET_DEVICE  CTL_CODE(FILE_DEVICE_OSRUSBFX2,     \
                                                     IOCTL_INDEX + 1, \
                                                     METHOD_BUFFERED,         \
                                                     FILE_WRITE_ACCESS)

#define IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE CTL_CODE(FILE_DEVICE_OSRUSBFX2,\
                                                    IOCTL_INDEX + 9, \
                                                    METHOD_OUT_DIRECT, \
                                                    FILE_READ_ACCESS)





#endif
