#include <initramfs.h>
#include <limine.h>
#include <stdint.h>
#include <memory.h>

// NOTE: this assumes the module loaded by limine
// is a tar archive and there is only one archive loaded
// This constraint doesnt really matter for now
// if we ever were to add filesystem support, the handling of initramfs needs to be reworked anyways

// also only read from the files and never write to them
// this way we dont need to worry about concurrent access and can do all this without a lock

__attribute__((used, section(".limine_requests")))
volatile struct limine_module_request module_request = {
	.id = LIMINE_MODULE_REQUEST_ID,
	.revision = 0,
};

static int oct2bin(uint8_t *str, int size) {
	int n = 0;
	uint8_t *c = str;

	while (size-- > 0) {
		n *= 8;
		n += *c - '0';
		c++;
	}

	return n;
}
static inline size_t strlen(const char *s) {
    size_t len = 0;

    while (s[len] != '\0')
        ++len;

    return len;
}

int initramfs_lookup(char *filename, uint8_t **out) {
	if (!filename || !out)
		return -1;
	
	if (!module_request.response || module_request.response->module_count != 1)
		return 0;

	uint8_t *ptr = module_request.response->modules[0]->address;

	while (!memcmp(ptr + 257, "ustar", 5)) {
		int filesize = oct2bin(ptr + 0x7c, 11);
		if (!memcmp(ptr + 1, filename, strlen(filename) + 1)) {
			*out = ptr + 512;
			return filesize;
		}
		ptr += (((filesize + 511) / 512) + 1) * 512;
	}
	return 0;
}
