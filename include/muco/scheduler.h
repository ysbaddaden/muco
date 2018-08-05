#ifndef MUCO_SCHEDULER_H
#define MUCO_SCHEDULER_H

#include "muco/fiber.h"
#include "muco/deque.h"

typedef struct scheduler {
  fiber_t *main;
  fiber_t *current;
  deque_t runnables;
  ucontext_t link;
} scheduler_t;

#endif
