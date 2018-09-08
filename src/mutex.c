#include "muco.h"
#include "ring.h"

#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>

#if defined(DEBUG) || defined(MTX_DEBUG)
#  define LOG(str, ...) dprintf(2, "fiber=%p self=" str, (void*)co_current(), __VA_ARGS__)
#else
#  define LOG(...)
#endif

static atomic_flag busy = ATOMIC_FLAG_INIT;

#define SPIN_LOCK \
    while(atomic_flag_test_and_set_explicit(&busy, memory_order_acquire)) {}

#define SPIN_UNLOCK \
    atomic_flag_clear_explicit(&busy, memory_order_release)


struct mutex {
    atomic_bool held;
    ring_t blocking;
};

struct cond {
    ring_t waiting;
};

// init / deinit

void co_mtx_init(struct mutex *self) {
    atomic_init(&self->held, 0);
    ring_initialize(&self->blocking);
}

void co_mtx_destroy(struct mutex *self) {
    ring_finalize(&self->blocking);
}


// Thread-unsafe private API. These functions must only be called when SPIN_LOCK
// has been acquired.

static inline void mtx_lock(struct mutex *self) {
    LOG("%p: mtx_lock\n", (void *)self);
    assert(!ring_contains(&self->blocking, co_current()));

    while (self->held) {
        // TODO: forbid recursive lock (deadlock)
        // TODO: allow *explicit* recursive lock with a counter
        ring_push(&self->blocking, co_current());

        SPIN_UNLOCK;
        co_suspend();
        SPIN_LOCK;

        // assert(!ring_contains(&self->blocking, co_current()));
        if (ring_contains(&self->blocking, co_current())) {
            dprintf(2, "ERROR: self=%p mutex->blocking *already* contains fiber=%p\n", (void *)self, (void *)co_current());
            abort();
        }
    }
    self->held = 1;
}

static inline int mtx_trylock(struct mutex *self) {
    LOG("%p: mtx_trylock\n", (void *)self);
    assert(!ring_contains(&self->blocking, co_current()));

    if (self->held) {
        return -1;
    }
    self->held = 1;
    return 0;
}

static inline void mtx_unlock(struct mutex *self) {
    LOG("%p: mtx_unlock\n", (void *)self);
    assert(self->held);
    self->held = 0;

    fiber_t *next = ring_shift(&self->blocking);
    if (next) {
        co_enqueue(next);
    }
}


// thread-safe public API:

void co_mtx_lock(struct mutex *self) {
    LOG("%p: co_mtx_lock\n", (void *)self);
    SPIN_LOCK;
    mtx_lock(self);
    SPIN_UNLOCK;
}

int co_mtx_trylock(struct mutex *self) {
    LOG("%p: co_mtx_trylock\n", (void *)self);
    SPIN_LOCK;
    int res = mtx_trylock(self);
    SPIN_UNLOCK;
    return res;
}

void co_mtx_unlock(struct mutex *self) {
    LOG("%p: co_mtx_unlock\n", (void *)self);
    SPIN_LOCK;
    mtx_unlock(self);
    SPIN_UNLOCK;
}

void co_cond_init(struct cond *self) {
    ring_initialize(&self->waiting);
}

void co_cond_destroy(struct cond *self) {
    ring_finalize(&self->waiting);
}

void co_cond_wait(struct cond *self, struct mutex *m) {
    LOG("%p: co_cond_wait(%p)\n", (void *)self, (void *)m);
    SPIN_LOCK;

    assert(m->held);
    mtx_unlock(m);
    ring_push(&self->waiting, co_current());

    SPIN_UNLOCK;
    co_suspend();

    co_mtx_lock(m);
}

void co_cond_signal(struct cond *self) {
    LOG("%p: co_cond_signal\n", (void *)self);
    SPIN_LOCK;
    fiber_t *next = ring_shift(&self->waiting);
    if (next) {
        co_enqueue(next);
    }
    SPIN_UNLOCK;
}

void co_cond_broadcast(struct cond *self) {
    LOG("%p: co_cond_broadcast\n", (void *)self);
    SPIN_LOCK;
    fiber_t *next;
    while ((next = ring_shift(&self->waiting))) {
        co_enqueue(next);
    }
    SPIN_UNLOCK;
}
