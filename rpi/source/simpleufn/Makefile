LINUX_TGT = $(HOME)/rpi/linux
EXTRA_CFLAGS := -I$(src)/src/inc -I$(LINUX_TGT)/drivers/usb/gadget -I$(LINUX_TGT)/drivers/usb/gadget/function
obj-m += simpleufn.o



simpleufn-objs := f_hunchback.o f_simpless.o



SRC = $(HOME)/rpi/simpleufn


PREFIX = $(HOME)/rpi/tools/arm-bcm2708/arm-bcm2708-linux-gnueabi/bin/arm-bcm2708-linux-gnueabi-


all:

	$(MAKE) ARCH=arm CROSS_COMPILE=$(PREFIX) -C $(LINUX_TGT) M=$(SRC) modules




clean:

	$(MAKE) -C $(LINUX_TGT) M=$(SRC) clean


