#ifndef MUCO_LMSQUEUE_PRIV_H
#define MUCO_LMSQUEUE_PRIV_H

/*
 * Based on "An Optimistic Approach to Lock-Free FIFO Queues" by Ladan-Mozes and
 * Shavit, hence LMS-queue.
 *
 * See * <https://people.csail.mit.edu/edya/publications/OptimisticFIFOQueue-DISC2004.pdf>
 */

#include "muco/lmsqueue.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

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

__attribute((__unused__))
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

__attribute((__unused__))
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
                    lmsnode_free(dummy);
                }
                continue;
            }

            // try to dequeue:
            lmspointer_t ptr = {firstNodePrev.ptr, head.tag + 1};
            if (atomic_compare_exchange_strong((lmspointer_t *)&self->head, &head, ptr)) {
                // success. free node; return value:
                lmsnode_free(head.ptr);
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

__attribute((__unused__))
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
                    lmsnode_free(dummy);
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
