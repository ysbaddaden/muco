#include "muco.h"
#include "muco/channel.h"
#include <error.h>
#include <stdio.h>

static co_chan_t chan;

void consumer() {
    long *m;

    while (!co_chan_receive(&chan, (void *)&m)) {
        printf("received: %ld\n", *m);
    }
    co_break();
}

void producer() {
    long i = 1;
    long j = 2;
    long k = 3;

    co_chan_send(&chan, &i);
    co_chan_send(&chan, &j);
    co_chan_send(&chan, &k);

    co_chan_close(&chan);
}

int main() {
    co_init(co_procs());

    // channel is unbuffered (1 slot) and synchronous (sender waits for receiver
    // to have received the value):
    co_chan_init(&chan, 1, 0);

    co_spawn(consumer);
    co_spawn(producer);

    co_run();

    co_chan_destroy(&chan);
    co_free();
}
