#ifndef MUCO_DEQUE_PRIV_H
#define MUCO_DEQUE_PRIV_H

#include "config.h"
#include <stdatomic.h>

struct age {
    int tag;
    int top;
};

typedef struct deque {
    atomic_int bot;
    _Atomic struct age age;
    void **deq;
} deque_t;

// FIXME: should be unlimited
#define DEQUE_SIZE (4UL * 1024 * 1024 * 1024)

static void deque_initialize(deque_t *self) {
    struct age age = {0, 0};

    atomic_init(&self->bot, 0);
    atomic_init(&self->age, age);

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_ANONYMOUS | MAP_PRIVATE;
    int fd = -1;
    int offset = 0;

    void *buf = mmap(NULL, DEQUE_SIZE, prot, flags, fd, offset);
    if (buf == MAP_FAILED) {
        error(1, errno, "mmap");
    }
    self->deq = buf;
}

static void deque_finalize(deque_t *self) {
    munmap(self->deq, DEQUE_SIZE);
}

static void deque_push_bottom(deque_t *self, void *item) {
    int bot = self->bot;
    self->deq[bot] = item;
    bot += 1;
    self->bot = bot;
}

static void *deque_pop_bottom(deque_t *self) {
    struct age old_age, new_age;
    void *item;

    int bot = self->bot;
    if (bot == 0) {
        return NULL;
    }
    bot -= 1;
    self->bot = bot;

    item = self->deq[bot];
    old_age = self->age;
    if (bot > old_age.top) {
        return item;
    }

    self->bot = 0;

    new_age.tag = old_age.tag + 1;
    new_age.top = 0;

    if (bot == old_age.top) {
        if (atomic_compare_exchange_weak((struct age *)&self->age, &old_age, new_age)) {
            return item;
        }
    }
    self->age = new_age;
    return NULL;
}

static void *deque_pop_top(deque_t *self) {
    struct age old_age, new_age;
    void *item;
    int bot;

    old_age = self->age;
    bot = self->bot;
    if (bot <= old_age.top) {
        return NULL;
    }

    item = self->deq[old_age.top];
    new_age = old_age;
    new_age.top += 1;

    if (atomic_compare_exchange_weak((struct age *)&self->age, &old_age, new_age)) {
        return item;
    }
    return NULL;
}

static int deque_lazy_size(deque_t *self) {
    int bot = self->bot;
    struct age age = self->age;
    return bot - age.top;
}

#endif
