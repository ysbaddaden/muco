#ifndef MUCO_LMSNODE_H
#define MUCO_LMSNODE_H

/*
 * Based on "An Optimistic Approach to Lock-Free FIFO Queues" by Ladan-Mozes and
 * Shavit, hence LMS-queue.
 *
 * See * <https://people.csail.mit.edu/edya/publications/OptimisticFIFOQueue-DISC2004.pdf>
 */

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct lmsnode lmsnode_t;

typedef struct lmspointer {
    lmsnode_t *ptr;
    uintptr_t tag;
} lmspointer_t;

static inline int lmspointer_equals(lmspointer_t a, lmspointer_t b) {
    return a.ptr == b.ptr && a.tag == b.tag;
}

typedef struct lmsnode {
    _Atomic lmspointer_t next;
    _Atomic lmspointer_t prev;
    void *value;
} lmsnode_t;

static void lmsnode_initialize(lmsnode_t *self, void *value) {
    self->value = value;
    lmspointer_t null = {NULL, 0};
    atomic_init((lmspointer_t *)&self->next, null);
    atomic_init((lmspointer_t *)&self->prev, null);
}

#endif
