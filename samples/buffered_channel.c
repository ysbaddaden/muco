#include "muco.h"
#include "muco/channel.h"
#include <error.h>
#include <stdio.h>

static co_chan_t chan;

// need globals because receiver fiber may outlive sending fiber:
static long i = 1;
static long j = 2;
static long k = 3;

void consumer() {
    long *m;

    while (!co_chan_receive(&chan, (void *)&m)) {
        printf("received: %ld\n", *m);
    }
    co_break();
}

void producer() {
    co_chan_send(&chan, &i);
    co_chan_send(&chan, &j);
    co_chan_send(&chan, &k);

    co_chan_close(&chan);
}

int main() {
    co_init(co_procs());

    // channel is buffered (1 slot) and asynchronous (sender doesn't wait for
    // receiver to have received the value):
    co_chan_init(&chan, 2, 1);

    co_spawn(consumer);
    co_spawn(producer);

    co_run();

    co_chan_destroy(&chan);
    co_free();
}
