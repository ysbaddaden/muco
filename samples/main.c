#include "scheduler.h"
#include <stdio.h>

scheduler_t scheduler;
fiber_t *f1;
fiber_t *f2;

void coroutine1() {
  printf("coroutine1: called\n");
  scheduler_resume(&scheduler, f2);

  printf("coroutine1: continued\n");
  scheduler_resume(&scheduler, scheduler.main);
}

void coroutine2() {
  printf("coroutine2: called\n");
  scheduler_resume(&scheduler, f1);
}

int main() {
    printf("main: initializing fibers\n");

    scheduler_initialize(&scheduler);
    f1 = fiber_new(coroutine1, NULL);
    f2 = fiber_new(coroutine2, NULL);

    scheduler_resume(&scheduler, f1);

    printf("main: terminating\n");

    scheduler_finalize(&scheduler);
    fiber_free(f1);
    fiber_free(f2);

    return 0;
}
