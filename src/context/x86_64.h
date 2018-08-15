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
        __attribute__((__unused__)) fiber_t *fiber
) {
    __asm__ volatile (
        "movl $0, 0(%rdi);"    // fiber->resumeable = 0
        "movq 8(%rdi), %rsp;"  // rsp = fiber->stack_top
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
        __attribute__((__unused__)) fiber_t *curr,
        __attribute__((__unused__)) fiber_t *next
) {
    __asm__ volatile (
        // getcontext (curr):
        "pushq %rdi;"          // push argument for initial resume
        "pushq %rbx;"          // push callee-saved registers on the stack
        "pushq %rbp;"
        "pushq %r12;"
        "pushq %r13;"
        "pushq %r14;"
        "pushq %r15;"
        "movq %rsp, 8(%rdi);"  // save stack top (curr->stack_top = rsp)
        "movl $1, 0(%rdi);"    // set previous fiber as resumeable (curr->resumeable = 1)

        // setcontext (next):
        "movl $0, (%rsi);"     // set next fiber as non resumeable (next->resumeable = 0)
        "movq 8(%rsi), %rsp;"  // load stack top (rsp = next->stack_top)
        "popq %r15;"           // pop callee-saved registers from the stack
        "popq %r14;"
        "popq %r13;"
        "popq %r12;"
        "popq %rbp;"
        "popq %rbx;"
        "popq %rdi;"           // pop argument for initial resume
        "retq;"                // popq rpi
    );
}
