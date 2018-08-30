#ifndef MUCO_DEQUE_H
#define MUCO_DEQUE_H

struct scheduler_deque_age {
    int tag;
    int top;
};

typedef struct scheduler_deque {
    _Atomic int bot;
    _Atomic struct scheduler_deque_age age;
    void **deq;
} scheduler_deque_t;

#endif
