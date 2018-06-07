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
BOOL G_fRead = FALSE;
BOOL G_fWrite = FALSE;
BOOL G_fGetDeviceInterrupt = FALSE;
BOOL G_fGenerateVirtualDeviceIntr = FALSE;
BOOL G_fMission = TRUE;
char *G_WriteText = "DefaultWrite";

BOOL G_fAutoBot = FALSE;
BOOL G_fCommandTrip = FALSE;

DEVICE_INTR_FLAGS G_IntrValue = 0;

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
    printf("-r <mission|response>\n");
    printf("-w <mission|response> [text] where n is number of bytes to write\n");


    printf("-i [n] where n is an hex number to send as INTERRUPT/IN from virtual device\n");
    printf("-v verbose -- dumps read data\n");
    printf("-p receive device interrupt\n");
    printf("-u to dump USB configuration and pipe info \n");

    printf("-a  -- autonomous back-channel agent(continuously wait for mission and complete)\n");
    printf("-c [text] -- send one command to autonomous agent (-a)\n");
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
            case 'r':
            case 'R':
                if (i+1 >= argc) {
                    Usage();
                    exit(1);
                }
                else {
                    G_fRead = TRUE;
                    G_fMission = ((strcmp(argv[i + 1], "mission") == 0) ? TRUE : FALSE);
                }
                i++;
                break;
            case 'w':
            case 'W':
                if (i+2 >= argc) {
                    Usage();
                    exit(1);
                } else {
                    G_fWrite = TRUE;
                    G_fMission = ((strcmp(argv[i+1], "mission") == 0) ? TRUE: FALSE);
                    G_WriteText = argv[i + 2];
                }
                i +=2;
                break;

            case 'u':
            case 'U':
                G_fDumpUsbConfig = TRUE;
                break;
            case 'p':
            case 'P':
                G_fGetDeviceInterrupt = TRUE;
                break;


            case 'a':
            case 'A':
                G_fAutoBot = TRUE;
                break;
            case 'c':
            case 'C':
                if (i + 1 >= argc) {
                    Usage();
                    exit(1);
                }
                else {
                    G_fCommandTrip = TRUE;
                    G_WriteText = argv[i + 1];
                }
                i++;
                break;

            case 'i':
            case 'I':
                if (i + 1 >= argc) {
                    Usage();
                    exit(1);
                } else {
                    G_IntrValue = strtol( argv[i + 1], NULL, 16 );
                }
                i++;
                G_fGenerateVirtualDeviceIntr = TRUE;
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




BOOL
ReadTextFrom(LPCGUID guid)
{
    HANDLE deviceHandle;
    DWORD  nBytesRead = 0;
    char   buffer[250];
    BOOL   success;

    printf("About to open device\n"); fflush(stdout);

    deviceHandle = OpenDevice(guid);

    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find device!\n"); fflush(stdout);

        return FALSE;

    }

    printf("Device open, read...\n"); fflush(stdout);

    success = ReadFile( deviceHandle, buffer, sizeof(buffer), &nBytesRead, NULL);
    if (!success) {
        printf("ReadFile failed - error %d\n", GetLastError());
    } else {
        buffer[(sizeof(buffer) / sizeof(buffer[0])) - 1] = 0;
        printf("ReadFile SUCCESS, text=%s, bytes=%d\n", buffer, nBytesRead);
    }

    CloseHandle(deviceHandle);
    return TRUE;
}





