#ifndef MUCO_FIBER_PRIV_H
#define MUCO_FIBER_PRIV_H

#include "stack.h"

typedef struct fiber fiber_t;
typedef void (*fiber_main_t)();
typedef void (*fiber_exit_t)(fiber_t *);
typedef void (*fiber_run_t)(fiber_t *);
static void fiber_run(fiber_t *);

typedef struct fiber {
    long resumeable; // don't move: required by context asm
    void *stack_top; // don't move: required by context asm

    stack_t stack;

    fiber_main_t proc;
    fiber_exit_t link;

    char *name;
} fiber_t;

#include "config.h"
#include "context.h"

#include <error.h>
#include <stdlib.h>

static void fiber_initialize(fiber_t *self, fiber_main_t proc, fiber_exit_t link, char *name) {
    self->resumeable = 1;
    self->proc = proc;
    self->link = link;
    fiber_makecontext(self);
    self->name = name;
}

static void fiber_finalize(fiber_t *self) {
    stack_deallocate(&self->stack);
}

static void fiber_run(fiber_t *self) {
    self->proc();
    if (self->link) {
        self->link(self);
    }
}

static fiber_t *fiber_main(void) {
    fiber_t *self = calloc(1, sizeof(fiber_t));
    if (self == NULL) {
        error(1, errno, "calloc");
    }
    self->resumeable = 0;
    self->name = "main";
    fiber_main_makecontext(self);
    return self;
}

static fiber_t *fiber_new(fiber_main_t proc, fiber_exit_t link, char *name) {
    fiber_t *self = calloc(1, sizeof(fiber_t));
    if (self == NULL) {
        error(1, errno, "calloc");
    }
    stack_allocate(&self->stack, STACK_SIZE);
    fiber_initialize(self, proc, link, name);
    return self;
}

static void fiber_free(fiber_t *self) {
    fiber_finalize(self);
    free(self);
}

#endif
