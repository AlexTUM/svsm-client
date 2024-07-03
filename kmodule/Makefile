obj-m += module.o

WD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(WD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(WD) clean
