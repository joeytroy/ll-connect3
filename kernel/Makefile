obj-m := Lian_Li_SL_INFINITY.o

KERNEL_VERSION ?= $(shell uname -r)
KERNEL_DIR := /lib/modules/$(KERNEL_VERSION)/build
MODULE_DIR := /lib/modules/$(KERNEL_VERSION)/extra

# Set USE_LLVM=1 when building on clang-based kernels (CachyOS, Arch w/ clang)
LLVM_FLAG :=
ifdef USE_LLVM
LLVM_FLAG := LLVM=1
endif

all:
	make $(LLVM_FLAG) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make $(LLVM_FLAG) -C $(KERNEL_DIR) M=$(PWD) clean

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

# --- DKMS (recommended): module is rebuilt automatically on every kernel update ---
DKMS_NAME    := lian-li-sl-infinity
DKMS_VERSION := $(shell sed -n 's/^PACKAGE_VERSION="\(.*\)"/\1/p' dkms.conf)
DKMS_SRC     := /usr/src/$(DKMS_NAME)-$(DKMS_VERSION)

dkms-install:
	sudo rm -rf $(DKMS_SRC)
	sudo mkdir -p $(DKMS_SRC)
	sudo cp Lian_Li_SL_INFINITY.c Makefile dkms.conf $(DKMS_SRC)/
	sudo dkms add -m $(DKMS_NAME) -v $(DKMS_VERSION) 2>/dev/null || true
	sudo dkms build -m $(DKMS_NAME) -v $(DKMS_VERSION)
	sudo dkms install -m $(DKMS_NAME) -v $(DKMS_VERSION) --force
	sudo depmod -a
	@echo "SL-Infinity driver registered with DKMS; it will be rebuilt on kernel updates."

dkms-uninstall:
	sudo dkms remove -m $(DKMS_NAME) -v $(DKMS_VERSION) --all || true
	sudo rm -rf $(DKMS_SRC)
	sudo depmod -a

reload:
	sudo rmmod Lian_Li_SL_INFINITY 2>/dev/null || true
	sudo insmod Lian_Li_SL_INFINITY.ko
	@echo "Module reloaded. Check detection with:"
	@echo "  sudo dmesg | grep -i sli | tail -20"

.PHONY: all clean install uninstall reload dkms-install dkms-uninstall