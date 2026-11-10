/* SPDX-License-Identifier: MIT
 * Bionic pthread ARM64 object translation.
 * Android's user-space opaque 40-byte mutex and 48-byte condition objects
 * are keyed to separate host pthread objects, never reinterpreted.
 * 
 * The registry intentionally does not recycle destroyed slots: a competing
 * guest thread retaining a stale pointer cannot receive a newly created
 * host mutex in the same slot. Concurrency-safe guest destruction/reuse
 * still requires proper game-side lifecycle sequencing.
 */
#define _GNU_SOURCE 1
#include "bionic_pthread_linux.h"
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define BP_MUTEX_CAPACITY 2048
#define BP_COND_CAPACITY 2048
typedef struct {
    void *guest;
    pthread_mutex_t host;
    int present;
    int destroyed;
} MutexEntry;
typedef struct {
    void *guest;
    pthread_cond_t host;
    int present;
    int destroyed;
} CondEntry;
static pthread_mutex_t table_guard=PTHREAD_MUTEX_INITIALIZER;
static MutexEntry mutexes[BP_MUTEX_CAPACITY];
static CondEntry conds[BP_COND_CAPACITY];

static int android_mutex_type(uint32_t value, int *type) {
    if (!type || (value & ~(uint32_t)0xc000)) return EINVAL;
    switch ((value >> 14) & 3) {
        case 0: *type=PTHREAD_MUTEX_NORMAL; return 0;
        case 1: *type=PTHREAD_MUTEX_RECURSIVE; return 0;
        case 2: *type=PTHREAD_MUTEX_ERRORCHECK; return 0;
    }
    return EINVAL;
}
static int make_mutex(MutexEntry *entry, void *guest, int type) {
    pthread_mutexattr_t attr;
    int rc=pthread_mutexattr_init(&attr);
    if (rc) return rc;
    rc=pthread_mutexattr_settype(&attr,type);
    if (!rc) rc=pthread_mutex_init(&entry->host,&attr);
    pthread_mutexattr_destroy(&attr);
    if (!rc) {entry->guest=guest;entry->present=1;entry->destroyed=0;}
    return rc;
}
static int mutex_get(VplBionicMutex *guest, int explicit_init, const long *attr,
                     pthread_mutex_t **out) {
    if (!guest || !out) return EINVAL;
    int type=0;
    uint32_t value=guest->state;
    if (explicit_init && attr) {
        if ((*attr & ~0x1fL) != 0 || (*attr & 0x10L)) return ENOTSUP;
        value=(uint32_t)(*attr & 3L)<<14;
    }
    int rc=android_mutex_type(value,&type);
    if (rc) return rc;
    pthread_mutex_lock(&table_guard);
    for (size_t i=0;i<BP_MUTEX_CAPACITY;i++) {
        if(mutexes[i].present && !mutexes[i].destroyed &&
           mutexes[i].guest==guest) {
            *out=&mutexes[i].host;
            pthread_mutex_unlock(&table_guard);
            return explicit_init ? EBUSY : 0;
        }
    }
    for(size_t i=0;i<BP_MUTEX_CAPACITY;i++) {
        if(!mutexes[i].present) {
            rc=make_mutex(&mutexes[i],guest,type);
            if(!rc) *out=&mutexes[i].host;
            pthread_mutex_unlock(&table_guard);
            return rc;
        }
    }
    pthread_mutex_unlock(&table_guard);
    return ENOMEM;
}
int vpl_bp_mutex_init(VplBionicMutex *mutex,const long *attr) {
    pthread_mutex_t *p=NULL;
    return mutex_get(mutex,1,attr,&p);
}
int vpl_bp_mutex_lock(VplBionicMutex *mutex) {
    pthread_mutex_t *p=NULL;
    int rc=mutex_get(mutex,0,NULL,&p);
    return rc?rc:pthread_mutex_lock(p);
}
int vpl_bp_mutex_trylock(VplBionicMutex *mutex) {
    pthread_mutex_t *p=NULL;
    int rc=mutex_get(mutex,0,NULL,&p);
    return rc?rc:pthread_mutex_trylock(p);
}
int vpl_bp_mutex_unlock(VplBionicMutex *mutex) {
    pthread_mutex_t *p=NULL;
    int rc=mutex_get(mutex,0,NULL,&p);
    return rc?rc:pthread_mutex_unlock(p);
}
int vpl_bp_mutex_destroy(VplBionicMutex *mutex) {
    if (!mutex) return EINVAL;
    pthread_mutex_t *p=NULL;
    int rc=mutex_get(mutex,0,NULL,&p);
    if (rc) return rc;
    pthread_mutex_lock(&table_guard);
    for(size_t i=0;i<BP_MUTEX_CAPACITY;i++) {
        if(mutexes[i].present && !mutexes[i].destroyed &&
           mutexes[i].guest==mutex) {
            rc=pthread_mutex_destroy(&mutexes[i].host);
            if(!rc) mutexes[i].destroyed=1;
            pthread_mutex_unlock(&table_guard);
            return rc;
        }
    }
    pthread_mutex_unlock(&table_guard);
    return EINVAL;
}
static int cond_get(VplBionicCond *guest,int explicit_init,
                    pthread_cond_t **out) {
    if (!guest || !out) return EINVAL;
    if (guest->state != 0) return ENOTSUP; /* unsupported custom attributes */
    pthread_mutex_lock(&table_guard);
    for(size_t i=0;i<BP_COND_CAPACITY;i++) {
        if(conds[i].present && !conds[i].destroyed && conds[i].guest==guest) {
            *out=&conds[i].host;
            pthread_mutex_unlock(&table_guard);
            return explicit_init?EBUSY:0;
        }
    }
    for(size_t i=0;i<BP_COND_CAPACITY;i++) {
        if(!conds[i].present) {
            int rc=pthread_cond_init(&conds[i].host,NULL);
            if(!rc) {
                conds[i].guest=guest;conds[i].present=1;conds[i].destroyed=0;
                *out=&conds[i].host;
            }
            pthread_mutex_unlock(&table_guard);
            return rc;
        }
    }
    pthread_mutex_unlock(&table_guard);
    return ENOMEM;
}
int vpl_bp_cond_init(VplBionicCond *cond,const long *attr) {
    if (attr && *attr) return ENOTSUP; /* process-shared/clock attrs not mapped */
    pthread_cond_t *p=NULL;
    return cond_get(cond,1,&p);
}
int vpl_bp_cond_signal(VplBionicCond *cond) {
    pthread_cond_t *p=NULL;
    int rc=cond_get(cond,0,&p);
    return rc?rc:pthread_cond_signal(p);
}
int vpl_bp_cond_broadcast(VplBionicCond *cond) {
    pthread_cond_t *p=NULL;
    int rc=cond_get(cond,0,&p);
    return rc?rc:pthread_cond_broadcast(p);
}
int vpl_bp_cond_wait(VplBionicCond *cond,VplBionicMutex *mutex) {
    pthread_cond_t *c=NULL;
    pthread_mutex_t *m=NULL;
    int rc=cond_get(cond,0,&c);
    if(!rc) rc=mutex_get(mutex,0,NULL,&m);
    return rc?rc:pthread_cond_wait(c,m);
}
int vpl_bp_cond_timedwait(VplBionicCond *cond,VplBionicMutex *mutex,
                          const struct timespec *deadline) {
    if(!deadline || deadline->tv_nsec<0 ||
       deadline->tv_nsec>=1000000000L) return EINVAL;
    pthread_cond_t *c=NULL;
    pthread_mutex_t *m=NULL;
    int rc=cond_get(cond,0,&c);
    if(!rc) rc=mutex_get(mutex,0,NULL,&m);
    return rc?rc:pthread_cond_timedwait(c,m,deadline);
}
int vpl_bp_cond_destroy(VplBionicCond *cond) {
    if(!cond) return EINVAL;
    pthread_cond_t *p=NULL;
    int rc=cond_get(cond,0,&p);
    if(rc) return rc;
    pthread_mutex_lock(&table_guard);
    for(size_t i=0;i<BP_COND_CAPACITY;i++) {
        if(conds[i].present && !conds[i].destroyed &&
           conds[i].guest==cond) {
            rc=pthread_cond_destroy(&conds[i].host);
            if(!rc) conds[i].destroyed=1;
            pthread_mutex_unlock(&table_guard);
            return rc;
        }
    }
    pthread_mutex_unlock(&table_guard);
    return EINVAL;
}
int vpl_bp_mutexattr_init(long *attr) {
    if(!attr) return EINVAL;
    *attr=0;return 0;
}
int vpl_bp_mutexattr_settype(long *attr,int type) {
    if(!attr || type<0 || type>2) return EINVAL;
    *attr=(*attr & ~0xfL) | (long)type;
    return 0;
}
int vpl_bp_mutexattr_destroy(long *attr) {
    if(!attr) return EINVAL;
    *attr=0;return 0;
}
