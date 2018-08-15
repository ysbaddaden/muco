#include "scheduler.c"
#include "lock.c"
#include "threads.h"
#include <stdlib.h>

static mtx_t co_mutex;
static cnd_t co_cond;

void co_init(int n) {
    if (n < 0) {
        error(0, 0, "WARNING: ignoring invalid nprocs=%d\n", n);
        n = 1;
    }
    int c = (n == 0) ? 1 : n;

    mtx_init(&co_mutex, mtx_plain);
    cnd_init(&co_cond);
    //tss_create(&scheduler, (void(*)(void *))scheduler_finalize);
    tss_create(&scheduler, NULL);

    nprocs = n;
    schedulers = calloc(c, sizeof(scheduler_t));
    scheduler_initialize(schedulers, 32);

    for (int i = 1; i < nprocs; i++) {
        scheduler_initialize(schedulers + i, 32 + i);
    }

    tss_set(scheduler, schedulers);
}

void co_free() {
    if (nprocs == 0) {
        scheduler_finalize(schedulers);
    } else {
        for (int i = 0; i < nprocs; i++) {
            scheduler_finalize(schedulers + i);
        }
    }
    free(schedulers);

    tss_delete(scheduler);

    mtx_destroy(&co_mutex);
    cnd_destroy(&co_cond);
}

static inline scheduler_t *co_scheduler() {
    return scheduler_current();
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

fiber_t *co_scheduler_main() {
  return co_scheduler()->main;
}

void co_run() {
    thrd_t thread;
    LOG("run", co_scheduler(), NULL);

    for (int i = 0; i < nprocs; i++) {
        if (thrd_create(&thread, scheduler_thread_start, schedulers + i) != thrd_success) {
            error(1, 0, "thrd_create failed");
        }
    }

    struct timespec ts = {5, 0};

    mtx_lock(&co_mutex);
    while (1) {
        cnd_timedwait(&co_cond, &co_mutex, &ts);
        if (!running) return;

        for (int i = 0; i < nprocs; i++) {
            scheduler_t *s = schedulers + i;
            int count = deque_lazy_size(&s->pending) / 2;
            scheduler_free_pending(s, count);
        }
        if (!running) return;
    }
    mtx_unlock(&co_mutex);
}

void co_break() {
    LOG("break", co_scheduler(), NULL);
    mtx_lock(&co_mutex);
    running = 0;
    cnd_signal(&co_cond);
    mtx_unlock(&co_mutex);
}
