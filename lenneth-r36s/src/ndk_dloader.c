/*
 * Lenneth ARM64 NDK-style libdl shim (MIT, independently written).
 *
 * The Linux / Android dl* API uses compatible primitive/pointer arguments,
 * but handles, namespaces and RTLD symbol rules differ significantly.
 * Do not forward guest handles to glibc. Expose only host GLES/EGL
 * with opaque static handles, allowlist exported GL/EGL symbols, and
 * reject absolute paths, Java/NDK packages, private APK libs, etc.
 *
 * This is a bounded diagnostic bridge, not a full Android linker. No
 * commercial guest instructions or constructors are ever called here.
 */
#define _GNU_SOURCE
#include "ndk_dloader.h"
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { MAX_GUEST_MODULES=3 };
typedef struct {
    const char *guest_soname,*major,*minor;
    void *host;
    unsigned refs;
} GuestModule;
static GuestModule modules[MAX_GUEST_MODULES]={
    {"libGLESv2.so","libGLESv2.so.2","libGLESv2.so",NULL,0},
    {"libEGL.so","libEGL.so.1","libEGL.so",NULL,0},
    /* Phase26: optional native ES1 provider. Not an ES1->ES2 translator.
     * Guest dlopen fails closed if no native ES1 implementation exists. */
    {"libGLESv1_CM.so","libGLESv1_CM.so.1","libGLESv1_CM.so",NULL,0}
};
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static _Thread_local char last_error[192];
static _Thread_local int error_active;

