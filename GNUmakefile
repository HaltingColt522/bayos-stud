BUILD_DIR ?= $(abspath $(CURDIR)/../build)
XBSTRAP   ?= xbstrap

XB = cd "$(BUILD_DIR)" && $(XBSTRAP)

.DEFAULT_GOAL := all
.PHONY: all bootstrap mlibc init kernel iso run clean distclean

all: clean
	$(MAKE) run

$(BUILD_DIR)/bootstrap.link:
	mkdir -p "$(BUILD_DIR)"
	cd "$(BUILD_DIR)" && $(XBSTRAP) init "$(CURDIR)"

bootstrap: $(BUILD_DIR)/bootstrap.link

mlibc: bootstrap
	rm -rf "$(BUILD_DIR)/pkg-builds/mlibc"
	$(XB) install --rebuild mlibc

init: mlibc
	rm -rf "$(BUILD_DIR)/pkg-builds/init"
	$(XB) install --rebuild init

kernel: init
	rm -rf "$(BUILD_DIR)/pkg-builds/kernel"
	$(XB) install --rebuild kernel limine

iso: kernel
	$(XB) run iso

run: iso
	$(XB) run qemu

clean:
	rm -f "$(BUILD_DIR)/bayos.iso"
	rm -rf "$(BUILD_DIR)/pkg-builds/kernel"

distclean:
	rm -rf "$(BUILD_DIR)"
