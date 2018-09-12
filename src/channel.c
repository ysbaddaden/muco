#include "muco.h"
#include "muco/channel.h"

#include <stdlib.h>
#include <stdio.h>

enum chan_state {
    chan_opened = 0,
    chan_closing = 1,
    chan_closed = 2
};

#define entry_t co_chan_entry_t
#define chan_t co_chan_t

//#if defined(DEBUG) || defined(CHAN_DEBUG)
//#  define LOG(str, ...) dprintf(2, "fiber=%p self=" str, (void*)co_current(), __VA_ARGS__)
//#else
//#  define LOG(...)
//#endif

void co_chan_init(chan_t *self, size_t capacity, int async) {
    self->state = chan_opened;
    self->async = async;

    self->capacity = capacity;
    self->size = 0;
    self->start = 0;
    self->buf = malloc(sizeof(entry_t) * capacity);

    co_mtx_init(&self->mutex);
    co_cond_init(&self->senders);
    co_cond_init(&self->receivers);
}

void co_chan_destroy(chan_t *self) {
    free(self->buf);
}

int co_chan_send(chan_t *self, void *value) {
    if (self->state) return -1;

    co_mtx_lock(&self->mutex);

    // wait until the channel queue has some room for a value:
    while (co_chan_full(self)) {
        if (self->state) {
            co_mtx_unlock(&self->mutex);
            return -1;
        }
        co_cond_wait(&self->senders, &self->mutex);
    }

    // enqueue item:
    size_t index = self->start + self->size;
    if (index >= self->capacity) index -= self->capacity;
    self->buf[index] = (entry_t){self->async ? NULL : co_current(), value};
    self->size++;

    // wakeup one waiting receiver:
    co_cond_signal(&self->receivers);

    // done.
    co_mtx_unlock(&self->mutex);

    // if synchronous: suspend until a receiver got the value:
    if (!self->async) {
        co_suspend();
    }

    return 0;
}

int co_chan_receive(chan_t *self, void **value) {
    if (self->state == chan_closed) return -1;

    co_mtx_lock(&self->mutex);

    // wait until the channel queue has a value:
    while (co_chan_empty(self)) {
        if (self->state == chan_closed) {
            co_mtx_unlock(&self->mutex);
            return -1;
        }
        co_cond_wait(&self->receivers, &self->mutex);
    }

    // dequeue item:
    entry_t entry = self->buf[self->start];
    self->size--;
    self->start++;
    if (self->start >= self->capacity) self->start -= self->capacity;

    if (self->state == chan_closing) {
        // close the channel once it's empty:
        if (co_chan_empty(self)) {
            self->state = chan_closed;
        }
    } else {
        // wakeup a waiting sender:
        co_cond_signal(&self->senders);
    }

    co_mtx_unlock(&self->mutex);

    // if synchronous: wakeup pending sender:
    if (entry.sender) {
        co_enqueue(entry.sender);
    }

    // done.
    *value = entry.value;
    return 0;
}

void co_chan_close(chan_t *self) {
    if (self->state) return;

    co_mtx_lock(&self->mutex);

    // close immediately or delay until the channel queue is emptied:
    if (co_chan_empty(self)) {
        self->state = chan_closed;
    } else {
        self->state = chan_closing;
    }

    // wakeup pending fibers:
    co_cond_broadcast(&self->senders);
    co_cond_broadcast(&self->receivers);

    // done.
    co_mtx_unlock(&self->mutex);
}
