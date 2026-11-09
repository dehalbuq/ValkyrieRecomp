/* Phase41 C++11 Itanium ABI guard shim candidate, MIT.
 *
 * Uses a host-side mutex and condition to serialize local-static
 * initialization, instead of treating Android/Bionic guard internal bytes
 * as a glibc condition/mutex. The first byte follows the Itanium ABI.
 *
 * Does NOT establish the original game's precise Bionic ABI or pthread
 * layout. No commercial ELF/constructors are executed.
 */
#define _GNU_SOURCE
#include "cxx_guard41.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_GUARDS 256u
typedef struct {
    uint64_t *address;
    pthread_t owner;
    unsigned active, waiters;
} GuardEntry41;
static GuardEntry41 entries[MAX_GUARDS];
static unsigned entry_count;
static pthread_mutex_t guard_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t guard_cv=PTHREAD_COND_INITIALIZER;

static int valid_guard(uint64_t *address) {
    return address && ((uintptr_t)address % _Alignof(uint64_t))==0;
}
/* Registry lock must be held. Exceeding the bounded registry fails closed. */
static GuardEntry41 *entry(uint64_t *address,int create){
    for(unsigned i=0;i<entry_count;i++)
        if(entries[i].address==address)return &entries[i];
    if(!create||entry_count>=MAX_GUARDS)return NULL;
    GuardEntry41 *p=&entries[entry_count++];
    memset(p,0,sizeof(*p));p->address=address;
    return p;
}
int lenneth_guard41_acquire(uint64_t *address){
    if(!valid_guard(address))return -1;
    if(pthread_mutex_lock(&guard_lock)!=0)return -1;
    GuardEntry41 *p=entry(address,1);
    if(!p){pthread_mutex_unlock(&guard_lock);return -1;}
    for(;;){
        if(__atomic_load_n((unsigned char*)address,__ATOMIC_ACQUIRE)){
            pthread_mutex_unlock(&guard_lock);return 0;
        }
        if(!p->active){
            p->active=1;p->owner=pthread_self();
            pthread_mutex_unlock(&guard_lock);return 1;
        }
        if(pthread_equal(p->owner,pthread_self())){
            pthread_mutex_unlock(&guard_lock);
            return -1; /* Recursive initializer: wrapper aborts. */
        }
        p->waiters++;
        int rc=pthread_cond_wait(&guard_cv,&guard_lock);
        p->waiters--;
        if(rc){pthread_mutex_unlock(&guard_lock);return -1;}
    }
}
int lenneth_guard41_release(uint64_t *address){
    if(!valid_guard(address)||pthread_mutex_lock(&guard_lock)!=0)return -1;
    GuardEntry41 *p=entry(address,0);
    if(!p||!p->active||!pthread_equal(p->owner,pthread_self())){
        pthread_mutex_unlock(&guard_lock);return -1;
    }
    __atomic_store_n((unsigned char*)address,1,__ATOMIC_RELEASE);
    p->active=0;
    pthread_cond_broadcast(&guard_cv);
    pthread_mutex_unlock(&guard_lock);
    return 0;
}
int lenneth_guard41_abort(uint64_t *address){
    if(!valid_guard(address)||pthread_mutex_lock(&guard_lock)!=0)return -1;
    GuardEntry41 *p=entry(address,0);
    if(!p||!p->active||!pthread_equal(p->owner,pthread_self())){
        pthread_mutex_unlock(&guard_lock);return -1;
    }
    p->active=0;
    pthread_cond_broadcast(&guard_cv);
    pthread_mutex_unlock(&guard_lock);
    return 0;
}
unsigned lenneth_guard41_waiters(uint64_t *address){
    if(!valid_guard(address)||pthread_mutex_lock(&guard_lock)!=0)return 0;
    GuardEntry41 *p=entry(address,0);
    unsigned n=p?p->waiters:0;
    pthread_mutex_unlock(&guard_lock);
    return n;
}
/* GNU ld --wrap redirects the fixture's compiler-generated guard calls.
 * These wrappers are NOT inserted into the real game's symbol provider. */
