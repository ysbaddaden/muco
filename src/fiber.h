#ifndef FIBER_H
#define FIBER_H

#include "ucontext.h"

typedef void (*fiber_main_t)();

typedef struct fiber {
    ucontext_t state;
} fiber_t;

static int fiber_initialize(fiber_t *self, fiber_main_t proc, ucontext_t *link);
static fiber_t *fiber_new(fiber_main_t proc, ucontext_t *link);
static fiber_t *fiber_main(void);
static void fiber_finalize(fiber_t *self);
static void fiber_free(fiber_t *self);

#endif
