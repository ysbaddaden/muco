#include "config.h"
#include "scheduler.h"
#include "stack.h"
#include <sched.h>

static inline fiber_t *next_runnable(scheduler_t *self) {
    fiber_t *fiber;
    while (1) {
        fiber = deque_pop_bottom(&self->runnables);
        if (fiber) {
            return fiber;
        }
        sched_yield();
    }
}

static void on_fiber_exit(scheduler_t *self) {
    // TODO: recycle fiber (and its stack)
    fiber_free(self->current);
    self->current = NULL;
    scheduler_reschedule(self);
}

void scheduler_initialize(scheduler_t *self) {
    self->main = fiber_main();
    self->current = self->main;

    deque_initialize(&self->runnables);

    if (getcontext(&self->link) == -1) {
        error(1, errno, "getcontext");
    }
    stack_allocate(&self->link.uc_stack, CO_STACK_SIZE);
    self->link.uc_link = NULL;
    makecontext(&self->link, (void (*)())on_fiber_exit, 1, self);
}

void scheduler_finalize(scheduler_t *self) {
    deque_finalize(&self->runnables);
    fiber_free(self->main);
}

void scheduler_reschedule(scheduler_t *self) {
    scheduler_resume(self, next_runnable(self));
}

void scheduler_resume(scheduler_t *self, fiber_t *fiber) {
    fiber_t *current = self->current;
    self->current = fiber;

    if (current) {
        if (swapcontext(&current->state, &fiber->state) == -1) {
            error(1, errno, "swapcontext");
        }
    } else {
        if (setcontext(&fiber->state) == -1) {
            error(1, errno, "setcontext");
        }
    }
}

fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc) {
    fiber_t *fiber = fiber_new(proc, &self->link);
    scheduler_enqueue(self, fiber);
    return fiber;
}

void scheduler_yield(scheduler_t *self) {
    fiber_t *fiber = next_runnable(self);
    scheduler_enqueue(self, self->current);
    scheduler_resume(self, fiber);
}
