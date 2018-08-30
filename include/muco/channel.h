#ifndef MUCO_CHANNEL_H
#define MUCO_CHANNEL_H

#include "muco/lock.h"

typedef struct co_channel {
  co_lock_t r_lock;          // receive lock
  co_lock_t w_lock;          // send lock
  co_lock_t s_lock;          // sync send/receive lock
  fiber_t *receiver;
  atomic_int has_value;      // uses an atomic to avoid potential OOO execution,
  void *value;               // because has_value must be set *after* value has.
} co_channel_t;

co_channel_t *co_channel_new();
void co_channel_free(co_channel_t *);
int co_channel_send(co_channel_t *, void *);
int co_channel_receive(co_channel_t *, void **);

#endif
