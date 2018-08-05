#include "scheduler.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COUNT (10000000ULL)

scheduler_t scheduler;
atomic_ulong done;
long count;

static void switchtask() {
    int i = count;

    while (i--) {
        scheduler_yield(&scheduler);
    }

    unsigned long old = atomic_fetch_sub((unsigned long *)&done, 1);
    if (old == 1) {
        scheduler_enqueue(&scheduler, scheduler.main);
    }
}

int main(int argc, char *argv[]) {
    struct timespec start, stop;

    unsigned long cocount = argc > 1 ? atol(argv[1]) : 2;
    count = COUNT / cocount;

    atomic_init(&done, cocount);
    scheduler_initialize(&scheduler);

    for (unsigned long i = 0; i < cocount; i++) {
        scheduler_spawn(&scheduler, switchtask);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    scheduler_reschedule(&scheduler);
    clock_gettime(CLOCK_MONOTONIC, &stop);

    unsigned long long duration =
        (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) -
        (start.tv_sec * 1000 + start.tv_nsec / 1000000);

    printf("switch[%lu]: coro: %llu switches in %lld ms, %lld switches per second\n",
            cocount, COUNT, duration, ((1000LL * COUNT) / duration));

    scheduler_finalize(&scheduler);
    return 0;
}
