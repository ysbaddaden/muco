#ifndef MUCO_LOCK_PRIV_H
#define MUCO_LOCK_PRIV_H

#include "lmsqueue.h"
#include "muco.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>

//#ifdef DEBUG
//#include <stdio.h>
//#include <unistd.h>
//static void LOG(char *action, scheduler_t *scheduler, fiber_t *fiber) {
//    if (!fiber) fiber = co_current();
//    dprintf(2, "                    %12s \033[%dmscheduler=%p fiber=%p [%s]\033[0m\n", action, scheduler->color, (void *)scheduler, (void *)fiber, fiber->name);
//}
//#else
//#define LOG(action, scheduler, fiber)
//#endif

typedef struct lock {
    _Atomic fiber_t *owner;
    lmsqueue_t waiters;
} lock_t;

void co_lock_initialize(lock_t *self) {
    atomic_init(&self->owner, NULL);
    lmsqueue_initialize(&self->waiters);
}

lock_t *co_lock_new() {
    lock_t *self = calloc(1, sizeof(lock_t));
    co_lock_initialize(self);
    return self;
}

void co_lock_free(lock_t *self) {
    free(self);
}

fiber_t *co_lock_owner(lock_t *self) {
    return atomic_load((fiber_t **)&self->owner);
}

int co_lock(lock_t *self) {
    fiber_t *current = co_current();
    fiber_t *owner = atomic_load((fiber_t **)&self->owner);

    // fastpath: prevent recursive deadlock.
    if (current == owner) {
        errno = EDEADLK;
        return -1;
    }

    // always enqueue the current fiber:
    lmsqueue_enqueue(&self->waiters, current);

    while (1) {
        if (current == lmsqueue_head(&self->waiters)) {
            // avoid a race condition with unlock: the unlocking fiber must
            // decide whether it should resume the HEAD fiber (aka the current
            // fiber) or not. we use an atomic to solve that, where both the
            // locking and unlocking fibers will try to set the current owner.

            if (atomic_compare_exchange_strong((fiber_t **)&self->owner, &owner, current)) {
                // success. lock acquired. the unlocking fiber won't try to
                // resume the locking fiber. we can return;
                // LOG("locked", co_scheduler(), current);
                return 0;
            }

            // failed to lock owner, but maybe the unlocking fiber set it to
            // NULL, which means that it didn't know about the locking fiber:
            owner = atomic_load((fiber_t **)&self->owner);
            if (!owner) {
                // try again:
                if (atomic_compare_exchange_strong((fiber_t **)&self->owner, &owner, current)) {
                    // LOG("locked", co_scheduler(), current);
                    return 0;
                }
            }

            // unlock did set the current fiber as owner and will resume it,
            // we *must* suspend —to be resumed immediately, yes.
        }

        // suspend:
        co_suspend();

        owner = atomic_load((fiber_t **)&self->owner);
        if (current == owner) {
            // done. fiber was given the lock by unlock.
            // LOG("locked", co_scheduler(), current);
            return 0;
        }
    }
}

int co_unlock2(lock_t *self, int enqueue) {
    fiber_t *current = co_current();
    fiber_t *owner = atomic_load((fiber_t **)&self->owner);

    if (current != owner) {
        errno = EPERM;
        return -1;
    }

    // release lock (remove current fiber from wait list):
    lmsqueue_dequeue(&self->waiters);

    if (enqueue) {
        // get next owner fiber, or NULL if not is queued, yet:
        fiber_t *next = lmsqueue_head(&self->waiters);

        // try to swap the owner.
        if (atomic_compare_exchange_strong((fiber_t **)&self->owner, &current, next)) {
            // success. we *must* resume the next fiber, that was previously
            // suspended or is just rescheduling (see co_lock):
            if (next) {
                co_enqueue(next);
            }
        }
    }

    // LOG("unlocked", co_scheduler(), current);
    return 0;
}

int co_unlock(lock_t *self) {
    return co_unlock2(self, 1);
}

#endif
