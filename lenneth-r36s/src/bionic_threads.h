#ifndef LENNETH_BIONIC_THREADS_H
#define LENNETH_BIONIC_THREADS_H
#include <stdint.h>
/* Experimental guest-object to host-pthread adapters.
 * Non-executing ELF preflight only; guest code cannot call these yet.
 * Mutex / cond / once / attr objects are NEVER assumed glibc-compatible.
 */
int lenneth_bionic_thread_lookup(const char *name,uint64_t *address);
int lenneth_bionic_thread_selftest(void);
#endif
