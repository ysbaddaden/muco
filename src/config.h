#ifndef CO_CONFIG_H
#define CO_CONFIG_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <errno.h>
#include <error.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ucontext.h>

#define CO_STACK_PROT (PROT_READ | PROT_WRITE)
#define CO_STACK_MAP (MAP_ANONYMOUS | MAP_PRIVATE | MAP_STACK)
#define CO_STACK_FD (-1)
#define CO_STACK_OFFSET (0)
#define CO_STACK_SIZE (SIGSTKSZ)

#define thread_local _Thread_local

#endif