static void set_error(const char *text){
    if(!text)text="unknown error";
    snprintf(last_error,sizeof(last_error),"%s",text);
    error_active=1;
}
static int is_symbol(const char *name,int egl){
    if(!name)return 0;
    size_t len=strnlen(name,128);
    if(len<4||len>=128)return 0;
    if(egl)return name[0]=='e'&&name[1]=='g'&&name[2]=='l'&&
                  name[3]>='A'&&name[3]<='Z';
    return name[0]=='g'&&name[1]=='l'&&
           name[2]>='A'&&name[2]<='Z';
}
static void *guest_dlopen(const char *name,int flags){
    (void)flags;
    if(!name || !*name || strchr(name,'/') || strchr(name,'\\')){
        set_error("guest dlopen: unsupported name or path");return NULL;
    }
    pthread_mutex_lock(&lock);
    for(int i=0;i<MAX_GUEST_MODULES;i++){
        GuestModule *m=&modules[i];
        if(strcmp(name,m->guest_soname))continue;
        if(!m->host){
            m->host=dlopen(m->major,RTLD_NOW|RTLD_LOCAL|RTLD_NODELETE);
            if(!m->host)m->host=dlopen(m->minor,RTLD_NOW|RTLD_LOCAL|RTLD_NODELETE);
        }
        if(!m->host){pthread_mutex_unlock(&lock);
            set_error("guest dlopen: host GLES/EGL unavailable");return NULL;}
        m->refs++;
        pthread_mutex_unlock(&lock);
        return m;
    }
    pthread_mutex_unlock(&lock);
    set_error("guest dlopen: unknown Android library (not mapped)");
    return NULL;
}
static void *guest_dlsym(void *handle,const char *name){
    if(!name){set_error("guest dlsym: NULL symbol");return NULL;}
    pthread_mutex_lock(&lock);
    for(int i=0;i<MAX_GUEST_MODULES;i++){
        GuestModule *m=&modules[i];
        if(handle!=m)continue;
        if(!m->host||!m->refs){
            pthread_mutex_unlock(&lock);
            set_error("guest dlsym: closed handle");return NULL;
        }
        if(!is_symbol(name,i==1)){
            pthread_mutex_unlock(&lock);
            set_error("guest dlsym: name is not GLES/EGL function");return NULL;
        }
        void *value=dlsym(m->host,name);
        if(!value){
            pthread_mutex_unlock(&lock);
            set_error("guest dlsym: native export unavailable");return NULL;
        }
        pthread_mutex_unlock(&lock);
        return value;
    }
    pthread_mutex_unlock(&lock);
    set_error("guest dlsym: unknown guest handle");return NULL;
}
static int guest_dlclose(void *handle){
    pthread_mutex_lock(&lock);
    for(int i=0;i<MAX_GUEST_MODULES;i++){
        GuestModule *m=&modules[i];
        if(handle!=m)continue;
        if(!m->refs){
            pthread_mutex_unlock(&lock);
            set_error("guest dlclose: already closed");return -1;
        }
        m->refs--;
        pthread_mutex_unlock(&lock);
        /* The actual native handle remains pinned until process exit:
           never unmap a GLES call target while guest still owns a pointer. */
        return 0;
    }
    pthread_mutex_unlock(&lock);
    set_error("guest dlclose: unknown guest handle");return -1;
}
static char *guest_dlerror(void){
    if(!error_active)return NULL;
    error_active=0;
    return last_error;
}
void lenneth_ndk_dl_close_all(void){
    /* This is a non-executable diagnostic. Do not close any provider
       library while other GL/EGL lookups may still own a pointer. */
    pthread_mutex_lock(&lock);
    for(int i=0;i<MAX_GUEST_MODULES;i++)modules[i].refs=0;
    pthread_mutex_unlock(&lock);
}
typedef struct {const char *name;void(*fn)(void);} Entry;
#define EXPORT(sym,fn) {sym,(void(*)(void))(fn)}
static const Entry entries[]={
    EXPORT("dlopen",guest_dlopen),
    EXPORT("dlsym",guest_dlsym),
    EXPORT("dlclose",guest_dlclose),
    EXPORT("dlerror",guest_dlerror)
};
int lenneth_ndk_dl_lookup(const char *name,uint64_t *address){
    if(!name||!address)return 0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
        if(strcmp(name,entries[i].name))continue;
        void(*fn)(void)=entries[i].fn;
        if(sizeof(fn)!=sizeof(*address))return 0;
        memcpy(address,&fn,sizeof(fn));return 1;
    }
    return 0;
}
int lenneth_ndk_dl_selftest(void){
    int ok=1;
    if(guest_dlopen("../etc/passwd",RTLD_NOW)!=NULL)ok=0;
    const char *error=guest_dlerror();
    if(!error||!strstr(error,"unsupported")||guest_dlerror()!=NULL)ok=0;
    if(guest_dlopen("libMyGame.so",RTLD_NOW)!=NULL)ok=0;
    if(!guest_dlerror())ok=0;
    void *gl=guest_dlopen("libGLESv2.so",RTLD_NOW);
    void *egl=guest_dlopen("libEGL.so",RTLD_NOW);
    if(!gl||!egl)ok=0;
    if(gl && !guest_dlsym(gl,"glClear"))ok=0;
    if(egl && !guest_dlsym(egl,"eglGetProcAddress"))ok=0;
    if(gl && guest_dlsym(gl,"system"))ok=0;
    if(gl && !guest_dlerror())ok=0;
    if(gl && guest_dlclose(gl)!=0)ok=0;
    if(egl && guest_dlclose(egl)!=0)ok=0;
    if(gl && guest_dlclose(gl)==0)ok=0;
    if(gl && !guest_dlerror())ok=0;
    /* Optional GLES1: absent is an expected, nonfatal host capability.
     * Never pass guest code a fake GLES1 handle backed only by GLES2. */
    void *g1=guest_dlopen("libGLESv1_CM.so",RTLD_NOW);
    if(g1){
        void *legacy=guest_dlsym(g1,"glMatrixMode");
        if(!legacy && !guest_dlerror())ok=0;
        if(guest_dlsym(g1,"system")!=NULL)ok=0;
        if(!guest_dlerror())ok=0;
        if(guest_dlclose(g1)!=0)ok=0;
    }else if(!guest_dlerror())ok=0;
    uint64_t p=0;
    if(!lenneth_ndk_dl_lookup("dlopen",&p)||!p ||
       lenneth_ndk_dl_lookup("dl_iterate_phdr",&p))ok=0;
    printf("[lenneth ndk-dl] SELFTEST=%s host_GLES_EGL=allowlisted "
           "guest_paths=DENIED guest_handles=isolated guest_gles1=OPTIONAL_NATIVE_ONLY "
           "dl_iterate_phdr=NOT_MAPPED commercial_execution=NO\n",
           ok?"PASS":"FAIL");
    return ok?0:8;
}