BOOL
GenerateDeviceInterrupt(DEVICE_INTR_FLAGS value)
{
    HANDLE          deviceHandle;
    DWORD           code;
    ULONG           index = 0;
    DEVICE_INTR_FLAGS  flagsState = 0;

    printf("About to open device\n"); fflush(stdout);

    deviceHandle = OpenDevice((LPGUID)&GUID_DEVINTERFACE_UDE_BACKCHANNEL);

    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find virtual controller device!\n"); fflush(stdout);

        return FALSE;

    }

    printf("Device open, will generate interrupt...\n"); fflush(stdout);

    if (!DeviceIoControl(deviceHandle,
        IOCTL_UDEFX2_GENERATE_INTERRUPT,
        &value,                // Ptr to InBuffer
        sizeof(value),         // Length of InBuffer
        NULL,                  // Ptr to OutBuffer
        0,                     // Length of OutBuffer
        &index,                // BytesReturned
        0)) {                  // Ptr to Overlapped structure

        code = GetLastError();

        printf("DeviceIoControl failed with error 0x%x\n", code);

    }
    else
    {
        printf("DeviceIoControl SUCCESS , returned 0x%x, bytes=%d\n", flagsState, index);
    }

    CloseHandle(deviceHandle);
    return TRUE;
}





BOOL
GetDeviceInterrupt()
{
    HANDLE          deviceHandle;
    DWORD           code;
    ULONG           index = 0;
    DEVICE_INTR_FLAGS  flagsState = 0;

    printf("About to open device\n"); fflush(stdout);

    deviceHandle = OpenDevice((LPGUID)&GUID_DEVINTERFACE_HOSTUDE );

    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find any OSR FX2 devices!\n"); fflush(stdout);

        return FALSE;

    }

    printf("Device open, waiting for interrupt...\n"); fflush(stdout);

    if (!DeviceIoControl(deviceHandle,
                            IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE,
                            NULL,                  // Ptr to InBuffer
                            0,                     // Length of InBuffer
                            &flagsState,           // Ptr to OutBuffer
                            sizeof(flagsState),    // Length of OutBuffer
                            &index,                // BytesReturned
                            0)) {                  // Ptr to Overlapped structure

        code = GetLastError();

        printf("DeviceIoControl failed with error 0x%x\n", code);

    }
    else
    {
        printf("DeviceIoControl SUCCESS , returned 0x%x, bytes=%d\n", flagsState, index);
    }

    CloseHandle(deviceHandle);
    return TRUE;
}




void
AutoBot(LPCGUID guid)
{
    HANDLE deviceHandle;
    DWORD  nBytesRead = 0;
    DWORD nBytesWritten = 0;
    char   buffer[250];
    BOOL   success;
    DEVICE_INTR_FLAGS  value = 0;
    ULONG           index = 0;

    printf("About to open device\n"); fflush(stdout);

    deviceHandle = OpenDevice(guid);

    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find device!\n"); fflush(stdout);

        return;

    }

    printf("Device open Successfully!\n"); fflush(stdout);

    for (;; )
    {
        printf("\n\n\nWaiting for new mission...\n"); fflush(stdout);

        success = ReadFile(deviceHandle, buffer, sizeof(buffer), &nBytesRead, NULL);
        if (!success) {
            printf("ReadFile failed - error %d\n", GetLastError());
            continue;
        }

        buffer[(sizeof(buffer) / sizeof(buffer[0])) - 1] = 0;
        printf("Got a Mission!!, text=%s, bytes=%d\n", buffer, nBytesRead);
        for (int x = 0; x < 8; ++x)
        {
            printf("Working on the mission.. ticktock...\n"); fflush(stdout);
            Sleep(1500);
        }



        strncat_s(buffer, (sizeof(buffer) / sizeof(buffer[0])),
            "_response", _TRUNCATE );
        buffer[(sizeof(buffer) / sizeof(buffer[0])) - 1] = 0;

        printf("About to transmit response\n"); fflush(stdout);
        success = WriteFile(deviceHandle, buffer, (DWORD)(strlen(buffer) + 1), &nBytesWritten, NULL);
        if (!success) {
            printf("WriteFile failed - error %d\n", GetLastError());
            continue;
        }

        value = MISSION_SUCCEEDED; // means done


        printf("Response stored, will generate interrupt!\n"); fflush(stdout);

        if (!DeviceIoControl(deviceHandle,
            IOCTL_UDEFX2_GENERATE_INTERRUPT,
            &value,                // Ptr to InBuffer
            sizeof(value),         // Length of InBuffer
            NULL,                  // Ptr to OutBuffer
            0,                     // Length of OutBuffer
            &index,                // BytesReturned
            0))
        {                  // Ptr to Overlapped structure
            printf("DeviceIoControl failed with error 0x%x\n", GetLastError());
            continue;
        }

        printf("Notification succeeded!\n");

    }

}





