# USB_UDE_Sample
UDE (USB Device Emulation) Hardware-less sample, with matching Host-Side drivers. Used as a USB study test bed.

This project is a reference/sample that serves to illustrate programming UDE (USB Device Emulation) devices, per the architecture at infrastructure Windows provides at https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-emulated-device--ude--architecture

The project contains two Windows 10 kernel-mode drivers and one test application (for now).

The Kernel mode drivers are:
* UDEFX2.sys : Sample USB Device Emulation driver (emulates both a virtual USB controller and a virtual USB device attached to that virtual controller) 
* osrusbfx2.sys : Host-side driver, stolen originally from the WDK sample that talks to the OSR FX2 device, then modified to match the endpoints defined by UDEFX2.sys (above)

UDEFX2.sys defines these endpoints:
* a BULK/IN endpoint:  generates pattern data when read.
* a BULK/OUT endpoint: traces incoming data for confirmation.
* an INTERRUPT/IN endpoint:  currently generates an test interrupt 15 seconds after the device goes to sleep, and another 5 seconds after every other completion (in practice, these look like randomly-timed interrupts, to simulate both the case of Remote Wakeup and the case when interrupts are generated with the device still awake).

To build the drivers, you need:
* Visual Studio 2017
* The WDK, along with the WDK extension for Visual Studio

To install the drivers, have all the sys/inf/cat files that result from the build on a single directory, and run this command from an Administrator DOS prompt:
* installem.bat
(you'll need to have your system in Test Mode, as the drivers aren't signed)

To watch the driver behavior, you can use these scripts:
* tr_on.bat :  turns trace on
* tr_off.bat: dumps trace and stops

It is especially instructional to watch the traces during install/uninstall of the drivers, or when the test application works (see below).


Once the drivers are installed, you can test them with the test app, which is also stoken from the WDK sample and modified.  It can be used in 3 ways:
* osrusbfx2.exe -w 20   (writes 20 bytes to BULK/OUT - the trace shows a dump of what is written )
* osrusbfx2.exe -v -r 20   (reads 20 bytes from BULK/IN, dumps result to screen)
* osrusbfx2.exe -p  (waits for an interrupt, which is generated every ~30 ish - see INTERRUPT/IN endpoint description above)



