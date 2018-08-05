#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "fiber.h"
#include "deque.h"

typedef struct scheduler {
  fiber_t *main;
  fiber_t *current;
  deque_t runnables;
  ucontext_t link;
} scheduler_t;

void scheduler_initialize(scheduler_t *self);
void scheduler_finalize(scheduler_t *self);

// Switches execution to *fiber* immediately. Doesn't queue the current fiber
// that will have to be resumed manually.
void scheduler_reschedule(scheduler_t *self);

// Switches execution to *fiber* immediately. Doesn't queue the current fiber
// that will have to be resumed manually.
void scheduler_resume(scheduler_t *self, fiber_t *fiber);

// Spawns a fiber, then puts it into the queue. Returns immediately.
fiber_t *scheduler_spawn(scheduler_t *self, fiber_main_t proc);

// Enqueues a fiber to be resumed later. Returns immediately.
inline static void scheduler_enqueue(scheduler_t *self, fiber_t *fiber) {
    deque_push_bottom(&self->runnables, (void *)fiber);
}

// Queues current fiber then resumes the next queued fiber.
void scheduler_yield(scheduler_t *self);

#endif
