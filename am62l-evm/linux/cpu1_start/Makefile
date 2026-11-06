# Out-of-tree build: make KDIR=/path/to/prepared/kernel/tree [CROSS_COMPILE=aarch64-none-linux-gnu-]
# On the board, KDIR defaults to /lib/modules/$(uname -r)/build.
KDIR ?= /lib/modules/$(shell uname -r)/build
ARCH ?= arm64
obj-m := cpu1_start.o

all:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(CURDIR) modules
clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) M=$(CURDIR) clean
