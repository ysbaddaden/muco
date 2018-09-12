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

#if defined(DEBUG) || defined(CHAN_DEBUG)
#  define LOG(str, ...) dprintf(2, "fiber=%p self=" str, (void*)co_current(), __VA_ARGS__)
#else
#  define LOG(...)
#endif

// FIXME: a race condition causes all fibers to be suspended!

void co_chan_init(chan_t *self, size_t capacity, int async) {
    self->async = async;
    self->capacity = capacity;
    self->size = 0;
    self->state = chan_opened;
    self->buf = malloc(sizeof(entry_t) * capacity);
    co_mtx_init(&self->mutex);
    co_cond_init(&self->senders);
    co_cond_init(&self->receivers);
}

void co_chan_destroy(chan_t *self) {
    free(self->buf);
}

int co_chan_send(chan_t *self, void *value) {
    LOG("%p: co_chan_send\n", (void *)self);
    if (self->state) return -1;

    //LOG("send: locking\n");
    co_mtx_lock(&self->mutex);
    //LOG("send: locked\n");

    // wait until the channel queue has some room for a value:
    while (co_chan_full(self)) {
        if (self->state) {
            co_mtx_unlock(&self->mutex);
            return -1;
        }
        //LOG("send: waiting\n");
        co_cond_wait(&self->senders, &self->mutex);
        //LOG("send: resumed\n");
    }

    // enqueue item:
    //LOG("send: push value (%ld)\n", self->size);
    self->buf[self->size] = (entry_t){self->async ? NULL : co_current(), value};
    self->size++;

    // wakeup a waiting receiver:
    //LOG("send: signal(receivers)\n");
    co_cond_signal(&self->receivers);

    // done.
    //LOG("send: unlocking\n");
    co_mtx_unlock(&self->mutex);
    //LOG("send: unlocked\n");

    if (!self->async) {
        //LOG("send: pending\n");
        co_suspend();
    }

    return 0;
}

int co_chan_receive(chan_t *self, void **value) {
    LOG("%p: co_chan_receive\n", (void *)self);
    if (self->state == chan_closed) return -1;

    //LOG("recv: locking\n");
    co_mtx_lock(&self->mutex);
    //LOG("recv: locked\n");

    // wait until the channel queue has a value:
    while (co_chan_empty(self)) {
        if (self->state == chan_closed) {
            co_mtx_unlock(&self->mutex);
            return -1;
        }
        //LOG("recv: waiting\n");
        co_cond_wait(&self->receivers, &self->mutex);
        //LOG("recv: resumed\n");
    }

    // dequeue item:
    self->size--;
    //LOG("recv: shift value (%ld)\n", self->size);
    entry_t entry = self->buf[self->size];
    *value = entry.value;

    if (self->state == chan_closing) {
        if (co_chan_empty(self)) {
            // close the channel now:
            //LOG("recv: closed!\n");
            self->state = chan_closed;
        }
    } else {
        // wakeup one waiting sender:
        //LOG("recv: signal(senders)\n");
        co_cond_signal(&self->senders);
    }

    //LOG("recv: unlocking\n");
    co_mtx_unlock(&self->mutex);
    //LOG("recv: unlocked\n");

    if (entry.sender) {
        // if synchronous: wakeup pending sender:
        //LOG("recv: enqueue(sender)\n");
        co_enqueue(entry.sender);
    }

    // done.
    return 0;
}

void co_chan_close(chan_t *self) {
    if (self->state) return;

    LOG("%p: co_chan_close\n", (void *)self);
    //LOG("close: locking\n");
    co_mtx_lock(&self->mutex);

    // close immediately or delay until the channel queue is emptied:
    if (co_chan_empty(self)) {
        //LOG("close: closed!\n");
        self->state = chan_closed;
    } else {
        //LOG("close: closing...\n");
        self->state = chan_closing;
    }

    // wakeup pending fibers:
    //LOG("close: broadcast(senders)\n");
    co_cond_broadcast(&self->senders);

    //LOG("close: broadcast(receivers)\n");
    co_cond_broadcast(&self->receivers);

    // done.
    //LOG("close: unlocking\n");
    co_mtx_unlock(&self->mutex);
}
