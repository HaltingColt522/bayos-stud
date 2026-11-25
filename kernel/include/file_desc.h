#ifndef FILE_DESC_H
#define FILE_DESC_H

#include <stdint.h>

typedef enum {
	PIPE = 1,
	CONSOLE = 2,
} file_type_t;

typedef struct file_ops_t file_ops_t;

typedef struct {
	file_ops_t *ops;
	void *priv;
	uint64_t flags;
} file_t;

typedef int64_t (*file_read_fn)(file_t *f, uint8_t *buf, uint64_t len);
typedef int64_t (*file_write_fn)(file_t *f, const uint8_t *buf, uint64_t len);

typedef struct file_ops_t {
	file_read_fn read;
	file_read_fn read_nb;
	file_write_fn write;
} file_ops_t;

#endif // !FILE_DESC_H
