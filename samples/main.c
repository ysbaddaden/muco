#include "muco.h"
#include <stdio.h>

fiber_t *f1;
fiber_t *f2;

void coroutine1() {
  printf("coroutine1: called\n");
  co_resume(f2);

  printf("coroutine1: continued\n");
  co_resume(co_scheduler()->main);
}

void coroutine2() {
  printf("coroutine2: called\n");
  co_resume(f1);
}

int main() {
    printf("main: initializing\n");

    co_init(1);
    f1 = co_fiber_new(coroutine1);
    f2 = co_fiber_new(coroutine2);

    co_resume(f1);

    printf("main: finalizing\n");

    co_free();
    co_fiber_free(f1);
    co_fiber_free(f2);

    return 0;
}
