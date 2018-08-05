#include "muco.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COUNT (10000000ULL)

atomic_ulong done;
long count;

static void switchtask() {
    int i = count;

    while (i--) {
        co_yield();
    }

    unsigned long old = atomic_fetch_sub((unsigned long *)&done, 1);
    if (old == 1) {
        co_enqueue(co_scheduler()->main);
    }
}

int main(int argc, char *argv[]) {
    struct timespec start, stop;

    unsigned long cocount = argc > 1 ? atol(argv[1]) : 2;
    count = COUNT / cocount;

    atomic_init(&done, cocount);
    co_init();

    for (unsigned long i = 0; i < cocount; i++) {
        co_spawn(switchtask);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    co_suspend();
    clock_gettime(CLOCK_MONOTONIC, &stop);

    unsigned long long duration =
        (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) -
        (start.tv_sec * 1000 + start.tv_nsec / 1000000);

    printf("switch[%lu]: muco: %llu switches in %lld ms, %lld switches per second\n",
            cocount, COUNT, duration, ((1000LL * COUNT) / duration));

    co_free();
    return 0;
}
