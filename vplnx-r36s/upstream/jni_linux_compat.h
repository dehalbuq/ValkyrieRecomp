/* SPDX-License-Identifier: MIT
 * Linux host glue for the MIT vpl_nx JNI fake VM implementation.
 * Do not emulate licensing, online authentication or Android ART here.
 */
#ifndef VPLNX_JNI_LINUX_COMPAT_H
#define VPLNX_JNI_LINUX_COMPAT_H
#define _GNU_SOURCE 1
#include <pthread.h>
#include "linux_audio_compat.h"
#define DEBUG_LOG 0
#define ASSETS_DIR "assets"
typedef pthread_mutex_t Mutex;
static inline void mutexInit(Mutex *m) { (void)pthread_mutex_init(m, NULL); }
static inline void mutexLock(Mutex *m) { (void)pthread_mutex_lock(m); }
static inline void mutexUnlock(Mutex *m) { (void)pthread_mutex_unlock(m); }
#endif
