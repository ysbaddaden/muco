#ifndef DEQUE_H
#define DEQUE_H

#include "config.h"
#include <stdatomic.h>

struct age {
    int tag;
    int top;
};

typedef struct deque {
    atomic_int bot;
    _Atomic struct age age;
    void **deq;
} deque_t;

// FIXME: should be unlimited
#define DEQUE_SIZE (4UL * 1024 * 1024 * 1024)

static void deque_initialize(deque_t *self);
static void deque_finalize(deque_t *self);
static void deque_push_bottom(deque_t *self, void *item);
static void *deque_pop_bottom(deque_t *self);
static void *deque_pop_top(deque_t *self);

#endif