BOOL
CommandTrip(LPCGUID guid, const char *commandStr)
{
    HANDLE deviceHandle;
    DWORD  nBytesRead = 0;
    DWORD nBytesWritten = 0;
    char   buffer[250];
    BOOL   success;
    DEVICE_INTR_FLAGS  value = 0;
    ULONG           index = 0;

    printf("About to open device\n"); fflush(stdout);

    deviceHandle = OpenDevice(guid);

    if (deviceHandle == INVALID_HANDLE_VALUE) {

        printf("Unable to find device!\n"); fflush(stdout);

        return FALSE;

    }

    printf("Device open Successfully!\n"); fflush(stdout);
    success = WriteFile(deviceHandle, commandStr, (DWORD)(strlen(commandStr) + 1), &nBytesWritten, NULL);
    if (!success) {
        printf("WriteFile failed - error %d\n", GetLastError());
        goto exit;
    }

    printf("Mission sent with %d bytes, waiting for response interrupt...\n", nBytesWritten);

    if (!DeviceIoControl(deviceHandle,
        IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE,
        NULL,                  // Ptr to InBuffer
        0,                     // Length of InBuffer
        &value,           // Ptr to OutBuffer
        sizeof(value),    // Length of OutBuffer
        &index,                // BytesReturned
        0)) {                  // Ptr to Overlapped structure
        printf("DeviceIoControl failed with error 0x%x\n", GetLastError());
        goto exit;
    }

    if (index != sizeof(value) || (value != MISSION_SUCCEEDED))
    {
        printf("Interrupt indicates error! size=%d value=%x\n", index, value );
        goto exit;
    }

    printf("Interrupt indicates success! Will get response\n");

    success = ReadFile(deviceHandle, buffer, sizeof(buffer), &nBytesRead, NULL);
    if (!success) {
        printf("ReadFile failed - error %d\n", GetLastError());
        goto exit;
    }

    buffer[(sizeof(buffer) / sizeof(buffer[0])) - 1] = 0;
    printf("Got a Response!!, text=%s, bytes=%d\n", buffer, nBytesRead);

exit:
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
    else if (G_fGetDeviceInterrupt) {
        printf("About to get device interrupt\n"); fflush(stdout);
        GetDeviceInterrupt();
    }
    else if (G_fGenerateVirtualDeviceIntr) {
        printf("About to generate device interrupt\n"); fflush(stdout);
        GenerateDeviceInterrupt(G_IntrValue);
    }
    else if (G_fWrite) {
        LPCGUID dguid = (G_fMission ? &GUID_DEVINTERFACE_HOSTUDE : &GUID_DEVINTERFACE_UDE_BACKCHANNEL);
        printf("About to write %s %s\n", (G_fMission ? "mission" : "response"), G_WriteText); fflush(stdout);
        WriteTextTo(dguid, G_WriteText);
    }
    else if (G_fRead) {
        LPCGUID dguid = (G_fMission ? &GUID_DEVINTERFACE_UDE_BACKCHANNEL : &GUID_DEVINTERFACE_HOSTUDE);
        printf("About to read %s\n", (G_fMission ? "mission" : "response")); fflush(stdout);
        ReadTextFrom(dguid);
    } else if (G_fAutoBot) {
        AutoBot(&GUID_DEVINTERFACE_UDE_BACKCHANNEL);
    }
    else if (G_fCommandTrip) {
        CommandTrip(&GUID_DEVINTERFACE_HOSTUDE, G_WriteText);
    } else  {
        retValue = 1;
        Usage();
    }


    return retValue;
}
