/*
 * Experimental Bionic pthread compatibility bridge for R36S (MIT).
 * The guest pthread objects are opaque *keys*, never glibc structs.
 * Host state lives in a bounded registry. This is presently tested ONLY
 * with independent synthetic code; original game is NEVER executed.
 *
 * Limitations: Android static recursive/errorcheck mutex initializers are
 * not yet decoded; some attrs, scheduling policies and cancellation are
 * unsupported. Passing the synthetic test is not proof of safe guest ABI.
 */
#define _GNU_SOURCE
#include "bionic_threads.h"
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <time.h>
#define MAX_GUEST_OBJECTS 2048
_Static_assert(sizeof(pthread_t)==sizeof(uintptr_t),"pthread_t must be pointer-sized");
_Static_assert(sizeof(pthread_key_t)==sizeof(uint32_t),"pthread_key_t must be 32-bit");
enum kind {K_MUTEX=1,K_COND,K_ONCE,K_ATTR,K_MUTEXATTR};
typedef struct Item {
    const void *guest;
    enum kind kind;
    int ready;
    union {pthread_mutex_t mu;pthread_cond_t cond;pthread_once_t once;
           pthread_attr_t attr;pthread_mutexattr_t mattr;} host;
    struct Item *next;
} Item;
static pthread_mutex_t reg_lock=PTHREAD_MUTEX_INITIALIZER;
static Item *items;
static unsigned count;
static Item *entry(const void *guest,enum kind kind,int create){
    if(!guest)return NULL;
    pthread_mutex_lock(&reg_lock);
    Item *p=items;
    while(p){if(p->guest==guest && p->kind==kind)break;p=p->next;}
    if(!p&&create&&count<MAX_GUEST_OBJECTS) {
        p=calloc(1,sizeof(*p));
        if(p){p->guest=guest;p->kind=kind;p->next=items;items=p;count++;}
    }
    pthread_mutex_unlock(&reg_lock);
    return p;
}
static Item *mutex_item(void *guest,int lazy) {
    Item *p=entry(guest,K_MUTEX,lazy);if(!p)return NULL;
    pthread_mutex_lock(&reg_lock);
    if(!p->ready && lazy && !pthread_mutex_init(&p->host.mu,NULL))p->ready=1;
    int ok=p->ready;
    pthread_mutex_unlock(&reg_lock);return ok?p:NULL;
}
static Item *cond_item(void *guest,int lazy) {
    Item *p=entry(guest,K_COND,lazy);if(!p)return NULL;
    pthread_mutex_lock(&reg_lock);
    if(!p->ready && lazy && !pthread_cond_init(&p->host.cond,NULL))p->ready=1;
    int ok=p->ready;
    pthread_mutex_unlock(&reg_lock);return ok?p:NULL;
}
static int b_mutex_init(void *guest,const void *guest_attr){
    Item *p=entry(guest,K_MUTEX,1);if(!p)return EAGAIN;
    pthread_mutexattr_t *a=NULL;
    if(guest_attr){
        Item *q=entry(guest_attr,K_MUTEXATTR,0);
        if(!q||!q->ready)return EINVAL;
        a=&q->host.mattr;
    }
    pthread_mutex_lock(&reg_lock);
    int rc=p->ready?EBUSY:pthread_mutex_init(&p->host.mu,a);
    if(!rc)p->ready=1;
    pthread_mutex_unlock(&reg_lock);return rc;
}
static int b_mutex_destroy(void *guest){
    Item *p=mutex_item(guest,0);if(!p)return EINVAL;
    int rc=pthread_mutex_destroy(&p->host.mu);
    if(!rc){pthread_mutex_lock(&reg_lock);p->ready=0;pthread_mutex_unlock(&reg_lock);}return rc;
}
static int b_mutex_lock(void *g){Item *p=mutex_item(g,1);return p?pthread_mutex_lock(&p->host.mu):EINVAL;}
static int b_mutex_trylock(void *g){Item *p=mutex_item(g,1);return p?pthread_mutex_trylock(&p->host.mu):EINVAL;}
static int b_mutex_unlock(void *g){Item *p=mutex_item(g,0);return p?pthread_mutex_unlock(&p->host.mu):EINVAL;}
static int b_cond_init(void *guest,const void *attr){
    if(attr)return ENOTSUP;
    Item *p=entry(guest,K_COND,1);if(!p)return EAGAIN;
    pthread_mutex_lock(&reg_lock);
    int rc=p->ready?EBUSY:pthread_cond_init(&p->host.cond,NULL);
    if(!rc)p->ready=1;
    pthread_mutex_unlock(&reg_lock);return rc;
}
static int b_cond_destroy(void *g){
    Item *p=cond_item(g,0);if(!p)return EINVAL;
    int rc=pthread_cond_destroy(&p->host.cond);
    if(!rc){pthread_mutex_lock(&reg_lock);p->ready=0;pthread_mutex_unlock(&reg_lock);}return rc;
}
static int b_cond_wait(void *c,void *m) {
    Item *ci=cond_item(c,1),*mu=mutex_item(m,0);
    return ci&&mu?pthread_cond_wait(&ci->host.cond,&mu->host.mu):EINVAL;
}
static int b_cond_timedwait(void *c,void *m,const struct timespec *time){
    Item *ci=cond_item(c,1),*mu=mutex_item(m,0);
    return ci&&mu&&time?pthread_cond_timedwait(&ci->host.cond,&mu->host.mu,time):EINVAL;
}
static int b_cond_signal(void *g){Item *p=cond_item(g,1);return p?pthread_cond_signal(&p->host.cond):EINVAL;}
static int b_cond_broadcast(void *g){Item *p=cond_item(g,1);return p?pthread_cond_broadcast(&p->host.cond):EINVAL;}
static int b_once(void *g,void(*fn)(void)){
    Item *p=entry(g,K_ONCE,1);return p&&fn?pthread_once(&p->host.once,fn):EINVAL;
}
static int b_mattr_init(void *g){
    Item *p=entry(g,K_MUTEXATTR,1);if(!p)return EAGAIN;
    pthread_mutex_lock(&reg_lock);
    int rc=p->ready?EBUSY:pthread_mutexattr_init(&p->host.mattr);
    if(!rc)p->ready=1;
    pthread_mutex_unlock(&reg_lock);return rc;
}
static int b_mattr_destroy(void *g){
    Item *p=entry(g,K_MUTEXATTR,0);if(!p||!p->ready)return EINVAL;
    int rc=pthread_mutexattr_destroy(&p->host.mattr);
    if(!rc){pthread_mutex_lock(&reg_lock);p->ready=0;pthread_mutex_unlock(&reg_lock);}return rc;
}
static int b_mattr_settype(void *g,int type){
    Item *p=entry(g,K_MUTEXATTR,0);
    return p&&p->ready?pthread_mutexattr_settype(&p->host.mattr,type):EINVAL;
}
static int b_attr_init(void *g){
    Item *p=entry(g,K_ATTR,1);if(!p)return EAGAIN;
    pthread_mutex_lock(&reg_lock);
    int rc=p->ready?EBUSY:pthread_attr_init(&p->host.attr);
    if(!rc)p->ready=1;
    pthread_mutex_unlock(&reg_lock);return rc;
}
static int b_attr_destroy(void *g){
    Item *p=entry(g,K_ATTR,0);if(!p||!p->ready)return EINVAL;
    int rc=pthread_attr_destroy(&p->host.attr);
    if(!rc){pthread_mutex_lock(&reg_lock);p->ready=0;pthread_mutex_unlock(&reg_lock);}return rc;
}
static int b_attr_policy(void *g,int v){
    Item *p=entry(g,K_ATTR,0);return p&&p->ready?pthread_attr_setschedpolicy(&p->host.attr,v):EINVAL;
}
static int b_attr_param(void *g,const struct sched_param *v){
    Item *p=entry(g,K_ATTR,0);
    return p&&p->ready&&v?pthread_attr_setschedparam(&p->host.attr,v):EINVAL;
}
static int b_create(uintptr_t *guest_id,const void *guest_attr,void *(*fn)(void*),void *arg){
    if(!guest_id||!fn)return EINVAL;
    pthread_attr_t *a=NULL;
    if(guest_attr){
        Item *p=entry(guest_attr,K_ATTR,0);
        if(!p||!p->ready)return EINVAL;
        a=&p->host.attr;
    }
    pthread_t id;
    int rc=pthread_create(&id,a,fn,arg);
    if(!rc)*guest_id=(uintptr_t)id;
    return rc;
}
static int b_join(uintptr_t id,void **ret){return pthread_join((pthread_t)id,ret);}
static int b_detach(uintptr_t id){return pthread_detach((pthread_t)id);}
static uintptr_t b_self(void){return (uintptr_t)pthread_self();}
static int b_equal(uintptr_t a,uintptr_t b){return pthread_equal((pthread_t)a,(pthread_t)b);}
static int b_sched(uintptr_t id,int *policy,struct sched_param *p){
    return pthread_getschedparam((pthread_t)id,policy,p);
}
static int b_key_create(uint32_t *g,void(*fn)(void*)){
    if(!g)return EINVAL;
    pthread_key_t key;int rc=pthread_key_create(&key,fn);
    if(!rc)*g=(uint32_t)key;return rc;
}
static int b_key_delete(uint32_t key){return pthread_key_delete((pthread_key_t)key);}
static int b_setspecific(uint32_t key,const void *value){return pthread_setspecific((pthread_key_t)key,value);}
static void *b_getspecific(uint32_t key){return pthread_getspecific((pthread_key_t)key);}
typedef struct {const char *name;void(*fn)(void);} Export;
#define X(name,fn) {"pthread_" #name,(void(*)(void))(fn)}
static const Export exports[]={
    X(mutex_init,b_mutex_init),X(mutex_destroy,b_mutex_destroy),
    X(mutex_lock,b_mutex_lock),X(mutex_trylock,b_mutex_trylock),
    X(mutex_unlock,b_mutex_unlock),X(cond_init,b_cond_init),
    X(cond_destroy,b_cond_destroy),X(cond_wait,b_cond_wait),
    X(cond_timedwait,b_cond_timedwait),X(cond_signal,b_cond_signal),
    X(cond_broadcast,b_cond_broadcast),X(once,b_once),
    X(mutexattr_init,b_mattr_init),X(mutexattr_destroy,b_mattr_destroy),
    X(mutexattr_settype,b_mattr_settype),X(attr_init,b_attr_init),
    X(attr_destroy,b_attr_destroy),X(attr_setschedpolicy,b_attr_policy),
    X(attr_setschedparam,b_attr_param),X(create,b_create),
    X(join,b_join),X(detach,b_detach),X(self,b_self),
    X(equal,b_equal),X(getschedparam,b_sched),
    X(key_create,b_key_create),X(key_delete,b_key_delete),
    X(setspecific,b_setspecific),X(getspecific,b_getspecific)
};
int lenneth_bionic_thread_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(exports)/sizeof(exports[0]);i++){
        if(strcmp(exports[i].name,name))continue;
        void (*fn)(void)=exports[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));return 1;
    }
    return 0;
}
/* Unit test all basic types without ever invoking any guest binary. */
static int guest_mutex,guest_cond,thread_ready,once_counter;
static void once_fn(void){once_counter++;}
static void *worker(void *unused){
    (void)unused;
    if(b_mutex_lock(&guest_mutex))return (void*)(uintptr_t)1;
    thread_ready=1;
    if(b_cond_signal(&guest_cond))return (void*)(uintptr_t)2;
    if(b_mutex_unlock(&guest_mutex))return (void*)(uintptr_t)3;
    return NULL;
}
int lenneth_bionic_thread_selftest(void){
    int err=0;uintptr_t tid=0;void *ret=NULL;
    if(b_mutex_init(&guest_mutex,NULL))err=1;
    if(b_cond_init(&guest_cond,NULL))err=2;
    if(b_mutex_lock(&guest_mutex))err=3;
    if(b_create(&tid,NULL,worker,NULL))err=4;
    struct timespec until={0};
    clock_gettime(CLOCK_REALTIME,&until);until.tv_sec+=3;
    while(!thread_ready&&!err){
        if(b_cond_timedwait(&guest_cond,&guest_mutex,&until)){err=5;break;}
    }
    if(b_mutex_unlock(&guest_mutex))err=6;
    if(tid && b_join(tid,&ret))err=7;
    if(ret)err=8;
    if(b_cond_destroy(&guest_cond))err=9;
    if(b_mutex_destroy(&guest_mutex))err=10;
    uint32_t key=0;
    if(b_key_create(&key,NULL))err=11;
    if(b_setspecific(key,(void*)(uintptr_t)0x1234))err=12;
    if(b_getspecific(key)!=(void*)(uintptr_t)0x1234)err=13;
    if(b_key_delete(key))err=14;
    int once_token=0;
    if(b_once(&once_token,once_fn)||b_once(&once_token,once_fn)||
       once_counter!=1)err=15;
    int attr_token=0,rec_mutex=0;
    if(b_mattr_init(&attr_token))err=16;
    if(b_mattr_settype(&attr_token,PTHREAD_MUTEX_RECURSIVE))err=17;
    if(b_mutex_init(&rec_mutex,&attr_token))err=18;
    if(b_mutex_lock(&rec_mutex)||b_mutex_lock(&rec_mutex))err=19;
    if(b_mutex_unlock(&rec_mutex)||b_mutex_unlock(&rec_mutex))err=20;
    if(b_mutex_destroy(&rec_mutex)||b_mattr_destroy(&attr_token))err=21;
    printf("[lenneth bionic-thread] SELFTEST=%s error=%d "
           "mutex_cond_thread_tls_once_attr=covered engine=not_executed\n",
           err?"FAIL":"PASS",err);
    return err?9:0;
}
