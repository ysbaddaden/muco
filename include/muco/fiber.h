#ifndef MUCO_FIBER_H
#define MUCO_FIBER_H

#include <signal.h>
#include <stdint.h>

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

    char *name;
} fiber_t;

fiber_t *co_fiber_new(fiber_main_t);
void co_fiber_free(fiber_t *);

#endif
