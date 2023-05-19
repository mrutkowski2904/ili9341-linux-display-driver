obj-m += ili9341_mod.o
ili9341_mod-objs += ili9341.o display.o

all: module dt
	echo Built module and dtbo

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

dt: pi3a.dts
	dtc -@ -I dts -O dtb -o pi3a.dtbo pi3a.dts

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf pi3a.dtbo