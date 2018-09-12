#include "muco.h"
#include "muco/mutex.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <time.h>

#define COUNT (10000000ULL)

static co_mtx_t mutex;
static unsigned long long increment = 0;
atomic_ulong done;
long count;
unsigned long cocount;
struct timespec start, stop;

static void locktask() {
    int i = count;

    while (i--) {
        co_mtx_lock(&mutex);
        increment += 1;
        co_mtx_unlock(&mutex);
    }

    unsigned long old = atomic_fetch_sub(&done, 1);
    if (old == 1) co_break();
}

int main(int argc, char *argv[]) {
    unsigned long cocount = argc > 1 ? atol(argv[1]) : 2;
    count = COUNT / cocount;
    atomic_init(&done, cocount);

    co_init(co_procs());
    co_mtx_init(&mutex);

    for (unsigned long i = 0; i < cocount; i++) {
        co_spawn(locktask);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    co_run();
    clock_gettime(CLOCK_MONOTONIC, &stop);

    unsigned long long duration =
        (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) -
        (start.tv_sec * 1000 + start.tv_nsec / 1000000);

    // should never happen:
    if (duration == 0) duration = 1;

    printf("switch[%d/%lu]: muco: %llu locks in %lld ms, %lld locks per second (increment=%llu)\n",
            co_nprocs, cocount, COUNT, duration, ((1000LL * COUNT) / duration), increment);

    co_free();
    return 0;
}
