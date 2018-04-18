LINUX_TGT = $(HOME)/rpi/linux
EXTRA_CFLAGS := -I$(src)/src/inc -I$(LINUX_TGT)/drivers/usb/gadget -I$(LINUX_TGT)/drivers/usb/gadget/udc -I$(LINUX_TGT)/drivers/usb/gadget/function -I$(src)/../simpleufn/inc
obj-m += simplegadget.o



simplegadget-objs := one.o



SRC = $(HOME)/rpi/simplegadget


PREFIX = $(HOME)/rpi/tools/arm-bcm2708/arm-bcm2708-linux-gnueabi/bin/arm-bcm2708-linux-gnueabi-


all:

	$(MAKE) ARCH=arm CROSS_COMPILE=$(PREFIX) -C $(LINUX_TGT) M=$(SRC) modules




clean:

	$(MAKE) -C $(LINUX_TGT) M=$(SRC) clean


