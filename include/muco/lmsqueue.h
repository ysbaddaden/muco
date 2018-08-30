#ifndef MUCO_LMSQUEUE_H
#define MUCO_LMSQUEUE_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct lmsnode lmsnode_t;

typedef struct lmspointer {
    lmsnode_t *ptr;
    uintptr_t tag;
} lmspointer_t;

typedef struct lmsnode {
    _Atomic lmspointer_t next;
    _Atomic lmspointer_t prev;
    void *value;
} lmsnode_t;

typedef struct lmsqueue {
    _Atomic lmspointer_t head;
    _Atomic lmspointer_t tail;
    lmsnode_t *dummy;
} lmsqueue_t;

#endif
