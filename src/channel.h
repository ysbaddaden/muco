#ifndef CO_CHANNEL_H
#define CO_CHANNEL_H

#include "deque.h"
#include "scheduler.h"

typedef struct channel {
  lmsqueue_t senders;        // pending sender fibers
  lmsqueue_t receivers;      // pending receiver fibers
  _Atomic fiber_t *sender;   // current sending fiber (for synchronization)
  _Atomic fiber_t *receiver; // current receiving fiber (for synchronization)
  atomic_int has_value;      // uses an atomic to avoid potential OOO execution,
  void *value;               // because has_value must be set *after* value has.
} channel_t;

channel_t *co_channel_new();
static void channel_initialize(channel_t *);
static void channel_finalize(channel_t *);
int co_channel_send(channel_t *, void *);
int co_channel_receive(channel_t *, void **);

#endif
