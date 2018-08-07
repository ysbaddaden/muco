#include "scheduler.c"
#include "threads.h"
#include <stdlib.h>

static mtx_t co_mutex;
static cnd_t co_cond;

static int co_thread(void *data) {
    scheduler_t *s = (scheduler_t *)data;
    tss_set(scheduler, s);
    scheduler_reschedule(s);
    return 0;
}

void co_init(int n) {
    thrd_t thread;

    if (n < 1) {
        error(0, 0, "WARNING: ignoring invalid nprocs=%d using default of %d\n", n, 1);
        n = 1;
    }

    mtx_init(&co_mutex, mtx_plain);
    cnd_init(&co_cond);
    tss_create(&scheduler, (void(*)(void *))scheduler_finalize);

    nprocs = n;
    schedulers = calloc(n + 1, sizeof(scheduler_t));

    tss_set(scheduler, schedulers);
    scheduler_initialize(schedulers, 32);

    for (int i = 1; i <= nprocs; i++) {
        scheduler_initialize(schedulers + i, 32 + i);
    }

    for (int i = 1; i <= nprocs; i++) {
        if (thrd_create(&thread, co_thread, schedulers + i) != thrd_success) {
            error(1, 0, "thrd_create failed");
        }
    }
}

void co_free() {
    scheduler_finalize(tss_get(scheduler));
    mtx_destroy(&co_mutex);
    cnd_destroy(&co_cond);
}

static inline scheduler_t *co_scheduler() {
    return tss_get(scheduler);
}

fiber_t *co_spawn(fiber_main_t proc) {
    return scheduler_spawn(co_scheduler(), proc);
}

fiber_t *co_fiber_new(fiber_main_t proc) {
    return fiber_new(proc, NULL);
}

void co_fiber_free(fiber_t *fiber) {
    fiber_free(fiber);
}

void co_enqueue(fiber_t *fiber) {
    scheduler_enqueue(co_scheduler(), fiber);
}

void co_suspend() {
    scheduler_reschedule(co_scheduler());
}

void co_resume(fiber_t *fiber) {
    scheduler_resume(co_scheduler(), fiber);
}

void co_yield() {
    scheduler_yield(co_scheduler());
}

void co_run() {
    DEBUG("run", co_scheduler(), NULL);
    mtx_lock(&co_mutex);
    cnd_wait(&co_cond, &co_mutex);
    mtx_unlock(&co_mutex);
}

void co_break() {
    DEBUG("break", co_scheduler(), NULL);
    mtx_lock(&co_mutex);
    running = 0;
    cnd_signal(&co_cond);
    mtx_unlock(&co_mutex);
}
