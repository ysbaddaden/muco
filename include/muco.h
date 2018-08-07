#ifndef CORO_H
#define CORO_H

typedef void fiber_t;
typedef void scheduler_t;
typedef void (*fiber_main_t)();

void co_init(int nprocs);
void co_free();
fiber_t *co_spawn(fiber_main_t proc);
fiber_t *co_fiber_new(fiber_main_t proc);
void co_fiber_free(fiber_t *fiber);
void co_enqueue(fiber_t *fiber);
void co_suspend();
void co_resume(fiber_t *fiber);
void co_yield();

void co_run();
void co_break();

#endif
