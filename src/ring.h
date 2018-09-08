#ifndef MUCO_RING_H
#define MUCO_RING_H

// Thread unsafe, unbounded & circular FIFO queue.

#include <stdlib.h>
#include <string.h>

#if defined(DEBUG) || defined(RING_DEBUG)
#  include <stdio.h>
#  define LOG(str, ...) dprintf(2, "fiber=%p self=" str, (void*)co_current(), __VA_ARGS__)
#else
#  define LOG(...)
#endif

#define RING_WORD_SIZE (sizeof(void *))
#define RING_INITIAL_CAPACITY (4)

typedef struct ring {
    void **buf;
    size_t capacity;
    size_t size;
    size_t start;
} ring_t;

static void ring_initialize(ring_t *self) {
    self->size = 0;
    self->capacity = 0;
    self->buf = NULL;
}

static void ring_finalize(ring_t *self) {
    if (self->buf) {
        free(self->buf);
    }
}

static inline void ring_increase_capacity(ring_t *self) {
    LOG("%p: ring_increase_capacity\n", (void *)self);
    if (!self->buf) {
        self->capacity = RING_INITIAL_CAPACITY;
        self->buf = malloc(RING_WORD_SIZE * self->capacity);
        return;
    }

    size_t old_capacity = self->capacity;
    self->capacity *= 2;

    self->buf = realloc(self->buf, RING_WORD_SIZE * self->capacity);
    char *buf = (char *)self->buf;

    size_t start = self->start;
    size_t finish = start + self->size;

    if (finish > old_capacity) {
        finish -= old_capacity;

        if ((old_capacity - start) >= start) {
            memcpy(buf + RING_WORD_SIZE * old_capacity, buf, RING_WORD_SIZE * finish);
            //memset(buf, 0, RING_WORD_SIZE * finish);
        } else {
            size_t to_move = old_capacity - start;
            size_t new_start = self->capacity - to_move;
            memcpy(buf + RING_WORD_SIZE * new_start, buf + RING_WORD_SIZE * start, RING_WORD_SIZE * to_move);
            //memset(buf + start, 0, RING_WORD_SIZE * to_move);
            self->start = new_start;
        }
    }
}

static void ring_push(ring_t *self, void *value) {
    if (self->size >= self->capacity) {
        ring_increase_capacity(self);
    }
    size_t index = self->start + self->size;
    if (index >= self->capacity) {
        index -= self->capacity;
    }
    LOG("%p: ring_push value=%p index=%ld\n", (void *)self, value, index);

    self->buf[index] = value;
    self->size += 1;
}

static void *ring_shift(ring_t *self) {
    if (self->size == 0) {
        LOG("%p: ring_shift value=%p\n", (void *)self, NULL);
        return NULL;
    }

    void *value = self->buf[self->start];
    LOG("%p: ring_shift value=%p index=%ld\n", (void *)self, value, self->start);

    self->size -= 1;
    self->start += 1;
    if (self->start >= self->capacity) {
        self->start -= self->capacity;
    }
    return value;
}

static int ring_contains(ring_t *self, void *value) {
    size_t start = self->start;
    size_t capacity = self->capacity;
    size_t finish = start + self->size;
    size_t i;

    if (finish > capacity) {
        finish -= capacity;

        for (i = start; i < capacity; i++) {
            if (self->buf[i] == value) return 1;
        }
        for (i = 0; i < finish; i++) {
            if (self->buf[i] == value) return 1;
        }
    } else {
        for (i = start; i < finish; i++) {
            if (self->buf[i] == value) return 1;
        }
    }

    return 0;
}

#endif
