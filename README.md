# USB UDE Sample
UDE (USB Device Emulation) Virtual USB device sample (hardware-less), with matching Host-Side drivers. Used as a USB study test bed. Can also be the starting point for developing other Virtual USB devices.

This project is a reference/sample that illustrates programming UDE (USB Device Emulation) devices, per the architecture/infrastructure Windows provides at https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-emulated-device--ude--architecture

The project contains two Windows 10 kernel-mode drivers and one test application (for now).

The Kernel mode drivers are:
* <B>UDEFX2.sys</b> : Sample USB Device Emulation driver (emulates both a virtual USB controller and a virtual USB device attached to that virtual controller) 
* <B>hostude.sys</b> : Host-side driver, stolen originally from the WDK sample that talks to the OSR FX2 device, then modified to match the endpoints defined by UDEFX2.sys (above)

UDEFX2.sys defines these endpoints:
* <B>a BULK/IN endpoint</B>:  generates pattern data when read.
* <B>a BULK/OUT endpoint</B>: traces incoming data for confirmation.
* <B>an INTERRUPT/IN endpoint</B>:  Upon request from a back-channel controller test app (via a back-channel IOCTL), generates an interrupt from the virtual device. Interrupt also generates Remote Wakeup if the virtual device is in low-power mode.

To build the drivers, you need:
* Visual Studio 2017
* The WDK, along with the WDK extension for Visual Studio

To install the drivers, have all the sys/inf/cat files that result from the build on a single directory, and run this command from an Administrator DOS prompt:
* <B>installem.bat</B>
(you'll need to have your system in Test Mode, as the drivers aren't signed)

To watch the driver behavior, you can use these scripts:
* <B>tr_on.bat</B> :  turns trace on
* <B>tr_off.bat</B>: dumps trace and stops

It is especially instructional to watch the traces during install/uninstall of the drivers, or when the test application works (see below).


Once the drivers are installed, you can test them with the test app, which is also stolen from the WDK sample and modified.  It can be used a few ways:

*FULL BLOWN TEST*
* *hostudetest.exe -a*   (goes into a loop waiting for commands over USB. Those can be sent from a separate instance, with the -c flag)
* *hostudetest.exe -c somemission*   
1) sends "somemission" over BULK/OUT
2)  waits for an interrupt on INTERRUPT/IN
3) finally  then reads USB/IN for response to the mission

*INTERRUPT ONLY TEST*
* *hostudetest.exe -i abc* (generates an INTERRUPT/IN transfer with a 4-byte little-endian payload matching the hexadecimal parameter provided)
* *hostudetest.exe -p*  (waits for an interrupt, which can be generated in a separate instance of the test app, with the -i command - see INTERRUPT/IN endpoint description above)



