#include "muco.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <time.h>

#define COUNT (2000000ULL)

static co_lock_t *mutex;
static unsigned long long increment = 0;
atomic_ulong done;
long count;
unsigned long cocount;
struct timespec start, stop;

static void locktask() {
    int i = count;

    while (i--) {
        if (co_lock(mutex) == -1) {
            error(1, errno, "co_lock");
        }

        increment += 1;

        if (co_unlock(mutex) == -1) {
            error(1, errno, "co_unlock");
        }
    }

    unsigned long old = atomic_fetch_sub((unsigned long *)&done, 1);
    if (old == 1) co_break();
}

int main(int argc, char *argv[]) {
    unsigned long cocount = argc > 1 ? atol(argv[1]) : 2;
    count = COUNT / cocount;
    atomic_init(&done, cocount);

    co_init(4);
    mutex = co_lock_new();

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

    printf("switch[%lu]: muco: %llu locks in %lld ms, %lld locks per second (increment=%llu)\n",
            cocount, COUNT, duration, ((1000LL * COUNT) / duration), increment);

    co_lock_free(mutex);
    co_free();
    return 0;
}
