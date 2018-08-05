/*
 * Implements lock-free double-ended queue as per "Verification of a Concurrent
 * Deque Implementation" published in June 1999 by Robert D. Blumote, C. Greg
 * Plaxton, Sandip Ray.
 */
#include "config.h"
#include "deque.h"

#define DEQUE_SIZE (sizeof(void*) * 1024 * 1024)

void deque_initialize(deque_t *self) {
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

void deque_finalize(deque_t *self) {
    munmap(self->deq, DEQUE_SIZE);
}

void deque_push_bottom(deque_t *self, void *item) {
    int bot = atomic_load((int *)&self->bot);
    self->deq[bot] = item;
    bot += 1;
    atomic_store((int *)&self->bot, bot);
}

void *deque_pop_bottom(deque_t *self) {
    struct age old_age, new_age;
    void *item;
    int bot;

    bot = atomic_load((int *)&self->bot);
    if (bot == 0) {
        return NULL;
    }

    bot -= 1;
    atomic_store((int *)&self->bot, bot);

    item = self->deq[bot];
    old_age = atomic_load((struct age *)&self->age);
    if (bot > old_age.top) {
        return item;
    }

    new_age.tag = old_age.tag + 1;
    new_age.top = 0;

    if (bot == old_age.top) {
        if (atomic_compare_exchange_strong((struct age *)&self->age, &old_age, new_age)) {
            return item;
        }
    }
    atomic_store((struct age *)&self->age, new_age);
    return NULL;
}

void *deque_pop_top(deque_t *self) {
    struct age old_age, new_age;
    void *item;
    int bot;

    old_age = atomic_load((struct age *)&self->age);
    bot = atomic_load((int *)&self->bot);

    if (bot <= old_age.top) {
        return NULL;
    }

    item = self->deq[old_age.top];
    new_age.tag = old_age.tag;
    new_age.top = old_age.top + 1;

    if (atomic_compare_exchange_strong((struct age *)&self->age, &old_age, new_age)) {
        return item;
    }
    return NULL;
}
