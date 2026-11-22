obj-m := sys_led.o
KERNEL_DIR ?= ../../kernel
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean