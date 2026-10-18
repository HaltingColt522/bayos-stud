#ifndef SEM_H
#define SEM_H

#include <sched.h>

typedef struct {
	int count;
	process_t *wait_head;
	process_t *wait_tail;
} semaphore_t;

semaphore_t *sem_create(semaphore_t *sem, int initial_count);
void sem_wait(semaphore_t *sem);
int sem_trywait(semaphore_t *sem);
void sem_signal(semaphore_t *sem);

typedef semaphore_t mutex_t;

#define mutex_create(m)		sem_create(m, 1)
#define mutex_lock(m)		sem_wait(m)
#define mutex_try_lock(m)	sem_trywait(m)
#define mutex_unlock(m)		sem_signal(m)

#define DEFINE_MUTEX_TYPE(T) \
	typedef struct { \
		T *data; \
		semaphore_t *lock; \
	} Mutex_##T

#define Mutex(T) Mutex_##T

#define MUTEX_LOCK(m)		( mutex_lock((m).lock), (m).data )
#define MUTEX_TRY_LOCK(m)	( mutex_try_lock((m).lock) ? (m).data : NULL )
#define MUTEX_UNLOCK(m)		( mutex_unlock((m).lock) )

#endif // SEM_H
