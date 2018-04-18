# Raspberry Pi Folder


This folder has a set of scripts and utilities to allow the host driver of this depot to work
with a RaspBerry Pi Zero as the device (not just the virtual UDE device).

The Raspberry Pi Zero works as a "gadget", per the description at:
* Linux official: https://www.kernel.org/doc/Documentation/usb/gadget_configfs.txt
* Composite framework overview: https://lwn.net/Articles/395712/
* Gadget API for Linux: https://www.kernel.org/doc/html/v4.13/driver-api/usb/gadget.html


This is interesting in that you can write a UDE device to validate the host, and then
switch to working on a RaspBerry Pi Zero.


This folder contains:

* scrips_for_pi: scripts you run on the Raspberry Pi for configuration, utility, etc.
* source:  cross-compiled drivers on Ubuntu you can install on the Pi
* scripts_for_ubuntu:  scripts run an Ubuntu 32-bit box where the cross-compiler and kernel is installed.


Building the kernel:
The internet is full of guides on how to build - don't be fooled! Guides posted by people everywhere tend to get outdated quickly!
Use the latest official guide from: 
         https://www.raspberrypi.org/documentation/linux/kernel/building.md

In scripts_for_ubuntu, I built some scripts as shortcuts to speed up the procedure from that page - but be sure to check the page before using the scripts.

