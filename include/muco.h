#ifndef CORO_H
#define CORO_H

#include "muco/scheduler.h"

scheduler_t *co_scheduler();
void co_init();
void co_free();
fiber_t *co_spawn(fiber_main_t proc);
fiber_t *co_fiber_new(fiber_main_t proc);
void co_fiber_free(fiber_t *fiber);
void co_enqueue(fiber_t *fiber);
void co_suspend();
void co_resume(fiber_t *fiber);
void co_yield();

#endif
