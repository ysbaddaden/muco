#include "config.h"
#include "deque.c"
#include "fiber.c"
#include "scheduler.h"
#include "stack.h"

#include <stdio.h>

static int running = 1;
static int nprocs;
static scheduler_t *schedulers;

static tss_t scheduler;

static inline fiber_t *scheduler_steal(scheduler_t *self) {
    int j = pcg32_boundedrand_r(&self->rng, nprocs);
    scheduler_t *victim = schedulers + j;

    if (victim != self) {
        return deque_pop_top(&victim->runnables);
    }
    return NULL;
}

static inline fiber_t *next_runnable(scheduler_t *self) {
    fiber_t *fiber;
    int i = 0;

    while (1) {
        thrd_yield();

        if (!running) {
            thrd_exit(thrd_current());
        }

        // try to deque
        fiber = deque_pop_bottom(&self->runnables);
        if (fiber) {
            return fiber;
        }

#ifndef NDEBUG
        if (i == 0) {
            i = 1;
            DEBUG("thief", self, NULL);
        }
#endif
        fiber = scheduler_steal(self);
        if (fiber) {
            DEBUG("stole", self, fiber);
            return fiber;
        }
    }
}

static void on_fiber_exit() {
    // TODO: recycle fiber (and its stack)

    scheduler_t *s = tss_get(scheduler);
    fiber_t *fiber = s->current;
    s->current = NULL;

    DEBUG("free", s, fiber);
    fiber_free(fiber);

    scheduler_reschedule(s);
}

static void scheduler_initialize(scheduler_t *self, int color) {
    self->color = color;
    DEBUG("initialize", self, NULL);

    //self->thread = thrd_current();

    self->main = fiber_main();
    self->current = self->main;
    //DEBUG("spawn_main", self, self->main);

    deque_initialize(&self->runnables);
    self->rng = (pcg32_random_t)PCG32_INITIALIZER;
    pcg32_srandom_r(&self->rng, rand(), 0);

    // self->enqueue_main = 0;

    if (getcontext(&self->link) == -1) {
        perror("getcontext");
        exit(EXIT_FAILURE);
    }
    stack_allocate(&self->link.uc_stack, STACK_SIZE);
    self->link.uc_link = NULL;
    makecontext(&self->link, (void (*)())on_fiber_exit, 0);
}

static void scheduler_finalize(scheduler_t *self) {
    DEBUG("finalize", self, NULL);
    deque_finalize(&self->runnables);
    fiber_free(self->main);
}

static void scheduler_reschedule(scheduler_t *self) {
    DEBUG("reschedule", self, NULL);
    scheduler_resume(self, next_runnable(self));
}

static void scheduler_resume(scheduler_t *self, fiber_t *fiber) {
    DEBUG("resume", self, fiber);
    fiber_t *current = self->current;
    self->current = fiber;

    if (current) {
        if (swapcontext(&current->state, &fiber->state) == -1) {
            perror("swapcontext");
            exit(EXIT_FAILURE);
        }
    } else {
        if (setcontext(&fiber->state) == -1) {
            perror("setcontext");
            exit(EXIT_FAILURE);
        }
    }
}

static fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc) {
    fiber_t *fiber = fiber_new(proc, &self->link);
    DEBUG("spawn", self, fiber);
    scheduler_enqueue(self, fiber);
    return fiber;
}

static void scheduler_yield(scheduler_t *self) {
    DEBUG("yield", self, NULL);

    fiber_t *fiber = deque_pop_bottom(&self->runnables);
    scheduler_enqueue(self, self->current);

    if (!fiber) {
        fiber = next_runnable(self);
    }
    scheduler_resume(self, fiber);
}
