#ifndef CO_CHANNEL_PRIV_H
#define CO_CHANNEL_PRIV_H

#include "muco.h"
#include "muco/lock.h"

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

typedef struct channel {
  co_lock_t r_lock;          // receive lock
  co_lock_t w_lock;          // send lock
  co_lock_t s_lock;          // sync send/receive lock
  fiber_t *receiver;
  atomic_int has_value;      // uses an atomic to avoid potential OOO execution,
  void *value;               // because has_value must be set *after* value has.
} channel_t;

channel_t *co_channel_new();
static void channel_initialize(channel_t *);
static void channel_finalize(channel_t *);
int co_channel_send(channel_t *, void *);
int co_channel_receive(channel_t *, void **);

static void channel_initialize(channel_t *self) {
    co_lock_initialize(&self->r_lock);
    co_lock_initialize(&self->w_lock);
    co_lock_initialize(&self->s_lock);
    self->receiver = NULL;
    atomic_init(&self->has_value, 0);
    self->value = NULL;
}

static void channel_finalize(channel_t *self) {
    co_lock_free(&self->r_lock);
    co_lock_free(&self->w_lock);
    self->value = NULL;
}

channel_t *co_channel_new() {
    channel_t *channel = calloc(1, sizeof(channel_t));
    channel_initialize(channel);
    return channel;
}

void co_channel_free(channel_t *self) {
    channel_finalize(self);
    free(self);
}

int co_channel_send(channel_t *self, void *value) {
    // sender wait list:
    co_lock(&self->w_lock);

    // set value (requires lock to synchronize with current receiver):
    co_lock(&self->s_lock);
    self->value = value;
    self->has_value = 1;
    fiber_t *receiver = self->receiver;
    co_unlock(&self->s_lock);

    if (receiver) {
        // resume waiting receiver:
        co_resume(receiver);
    } else {
        co_suspend();
    }

    // done. release write lock and enqueue next sender (that will set the
    // channel value, and maybe resume a receiver):
    co_unlock(&self->w_lock);
    return 0;
}

int co_channel_receive(channel_t *self, void **value) {
    // receiver wait list:
    co_lock(&self->r_lock);

    // check for value presence (fast path):
    if (!self->has_value) {
        // retry with lock: (required to synchronize with current sender):
        co_lock(&self->s_lock);

        if (self->has_value) {
            co_unlock(&self->s_lock);
        } else {
            // wait for value:
            self->receiver = co_current();
            co_unlock(&self->s_lock);
            co_suspend();
            self->receiver = NULL;
        }
    }

    // get value:
    *value = self->value;
    self->value = NULL;
    self->has_value = 0;

    // enqueue sender:
    co_enqueue(co_lock_owner(&self->w_lock));

    // done. release read lock but don't enqueue next receiver (sender will
    // release write lock and enqueue next sender, if any):
    co_unlock2(&self->r_lock, 0);
    return 0;
}

#endif
