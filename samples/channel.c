#include "muco.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

static co_channel_t *chan;

void generate() {
    void *i = (void *)1;

    while (1) {
        co_channel_send(chan, i);
        i = (char *)i + 1;
        //usleep(500000);
        co_yield();
    }
}

void consume() {
    void *i;

    while (1) {
        co_channel_receive(chan, &i);
        printf("%lu\n", (uintptr_t)i);
    }
}

int main() {
    co_init(2);

    chan = co_channel_new();

    co_spawn(consume);
    //co_spawn(consume);
    //co_spawn(consume);

    co_spawn(generate);
    //co_spawn(generate);

    co_run();

    co_channel_free(chan);
    co_free();

    return 0;
}
