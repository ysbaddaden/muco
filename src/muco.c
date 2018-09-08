#include "scheduler.h"
#include <error.h>
#include <stdlib.h>

static pthread_mutex_t mutex;
static pthread_cond_t cond;

#define CO_SCHEDULER ((scheduler_t *)pthread_getspecific(tl_scheduler))

void co_init(int n) {
    if (n < 0) {
        error(0, 0, "WARNING: ignoring invalid nprocs=%d\n", n);
        n = 1;
    }
    int c = (n == 0) ? 1 : n;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_key_create(&tl_scheduler, NULL);

    co_nprocs = n;
    co_schedulers = calloc(c, sizeof(scheduler_t));
    scheduler_initialize(co_schedulers, 32);

    for (int i = 1; i < co_nprocs; i++) {
        scheduler_initialize((scheduler_t *)co_schedulers + i, 32 + i);
    }

    pthread_setspecific(tl_scheduler, co_schedulers);
}

int co_procs() {
    char *nprocs = getenv("NPROCS");
    if (nprocs) {
        return atoi(nprocs);
    }
    return 4;
}

void co_free() {
    if (co_nprocs == 0) {
        scheduler_finalize(co_schedulers);
    } else {
        for (int i = 0; i < co_nprocs; i++) {
            scheduler_finalize((scheduler_t *)co_schedulers + i);
        }
    }
    free(co_schedulers);

    pthread_key_delete(tl_scheduler);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

scheduler_t *co_scheduler() {
    return CO_SCHEDULER;
}

fiber_t *co_spawn(fiber_main_t proc) {
    return scheduler_spawn(CO_SCHEDULER, proc, NULL);
}

fiber_t *co_spawn_named(fiber_main_t proc, char *name) {
    return scheduler_spawn(CO_SCHEDULER, proc, name);
}

fiber_t *co_fiber_new(fiber_main_t proc, char *name) {
    return fiber_new(proc, NULL, name);
}

void co_fiber_free(fiber_t *fiber) {
    fiber_free(fiber);
}

void co_enqueue(fiber_t *fiber) {
    scheduler_enqueue(CO_SCHEDULER, fiber);
}

void co_suspend() {
    scheduler_reschedule(CO_SCHEDULER);
}

void co_resume(fiber_t *fiber) {
    scheduler_resume(CO_SCHEDULER, fiber);
}

void co_yield() {
    scheduler_yield(CO_SCHEDULER);
}

fiber_t *co_current() {
  return CO_SCHEDULER->current;
}

fiber_t *co_main() {
  return CO_SCHEDULER->main;
}

void co_run() {
    pthread_t thread;
    LOG("run", CO_SCHEDULER, NULL);

    co_running = 1;

    for (int i = 0; i < co_nprocs; i++) {
        if (pthread_create(&thread, NULL, scheduler_start, (scheduler_t *)co_schedulers + i)) {
            error(1, 0, "pthread_create failed");
        }
    }

    struct timespec ts = {5, 0};

    pthread_mutex_lock(&mutex);
    while (1) {
        pthread_cond_timedwait(&cond, &mutex, &ts);
        if (!co_running) return;

        for (int i = 0; i < co_nprocs; i++) {
            scheduler_t *s = (scheduler_t *)co_schedulers + i;
            int count = queue_lazy_size(&s->pending) / 2;
            scheduler_free_pending(s, count);
        }
        if (!co_running) return;
    }
    pthread_mutex_unlock(&mutex);
}

void co_break() {
    LOG("break", CO_SCHEDULER, NULL);
    pthread_mutex_lock(&mutex);
    co_running = 0;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}
