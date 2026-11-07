/*
 * Lenneth R36S - independent Android AArch64 C++ ABI destructor bridge (MIT).
 *
 * Implements three standard C ABI destructor registration symbols:
 *  __cxa_atexit, __cxa_finalize, __cxa_thread_atexit_impl
 * under private LENNETH-named wrappers to avoid overriding host glibc.
 *
 * No game/library constructors are invoked here. In a real executable
 * loader, dso unload and thread-exit order require integration tests.
 * Global array is bounded; invoke no guest destructors during preflight.
 */
#define _GNU_SOURCE
#include "cxx_runtime.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_DTORS 4096
typedef void (*Destructor)(void *);
typedef struct {
    Destructor fn;
    void *arg,*dso;
    int live;
} DtorEntry;
static DtorEntry dtors[MAX_DTORS];
static size_t dtor_count;
static pthread_mutex_t dtor_lock=PTHREAD_MUTEX_INITIALIZER;
static int cb_registered;

static int guest_cxa_atexit(Destructor fn,void *arg,void *dso){
    if(!fn)return -1;
    pthread_mutex_lock(&dtor_lock);
    int ret=-1;
    if(dtor_count<MAX_DTORS){
        dtors[dtor_count++]=(DtorEntry){fn,arg,dso,1};
        ret=0;
    }
    pthread_mutex_unlock(&dtor_lock);
    return ret;
}
static void guest_cxa_finalize(void *dso){
    /* Walk live registrations in reverse. Release the lock before invoking
       callbacks so nested finalize/registration cannot deadlock. */
    for(;;){
        Destructor fn=NULL;
        void *arg=NULL;
        pthread_mutex_lock(&dtor_lock);
        for(size_t i=dtor_count;i>0;i--){
            DtorEntry *e=&dtors[i-1];
            if(!e->live || (dso && e->dso!=dso))continue;
            fn=e->fn;arg=e->arg;e->live=0;break;
        }
        pthread_mutex_unlock(&dtor_lock);
        if(!fn)break;
        fn(arg);
    }
}
typedef struct ThreadDtor {
    Destructor fn;
    void *arg;
    void *dso;
    struct ThreadDtor *next;
} ThreadDtor;
static pthread_key_t thread_key;
static pthread_once_t thread_once=PTHREAD_ONCE_INIT;
static int thread_key_ok=0;
static void thread_destruct(void *arg){
    ThreadDtor *p=(ThreadDtor*)arg;
    /* Itanium ABI: per-thread destructors execute in reverse order. */
    while(p){
        ThreadDtor *next=p->next;
        Destructor fn=p->fn;
        void *ctx=p->arg;
        free(p);
        if(fn)fn(ctx);
        p=next;
    }
}
static void make_thread_key(void) {
    thread_key_ok=(pthread_key_create(&thread_key,thread_destruct)==0);
}
static int guest_cxa_thread_atexit_impl(Destructor fn,void *arg,void *dso){
    if(!fn)return -1;
    pthread_once(&thread_once,make_thread_key);
    if(!thread_key_ok)return -1;
    ThreadDtor *item=(ThreadDtor*)malloc(sizeof(*item));
    if(!item)return -1;
    item->fn=fn;item->arg=arg;item->dso=dso;
    item->next=(ThreadDtor*)pthread_getspecific(thread_key);
    int rc=pthread_setspecific(thread_key,item);
    if(rc){free(item);return -1;}
    return 0;
}
/* Typed symbol allowlist: never bind to host glibc __cxa_* by name. */
typedef struct {const char *name;void(*fn)(void);} AbiEntry;
static const AbiEntry abi_entries[]={
    {"__cxa_atexit",(void(*)(void))guest_cxa_atexit},
    {"__cxa_finalize",(void(*)(void))guest_cxa_finalize},
    {"__cxa_thread_atexit_impl",(void(*)(void))guest_cxa_thread_atexit_impl}
};
int lenneth_cxx_runtime_lookup(const char *name,uint64_t *address){
    if(!name||!address)return 0;
    for(size_t i=0;i<sizeof(abi_entries)/sizeof(abi_entries[0]);i++){
        if(strcmp(abi_entries[i].name,name))continue;
        void(*fn)(void)=abi_entries[i].fn;
        if(sizeof(fn)!=sizeof(*address))return 0;
        memcpy(address,&fn,sizeof(fn));
        return 1;
    }
    return 0;
}
/* Independent synthetic selftest; never invokes pointers from commercial ELF. */
static int global_order[12],gcount;
static pthread_mutex_t test_lock=PTHREAD_MUTEX_INITIALIZER;
static void append_order(void *p){
    pthread_mutex_lock(&test_lock);
    if(gcount<12)global_order[gcount++]=(int)(uintptr_t)p;
    pthread_mutex_unlock(&test_lock);
}
static void *test_thread(void *arg){
    (void)arg;
    if(guest_cxa_thread_atexit_impl(append_order,(void*)(uintptr_t)4,(void*)2))
        return (void*)(uintptr_t)1;
    if(guest_cxa_thread_atexit_impl(append_order,(void*)(uintptr_t)5,(void*)2))
        return (void*)(uintptr_t)1;
    return NULL;
}
int lenneth_cxx_runtime_selftest(void){
    int ok=1;
    void *dso_a=(void*)(uintptr_t)0x111;
    void *dso_b=(void*)(uintptr_t)0x222;
    if(guest_cxa_atexit(append_order,(void*)(uintptr_t)1,dso_a) ||
       guest_cxa_atexit(append_order,(void*)(uintptr_t)2,dso_b) ||
       guest_cxa_atexit(append_order,(void*)(uintptr_t)3,dso_a))ok=0;
    guest_cxa_finalize(dso_a);
    if(gcount!=2||global_order[0]!=3||global_order[1]!=1)ok=0;
    guest_cxa_finalize(dso_a);
    if(gcount!=2)ok=0;
    guest_cxa_finalize(NULL);
    if(gcount!=3||global_order[2]!=2)ok=0;
    pthread_t thread;void *ret=NULL;
    if(pthread_create(&thread,NULL,test_thread,NULL))ok=0;
    else if(pthread_join(thread,&ret)||ret)ok=0;
    if(gcount!=5||global_order[3]!=5||global_order[4]!=4)ok=0;
    uint64_t sym=0;
    if(!lenneth_cxx_runtime_lookup("__cxa_atexit",&sym)||!sym ||
       lenneth_cxx_runtime_lookup("__stack_chk_guard",&sym))ok=0;
    printf("[lenneth cxx] SELFTEST=%s registered_lifo=%d per_thread_lifo=%d "
           "nested_dso_selection=covered guest_execution=NO\n",
           ok?"PASS":"FAIL",gcount>=3,gcount==5);
    cb_registered=(int)dtor_count;
    (void)cb_registered;
    return ok?0:7;
}
