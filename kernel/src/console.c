#include <console.h>
#include <stdint.h>
#include <file_desc.h>
#include <printk.h>
#include <errno.h>
#include <stddef.h>
#include <keyboard.h>

static int64_t console_write(file_t *f, const uint8_t *buf, uint64_t len) {
	printk_raw(QEMU_SERIAL | FB, buf, len);
	return len;
}

static int64_t console_read(file_t *f, uint8_t *buf, uint64_t len) {
	return pipe_read(&kb_pipe, buf, len);
}

static int64_t console_read_nb(file_t *f, uint8_t *buf, uint64_t len) {
	return pipe_read_nb(&kb_pipe, buf, len);
}

static const file_ops_t console_ops = {
	.read = console_read,
	.read_nb = console_read_nb,
	.write = console_write,
};

file_t console_file = {
	.ops = &console_ops,
	.priv = NULL,
	.flags = (uint64_t)0,
};
