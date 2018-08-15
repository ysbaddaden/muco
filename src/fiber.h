#ifndef FIBER_H
#define FIBER_H

#include <signal.h>

typedef struct fiber fiber_t;
typedef void (*fiber_main_t)();
typedef void (*fiber_exit_t)(fiber_t *);
typedef void (*fiber_run_t)(fiber_t *);

typedef struct fiber {
    long resumeable; // don't move: required by context asm
    void *stack_top; // don't move: required by context asm

    stack_t stack;

    fiber_main_t proc;
    fiber_exit_t link;
} fiber_t;

static int fiber_initialize(fiber_t *self, fiber_main_t proc, fiber_exit_t link);
static fiber_t *fiber_new(fiber_main_t proc, fiber_exit_t link);
static fiber_t *fiber_main(void);
static void fiber_run(fiber_t *self);
static void fiber_finalize(fiber_t *self);
static void fiber_free(fiber_t *self);

static void fiber_makecontext(fiber_t *);
static void fiber_main_makecontext(fiber_t *);
void co_setcontext(fiber_t *);
void co_swapcontext(fiber_t *, fiber_t *);

#endif
