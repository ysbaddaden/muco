#include "muco.h"
#include "muco/channel.h"
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define COUNT (1000000ULL)

atomic_ulong done;
long count;
struct timespec start, stop;

static co_chan_t chan;

void generate() {
    long i = count;

    while (i--) {
        if (co_chan_send(&chan, (void *)i)) {
            error(1, 0, "chan is closed");
        }
    }

    if (atomic_fetch_sub(&done, 1) == 1) {
        co_chan_close(&chan);
        co_break();
    }
}

void consume() {
    long i;

    while (1) {
        if (co_chan_receive(&chan, (void *)&i)) {
            break;
        }
    }

    printf("done: %lu\n", i);
    // co_break();
}

int main(int argc, char **argv) {
    unsigned long gcount = 2, ccount = 2, cocount;

    if (argc > 2) {
        gcount = atol(argv[1]);
        ccount = atol(argv[2]);
    } else if (argc > 1) {
        gcount = atol(argv[1]);
    }

    cocount = gcount + ccount;
    count = COUNT / gcount;
    atomic_init(&done, gcount);

    co_init(co_procs());

    // channel: unbuffered + synchronous
    co_chan_init(&chan, 1, 0);

    while (gcount--) {
        co_spawn_named(generate, "gen");
    }
    while (ccount--) {
        co_spawn_named(consume, "con");
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    co_run();
    clock_gettime(CLOCK_MONOTONIC, &stop);

    unsigned long long duration =
        (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) -
        (start.tv_sec * 1000 + start.tv_nsec / 1000000);

    // should never happen:
    if (duration == 0) duration = 1;

    printf("chan[%lu]: muco: %llu messages in %lld ms, %lld messages per second\n",
            cocount, COUNT, duration, ((1000LL * COUNT) / duration));

    co_chan_destroy(&chan);
    co_free();

    return 0;
}
