#ifndef MUCO_MUTEX_H
#define MUCO_MUTEX_H

#include <stdatomic.h>

typedef struct {
    atomic_int held;
    atomic_flag busy;
    struct { fiber_t *head, *tail; } blocking;
} co_mtx_t;

typedef struct {
    atomic_flag busy;
    struct { fiber_t *head, *tail; } waiting;
} co_cond_t;

void co_mtx_init(co_mtx_t *);
int co_mtx_lock(co_mtx_t *);
int co_mtx_trylock(co_mtx_t *);
void co_mtx_unlock(co_mtx_t *);

void co_cond_init(co_cond_t *);
void co_cond_wait(co_cond_t *, co_mtx_t *);
void co_cond_signal(co_cond_t *);
void co_cond_broadcast(co_cond_t *);

#endif
