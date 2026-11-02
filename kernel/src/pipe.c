#include <pipe.h>
#include <stddef.h>

pipe_t *pipe_create(pipe_t *p, uint8_t *buffer, uint64_t buffer_len) {
	if (!p || !buffer || buffer_len == 0) {
		return NULL;
	}

	p->buffer = buffer;
	p->buffer_len = buffer_len;

	p->read_pos = 0;
	p->write_pos = 0;
	p->count = 0;
	p->read_refs = 1;
	p->write_refs = 1;

	mutex_create(&p->buf_lock);
	sem_create(&p->sem_empty, (int)buffer_len);
	sem_create(&p->sem_full, 0);

	return p;
}

int64_t pipe_write(pipe_t *p, const uint8_t *data, uint64_t len) {
	if (!p || !data) {
		return -1;
	}

	for (uint64_t i = 0; i < len; i++) {
		sem_wait(&p->sem_empty);
		mutex_lock(&p->buf_lock);

		if (p->read_refs == 0) {
			mutex_unlock(&p->buf_lock);
			sem_signal(&p->sem_empty);
			return i ? (int64_t)i : -1;
		}

		p->buffer[p->write_pos] = data[i];
		p->write_pos = (p->write_pos + 1) % p->buffer_len;
		p->count++;

		mutex_unlock(&p->buf_lock);
		sem_signal(&p->sem_full);
	}

	return (int64_t)len;
}

int pipe_try_write(pipe_t *p, uint8_t byte) {
	if (!p) {
		return -1;
	}

	if (!sem_trywait(&p->sem_empty)) {
		return 0;
	}

	if (!mutex_try_lock(&p->buf_lock)) {
		sem_signal(&p->sem_empty);
		return 0;
	}

	if (p->read_refs == 0) {
		mutex_unlock(&p->buf_lock);
		sem_signal(&p->sem_empty);
		return -1;
	}

	p->buffer[p->write_pos] = byte;
	p->write_pos = (p->write_pos + 1) % p->buffer_len;
	p->count++;

	mutex_unlock(&p->buf_lock);
	sem_signal(&p->sem_full);
	return 1;
}

int64_t pipe_read(pipe_t *p, uint8_t *out, uint64_t len) {
	if (!p || !out) {
		return -1;
	}

	if (len == 0) {
		return 0;
	}

	sem_wait(&p->sem_full);
	mutex_lock(&p->buf_lock);

	if (p->count == 0) {
		mutex_unlock(&p->buf_lock);
		sem_signal(&p->sem_full);
		return 0;
	}

	out[0] = p->buffer[p->read_pos];
	p->read_pos = (p->read_pos + 1) % p->buffer_len;
	p->count--;
	mutex_unlock(&p->buf_lock);
	sem_signal(&p->sem_empty);

	uint64_t i = 1;

	for (; i < len; i++) {
		if (!sem_trywait(&p->sem_full)) {
			break;
		}

		mutex_lock(&p->buf_lock);

		if (p->count == 0) {
			mutex_unlock(&p->buf_lock);
			sem_signal(&p->sem_full);
			break;
		}

		out[i] = p->buffer[p->read_pos];
		p->read_pos = (p->read_pos + 1) % p->buffer_len;
		p->count--;
		mutex_unlock(&p->buf_lock);
		sem_signal(&p->sem_empty);
	}

	return (int64_t)i;
}

void pipe_close_read(pipe_t *p) {
	if (!p) {
		return;
	}

	mutex_lock(&p->buf_lock);
	int last = (--p->read_refs == 0);
	mutex_unlock(&p->buf_lock);

	if (last) {
		sem_signal(&p->sem_empty);
	}
}

void pipe_close_write(pipe_t *p) {
	if (!p) {
		return;
	}

	mutex_lock(&p->buf_lock);
	int last = (--p->write_refs == 0);
	mutex_unlock(&p->buf_lock);

	if (last) {
		sem_signal(&p->sem_full);
	}
}
