obj-m += svsm-client-handler.o

KVERSION = $(shell uname -r)
WD := $(CURDIR)

.PHONY: all clean reload reload_full

all:
	make -C /lib/modules/$(KVERSION)/build M=$(WD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(WD) clean

reload:
	rmmod svsm_client_handler
	insmod svsm-client-handler.ko

reload_full: | clean all reload
