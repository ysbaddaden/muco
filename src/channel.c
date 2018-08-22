#include "channel.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <assert.h>

static void channel_initialize(channel_t *self) {
    lmsqueue_initialize(&self->senders);
    lmsqueue_initialize(&self->receivers);
    atomic_init(&self->sender, NULL);
    atomic_init(&self->receiver, NULL);
    atomic_init(&self->has_value, 0);
    self->value = NULL;
}

static void channel_finalize(channel_t *self) {
    lmsqueue_finalize(&self->senders);
    lmsqueue_finalize(&self->receivers);
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
    scheduler_t *scheduler = scheduler_current();
    fiber_t *current = scheduler->current;

    // always enqueue:
    lmsqueue_enqueue(&self->senders, &current->node);

    // repeat until the fiber is *the* current sender:
    while (1) {
        if (current == lmsqueue_head(&self->senders)) {
            LOG("head(sends)", scheduler, current);

            fiber_t *curr = NULL;

            // partial lock. decide whether we suspend and a receiver fiber
            // will resume us (cas fail), or we continue (cas success):
            if (!atomic_compare_exchange_strong((fiber_t **)&self->sender, &curr, current)) {
                LOG("wait(send)", scheduler, current);
                // failed cas with receiver. suspend:
                scheduler_reschedule(scheduler);
                scheduler = scheduler_current();
            }

            // done. skip to set value.
            break;
        }

        // not head or failed cas with receiver. suspend:
        scheduler_reschedule(scheduler);
        scheduler = scheduler_current();
    }

    // set the value:
    LOG("set value", scheduler, current);
    self->value = value;
    self->has_value = 1;

    do {
        fiber_t *receiver = lmsqueue_head(&self->receivers);
        if (receiver) {
            fiber_t *curr = NULL;

            // synchronize with receive to decide to continue or suspend, so
            // receiving fiber will resume us:
            if (atomic_compare_exchange_strong((fiber_t **)&self->receiver, &curr, receiver)) {
                LOG("unwait(recv)", scheduler, receiver);
                // success. resume pending receiver (suspending current):
                scheduler_resume(scheduler, receiver);
                break;
            }
        }

        // no receiver to resume, or failed cas. suspend (receiver will resume
        // us):
        scheduler_reschedule(scheduler);
    } while (0);

    // TODO: move to co_channel_receive cleanup (faster):
    self->sender = NULL;
    lmsqueue_dequeue(&self->senders);

    // done.
    return 0;
}

int co_channel_receive(channel_t *self, void **value) {
    scheduler_t *scheduler = scheduler_current();
    fiber_t *current = scheduler->current;

    // always enqueue:
    lmsqueue_enqueue(&self->receivers, &current->node);

    // repeat until the fiber is *the* current receiver:
    while (1) {
        if (current == lmsqueue_head(&self->receivers)) {
            LOG("head(recvs)", scheduler, current);

            if (self->has_value == 1) {
                LOG("has_value == 1", scheduler, current);
                // done. skip to get value:
                break;
            }

            // FIXME: potential race condition with sender here (?)

            // try to wake up pending sender. needs synchronization with sender
            // to decide whether we resume the sender fiber, or we suspend and the
            // sender will resume us:
            fiber_t *sender = lmsqueue_head(&self->senders);
            if (sender) {
                fiber_t *curr = NULL;

                if (atomic_compare_exchange_strong((fiber_t **)&self->sender, &curr, sender)) {
                    LOG("unwait(send)", scheduler, sender);
                    // success: resume pending sender:
                    scheduler_resume(scheduler, sender);
                } else {
                    // failed cas with sender. suspend:
                    scheduler_reschedule(scheduler);
                }
                scheduler = scheduler_current();
            }

            // done. skip to get value:
            break;
        }

        // not head. suspend:
        scheduler_reschedule(scheduler);
        scheduler = scheduler_current();
    }

    // synchronize with send to decide to continue or suspend (sending fiber
    // will resume us):
    if (!self->receiver) {
        fiber_t *curr = NULL;
        if (!atomic_compare_exchange_strong((fiber_t **)&self->receiver, &curr, current)) {
            LOG("wait(recv)", scheduler, current);
            // failure. suspend:
            scheduler_reschedule(scheduler);
            scheduler = scheduler_current();
        }
    }

    // wait for value:
    while (1) {
        int i = 1;

        if (atomic_compare_exchange_strong((int *)&self->has_value, &i, 0)) {
            LOG("get value", scheduler, current);
            *value = self->value;
            break;
        }

        // try again (later):
        scheduler_yield(scheduler);
        scheduler = scheduler_current();
    }

    // schedule pending sender:
    fiber_t *sender = atomic_load((fiber_t **)&self->sender);
    scheduler_enqueue(scheduler, sender);

    // cleanup.
    //self->sender = NULL;
    self->receiver = NULL;

    //lmsqueue_dequeue(&self->senders);
    lmsqueue_dequeue(&self->receivers);

    // done.
    return 0;
}
