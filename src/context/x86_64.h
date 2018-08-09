#include "../config.h"
#include "../fiber.h"
#include <stdint.h>
#include <string.h>

// http://cons.mit.edu/fa16/x86-64-architecture-guide.html#registers

#define STACK_ALIGN_MASK (~(uintptr_t)16)
#define WORD_SIZE sizeof(uintptr_t)

static void fiber_makecontext(fiber_t *self) {
    stack_t s = self->stack;

    void *sp = (char *)s.ss_sp + s.ss_size;           // stack grows down
    sp = (char *)sp - sizeof(void *);                 // ???
    sp = (void *)((uintptr_t)sp & STACK_ALIGN_MASK);  // align stack to 16 bytes

    *((char *)sp - 7 * WORD_SIZE) = 0;                // R15
    *((char *)sp - 6 * WORD_SIZE) = 0;                // R14
    *((char *)sp - 5 * WORD_SIZE) = 0;                // R13
    *((char *)sp - 4 * WORD_SIZE) = 0;                // R12
    *((char *)sp - 3 * WORD_SIZE) = 0;                // RBP
    *((char *)sp - 2 * WORD_SIZE) = 0;                // RBX
    *(fiber_t **)((char *)sp - 1*8) = self;           // RDI (argument to initial resume)
    *(fiber_run_t *)((char *)sp - 0*8) = fiber_run;   // RPI (initial resume symbol)

    self->stack_top = (char *)sp - 7 * WORD_SIZE;     // save stack pointer
}

static void fiber_main_makecontext(fiber_t *self) {
    //self->stack.ss_sp = stack_bottom  // TODO: detect main stack bottom
    //self->stack.ss_size = stack_size  // TODO: detect main stack size
    self->stack.ss_flags = 0;

    uintptr_t registers = 0;
    self->stack_top = (char *)&registers - WORD_SIZE;
}

__attribute__((naked)) void co_setcontext(
        __attribute__((__unused__)) void *sp,
        __attribute__((__unused__)) int *resumeable
) {
    __asm__ volatile (
        "movl $0, (%rsi);" // set fiber as non resumeable
        "movq %rdi, %rsp;" // rdi = sp
        "popq %r15;"
        "popq %r14;"
        "popq %r13;"
        "popq %r12;"
        "popq %rbp;"
        "popq %rbx;"
        "popq %rdi;"
        "retq;"            // popq rpi
    );
}

__attribute__((naked)) void co_swapcontext(
        __attribute__((__unused__)) void *ssp,
        __attribute__((__unused__)) int *sresumeable,
        __attribute__((__unused__)) void *dsp,
        __attribute__((__unused__)) int *dresumeable
) {
    __asm__ volatile (
        "pushq %rdi;"        // push argument for initial resume
        "pushq %rbx;"        // push callee-saved registers on the stack
        "pushq %rbp;"
        "pushq %r12;"
        "pushq %r13;"
        "pushq %r14;"
        "pushq %r15;"
        "movq %rsp, (%rdi);" // save stack top (ssp)
        "movl $1, (%rsi);"   // set previous fiber as resumeable

        "movl $0, (%rcx);"   // set next fiber as non resumeable
        "movq %rdx, %rsp;"   // load stack top (dsp)
        "popq %r15;"         // pop callee-saved registers from the stack
        "popq %r14;"
        "popq %r13;"
        "popq %r12;"
        "popq %rbp;"
        "popq %rbx;"
        "popq %rdi;"         // pop argument for initial resume
        "retq;"              // popq rpi
    );
}
