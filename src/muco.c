#include "scheduler.c"
#include <stdlib.h>

static scheduler_t scheduler;

scheduler_t *co_scheduler() {
    return &scheduler;
}

void co_init() {
    scheduler_initialize(&scheduler);
}

void co_free() {
    scheduler_finalize(&scheduler);
}

fiber_t *co_spawn(fiber_main_t proc) {
    return scheduler_spawn(&scheduler, proc);
}

fiber_t *co_fiber_new(fiber_main_t proc) {
    return fiber_new(proc, NULL);
}

void co_fiber_free(fiber_t *fiber) {
    fiber_free(fiber);
}

void co_enqueue(fiber_t *fiber) {
    scheduler_enqueue(&scheduler, fiber);
}

void co_suspend() {
    scheduler_reschedule(&scheduler);
}

void co_resume(fiber_t *fiber) {
    scheduler_resume(&scheduler, fiber);
}

void co_yield() {
    scheduler_yield(&scheduler);
}
