# Multithreaded Coroutines (MUCO)

A working experiment at stackful coroutines that can be spawned, suspended and
resumed by any thread; without sacrificing much performance.

Each thread has its own scheduler that first try to exhaust its own queue, then
try to steal from a random scheduler. Schedulers' queue follow the "Scheduling
Multithreaded Computations by Work Stealing" (1999) paper, with tweaks from the
follow-up "Thread Scheduling for Multiprogrammed Multiprocessors" (2001) paper.

Most recently enqueued fibers are resumed sooner by the schedulers (to improve
cache reuses) while the least recently enqueued fibers will be stolen by empty
schedulers (to avoid starvation).

Thread-safe and fiber-aware synchronization primitives such as mutexes and
monitors (condition variables) are available. An example channel implementation
is also available, but limited to pass pointers and the overall performance is
still quite poor.


## Usage

See the `samples` and `benchmarks` directories for usage examples.


## Notes

1. Being multithreaded doesn't mean that an application performance will be
   improved.

   An application that switches contexts a lot, or doesn't need to communicate
   much between coroutines, or is CPU-bound will have better performance with
   many threads, because many fibers can be advanced in parallel.

   But an application that communicates a lot between coroutines and is mostly
   IO-bound can perform poorly with many threads, because the threads will have
   to synchronize a lot, which can significantly hinder performance.

2. Starting more threads than available CPU cores/threads will perform better
   than starting less threads.

   I'm not sure about the reasons. Maybe this delegates more to the kernel,
   which does a better job at scheduling threads.


## License

Distributed under the Apache 2.0 license.


## References

Papers on-non blocking structures, shared-memory multiprocessor schedulers and
synchronization primitives:

- "Empirical Studies of Competitive Spinning for a Shared-Memory Multiprocessor" (1991)
- "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms" (1996)
- "Scheduling Multithreaded Computations by Work Stealing" (1999)
- "Verification of a Concurrent Deque Implementation" (1999)
- "Thread Scheduling for Multiprogrammed Multiprocessors" (2001)
- "An optimistic approach to lock-free FIFO queues" (2008)

Helpful code samples & thread synchronisation informations:

- <https://github.com/Xudong-Huang/may/issues/32>
- <https://www.rethinkdb.com/blog/making-coroutines-fast/>
- <https://en.wikipedia.org/wiki/Monitor_(synchronization)>
- <https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem>

