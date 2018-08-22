#include "config.h"
#include "fiber.h"
#include "context.h"
#include "stack.h"

static void fiber_run(fiber_t *self) {
    self->proc();
    if (self->link) {
        self->link(self);
    }
}

static int fiber_initialize(fiber_t *self, fiber_main_t proc, fiber_exit_t link) {
    self->resumeable = 1;
    self->proc = proc;
    self->link = link;
    fiber_makecontext(self);
    lmsnode_init(&self->node, self);
    return 0;
}

static fiber_t *fiber_new(fiber_main_t proc, fiber_exit_t link) {
    fiber_t *self = calloc(1, sizeof(fiber_t));
    if (self == NULL) {
        error(1, errno, "calloc");
    }
    stack_allocate(&self->stack, STACK_SIZE);
    fiber_initialize(self, proc, link);
    return self;
}

static fiber_t *fiber_main(void) {
    fiber_t *self = calloc(1, sizeof(fiber_t));
    if (self == NULL) {
        error(1, errno, "calloc");
    }
    self->resumeable = 0;
    fiber_main_makecontext(self);
    lmsnode_init(&self->node, self);
    return self;
}

static void fiber_finalize(fiber_t *self) {
    stack_deallocate(&self->stack);
}

static void fiber_free(fiber_t *self) {
    fiber_finalize(self);
    free(self);
}
