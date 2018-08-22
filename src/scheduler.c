#include "config.h"
#include "deque.c"
#include "fiber.c"
#include "scheduler.h"
#include "stack.h"

static int running = 1;
static int nprocs;
static scheduler_t *schedulers;

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
#ifdef DEBUG
    int i = 0;
#endif

    while (1) {
        thrd_yield();

        if (!running) {
            return NULL;
        }

        fiber = deque_pop_bottom(&self->runnables);
        if (fiber) {
            return fiber;
        }

#ifdef DEBUG
        if (i == 0) {
            i = 1;
            LOG("thief", self, NULL);
        }
#endif
        fiber = scheduler_steal(self);
        if (fiber) {
            LOG("stole", self, fiber);
            return fiber;
        }
    }
}

static void on_fiber_exit() {
    // We can't munmap the stack of the current fiber, otherwise current stack
    // frames would become unaccessible, resulting in an immediate segfault. We
    // thus delay the call to `fiber_free` to later on. This also makes it
    // possible to recycle a fiber + stack.

    scheduler_t *s = tss_get(scheduler);
    fiber_t *fiber = s->current;
    s->current = NULL;

    LOG("done", s, fiber);
    deque_push_bottom(&s->pending, fiber);

    scheduler_resume(s, s->main);
}

static void scheduler_initialize(scheduler_t *self, int color) {
    self->color = color;
    LOG("initialize", self, NULL);

    self->main = fiber_main();
    self->current = self->main;
    LOG("spawn_main", self, self->main);

    deque_initialize(&self->runnables);
    deque_initialize(&self->pending);
    self->rng = (pcg32_random_t)PCG32_INITIALIZER;
    pcg32_srandom_r(&self->rng, rand(), 0);
}

static void scheduler_finalize(scheduler_t *self) {
    LOG("finalize", self, NULL);
    scheduler_free_pending(self, -1);
    deque_finalize(&self->runnables);
    deque_finalize(&self->pending);
    fiber_free(self->main);
}

static int scheduler_thread_start(void *data) {
    scheduler_t *s = (scheduler_t *)data;
    tss_set(scheduler, s);

    while (running) {
        fiber_t *fiber = next_runnable(s);
        if (fiber) {
            scheduler_resume(s, fiber);
        }
    }
    return 0;
}

static void scheduler_free_pending(scheduler_t *self, int count) {
    for (int i = 0; count < 0 || i < count; i++) {
        fiber_t *fiber = deque_pop_top(&self->pending);
        if (!fiber) break;
        //if (fiber != self->main) {
            LOG("free", self, fiber);
            fiber_free(fiber);
        //}
    }
}

static void scheduler_reschedule(scheduler_t *self) {
    LOG("reschedule", self, NULL);

    // if any fiber is in queue, resume it:
    fiber_t *fiber = deque_pop_bottom(&self->runnables);

    if (!fiber) {
        // try to steal a fiber:
        fiber = scheduler_steal(self);

        // fallback to resume main fiber:
        if (!fiber) {
            fiber = self->main;
        }
    }

    scheduler_resume(self, fiber);
}

static void scheduler_resume(scheduler_t *self, fiber_t *fiber) {
    LOG("resume", self, fiber);

    fiber_t *current = self->current;
    self->current = fiber;

    // Avoid a race condition where a fiber may be stolen *before* it's state is
    // actually saved: we spin until `co_swapcontext` stored the fiber's state.
    while (!fiber->resumeable) {
        // LOG("not resumeable", self, fiber);
        thrd_yield();
    }

    if (current) {
        //LOG("swapcontext", self, fiber);
        co_swapcontext(current, fiber);
    } else {
        //LOG("setcontext", self, fiber);
        co_setcontext(fiber);
    }
}

static fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc) {
    fiber_t *fiber;

    // try to recycle fiber + stack:
    fiber = deque_pop_bottom(&self->pending);
    if (fiber) {
        fiber_initialize(fiber, proc, on_fiber_exit);
    } else {
        // allocate new fiber + stack:
        fiber = fiber_new(proc, on_fiber_exit);
    }

    LOG("spawn", self, fiber);
    scheduler_enqueue(self, fiber);
    return fiber;
}

static void scheduler_yield(scheduler_t *self) {
    LOG("yield", self, NULL);

    // if any fiber is in queue, resume it:
    fiber_t *fiber = deque_pop_bottom(&self->runnables);

    // resume scheduler's main fiber that will steal work (allowing current
    // fiber to be stolen):
    if (!fiber) {
        fiber = self->main;
    }

    // actual yield:
    scheduler_enqueue(self, self->current);
    scheduler_resume(self, fiber);
}
