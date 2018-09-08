#ifndef MUCO_MUTEX_H
#define MUCO_MUTEX_H

#include <stdatomic.h>

typedef struct {
    atomic_bool held;
    struct {
        void **buf;
        size_t capacity;
        size_t size;
        size_t start;
    } blocking;
} co_mtx_t;

typedef struct {
    struct {
        void **buf;
        size_t capacity;
        size_t size;
        size_t start;
    } waiting;
} co_cond_t;

void co_mtx_init(co_mtx_t *);
void co_mtx_destroy(co_mtx_t *);
void co_mtx_lock(co_mtx_t *);
void co_mtx_unlock(co_mtx_t *);

void co_cond_init(co_cond_t *);
void co_cond_destroy(co_cond_t *);
void co_cond_wait(co_cond_t *, co_mtx_t *);
void co_cond_signal(co_cond_t *);
void co_cond_broadcast(co_cond_t *);

#endif
