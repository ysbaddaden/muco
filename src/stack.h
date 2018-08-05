#ifndef STACK_H
#define STACK_H

static inline void stack_allocate(stack_t *stack, size_t size) {
    void *sp = mmap(NULL, size, CO_STACK_PROT, CO_STACK_MAP, CO_STACK_FD, CO_STACK_OFFSET);
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
