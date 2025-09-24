# 커널 모듈용
obj-m := ledkey_simple_dev.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -o ledkey_server ledkey_server.c -lpthread

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f ledkey_server

install:
	sudo insmod ledkey_simple_dev.ko
	sudo chmod 666 /dev/ledkey

uninstall:
	sudo rmmod ledkey_simple_dev