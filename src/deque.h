#ifndef DEQUE_H
#define DEQUE_H

#include "config.h"
#include "muco/deque.h"

// FIXME: should be unlimited
#define DEQUE_SIZE (sizeof(void *) * 1024 * 1024)

static void deque_initialize(deque_t *self);
static void deque_finalize(deque_t *self);
static void deque_push_bottom(deque_t *self, void *item);
static void *deque_pop_bottom(deque_t *self);
static void *deque_pop_top(deque_t *self);

#endif
