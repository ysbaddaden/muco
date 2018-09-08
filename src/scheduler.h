#ifndef MUCO_SCHEDULER_PRIV_H
#define MUCO_SCHEDULER_PRIV_H

#include "pcg_basic.h"
#include "fiber.h"
#include "queue.h"

typedef struct scheduler {
    int color;

    fiber_t *main;
    fiber_t *current;

    queue_t runnables;
    queue_t pending;

    pcg32_random_t rng;
} scheduler_t;

#ifdef DEBUG
#include <stdio.h>
#include <unistd.h>
static void LOG(char *action, scheduler_t *scheduler, fiber_t *fiber) {
    if (fiber) {
        dprintf(2, "\033[%dmthread=%lx %12s scheduler=%p fiber=%p [%s]\033[0m\n", scheduler->color, pthread_self(), action, (void *)scheduler, (void *)fiber, fiber->name);
    } else {
        dprintf(2, "\033[%dmthread=%lx %12s scheduler=%p\033[0m\n", scheduler->color, pthread_self(), action, (void *)scheduler);
    }
    //fsync(2);
}
#else
#define LOG(action, scheduler, fiber)
#endif

static void scheduler_initialize(scheduler_t *self, int color);
static void scheduler_finalize(scheduler_t *self);

static fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc, char *name);
static void on_fiber_exit();
static void scheduler_free_pending(scheduler_t *self, int count);

static void scheduler_enqueue(scheduler_t *self, fiber_t *fiber);
static void scheduler_resume(scheduler_t *self, fiber_t *fiber);
static void scheduler_reschedule(scheduler_t *self);
static void scheduler_yield(scheduler_t *self);

static fiber_t *scheduler_steal_once(scheduler_t *self);
static fiber_t *scheduler_steal_loop(scheduler_t *self);
static void *scheduler_start(void *data);

static void scheduler_initialize(scheduler_t *self, int color) {
    self->color = color;
    LOG("initialize", self, NULL);

    self->main = fiber_main();
    self->current = self->main;
    //LOG("spawn_main", self, self->main);

    queue_initialize(&self->runnables);
    queue_initialize(&self->pending);
    self->rng = (pcg32_random_t)PCG32_INITIALIZER;
    pcg32_srandom_r(&self->rng, rand(), 0);
}

static void scheduler_finalize(scheduler_t *self) {
    //LOG("finalize", self, NULL);
    scheduler_free_pending(self, -1);
    queue_finalize(&self->runnables);
    queue_finalize(&self->pending);
    fiber_free(self->main);
}

static fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc, char *name) {
    fiber_t *fiber;

    // try to recycle fiber + stack:
    fiber = queue_pop_bottom(&self->pending);
    if (fiber) {
        fiber_initialize(fiber, proc, on_fiber_exit, name);
    } else {
        // allocate new fiber + stack:
        fiber = fiber_new(proc, on_fiber_exit, name);
    }

    LOG("spawn", self, fiber);
    scheduler_enqueue(self, fiber);
    return fiber;
}

static void on_fiber_exit() {
    // We can't munmap the stack of the current fiber, otherwise current stack
    // frames would become inaccessible, resulting in an immediate segfault. We
    // thus delay the call to `fiber_free` to later on. This also makes it
    // possible to recycle a fiber + stack.

    scheduler_t *scheduler = pthread_getspecific(tl_scheduler);
    fiber_t *fiber = scheduler->current;
    scheduler->current = NULL;

    LOG("done", scheduler, fiber);
    queue_push_bottom(&scheduler->pending, fiber);

    scheduler_resume(scheduler, scheduler->main);
}

static void scheduler_free_pending(scheduler_t *self, int count) {
    for (int i = 0; count < 0 || i < count; i++) {
        fiber_t *fiber = queue_pop_top(&self->pending);
        if (!fiber) break;
        //if (fiber != self->main) {
            LOG("free", self, fiber);
            fiber_free(fiber);
        //}
    }
}

static void scheduler_enqueue(scheduler_t *self, fiber_t *fiber) {
    LOG("enqueue", self, fiber);
    queue_push_bottom(&self->runnables, (void *)fiber);
}

static void scheduler_resume(scheduler_t *self, fiber_t *fiber) {
    fiber_t *current = self->current;
    self->current = fiber;

    // avoid a race condition when a thread may stole a just enqueued fiber
    // whose context hasn't be saved yet, which can happen when a thread
    // enqueues then suspends a Fiber. swapcontext is thus responsible for
    // (un)setting fiber->resumeable when appropriate.
    while (!fiber->resumeable) {
        // LOG("not resumeable", self, fiber);
        sched_yield();
    }
    LOG("resume", self, fiber);

    if (current) {
        //LOG("swapcontext", self, fiber);
        co_swapcontext(current, fiber);
    } else {
        //LOG("setcontext", self, fiber);
        co_setcontext(fiber);
    }
}

static void scheduler_reschedule(scheduler_t *self) {
    LOG("suspend", self, self->current);

    // if any fiber is in queue, resume it:
    fiber_t *fiber = queue_pop_bottom(&self->runnables);

    //if (!fiber) {
    //    // try to steal a fiber (avoid a context switch to main):
    //    fiber = scheduler_steal_once(self);
    //
        // fallback to resume main fiber, allowing to save the current fiber
        // context (making it resumeable):
        if (!fiber) {
            fiber = self->main;
        }
    //}

    scheduler_resume(self, fiber);
}

static void scheduler_yield(scheduler_t *self) {
    // duplicates scheduler_reschedule() but takes care to queue/steal a fiber
    // *before* enqueuing the current fiber, which would otherwise be the one
    // to be picked up, which is pointless.

    // if any fiber is in queue, resume it:
    fiber_t *fiber = queue_pop_bottom(&self->runnables);

    //if (!fiber) {
    //    // try to steal a fiber (avoid a context switch to main):
    //    fiber = scheduler_steal_once(self);

        // fallback to resume main fiber, allowing to save the current fiber
        // context (making it resumeable):
        if (!fiber) {
            fiber = self->main;
        }
    //}

    // actual yield:
    scheduler_enqueue(self, self->current);
    scheduler_resume(self, fiber);
}

static fiber_t *scheduler_steal_once(scheduler_t *self) {
    int j = pcg32_boundedrand_r(&self->rng, co_nprocs);
    scheduler_t *victim = (scheduler_t *)co_schedulers + j;

    if (victim != self) {
        return queue_pop_top(&victim->runnables);
    }
    return NULL;
}

static fiber_t *scheduler_steal_loop(scheduler_t *self) {
    fiber_t *fiber;

    // FIXME: loop for a while (e.g. 100 iterations) then suspend thread and
    //        find a solution to resume a thread when fibers are queued and
    //        there are no thief thread.
    while (1) {
        sched_yield();

        if (!co_running) {
            return NULL;
        }

        fiber = scheduler_steal_once(self);
        if (fiber) {
            //LOG("stole", self, fiber);
            return fiber;
        }
    }
}

static void *scheduler_start(void *data) {
    scheduler_t *scheduler = (scheduler_t *)data;
    pthread_setspecific(tl_scheduler, scheduler);

    while (co_running) {
        // consume from internal queue:
        fiber_t *fiber = queue_pop_bottom(&scheduler->runnables);

        if (!fiber) {
            // empty queue: become thief:
            LOG("thief", scheduler, NULL);
            fiber = scheduler_steal_loop(scheduler);
        }

        if (fiber) {
            scheduler_resume(scheduler, fiber);
        }
    }

    return (void *)0;
}

#endif
