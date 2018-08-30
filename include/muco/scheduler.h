#ifndef MUCO_SCHEDULER_H
#define MUCO_SCHEDULER_H

#include "muco/fiber.h"
#include <stdint.h>

struct scheduler_queue {
    _Atomic int bot;
    _Atomic struct {
        int tag;
        int top;
    } age;
};

typedef struct scheduler {
    int color;

    fiber_t *main;
    fiber_t *current;

    struct scheduler_queue runnables;
    struct scheduler_queue pending;

    struct {
        uint64_t state;
        uint64_t inc;
    } rng;
} scheduler_t;

#endif
