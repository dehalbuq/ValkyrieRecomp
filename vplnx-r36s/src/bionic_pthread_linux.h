/* SPDX-License-Identifier: MIT
 * Bionic pthread opaque-object adapters for Android LP64 API21+.
 *
 * Android ARM64 mutex=40 bytes, cond=48 bytes (NOT glibc pthread_t ABI).
 * Guest data stays in guest memory. Host pthread objects are kept in a
 * bounded table keyed by guest addresses. No host struct is copied into the
 * guest's Android object.
 */
#ifndef VPLNX_BIONIC_PTHREAD_LINUX_H
#define VPLNX_BIONIC_PTHREAD_LINUX_H
#include <stdint.h>
#include <time.h>
typedef struct { uint32_t state; uint8_t reserved[36]; } VplBionicMutex;
typedef struct { uint32_t state; uint8_t reserved[44]; } VplBionicCond;
_Static_assert(sizeof(VplBionicMutex)==40, "Android LP64 mutex is 40 bytes");
_Static_assert(sizeof(VplBionicCond)==48, "Android LP64 cond is 48 bytes");

int vpl_bp_mutex_init(VplBionicMutex *mutex, const long *attr);
int vpl_bp_mutex_lock(VplBionicMutex *mutex);
int vpl_bp_mutex_trylock(VplBionicMutex *mutex);
int vpl_bp_mutex_unlock(VplBionicMutex *mutex);
int vpl_bp_mutex_destroy(VplBionicMutex *mutex);
int vpl_bp_cond_init(VplBionicCond *cond, const long *attr);
int vpl_bp_cond_signal(VplBionicCond *cond);
int vpl_bp_cond_broadcast(VplBionicCond *cond);
int vpl_bp_cond_wait(VplBionicCond *cond, VplBionicMutex *mutex);
int vpl_bp_cond_timedwait(VplBionicCond *cond, VplBionicMutex *mutex, const struct timespec *deadline);
int vpl_bp_cond_destroy(VplBionicCond *cond);
int vpl_bp_mutexattr_init(long *attr);
int vpl_bp_mutexattr_settype(long *attr, int type);
int vpl_bp_mutexattr_destroy(long *attr);
#endif
