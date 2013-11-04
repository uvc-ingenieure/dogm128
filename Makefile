ifneq ($(KERNELRELEASE),)
obj-m := dogm128.o dogm128fb.o

else
KDIR	:= ../linux-3.3.4
PWD	:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
endif

