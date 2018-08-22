#ifndef CORO_H
#define CORO_H

typedef void co_fiber_t;
typedef void co_scheduler_t;
typedef void (*co_fiber_main_t)();
typedef void co_lock_t;
typedef void co_channel_t;

void co_init(int nprocs);
void co_free();
co_fiber_t *co_spawn(co_fiber_main_t proc);
co_fiber_t *co_fiber_new(co_fiber_main_t proc);
void co_fiber_free(co_fiber_t *fiber);
void co_enqueue(co_fiber_t *fiber);
void co_suspend();
void co_resume(co_fiber_t *fiber);
void co_yield();

co_fiber_t *co_scheduler_main();
void co_run();
void co_break();

co_lock_t *co_lock_new();
void co_lock_free(co_lock_t *);
int co_lock(co_lock_t *);
int co_unlock(co_lock_t *);

co_channel_t *co_channel_new();
void co_channel_free(co_channel_t *);
int co_channel_send(co_channel_t *, void *);
int co_channel_receive(co_channel_t *, void **);

#endif
