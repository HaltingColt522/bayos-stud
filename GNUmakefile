BUILD_DIR ?= $(abspath $(CURDIR)/../build)
XBSTRAP ?= xbstrap

XB = cd $(BUILD_DIR) && $(XBSTRAP)

.DEFAULT_GOAL := all
.PHONY: all init kernel iso run clean distclean xbstrap rebuild-init
.PHONY: rebuild-mlibc rebuild-kernel clean-init clean-mlibc clean-kernel mlibc

all: iso

$(BUILD_DIR)/bootstrap.link:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(XBSTRAP) init $(CURDIR)

init: $(BUILD_DIR)/bootstrap.link mlibc

kernel: init
	$(XB) install --rebuild kernel limine

iso: kernel
	$(XB) run iso

run: iso
	$(XB) run qemu

mlibc:
	$(XB) install --rebuild mlibc

clean-init:
	rm -rf $(BUILD_DIR)/pkg-builds/init

clean-mlibc:
	rm -rf $(BUILD_DIR)/pkg-builds/mlibc

clean-kernel:
	rm -rf $(BUILD_DIR)/pkg-builds/kernel

rebuild-init:
	$(MAKE) clean-init
	$(MAKE) init

rebuild-mlibc:
	$(MAKE) clean-mlibc
	$(MAKE) mlibc

rebuild-kernel:
	$(MAKE) clean-kernel
	$(MAKE) kernel

clean-all:
	rm -f $(BUILD_DIR)/bayos.iso
	$(MAKE) clean-init
	$(MAKE) clean-mlibc
	$(MAKE) clean-kernel

rebuild:
	$(MAKE) clean-all
	$(MAKE) rebuild-mlibc
	$(MAKE) rebuild-init
	$(MAKE) rebuild-kernel

clean:
	rm -f $(BUILD_DIR)/bayos.iso
	rm -rf $(BUILD_DIR)/pkg-builds/kernel

# deletes WHOLE build-dir! handle with care
distclean:
	rm -rf $(BUILD_DIR)

xbstrap: init
	$(XB) $(ARGS)
