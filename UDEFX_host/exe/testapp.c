/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    TESTAPP.C

Abstract:

    Console test app for HOSTUDE driver and for UDEFX2 driver.
	This app can send commands to two drivers:
	   - HOSTUDE - host-side driver for the Virtual USB device
	   - UDEFX2 (back-channel)  -  test interface used to feed info/get info
         from the UDEFX2 device side, by means other than USB. 
		 Back-channel is used as scaffolding to test the USB interface  
		 between HOSTUDE and UDEFX2.

Environment:

    user mode only

--*/


#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "devioctl.h"
#include "strsafe.h"

#pragma warning(disable:4200)  //
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

#include <cfgmgr32.h>
#include <basetyps.h>
#include "usbdi.h"
#include "public.h"
#include "..\..\UDEFX2\public.h"

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)

#define WHILE(a) \
while(__pragma(warning(disable:4127)) a __pragma(warning(disable:4127)))

#define MAX_DEVPATH_LENGTH 256
#define NUM_ASYNCH_IO   100
#define BUFFER_SIZE     1024
#define READER_TYPE   1
#define WRITER_TYPE   2


#define MISSION_SUCCEEDED 10
#define MISSION_FAILED 12


BOOL G_fDumpUsbConfig = FALSE;    // flags set in response to console command line switches
BOOL G_fWrite = FALSE;
char *G_WriteText = "DefaultWrite";



BOOL
DumpUsbConfig( // defined in dump.c
    );


_Success_(return)
BOOL
GetDevicePath(
    _In_  LPGUID InterfaceGuid,
    _Out_writes_z_(BufLen) PWCHAR DevicePath,
    _In_ size_t BufLen
    )
{
    CONFIGRET cr = CR_SUCCESS;
    PWSTR deviceInterfaceList = NULL;
    ULONG deviceInterfaceListLength = 0;
    PWSTR nextInterface;
    HRESULT hr = E_FAIL;
    BOOL bRet = TRUE;

    cr = CM_Get_Device_Interface_List_Size(
        &deviceInterfaceListLength,
        InterfaceGuid,
        NULL,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list size.\n", cr);
        goto clean0;
    }

    if (deviceInterfaceListLength <= 1) {
        bRet = FALSE;
        printf("Error: No active device interfaces found.\n"
            " Is the sample driver loaded?");
        goto clean0;
    }

    deviceInterfaceList = (PWSTR)malloc(deviceInterfaceListLength * sizeof(WCHAR));
    if (deviceInterfaceList == NULL) {
        bRet = FALSE;
        printf("Error allocating memory for device interface list.\n");
        goto clean0;
    }
    ZeroMemory(deviceInterfaceList, deviceInterfaceListLength * sizeof(WCHAR));

    cr = CM_Get_Device_Interface_List(
        InterfaceGuid,
        NULL,
        deviceInterfaceList,
        deviceInterfaceListLength,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) {
        printf("Error 0x%x retrieving device interface list.\n", cr);
        goto clean0;
    }

    nextInterface = deviceInterfaceList + wcslen(deviceInterfaceList) + 1;
    if (*nextInterface != UNICODE_NULL) {
        printf("Warning: More than one device interface instance found. \n"
            "Selecting first matching device.\n\n");
    }

    hr = StringCchCopy(DevicePath, BufLen, deviceInterfaceList);
    if (FAILED(hr)) {
        bRet = FALSE;
        printf("Error: StringCchCopy failed with HRESULT 0x%x", hr);
        goto clean0;
    }

clean0:
    if (deviceInterfaceList != NULL) {
        free(deviceInterfaceList);
    }
    if (CR_SUCCESS != cr) {
        bRet = FALSE;
    }

    return bRet;
}






_Check_return_
_Ret_notnull_
_Success_(return != INVALID_HANDLE_VALUE)
HANDLE
OpenDevice(
    _In_ LPCGUID pguid
    )

