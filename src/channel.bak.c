#ifndef CO_CHANNEL_PRIV_H
#define CO_CHANNEL_PRIV_H

#include "lmsqueue.h"
#include "muco.h"
#include "muco/channel.h"

#define channel_t co_channel_t

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

channel_t *co_channel_new() {
    channel_t *channel = calloc(1, sizeof(channel_t));
    co_channel_initialize(channel);
    return channel;
}

void co_channel_free(channel_t *self) {
    co_channel_finalize(self);
    free(self);
}

// TODO: use internal sender/receiver wait lists (instead of using lock);
//       keep lock for send/receive/close synchronization (s_lock);
//       drop co_lock_clear() method;
//       better react on self->closed in send/receive wait lists (avoid
//       redundant closed checks if the current fiber wasn't suspended);

int co_channel_send(channel_t *self, void *value) {
    if (self->closed) return -1;

    fiber_t *current = co_current();

    // sender wait list (acquire write lock):
    fiber_t *owner = atomic_load((fiber_t **)&self->w_lock.owner);
    lmsqueue_enqueue(&self->w_lock.waiters, current);

    while (1) {
        if (current == lmsqueue_head(&self->w_lock.waiters)) {
            if (atomic_compare_exchange_strong((fiber_t **)&self->w_lock.owner, &owner, current)) {
                break;
            }
            if (!(owner = atomic_load((fiber_t **)&self->w_lock.owner))) {
                if (atomic_compare_exchange_strong((fiber_t **)&self->w_lock.owner, &owner, current)) {
                    break;
                }
            }
        }
        co_suspend();
        if (self->closed) return -1;

        owner = atomic_load((fiber_t **)&self->w_lock.owner);
        if (current == owner) break;
    }

    // set value (synchronize with receive):
    co_lock(&self->s_lock);
    if (self->closed) return -1; // TODO: only re-check if we didn't immediately got the lock (?)

    self->value = value;
    self->has_value = 1;
    fiber_t *receiver = self->receiver;
    self->sender = current;
    co_unlock(&self->s_lock);

    // resume waiting receiver (or wait):
    if (receiver) {
        co_resume(receiver);
    } else {
        co_suspend();
    }
    self->sender = NULL;

    // release write lock & enqueue next sender that will set the channel value,
    // and resume a receiver (if any):
    co_unlock(&self->w_lock);
    return 0;
}

int co_channel_receive(channel_t *self, void **value) {
    if (self->closed && !self->has_value) return -1;

    fiber_t *current = co_current();

    // receiver wait list (acquire read lock):
    fiber_t *owner = atomic_load((fiber_t **)&self->r_lock.owner);
    lmsqueue_enqueue(&self->r_lock.waiters, current);

    while (1) {
        if (current == lmsqueue_head(&self->r_lock.waiters)) {
            if (atomic_compare_exchange_strong((fiber_t **)&self->r_lock.owner, &owner, current)) {
                break;
            }
            if (!(owner = atomic_load((fiber_t **)&self->r_lock.owner))) {
                if (atomic_compare_exchange_strong((fiber_t **)&self->r_lock.owner, &owner, current)) {
                    break;
                }
            }
        }
        co_suspend();
        if (self->closed && !self->has_value) return -1;

        owner = atomic_load((fiber_t **)&self->r_lock.owner);
        if (current == owner) break;
    }

    // check for value presence (must synchronize with send & close):
    co_lock(&self->s_lock);

    if (self->has_value) {
        co_unlock(&self->s_lock);
    } else {
        if (self->closed) return -1;

        // wait for value:
        self->receiver = co_current();
        co_unlock(&self->s_lock);
        co_suspend();
        if (self->closed) return -1;
    }

    // get value (must synchronize with close):
    co_lock(&self->s_lock);
    self->receiver = NULL;
    *value = self->value;
    self->has_value = 0;

    // enqueue pending sender:
    co_enqueue(atomic_load((fiber_t **)&self->sender));

    if (self->closed) {
        co_lock_clear(&self->r_lock, 0); // don't enqueue current fiber
        co_lock_clear(&self->w_lock, 0); // don't enqueue pending sender (twice)
    }

    // release read lock & don't enqueue next receiver (let a sender do it):
    co_unlock(&self->s_lock);
    return 0;
}

void co_channel_close(channel_t *self) {
    co_lock(&self->s_lock);
    if (!self->closed) {
        self->closed = 1;

        if (!self->has_value) {
            co_lock_clear(&self->r_lock, !self->receiver); // don't enqueue pending receiver (twice)
            co_lock_clear(&self->w_lock, !self->sender);   // don't enqueue pending sender (twice)
        }
    }
    co_unlock(&self->s_lock);
}

#endif
