TARGET_MOUDLE := reset-lpc11u68
obj-m += $(TARGET_MOUDLE).o
#KERNEL_SRC := /lib/modules/$(shell uname -r )/build
KERNEL_SRC := /mydata/github/linux-fslc
SRC := $(shell pwd)

all:
	make -C $(KERNEL_SRC) M=$(SRC) modules
clean:
	make -C $(KERNEL_SRC) M=$(SRC) clear
load:
	insmod ./$(TARGET_MOUDLE).ko
unload:
	rmmod ./$(TARGET_MOUDLE).ko
