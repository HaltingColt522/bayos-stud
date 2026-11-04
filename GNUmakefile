BUILD_DIR ?= $(abspath $(CURDIR)/../build)
XBSTRAP ?= xbstrap

XB = cd $(BUILD_DIR) && $(XBSTRAP)

.DEFAULT_GOAL := all
.PHONY: all init kernel iso run clean distclean xbstrap

all: iso

$(BUILD_DIR)/bootstrap.link:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(XBSTRAP) init $(CURDIR)

init: $(BUILD_DIR)/bootstrap.link

kernel: init
	$(XB) install --rebuild kernel limine

iso: kernel
	$(XB) run iso

run: iso
	$(XB) run qemu

clean:
	rm -f $(BUILD_DIR)/bayos.iso
	rm -rf $(BUILD_DIR)/pkg-builds/kernel

# deletes WHOLE build-dir! handle with care
distclean:
	rm -rf $(BUILD_DIR)

xbstrap: init
	$(XB) $(ARGS)
