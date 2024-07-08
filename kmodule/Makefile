obj-m += svsm-client-handler.o

KVERSION = $(shell uname -r)
WD := $(CURDIR)

all:
	make -C /lib/modules/$(KVERSION)/build M=$(WD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(WD) clean
