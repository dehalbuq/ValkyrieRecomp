/* SPDX-License-Identifier: MIT
 * Exercise translated Android LP64 (40/48 byte) pthread objects.
 * Does not load, execute or distribute any commercial library.
 */
#define _POSIX_C_SOURCE 200809L
#include "bionic_pthread_linux.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

static VplBionicMutex mutex={0};
static VplBionicCond cond={0};
static int signaled=0;
static void *worker(void *unused) {
    (void)unused;
    struct timespec delay={0,30000000L};
    nanosleep(&delay,NULL);
    if(vpl_bp_mutex_lock(&mutex)!=0) return (void *)(uintptr_t)1;
    signaled=1;
    if(vpl_bp_cond_signal(&cond)!=0) return (void *)(uintptr_t)2;
    if(vpl_bp_mutex_unlock(&mutex)!=0) return (void *)(uintptr_t)3;
    return NULL;
}
static int fail(const char *msg) {
    fprintf(stderr,"BIONIC_PTHREAD=FAIL reason=%s\n",msg);
    return 1;
}
int main(void) {
    if (sizeof(VplBionicMutex)!=40 || sizeof(VplBionicCond)!=48)
        return fail("guest object size");
    long attr=0;
    if(vpl_bp_mutexattr_init(&attr)!=0 ||
       vpl_bp_mutexattr_settype(&attr,1)!=0 ||
       attr!=1)
        return fail("android recursive mutex attr");
    VplBionicMutex recursive={0};
    if(vpl_bp_mutex_init(&recursive,&attr)!=0 ||
       vpl_bp_mutex_lock(&recursive)!=0 ||
       vpl_bp_mutex_trylock(&recursive)!=0 ||
       vpl_bp_mutex_unlock(&recursive)!=0 ||
       vpl_bp_mutex_unlock(&recursive)!=0 ||
       vpl_bp_mutex_destroy(&recursive)!=0)
        return fail("recursive mutex lifecycle");
    if(vpl_bp_mutexattr_destroy(&attr)!=0)
        return fail("attr destroy");

    VplBionicMutex static_recursive={.state=0x4000};
    if(vpl_bp_mutex_lock(&static_recursive)!=0 ||
       vpl_bp_mutex_trylock(&static_recursive)!=0 ||
       vpl_bp_mutex_unlock(&static_recursive)!=0 ||
       vpl_bp_mutex_unlock(&static_recursive)!=0 ||
       vpl_bp_mutex_destroy(&static_recursive)!=0)
        return fail("static recursive initializer");
    if (vpl_bp_mutex_lock(&mutex)!=0)
        return fail("normal static mutex lock");
    if (vpl_bp_mutex_trylock(&mutex)!=EBUSY)
        return fail("normal mutex trylock must return EBUSY");
    pthread_t thread;
    if(pthread_create(&thread,NULL,worker,NULL)!=0)
        return fail("create host-side test worker");
    while(!signaled) {
        if (vpl_bp_cond_wait(&cond,&mutex)!=0)
            return fail("condition wait");
    }
    if(vpl_bp_mutex_unlock(&mutex)!=0)
        return fail("normal mutex unlock");
    void *ret=NULL;
    if(pthread_join(thread,&ret)!=0 || ret!=NULL)
        return fail("host worker completion");

    if(vpl_bp_mutex_lock(&mutex)!=0)
        return fail("mutex lock timedwait");
    struct timespec past={0,0};
    if(vpl_bp_cond_timedwait(&cond,&mutex,&past)!=ETIMEDOUT)
        return fail("condition timedwait");
    if(vpl_bp_mutex_unlock(&mutex)!=0)
        return fail("mutex unlock after timedwait");
    if(vpl_bp_cond_broadcast(&cond)!=0 ||
       vpl_bp_cond_destroy(&cond)!=0 ||
       vpl_bp_mutex_destroy(&mutex)!=0)
        return fail("destroy cond and mutex");
    puts("BIONIC_PTHREAD_ABI=PASS guest_mutex_bytes=40 guest_cond_bytes=48");
    puts("BIONIC_PTHREAD_LIFECYCLE=PASS recursive=PASS static=PASS cond_wait=PASS timedwait=PASS");
    puts("COMMERCIAL_ENGINE_EXECUTED=NO");
    return 0;
}
