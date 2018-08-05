#ifndef MUCO_DEQUE_H
#define MUCO_DEQUE_H

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

#endif
