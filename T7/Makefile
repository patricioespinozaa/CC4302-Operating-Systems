ccflags-y := -Wall -std=gnu99 -Wno-declaration-after-statement

obj-m := cena-filosofos.o
cena-filosofos-objs := kmutex.o cena-filosofos-impl.o

KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)

#include $(KDIR)/.config

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

cena-filosofos.o kmutex.o: kmutex.h

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
