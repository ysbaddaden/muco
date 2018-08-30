#ifndef MUCO_LOCK_H
#define MUCO_LOCK_H

#include "muco/fiber.h"
#include "muco/lmsqueue.h"

typedef struct co_lock {
    _Atomic fiber_t *owner;
    lmsqueue_t waiters;
} co_lock_t;

void co_lock_initialize(co_lock_t *);
co_lock_t *co_lock_new();
void co_lock_free(co_lock_t *);

int co_lock(co_lock_t *);
int co_unlock(co_lock_t *);
int co_unlock2(co_lock_t *, int);

fiber_t *co_lock_owner();

#endif