/*++
Routine Description:

    Called by main() to open an instance of our device.

Arguments:

    pguid - Device interface

Return Value:

    Device handle on success else INVALID_HANDLE_VALUE

--*/

{
    HANDLE hDev;
    WCHAR completeDeviceName[MAX_DEVPATH_LENGTH];

    if ( !GetDevicePath(
            (LPGUID)pguid,
            completeDeviceName,
            sizeof(completeDeviceName)/sizeof(completeDeviceName[0])) )
    {
        return  INVALID_HANDLE_VALUE;
    }

    printf("DeviceName = (%S)\n", completeDeviceName); fflush(stdout);

    hDev = CreateFile(completeDeviceName,
            GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL, // default security
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Failed to open the device, error - %d", GetLastError()); fflush(stdout);
    } else {
        printf("Opened the device successfully.\n"); fflush(stdout);
    }

    return hDev;
}



VOID
Usage()

/*++
Routine Description:

    Called by main() to dump usage info to the console when
    the app is called with no parms or with an invalid parm

Arguments:

    None

Return Value:

    None

--*/

{
    printf("Usage for osrusbfx2 testapp:\n");
    printf("-w [text] where n is number of bytes to write\n");


    printf("-v verbose -- dumps read data\n");
    printf("-u to dump USB configuration and pipe info \n");

    return;
}


void
Parse(
    _In_ int argc,
    _In_reads_(argc) LPSTR  *argv
    )

/*++
Routine Description:

    Called by main() to parse command line parms

Arguments:

    argc and argv that was passed to main()

Return Value:

    Sets global flags as per user function request

--*/

{
    int i;

    if ( argc < 2 ) // give usage if invoked with no parms
        Usage();

    for (i=0; i<argc; i++) {
        if (argv[i][0] == '-' ||
            argv[i][0] == '/') {
            switch(argv[i][1]) {
            case 'w':
            case 'W':
                if (i+1 >= argc) {
                    Usage();
                    exit(1);
                } else {
                    G_fWrite = TRUE;
                    G_WriteText = argv[i + 1];
                }
                i +=1;
                break;

            case 'u':
            case 'U':
                G_fDumpUsbConfig = TRUE;
                break;

            default:
                Usage();
            }
        }
    }
}



BOOL
WriteTextTo(LPCGUID guid, const char *pText )
{
    HANDLE deviceHandle;
    DWORD  nBytesWrite = 0;
    BOOL   success;

    printf("About to open device\n"); fflush(stdout);

    deviceHandle = OpenDevice( guid );

    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find device!\n"); fflush(stdout);

        return FALSE;

    }

    printf("Device open, will write...\n"); fflush(stdout);

    success = WriteFile(deviceHandle, pText, (DWORD)(strlen(pText) + 1), &nBytesWrite, NULL);
    if ( !success ) {
        printf("WriteFile failed - error %d\n", GetLastError());
    } else  {
        printf("WriteFile SUCCESS, text=%s, bytes=%d\n", pText, nBytesWrite);
    }

    CloseHandle(deviceHandle);
    return TRUE;
}





int
_cdecl
main(
    _In_ int argc,
    _In_reads_(argc) LPSTR  *argv
    )
/*++
Routine Description:

    Entry point to rwbulk.exe
    Parses cmdline, performs user-requested tests

Arguments:

    argc, argv  standard console  'c' app arguments

Return Value:

    Zero

--*/

{
    int    retValue = 0;

    Parse(argc, argv );

    //
    // dump USB configuation and pipe info
    //
    if (G_fDumpUsbConfig) {
        DumpUsbConfig();
    }
    else if (G_fWrite) {
        LPCGUID dguid = &GUID_DEVINTERFACE_HOSTUDE;
        printf("About to write %s\n", G_WriteText); fflush(stdout);
        WriteTextTo(dguid, G_WriteText);
    } else  {
        retValue = 1;
        Usage();
    }


    return retValue;
}
