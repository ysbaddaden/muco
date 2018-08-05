#ifndef FIBER_H
#define FIBER_H

#include "muco/fiber.h"

static int fiber_initialize(fiber_t *self, fiber_main_t proc, ucontext_t *link);
static fiber_t *fiber_new(fiber_main_t proc, ucontext_t *link);
static fiber_t *fiber_main(void);
static void fiber_finalize(fiber_t *self);
static void fiber_free(fiber_t *self);

#endif
