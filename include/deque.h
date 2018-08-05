#ifndef DEQUE_H
#define DEQUE_H

#include <stdatomic.h>

struct age {
    int tag;
    int top;
};

typedef struct deque {
    _Atomic int bot;
    _Atomic struct age age;
    void **deq;
} deque_t;

void deque_initialize(deque_t *self);
void deque_finalize(deque_t *self);
void deque_push_bottom(deque_t *self, void *item);
void *deque_pop_bottom(deque_t *self);
void *deque_pop_top(deque_t *self);

#endif
