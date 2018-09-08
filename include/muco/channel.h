#ifndef MUCO_CHANNEL_H
#define MUCO_CHANNEL_H

#include "muco/mutex.h"

typedef struct {
  fiber_t *sender;
  void *value;
} co_chan_entry_t;

typedef struct {
    int async;
    int state;
    size_t capacity;
    size_t size;
    co_chan_entry_t *buf;
    co_mtx_t mutex;
    co_cond_t senders;
    co_cond_t receivers;
} co_chan_t;

void co_chan_init(co_chan_t *, size_t capacity, int async);
void co_chan_destroy(co_chan_t *);
int co_chan_send(co_chan_t *, void *);
int co_chan_receive(co_chan_t *, void **);
void co_chan_close(co_chan_t *);

static inline int co_chan_empty(co_chan_t *self) {
    return self->size == 0;
}

static inline int co_chan_full(co_chan_t *self) {
    return self->size == self->capacity;
}

#endif
