#ifndef MUCO_H
#define MUCO_H

#include <signal.h>
#include "muco/fiber.h"
#include "muco/scheduler.h"

int co_nprocs;
int co_procs();

void co_init(int);
void co_free();
void co_run();
void co_break();

fiber_t *co_spawn(fiber_main_t);
fiber_t *co_spawn_named(fiber_main_t, char *);

scheduler_t *co_scheduler();
fiber_t *co_current();
fiber_t *co_main();

void co_enqueue(fiber_t *);
void co_suspend();
void co_resume(fiber_t *);
void co_yield();

#endif
