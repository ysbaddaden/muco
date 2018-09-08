#include "muco.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define COUNT (10000000ULL)

atomic_ulong done;
long count;
unsigned long cocount;
struct timespec start, stop;

static void switchtask() {
    int i = count;

    while (i--) {
        co_yield();
    }

    unsigned long old = atomic_fetch_sub(&done, 1);
    if (old == 1) {
        co_break();
    }
}

int main(int argc, char *argv[]) {
    unsigned long cocount = argc > 1 ? atol(argv[1]) : 2;
    count = COUNT / cocount;
    atomic_init(&done, cocount);

    co_init(co_procs());

    for (unsigned long i = 0; i < cocount; i++) {
        co_spawn(switchtask);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    co_run();
    clock_gettime(CLOCK_MONOTONIC, &stop);

    unsigned long long duration =
        (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) -
        (start.tv_sec * 1000 + start.tv_nsec / 1000000);

    // should never happen:
    if (duration == 0) duration = 1;

    printf("switch[%lu]: muco: %llu yields in %lld ms, %lld yields per second\n",
            cocount, COUNT, duration, ((1000LL * COUNT) / duration));
    co_free();

    return 0;
}
