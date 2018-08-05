#include "config.h"
#include "fiber.h"
#include "stack.h"

int fiber_initialize(fiber_t *self, fiber_main_t proc, ucontext_t *link) {
    if (getcontext(&self->state) == -1) {
        error(1, errno, "getcontext");
    }
    stack_allocate(&self->state.uc_stack, CO_STACK_SIZE);
    self->state.uc_link = link;
    makecontext(&self->state, proc, 0);
    return 0;
}

fiber_t *fiber_new(fiber_main_t proc, ucontext_t *link) {
    fiber_t *self = calloc(1, sizeof(fiber_t));
    if (self == NULL) {
        error(1, errno, "calloc");
    }
    fiber_initialize(self, proc, link);
    return self;
}

fiber_t *fiber_main(void) {
    fiber_t *self = calloc(1, sizeof(fiber_t));
    if (self == NULL) {
        error(1, errno, "calloc");
    }
    if (getcontext(&self->state) == -1) {
        error(1, errno, "getcontext");
    }
    return self;
}

void fiber_finalize(fiber_t *self) {
    stack_deallocate(&self->state.uc_stack);
}

void fiber_free(fiber_t *self) {
    fiber_finalize(self);
    free(self);
}
