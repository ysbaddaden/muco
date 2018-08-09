#ifndef CONFIG_H
#define CONFIG_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <errno.h>
#include <error.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ucontext.h>

#define STACK_PROT (PROT_READ | PROT_WRITE)
#define STACK_MAP (MAP_ANONYMOUS | MAP_PRIVATE | MAP_STACK)
#define STACK_FD (-1)
#define STACK_OFFSET (0)
#define STACK_SIZE (8 * 1024 * 1024)

#endif
