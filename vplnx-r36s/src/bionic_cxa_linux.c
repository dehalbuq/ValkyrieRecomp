/* SPDX-License-Identifier: MIT
 * Bionic C++ destructor lifecycle: no glibc atexit registration of guest
 * pointers, and no execution of functions from unloaded Android modules.
 * The caller must invoke vpl_bcxa_finalize(dso) BEFORE unmapping an ELF.
 */
#define _GNU_SOURCE 1
#include "bionic_cxa_linux.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

#define VPL_CXA_MAX 8192
typedef struct {
    void (*destructor)(void *);
    void *arg;
    void *dso;
    int pending;
} DestructorEntry;
static DestructorEntry dtors[VPL_CXA_MAX];
static size_t dtor_count=0;
static pthread_mutex_t dtor_lock=PTHREAD_MUTEX_INITIALIZER;

int vpl_bcxa_atexit(void (*destructor)(void *), void *arg, void *dso) {
    if(!destructor) return -1;
    pthread_mutex_lock(&dtor_lock);
    if(dtor_count>=VPL_CXA_MAX) {
        pthread_mutex_unlock(&dtor_lock);
        return -1;
    }
    DestructorEntry *entry=&dtors[dtor_count++];
    entry->destructor=destructor;
    entry->arg=arg;
    entry->dso=dso;
    entry->pending=1;
    pthread_mutex_unlock(&dtor_lock);
    return 0;
}
void vpl_bcxa_finalize(void *dso) {
    /* Reverse registration order, as mandated by the Itanium C++ ABI.
     * Claim under lock, invoke outside lock to support reentrant dtors.
     */
    pthread_mutex_lock(&dtor_lock);
    size_t max=dtor_count;
    pthread_mutex_unlock(&dtor_lock);
    while(max>0) {
        void (*func)(void*)=NULL;
        void *arg=NULL;
        pthread_mutex_lock(&dtor_lock);
        for(size_t i=max;i>0;i--) {
            DestructorEntry *entry=&dtors[i-1];
            if(entry->pending && (!dso || entry->dso==dso)) {
                entry->pending=0;
                func=entry->destructor;
                arg=entry->arg;
                max=i-1;
                break;
            }
        }
        pthread_mutex_unlock(&dtor_lock);
        if(!func) break;
        func(arg);
    }
}
size_t vpl_bcxa_pending(void) {
    size_t count=0;
    pthread_mutex_lock(&dtor_lock);
    for(size_t i=0;i<dtor_count;i++)
        if(dtors[i].pending) count++;
    pthread_mutex_unlock(&dtor_lock);
    return count;
}

/* Bionic ARM64 uses a global __stack_chk_guard import as a data symbol.
 * Preserve one strong unpredictable canary for the lifetime of the bridge.
 * If entropy cannot be obtained, the resolver must fail rather than
 * exporting an easily predicted canary.
 */
static uintptr_t guest_stack_guard=0;
static pthread_once_t guard_once=PTHREAD_ONCE_INIT;
static void init_guest_stack_guard(void) {
    uintptr_t value=0;
    size_t received=0;
    while(received<sizeof(value)) {
        ssize_t n=getrandom((unsigned char*)&value+received,
                            sizeof(value)-received,0);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) return;
        received+=(size_t)n;
    }
    if(value==0) return;
    guest_stack_guard=value;
}
uintptr_t vpl_bionic_stack_guard_address(void) {
    pthread_once(&guard_once,init_guest_stack_guard);
    return guest_stack_guard ? (uintptr_t)&guest_stack_guard : 0;
}
