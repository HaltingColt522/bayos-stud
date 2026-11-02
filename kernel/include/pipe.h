#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <sem.h>

typedef struct {
	uint8_t *buffer;
	uint64_t buffer_len;

	uint64_t read_pos;
	uint64_t write_pos;
	uint64_t count;       // bytes currently in the buffer (protected by buf_lock)

	int read_refs;        // protected by buf_lock
	int write_refs;       // protected by buf_lock

	mutex_t buf_lock;
	semaphore_t sem_empty;
	semaphore_t sem_full;
} pipe_t;

pipe_t *pipe_create(pipe_t *pipe, uint8_t *buffer, uint64_t buffer_len);
int64_t pipe_write(pipe_t *pipe, const uint8_t *data, uint64_t len);
int pipe_try_write(pipe_t *pipe, uint8_t byte);
int64_t pipe_read(pipe_t *pipe, uint8_t *out, uint64_t len);
void pipe_close_read(pipe_t *pipe);
void pipe_close_write(pipe_t *pipe);

#endif // PIPE_H
