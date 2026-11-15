#include <console.h>
#include <stdint.h>
#include <file_desc.h>
#include <printk.h>
#include <errno.h>
#include <stddef.h>

static int64_t console_write(file_t *f, const uint8_t *buf, uint64_t len) {
	printk_raw(QEMU_SERIAL | FB, buf, len);
	return len;
}

static int64_t console_read(file_t *f, uint8_t *buf, uint64_t len) {
	return -EIO;
}

static const file_ops_t console_ops = {
	.read = console_read,
	.write = console_write,
};

file_t console_file = {
	.ops = &console_ops,
	.priv = NULL,
};
