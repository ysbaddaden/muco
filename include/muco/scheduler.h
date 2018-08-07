#ifndef MUCO_SCHEDULER_H
#define MUCO_SCHEDULER_H

#include "pcg_basic.h"
#include "muco/fiber.h"
#include "muco/deque.h"

typedef struct scheduler {
    int color;

    fiber_t *main;
    fiber_t *current;

    deque_t runnables;
    pcg32_random_t rng;

    ucontext_t link;
} scheduler_t;

#endif