int __wrap___cxa_guard_acquire(uint64_t *guard){
    int result=lenneth_guard41_acquire(guard);
    if(result<0)abort(); /* Never treat an error as permission to init. */
    return result;
}
void __wrap___cxa_guard_release(uint64_t *guard){
    if(lenneth_guard41_release(guard))abort();
}
void __wrap___cxa_guard_abort(uint64_t *guard){
    if(lenneth_guard41_abort(guard))abort();
}

typedef struct {uint64_t *guard;int result,release_after;} Worker;
static void *worker(void *arg){
    Worker *w=(Worker*)arg;
    w->result=lenneth_guard41_acquire(w->guard);
    if(w->result==1 && w->release_after)
        if(lenneth_guard41_release(w->guard))w->result=-1;
    return NULL;
}
static int wait_for_guard(uint64_t *g){
    for(int k=0;k<1000;k++){
        if(lenneth_guard41_waiters(g)>=1)return 1;
        struct timespec short_sleep={.tv_sec=0,.tv_nsec=1000000};
        nanosleep(&short_sleep,NULL);
    }
    return 0;
}
int lenneth_guard41_selftest(void){
    int ok=1;
    uint64_t guard_a=0,guard_b=0;
    ok &= lenneth_guard41_acquire(NULL)==-1;
    ok &= lenneth_guard41_acquire((uint64_t*)((char*)&guard_a+1))==-1;
    ok &= lenneth_guard41_acquire(&guard_a)==1;
    ok &= lenneth_guard41_acquire(&guard_a)==-1;
    ok &= lenneth_guard41_abort(&guard_a)==0;
    ok &= __atomic_load_n((unsigned char*)&guard_a,__ATOMIC_ACQUIRE)==0;
    ok &= lenneth_guard41_acquire(&guard_a)==1;
    ok &= lenneth_guard41_release(&guard_a)==0;
    ok &= __atomic_load_n((unsigned char*)&guard_a,__ATOMIC_ACQUIRE)==1;
    ok &= lenneth_guard41_acquire(&guard_a)==0;
    ok &= lenneth_guard41_release(&guard_a)==-1;

    ok &= lenneth_guard41_acquire(&guard_b)==1;
    Worker waiting={.guard=&guard_b,.result=-9,.release_after=0};
    pthread_t tid;
    int started=(pthread_create(&tid,NULL,worker,&waiting)==0);
    if(!started)return 41;
    int observed=wait_for_guard(&guard_b);
    ok &= observed && waiting.result==-9;
    ok &= lenneth_guard41_release(&guard_b)==0;
    ok &= pthread_join(tid,NULL)==0;
    ok &= waiting.result==0;

    uint64_t guard_retry=0;
    ok &= lenneth_guard41_acquire(&guard_retry)==1;
    Worker second={.guard=&guard_retry,.result=-9,.release_after=1};
    started=(pthread_create(&tid,NULL,worker,&second)==0);
    if(!started)return 41;
    observed=wait_for_guard(&guard_retry);
    ok &= observed && second.result==-9;
    ok &= lenneth_guard41_abort(&guard_retry)==0;
    ok &= pthread_join(tid,NULL)==0;
    ok &= second.result==1;
    ok &= lenneth_guard41_acquire(&guard_retry)==0;

    printf("[lenneth phase41] GUARD_SELFTEST=%s "
           "once=PASS abort_retry=%s concurrent_wait=%s "
           "recursive_init=REJECTED invalid_pointer=REJECTED "
           "commercial_game=NOT_EXECUTED\n",
           ok?"PASS":"FAIL", second.result==1?"PASS":"FAIL",
           observed?"PASS":"FAIL");
    return ok?0:41;
}
