#ifndef LENNETH_CXX_GUARD41_H
#define LENNETH_CXX_GUARD41_H
#include <stdint.h>
/* Independently-authored, limited Itanium C++ guard state adapter.
 * Bionic/Android per-version compatibility with the commercial game is
 * UNVERIFIED. Never pass arbitrary/untrusted guest pointers to these APIs. */
int lenneth_guard41_acquire(uint64_t *guard);
int lenneth_guard41_release(uint64_t *guard);
int lenneth_guard41_abort(uint64_t *guard);
unsigned lenneth_guard41_waiters(uint64_t *guard);
int lenneth_guard41_selftest(void);
/* Only test-specific __wrap__ entrypoints: never interpose on host libc++. */
int __wrap___cxa_guard_acquire(uint64_t *guard);
void __wrap___cxa_guard_release(uint64_t *guard);
void __wrap___cxa_guard_abort(uint64_t *guard);
#endif
