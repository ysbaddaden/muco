#ifndef MUCO_STACK_H
#define MUCO_STACK_H

#include "config.h"

static inline void stack_allocate(stack_t *stack, size_t size) {
    void *sp = mmap(NULL, size, STACK_PROT, STACK_MAP, STACK_FD, STACK_OFFSET);
    if (sp == MAP_FAILED) {
        error(1, errno, "mmap");
    }

#if defined(__linux__)
    if (madvise(sp, size, MADV_NOHUGEPAGE) == -1) {
        error(1, errno, "madvise");
    }
#endif

    if (mprotect(sp, 4096, PROT_NONE) == -1) {
        error(1, errno, "mprotect");
    }

    stack->ss_sp = sp;
    stack->ss_size = size;
    stack->ss_flags = 0;
}

static inline void stack_deallocate(stack_t *stack) {
    if (stack->ss_sp) {
        munmap(stack->ss_sp, stack->ss_size);
    }
}

#endif
