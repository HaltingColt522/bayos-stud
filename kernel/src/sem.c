#include <sem.h>
#include <sched.h>
#include <io.h>
#include <stddef.h>

semaphore_t *sem_create(semaphore_t *sem, int initial_count) {
	sem->count = initial_count;
	sem->wait_head = NULL;
	sem->wait_tail = NULL;

	return sem;
}

void sem_wait(semaphore_t *sem) {
	uint64_t flags = save_irqdisable();

	sem->count--;

	if (sem->count < 0) {
		get_current_process()->status = BLOCKED;
		get_current_process()->wait_next = NULL;

		if (sem->wait_tail) {
			sem->wait_tail->wait_next = get_current_process();
		} else {
			sem->wait_head = get_current_process();
		}
		sem->wait_tail = get_current_process();
		irqrestore(flags);
		yield();
		return;
	}

	irqrestore(flags);
}

int sem_trywait(semaphore_t *sem) {
	uint64_t flags = save_irqdisable();

	if (sem->count > 0) {
		sem->count--;
		irqrestore(flags);
		return 1;
	}

	irqrestore(flags);
	return 0;
}

void sem_signal(semaphore_t *sem) {
	uint64_t flags = save_irqdisable();

	sem->count++;

	if (sem->count <= 0 && sem->wait_head != NULL) {
		process_t *woken = sem->wait_head;
		sem->wait_head = woken->wait_next;
		if (sem->wait_head == NULL) { sem->wait_tail = NULL; }
		woken->wait_next = NULL;
		woken->status = READY;
	}

	irqrestore(flags);
}
