#ifndef MUCO_MUTEX_PRIV_H
#define MUCO_MUTEX_PRIV_H

#include "muco.h"
#include "muco/mutex.h"
#include <sched.h>
#include <stdlib.h>
#include <time.h>

#if defined(DEBUG) || defined(MTX_DEBUG)
#  include <stdio.h>
#  define LOG(str, ...) dprintf(2, "fiber=%p [%s] self=" str, (void*)co_current(), co_current()->name, __VA_ARGS__)
#else
#  define LOG(...)
#endif

#define SPIN_THRESHOLD (100)

static inline void SPIN_LOCK(atomic_flag *x) {
    // fast path:
    if (!atomic_flag_test_and_set_explicit(x, memory_order_acquire)) {
        return;
    }

    // constant busy loop:
    unsigned int count = SPIN_THRESHOLD;
    while (count--) {
        if (!atomic_flag_test_and_set_explicit(x, memory_order_acquire)) {
            return;
        }
    }

    // TODO: consider an adaptive spinning strategy, for example random walk
    // from "Empirical Studies of Competitive Spinning for a Shared-Memory
    // Multiprocessor" (1991) page 5.

    // blocking loop:
    while (atomic_flag_test_and_set_explicit(x, memory_order_acquire)) {
        sched_yield();
    }
}

static inline void SPIN_UNLOCK(atomic_flag *x) {
    atomic_flag_clear_explicit(x, memory_order_release);
}

void co_mtx_init(co_mtx_t *m) {
    atomic_init(&m->held, 0);
    m->busy = (atomic_flag)ATOMIC_FLAG_INIT;
    m->blocking.head = NULL;
    m->blocking.tail = NULL;
}

int co_mtx_lock(co_mtx_t *m) {
    LOG("%p: co_mtx_lock\n", (void *)m);

    // try to acquire lock (without spin lock):
    if (co_mtx_trylock(m) == 0) {
        LOG("%p: co_mtx_lock (acquired=trylock)\n", (void *)m);
        return 0;
    }

    fiber_t *current = co_current();

    // need exclusive access to re-check 'held' then manipulate 'blocking'
    // based on the CAS result:
    int zero = 0;
    SPIN_LOCK(&m->busy);

    // must loop because a wakeup may be concurrential (e.g. co_cond_broadcast)
    // and another lock/trylock already acquired the lock:
    while (!atomic_compare_exchange_strong(&m->held, &zero, 1)) {
        // push current fiber to blocking list:
        current->m_next = NULL;

        if (!m->blocking.head) {
            m->blocking.tail = m->blocking.head = current;
        } else {
            m->blocking.tail = m->blocking.tail->m_next = current;
        }

        // release exclusive access while current fiber is suspended:
        SPIN_UNLOCK(&m->busy);
        co_suspend();

        // need exclusive access (again):
        zero = 0;
        SPIN_LOCK(&m->busy);
    }

    // done.
    SPIN_UNLOCK(&m->busy);
    LOG("%p: co_mtx_lock (acquired=wakeup)\n", (void *)m);
    return 0;
}

int co_mtx_trylock(co_mtx_t *m) {
    //LOG("%p: co_mtx_trylock\n", (void *)m);
    // try to acquire the lock, without exclusive access because we don't
    // manipulate 'blocking' based on 'held' result.
    int zero = 0;
    if (atomic_compare_exchange_strong(&m->held, &zero, 1)) {
        //LOG("%p: co_mtx_trylock (acquired)\n", (void *)m);
        return 0;
    }
    //LOG("%p: co_mtx_trylock (failed)\n", (void *)m);
    return -1;
}

void co_mtx_unlock(co_mtx_t *m) {
    LOG("%p: co_mtx_unlock\n", (void *)m);
    // need exclusive access because we modify both 'held' and 'blocking' that
    // could introduce a race condition with lock:
    SPIN_LOCK(&m->busy);

    // removes the lock (assuming the current fiber holds the lock):
    m->held = 0;

    // wakeup next blocking fiber (if any):
    fiber_t *fiber = m->blocking.head;
    if (fiber) {
        m->blocking.head = fiber->m_next;
        SPIN_UNLOCK(&m->busy);
        co_enqueue(fiber);
    } else {
        SPIN_UNLOCK(&m->busy);
    }
}

void co_cond_init(co_cond_t *c) {
    c->busy = (atomic_flag)ATOMIC_FLAG_INIT;
    c->waiting.head = NULL;
    c->waiting.tail = NULL;
}

void co_cond_wait(co_cond_t *restrict c, co_mtx_t *restrict m) {
    LOG("%p: co_cond_wait(%p)\n", (void *)c, (void *)m);
    // assert(m->held);
    fiber_t *current = co_current();

    // need exclusive access to manipulate wait list:
    SPIN_LOCK(&c->busy);

    // queue current fiber into wait list:
    current->m_next = NULL;
    if (!c->waiting.head) {
        c->waiting.tail = c->waiting.head = current;
    } else {
        c->waiting.tail = c->waiting.tail->m_next = current;
    }
    SPIN_UNLOCK(&c->busy);

    // release mutex lock, enqueueing a blocked fiber:
    // must always succeed (because current fiber holds the lock):
    co_mtx_unlock(m);

    // suspend execution of current fiber:
    co_suspend();

    // must re-acquire the mutex lock to continue:
    co_mtx_lock(m);
}

void co_cond_signal(co_cond_t *c) {
    LOG("%p: co_cond_signal\n", (void *)c);
    // need exclusive access to manipulate wait list:
    SPIN_LOCK(&c->busy);

    // enqueue next waiting fiber (if any):
    fiber_t *fiber = c->waiting.head;
    if (fiber) {
        c->waiting.head = fiber->m_next;
        SPIN_UNLOCK(&c->busy);

        // TODO: or co_resume(fiber) ?
        co_enqueue(fiber);
    } else {
        SPIN_UNLOCK(&c->busy);
    }
}

void co_cond_broadcast(co_cond_t *c) {
    LOG("%p: co_cond_broadcast\n", (void *)c);
    SPIN_LOCK(&c->busy);

    // enqueue all waiting fibers:
    fiber_t *fiber = c->waiting.head;
    while (fiber) {
        co_enqueue(fiber);
        fiber = fiber->m_next;
    }

    // clear linked list:
    c->waiting.head = NULL;
    SPIN_UNLOCK(&c->busy);
}

#endif
