obj-m := Lian_Li_SL_INFINITY.o

KERNEL_VERSION := $(shell uname -r)
KERNEL_DIR := /lib/modules/$(KERNEL_VERSION)/build
MODULE_DIR := /lib/modules/$(KERNEL_VERSION)/extra

all:
	make LLVM=1 -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make LLVM=1 -C $(KERNEL_DIR) M=$(PWD) clean

install: all
	sudo mkdir -p $(MODULE_DIR)
	sudo cp Lian_Li_SL_INFINITY.ko $(MODULE_DIR)/
	sudo depmod -a
	@echo "SL-Infinity driver installed. Reload with:"
	@echo "  sudo rmmod Lian_Li_SL_INFINITY"
	@echo "  sudo modprobe Lian_Li_SL_INFINITY"

uninstall:
	sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
	sudo rm -f $(MODULE_DIR)/Lian_Li_SL_INFINITY.ko
	sudo depmod -a
	@echo "SL-Infinity driver removed"

reload:
	sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
	sudo insmod Lian_Li_SL_INFINITY.ko
	@echo "Module reloaded. Check detection with:"
	@echo "  sudo dmesg | grep -i sli | tail -20"

.PHONY: all clean install uninstall reload
