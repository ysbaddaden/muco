#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcg_basic.h"
#include "fiber.h"
#include "deque.h"

#ifndef NDEBUG
#include "threads.h"
#include <stdio.h>
#include <unistd.h>

typedef struct scheduler {
    int color;

    fiber_t *main;
    fiber_t *current;

    deque_t runnables;
    pcg32_random_t rng;

    ucontext_t link;
} scheduler_t;

static void DEBUG(char *action, scheduler_t *scheduler, fiber_t *fiber) {
    if (fiber) {
        dprintf(2, "\033[%dmthread=%lx %12s scheduler=%p fiber=%p\033[0m\n", scheduler->color, thrd_current(), action, (void *)scheduler, (void *)fiber);
    } else {
        dprintf(2, "\033[%dmthread=%lx %12s scheduler=%p\033[0m\n", scheduler->color, thrd_current(), action, (void *)scheduler);
    }
    fsync(2);
}
#else
#define DEBUG(action, scheduler, fiber)
#endif

static void scheduler_initialize(scheduler_t *self, int color);
static void scheduler_finalize(scheduler_t *self);
static void scheduler_reschedule(scheduler_t *self);
static void scheduler_resume(scheduler_t *self, fiber_t *fiber);
static fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc);
static void scheduler_yield(scheduler_t *self);

static inline void scheduler_enqueue(scheduler_t *self, fiber_t *fiber) {
    DEBUG("enqueue", self, fiber);
    deque_push_bottom(&self->runnables, (void *)fiber);
}

// static inline void scheduler_enqueue_main(scheduler_t *self) {
//     DEBUG("enqueue_main", self, self->main);
//     self->enqueue_main = 1;
// }

#endif
