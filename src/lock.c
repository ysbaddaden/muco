#include "scheduler.h"
#include "lmsqueue.h"
#include <stdio.h>
#include <errno.h>

typedef struct lock {
    _Atomic fiber_t *owner;
    lmsqueue_t waiters;
} lock_t;

static void lock_initialize(lock_t *self) {
    atomic_init(&self->owner, NULL);
    lmsqueue_initialize(&self->waiters);
}

lock_t *co_lock_new() {
    lock_t *self = calloc(1, sizeof(lock_t));
    lock_initialize(self);
    return self;
}

void co_lock_free(lock_t *self) {
    lmsqueue_finalize(&self->waiters);
    free(self);
}

int co_lock(lock_t *self) {
    scheduler_t *scheduler = scheduler_current();
    fiber_t *current = scheduler->current;
    fiber_t *owner = atomic_load((fiber_t **)&self->owner);

    // fastpath: prevent recursive deadlock.
    if (current == owner) {
        //LOG("EDEADLK", scheduler, current);
        errno = EDEADLK;
        return -1;
    }

    // always enqueue the current fiber:
    lmsqueue_enqueue(&self->waiters, &current->node);

    while (1) {
        if (current == lmsqueue_head(&self->waiters)) {
            // avoid a race condition with unlock: the unlocking fiber must
            // decide whether it should resume the HEAD fiber (aka the current
            // fiber) or not. we use an atomic to solve that, where both the
            // locking and unlocking fibers will try to set the current owner.

            if (atomic_compare_exchange_strong((fiber_t **)&self->owner, &owner, current)) {
                // success. lock acquired. the unlocking fiber won't try to
                // resume the locking fiber. we can return;
                // LOG("locked", scheduler, current);
                return 0;
            }

            // failed to lock owner, but maybe the unlocking fiber set it to
            // NULL, which means that it didn't know about the locking fiber:
            owner = atomic_load((fiber_t **)&self->owner);
            if (!owner) {
                // try again:
                if (atomic_compare_exchange_strong((fiber_t **)&self->owner, &owner, current)) {
                    // LOG("locked", scheduler, current);
                    return 0;
                }
            }

            // unlock did set the current fiber as owner and will resume it,
            // we *must* suspend —to be resumed immediately, yes.
        }

        // suspend:
        scheduler_reschedule(scheduler);

        // get updated references:
        scheduler = scheduler_current();
        //current = scheduler->current;

        owner = atomic_load((fiber_t **)&self->owner);
        if (current == owner) {
            // done. fiber was given the lock by unlock.
            // LOG("locked", scheduler, current);
            return 0;
        }
    }
}

int co_unlock(lock_t *self) {
    scheduler_t *scheduler = scheduler_current();
    fiber_t *current = scheduler->current;
    fiber_t *owner = atomic_load((fiber_t **)&self->owner);

    if (current != owner) {
        //LOG("EPERM", scheduler, current);
        errno = EPERM;
        return -1;
    }

    // release lock (remove current fiber from wait list):
    lmsqueue_dequeue(&self->waiters);

    // get next owner fiber, or NULL if not is queued, yet:
    fiber_t *next = lmsqueue_head(&self->waiters);

    // try to swap the owner.
    if (atomic_compare_exchange_strong((fiber_t **)&self->owner, &current, next)) {
        // success. we *must* resume the next fiber, that was previously
        // suspended or is just rescheduling (see co_lock):
        if (next) {
            scheduler_enqueue(scheduler, next);
        }
    }

    // LOG("unlocked", scheduler, current);
    return 0;
}
