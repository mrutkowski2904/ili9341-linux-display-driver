obj-m += ili9341_mod.o
ili9341_mod-objs += ili9341.o display.o

all: module
	echo Built module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean