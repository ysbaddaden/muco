#ifndef MUCO_LMSQUEUE_H
#define MUCO_LMSQUEUE_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * Based on "An Optimistic Approach to Lock-Free FIFO Queues" by Ladan-Mozes and
 * Shavit, hence LMS-queue.
 *
 * See * <https://people.csail.mit.edu/edya/publications/OptimisticFIFOQueue-DISC2004.pdf>
 */

typedef struct lmsnode lmsnode_t;

typedef struct lmspointer {
    lmsnode_t *ptr;
    uintptr_t tag;
} lmspointer_t;

static inline int lmspointer_equals(lmspointer_t a, lmspointer_t b) {
    return a.ptr == b.ptr && a.tag == b.tag;
}

typedef struct lmsnode {
    _Atomic lmspointer_t next;
    _Atomic lmspointer_t prev;
    void *value;
} lmsnode_t;

static void lmsnode_initialize(lmsnode_t *self, void *value) {
    self->value = value;
    lmspointer_t null = {NULL, 0};
    atomic_init((lmspointer_t *)&self->next, null);
    atomic_init((lmspointer_t *)&self->prev, null);
}

static lmsnode_t *lmsnode_new(void *value) {
    lmsnode_t *self = calloc(1, sizeof(lmsnode_t));
    lmsnode_initialize(self, value);
    return self;
}

typedef struct lmsqueue {
    _Atomic lmspointer_t head;
    _Atomic lmspointer_t tail;
} lmsqueue_t;

static void lmsqueue_initialize(lmsqueue_t *self) {
    // allocate dummy node:
    lmsnode_t *node = lmsnode_new(NULL);

    // initialize queue:
    lmspointer_t ptr = {node, 0};
    atomic_init((lmspointer_t *)&self->head, ptr);
    atomic_init((lmspointer_t *)&self->tail, ptr);
}

static inline void lmsqueue_fixlist(lmsqueue_t *self, lmspointer_t tail, lmspointer_t head) {
    lmspointer_t curNode, curNodeNext, nextNodePrev;

    // start from tail:
    curNode = tail;

    // repeat until head:
    while (lmspointer_equals(head, self->head) && !lmspointer_equals(curNode, head)) {
        curNodeNext = curNode.ptr->next;

        if (curNodeNext.tag != curNode.tag) {
            return; // ABA, return!
        }

        nextNodePrev = curNodeNext.ptr->prev;

        lmspointer_t expected = {curNode.ptr, curNode.tag - 1};
        if (!lmspointer_equals(nextNodePrev, expected)) { // ptr don't equal?
            curNodeNext.ptr->prev = expected;             // fix
        }

        // advance:
        curNode.ptr = curNodeNext.ptr;
        curNode.tag = curNode.tag - 1;
    }
}

static void lmsqueue_enqueue(lmsqueue_t *self, void *value) {
    lmspointer_t tail;

    // allocate new node:
    lmsnode_t *node = lmsnode_new(value);

    while (1) {
        tail = self->tail;
        lmspointer_t next = {tail.ptr, tail.tag + 1};
        node->next = next;

        // try to enqueue:
        lmspointer_t ptr = {node, tail.tag + 1};
        if (atomic_compare_exchange_strong((lmspointer_t *)&self->tail, &tail, ptr)) {
            // success. write prev:
            lmspointer_t prev = {node, tail.tag};
            tail.ptr->prev = prev;
            return;
        }
    }
}

static void *lmsqueue_dequeue(lmsqueue_t *self) {
    lmspointer_t head, tail, firstNodePrev;
    void *value;

    while (1) {
        head = self->head;
        tail = self->tail;
        firstNodePrev = head.ptr->prev;
        value = head.ptr->value;

        // check consistency:
        if (!lmspointer_equals(head, self->head)) {
            continue;
        }

        if (value != NULL) {                            // dummy value?
            if (!lmspointer_equals(tail, head)) {       // more than 1 node?
                if (firstNodePrev.tag != head.tag) {    // tags inequal?
                    lmsqueue_fixlist(self, tail, head); // must fix list!
                    continue;
                }
            } else {
                // last node in queue: allocate dummy node
                lmsnode_t *dummy = lmsnode_new(NULL);
                lmspointer_t next = {tail.ptr, tail.tag + 1};
                dummy->next = next;

                // set dummy node as prev:
                lmspointer_t ptr = {dummy, tail.tag + 1};
                if (atomic_compare_exchange_strong((lmspointer_t *)&self->tail, &tail, ptr)) {
                    lmspointer_t prev = {dummy, tail.tag};
                    head.ptr->prev = prev;
                } else {
                    free(dummy);
                }
                continue;
            }

            // try to dequeue:
            lmspointer_t ptr = {firstNodePrev.ptr, head.tag + 1};
            if (atomic_compare_exchange_strong((lmspointer_t *)&self->head, &head, ptr)) {
                // success. free node; return value:
                free(head.ptr);
                return value;
            }
        } else {                                    // head points to dummy
            if (tail.ptr == head.ptr) {             // tail points to dummy?
                return NULL;                        // queue is empty!
            }
            if (firstNodePrev.tag != head.tag) {    // tags not equal?
                lmsqueue_fixlist(self, tail, head); // must fix list!
                continue;
            }

            // must skip dummy node:
            lmspointer_t ptr = {firstNodePrev.ptr, head.tag + 1};
            atomic_compare_exchange_strong((lmspointer_t *)&self->head, &head, ptr);
        }
    }
}

static void *lmsqueue_head(lmsqueue_t *self) {
    lmspointer_t head, tail, firstNodePrev;
    void *value;

    while (1) {
        head = self->head;
        tail = self->tail;
        firstNodePrev = head.ptr->prev;
        value = head.ptr->value;

        // check consistency:
        if (!lmspointer_equals(head, self->head)) {
            continue;
        }

        if (value != NULL) {                            // dummy value?
            if (!lmspointer_equals(tail, head)) {       // more than 1 node?
                if (firstNodePrev.tag != head.tag) {    // tags inequal?
                    lmsqueue_fixlist(self, tail, head); // must fix list!
                    continue;
                }
            } else {
                // last node in queue: allocate dummy node
                lmsnode_t *dummy = lmsnode_new(NULL);
                lmspointer_t next = {tail.ptr, tail.tag + 1};
                dummy->next = next;

                // set dummy node as prev:
                lmspointer_t ptr = {dummy, tail.tag + 1};
                if (atomic_compare_exchange_strong((lmspointer_t *)&self->tail, &tail, ptr)) {
                    lmspointer_t prev = {dummy, tail.tag};
                    head.ptr->prev = prev;
                } else {
                    free(dummy);
                }
                continue;
            }

            // done.
            return value;
        } else {                                    // head points to dummy
            if (tail.ptr == head.ptr) {             // tail points to dummy?
                return NULL;                        // queue is empty!
            }
            if (firstNodePrev.tag != head.tag) {    // tags not equal?
                lmsqueue_fixlist(self, tail, head); // must fix list!
                continue;
            }

            // must skip dummy node:
            lmspointer_t ptr = {firstNodePrev.ptr, head.tag + 1};
            atomic_compare_exchange_strong((lmspointer_t *)&self->head, &head, ptr);
        }
    }
}

#endif
