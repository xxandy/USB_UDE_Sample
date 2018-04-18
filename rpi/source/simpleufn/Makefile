LINUX_TGT = $(HOME)/rpi/linux
EXTRA_CFLAGS := -I$(src)/src/inc -I$(LINUX_TGT)/drivers/usb/gadget -I$(LINUX_TGT)/drivers/usb/gadget/function -I$(src)/inc
obj-m += simpleufn.o



simpleufn-objs := f_one.o



SRC = $(HOME)/rpi/simpleufn


PREFIX = $(HOME)/rpi/tools/arm-bcm2708/arm-bcm2708-linux-gnueabi/bin/arm-bcm2708-linux-gnueabi-


all:

	$(MAKE) ARCH=arm CROSS_COMPILE=$(PREFIX) -C $(LINUX_TGT) M=$(SRC) modules




clean:

	$(MAKE) -C $(LINUX_TGT) M=$(SRC) clean


