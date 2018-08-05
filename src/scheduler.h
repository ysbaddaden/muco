#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "muco/scheduler.h"

static void scheduler_initialize(scheduler_t *self);
static void scheduler_finalize(scheduler_t *self);
static void scheduler_reschedule(scheduler_t *self);
static void scheduler_resume(scheduler_t *self, fiber_t *fiber);
static fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc);
static void scheduler_yield(scheduler_t *self);

static inline void scheduler_enqueue(scheduler_t *self, fiber_t *fiber) {
    deque_push_bottom(&self->runnables, (void *)fiber);
}

#endif
