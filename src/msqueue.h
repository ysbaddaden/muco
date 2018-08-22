#ifndef QUEUE_H
#define QUEUE_H

/*
 * Based on "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent
 * Queue Algorithms" by Michael and Scott, hence MS-queue.
 *
 * See <http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf>
 */
#include <stdint.h>
#include <stdatomic.h>

typedef struct node node_t;

typedef struct pointer {
    node_t *ptr;
    uintptr_t count;
} pointer_t;

typedef struct node {
    _Atomic pointer_t next;
    void *value;
} node_t;

static void node_init(node_t *node, void *value) {
    node->value = value;
    pointer_t null = {NULL, 0};
    atomic_init((pointer_t *)&node->next, null);
}

static node_t *node_new(void *value) {
    node_t *node = malloc(sizeof(node_t));
    node_init(node, value);
    return node;
}

typedef struct msqueue {
    _Atomic pointer_t head;
    _Atomic pointer_t tail;
} msqueue_t;

static inline int pointer_consistent(pointer_t a, pointer_t b) {
    return a.ptr == b.ptr && a.count == b.count;
}

static void msqueue_initialize(msqueue_t *self) {
    // allocate dummy node:
    node_t *node = node_new(NULL);

    // initialize queue:
    pointer_t dummy = {node, 0};
    atomic_init((pointer_t *)&self->head, dummy);
    atomic_init((pointer_t *)&self->tail, dummy);
}

static void *msqueue_head(msqueue_t *self) {
    while (1) {
        pointer_t head = self->head;
        pointer_t tail = self->tail;
        pointer_t next = head.ptr->next;

        // are head, tail and next consistent?
        if (pointer_consistent(head, self->head)) {
            if (head.ptr == tail.ptr) {
                // queue is empty:
                if (next.ptr == NULL) return NULL;

                // tail is falling behind. try to advance it:
                pointer_t ptr = {next.ptr, tail.count + 1};
                atomic_compare_exchange_strong((pointer_t *)&self->tail, &tail, ptr);
            } else {
                // no need to deal with tail. return head:
                return next.ptr->value;
            }
        }
    }
}

static void msqueue_enqueue(msqueue_t *self, void *value) {
    // node will be last element in queue:
    node_t *node = node_new(value);
    pointer_t null = {NULL, 0};
    node->next = null;

    while (1) {
        pointer_t tail = self->tail;
        pointer_t next = tail.ptr->next;

        // are tail and next consistent?
        if (pointer_consistent(tail, self->tail)) {
            if (next.ptr == NULL) {
                // tail points to the last node:
                pointer_t ptr = {node, next.count + 1};
                if (atomic_compare_exchange_strong((pointer_t *)&tail.ptr->next, &next, ptr)) {
                    // success. enqueue is done: try to swing tail to the inserted node:
                    pointer_t ptr = {node, tail.count + 1};
                    atomic_compare_exchange_strong((pointer_t *)&self->tail, &tail, ptr);
                    return;
                }
            } else {
                // try to swing tail to next node
                pointer_t ptr = {next.ptr, tail.count + 1};
                atomic_compare_exchange_strong((pointer_t *)&self->tail, &tail, ptr);
            }
        }
    }
}

static void *msqueue_dequeue(msqueue_t *self) {
    while (1) {
        pointer_t head = self->head;
        pointer_t tail = self->tail;
        pointer_t next = head.ptr->next;

        // are head, tail and next consistent?
        if (pointer_consistent(head, self->head)) {
            if (head.ptr == tail.ptr) {
                // queue is empty:
                if (next.ptr == NULL) return NULL;

                // tail is falling behind. try to advance it:
                pointer_t ptr = {next.ptr, tail.count + 1};
                atomic_compare_exchange_strong((pointer_t *)&self->tail, &tail, ptr);
            } else {
                // no need to deal with tail. try to swing head to the next node:
                pointer_t ptr = {next.ptr, head.count + 1};
                void *value = next.ptr->value;
                if (atomic_compare_exchange_strong((pointer_t *)&self->head, &head, ptr)) {
                    // success. dequeue is done:
                    free(head.ptr);
                    return value;
                }
            }
        }
    }
}

static void msqueue_finalize(msqueue_t *self) {
    fiber_t *fiber;
    while (fiber = msqueue_dequeue(self)) {
    }
}

#endif
