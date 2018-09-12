#ifndef MUCO_SPIN_PRIV_H
#define MUCO_SPIN_PRIV_H

// Spin-locks block the current thread until a condition becomes truthy.
//
// Since spin locks will block threads, they will create contention points.
// As such they should only be used to implement thread-synchronisation
// primitives (e.g. mutex), and to protect critical sections as small and
// fast as possible.
//
// All spin-locks follow the same implementation:
//
// 1. test the value once;
//    always succeeds when there is a single thread, avoiding most
//    thread-synchronisation overhead when it's not required.
//
// 2. test the value in a fixed busy loop;
//    avoids thread context switches that require a syscall, when a thread could
//    quickly acquire the lock;
//
// 3. test the value then block loop;
//    eventually delegates to the kernel to block and resume the thread.
//
// References:
// - "Empirical Studies of Competitive Spinning for a Shared-Memory Multiprocessor" (1991).

#include <sched.h>

// Threshold is arbitrarily chosen. Using a computed value for an
// x86_64-linux-gnu target actually led to worse performance.
#define SPIN_LOCK_THRESHOLD (100)

static inline void spin_lock_long(long *x) {
    // fast path (always succeeds with a single threaded):
    if (*x) return;

    // fixed busy loop (avoids a thread context switch, which improves
    // performance with many threads). we cut the threshold in half
    int count = SPIN_LOCK_THRESHOLD;
    while (count--) {
        if (*x) return;
    }

    // blocking loop (let the kernel resume another thread):
    while (!(*x)) sched_yield();
}

static inline void spin_unlock_long(long *x) {
    *x = 0;
}

static inline void spin_lock_flag(atomic_flag *x) {
    // fast path (always succeeds with a single threaded):
    if (!atomic_flag_test_and_set_explicit(x, memory_order_acquire)) {
        return;
    }

    // fixed busy loop (avoids a thread context switch, which improves
    // performance with many threads). we cut the threshold in half
    int count = SPIN_LOCK_THRESHOLD;
    while (count--) {
        if (!atomic_flag_test_and_set_explicit(x, memory_order_acquire)) {
            return;
        }
    }

    // TODO: consider an adaptive spinning strategy, for example random walk
    // from "Empirical Studies of Competitive Spinning for a Shared-Memory
    // Multiprocessor" (1991) page 5.

    // blocking loop (let the kernel resume another thread):
    while (atomic_flag_test_and_set_explicit(x, memory_order_acquire)) {
        sched_yield();
    }
}

static inline void spin_unlock_flag(atomic_flag *x) {
    atomic_flag_clear_explicit(x, memory_order_release);
}

#endif
