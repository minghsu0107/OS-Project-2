obj-m += ksocket.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) EXTRA_CFLAGS="$(CFLAGS)" modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
