#include "muco.h"
#include "muco/mutex.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#if defined(DEBUG) || defined(MTX_DEBUG)
#  define LOG(str, ...) dprintf(2, "fiber=%p [%s] self=" str, (void*)co_current(), co_current()->name, __VA_ARGS__)
#else
#  define LOG(...)
#endif

typedef struct queue {
    int capacity;
    int index;
    int *que;
    co_mtx_t mutex;
    co_cond_t resource;
} queue_t;

void queue_init(queue_t *self, int capacity) {
    self->capacity = capacity;
    self->index = 0;
    self->que = calloc(self->capacity, sizeof(int));
    co_mtx_init(&self->mutex);
    co_cond_init(&self->resource);
}

void queue_destroy(queue_t *self) {
    free(self->que);
}

int queue_push(queue_t *self, int value) {
    LOG("%p: queue_push(%d)\n", (void *)self, value);
    co_mtx_lock(&self->mutex);

    if (self->index == self->capacity) {
        co_mtx_unlock(&self->mutex);
        return -1;
    }
    self->que[self->index] = value;
    self->index++;

    co_cond_signal(&self->resource);
    co_mtx_unlock(&self->mutex);

    return 0;
}

int queue_shift(queue_t *restrict self, int *restrict value) {
    LOG("%p: queue_shift()\n", (void *)self);
    co_mtx_lock(&self->mutex);

    while (1) {
        if (self->index == 0) {
            co_cond_wait(&self->resource, &self->mutex);
        } else {
            self->index--;
            *value = self->que[self->index];

            co_cond_signal(&self->resource);
            co_mtx_unlock(&self->mutex);

            return 0;
        }
    }
}

#define COUNT (1000000ULL)

static atomic_ulong done;
static atomic_ulong result;
static int count;
static struct timespec start, stop;
static queue_t qu;

static void enqueuetask() {
    int i = count;

    while (i--) {
        // retry until the value is sent:
        while (queue_push(&qu, i)) {
            co_yield();
        }
    }
}

static void dequeuetask() {
    int i, j = count;

    while (j--) {
        queue_shift(&qu, &i);
        result += i;
    }

    unsigned long old = atomic_fetch_sub(&done, 1);
    if (old == 1) co_break();
}

int main(int argc, char *argv[]) {
    unsigned long cocount = argc > 1 ? atol(argv[1]) : 2;
    count = COUNT / cocount;
    atomic_init(&done, cocount / 2);

    co_init(co_procs());
    queue_init(&qu, cocount / 2);

    for (unsigned long i = 0; i < (cocount / 2); i++) {
        co_spawn_named(enqueuetask, "push");
        co_spawn_named(dequeuetask, "shft");
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    co_run();
    clock_gettime(CLOCK_MONOTONIC, &stop);

    unsigned long long duration =
        (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) -
        (start.tv_sec * 1000 + start.tv_nsec / 1000000);

    // should never happen:
    if (duration == 0) duration = 1;

    printf("queue[%d/%lu]: muco: %llu values in %lld ms, %lld values per second (result=%lu)\n",
            co_nprocs, cocount, COUNT, duration, ((1000LL * COUNT) / duration), result);

    queue_destroy(&qu);
    co_free();
    return 0;
}
