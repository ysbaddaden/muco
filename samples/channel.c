#include "muco.h"
#include "muco/channel.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define COUNT (500000ULL)

atomic_ulong done;
long count;
struct timespec start, stop;

static co_channel_t *chan;

void generate() {
    long i = count;

    while (i--) {
        if (co_channel_send(chan, (void *)i)) break;
    }

    unsigned long old = atomic_fetch_sub((unsigned long *)&done, 1);
    if (old == 1) {
        // TODO: close the channel so consumers could terminate nicely
        // co_channel_close(chan);
        co_break();
    }
}

void consume() {
    long i;

    while (1) {
        if (co_channel_receive(chan, (void *)&i)) break;
    }

    printf("%lu\n", i);
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

    co_init(8);

    chan = co_channel_new();

    while (gcount--) {
        co_spawn(generate);
    }
    while (ccount--) {
        co_spawn(consume);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    co_run();
    clock_gettime(CLOCK_MONOTONIC, &stop);

    unsigned long long duration =
        (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) -
        (start.tv_sec * 1000 + start.tv_nsec / 1000000);

    // should never happen:
    if (duration == 0) duration = 1;

    printf("channel[%lu]: muco: %llu messages in %lld ms, %lld messages per second\n",
            cocount, COUNT, duration, ((1000LL * COUNT) / duration));

    // FIXME: segfaults:
    // co_channel_free(chan);
    co_free();

    return 0;
}
