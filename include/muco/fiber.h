#ifndef MUCO_FIBER_H
#define MUCO_FIBER_H

#include "ucontext.h"
#include "threads.h"

typedef void (*fiber_main_t)();

typedef struct fiber {
    ucontext_t state;
} fiber_t;

#endif
