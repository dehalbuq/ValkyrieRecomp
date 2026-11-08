/*
 * Lenneth Phase27: independent, synthetic JNI JavaVM/JNIEnv ABI fixture.
 * MIT. This is NOT a substitute for Android's ART/Java VM or class loader.
 * It does not expose a JVM to the commercial game or execute game JNI code.
 *
 * C JNI ABI: VM and Env are each a pointer to a function-table pointer.
 * JavaVM Invoke table indices: Attach=4, Detach=5, GetEnv=6.
 * JNIEnv Native table indices: GetVersion=4, FindClass=6.
 */
#define _GNU_SOURCE
#include "jni_synthetic27.h"
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { J_OK=0, J_ERR=-1, J_EDETACHED=-2, J_EVERSION=-3,
       J_VERSION_1_6=0x00010006, J_VERSION_1_4=0x00010004 };
typedef const struct EnvFns *JNIEnv27;
typedef const struct VmFns *JavaVM27;
typedef struct EnvFns {
    void *reserved0,*reserved1,*reserved2,*reserved3;
    int (*GetVersion)(JNIEnv27 *);
    void *(*DefineClass)(JNIEnv27 *,const char *,void *,const signed char *,int);
    void *(*FindClass)(JNIEnv27 *,const char *);
} EnvFns;
typedef struct VmFns {
    void *reserved0,*reserved1,*reserved2;
    int (*DestroyJavaVM)(JavaVM27 *);
    int (*AttachCurrentThread)(JavaVM27 *,void **,void *);
    int (*DetachCurrentThread)(JavaVM27 *);
    int (*GetEnv)(JavaVM27 *,void **,int);
    int (*AttachCurrentThreadAsDaemon)(JavaVM27 *,void **,void *);
} VmFns;
_Static_assert(sizeof(void *)==8,"AArch64 JNI pointer size");
_Static_assert(offsetof(VmFns,GetEnv)==6*sizeof(void *),"JNI Invoke slot 6");
_Static_assert(offsetof(EnvFns,GetVersion)==4*sizeof(void *),"JNI Native slot 4");
_Static_assert(offsetof(EnvFns,FindClass)==6*sizeof(void *),"JNI Native slot 6");

static int env_get_version(JNIEnv27 *);
static void *env_find_class(JNIEnv27 *,const char *);
static int vm_destroy(JavaVM27 *);
static int vm_attach(JavaVM27 *,void **,void *);
static int vm_detach(JavaVM27 *);
static int vm_get_env(JavaVM27 *,void **,int);

static const EnvFns env_functions={
    .GetVersion=env_get_version,.FindClass=env_find_class
};
static const VmFns vm_functions={
    .DestroyJavaVM=vm_destroy,.AttachCurrentThread=vm_attach,
    .DetachCurrentThread=vm_detach,.GetEnv=vm_get_env,
    .AttachCurrentThreadAsDaemon=vm_attach
};
static _Thread_local int tls_attached;
static _Thread_local JNIEnv27 tls_env_pointer;
static _Thread_local unsigned tls_find_denials;
static int verified_vm(JavaVM27 *vm){
    return vm && *vm==&vm_functions;
}
static int verified_env(JNIEnv27 *env){
    return env && tls_attached && *env==&env_functions &&
           env==&tls_env_pointer;
}
static int env_get_version(JNIEnv27 *env){
    return verified_env(env)?J_VERSION_1_6:J_ERR;
}
static void *env_find_class(JNIEnv27 *env,const char *name){
    (void)name;
    if(verified_env(env))tls_find_denials++;
    /* Class loading requires Android ART + app ClassLoader. No fake class
     * tokens are returned and no synthetic Java object is made accessible
     * to commercial native code. */
    return NULL;
}
static int vm_destroy(JavaVM27 *vm){
    return verified_vm(vm)?J_ERR:J_ERR; /* no actual VM to destroy */
}
static int valid_version(int version){
    return version==J_VERSION_1_6||version==J_VERSION_1_4;
}
static int vm_get_env(JavaVM27 *vm,void **out,int version){
    if(!verified_vm(vm)||!out)return J_ERR;
    *out=NULL;
    if(!valid_version(version))return J_EVERSION;
    if(!tls_attached)return J_EDETACHED;
    tls_env_pointer=&env_functions;
    *out=&tls_env_pointer;
    return J_OK;
}
static int vm_attach(JavaVM27 *vm,void **out,void *args){
    (void)args;
    if(!verified_vm(vm)||!out)return J_ERR;
    tls_attached=1;
    tls_env_pointer=&env_functions;
    *out=&tls_env_pointer;
    return J_OK;
}
static int vm_detach(JavaVM27 *vm){
    if(!verified_vm(vm)||!tls_attached)return J_ERR;
    tls_attached=0;tls_env_pointer=NULL;
    return J_OK;
}
/* Only this independently compiled mock is ever invoked. */
static int mock_JNI_OnLoad(JavaVM27 *vm,void *reserved){
    (void)reserved;
    if(!verified_vm(vm))return J_ERR;
    void *environment=NULL;
    if((*vm)->GetEnv(vm,&environment,J_VERSION_1_6)!=J_OK ||
       !environment)return J_ERR;
    JNIEnv27 *env=environment;
    if((*env)->GetVersion(env)!=J_VERSION_1_6)return J_ERR;
    if((*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxRenderer")!=NULL)
        return J_ERR;
    return J_VERSION_1_6;
}
typedef struct {JavaVM27 *vm;int passed;} WorkerState;
static void *secondary_thread(void *p){
    WorkerState *s=p;void *env=NULL;
    int detach_check=(*s->vm)->GetEnv(s->vm,&env,J_VERSION_1_6)==J_EDETACHED && !env;
    int attach_ok=(*s->vm)->AttachCurrentThread(s->vm,&env,NULL)==J_OK &&
                  env && mock_JNI_OnLoad(s->vm,NULL)==J_VERSION_1_6;
    int detach_ok=(*s->vm)->DetachCurrentThread(s->vm)==J_OK &&
                  (*s->vm)->GetEnv(s->vm,&env,J_VERSION_1_6)==J_EDETACHED && !env;
    s->passed=detach_check&&attach_ok&&detach_ok;
    return NULL;
}
int lenneth_jni_synthetic27_selftest(void){
    JavaVM27 vm_ptr=&vm_functions;
    JavaVM27 *vm=&vm_ptr;
    void *env=NULL;
    int ok=(*vm)->GetEnv(vm,&env,J_VERSION_1_6)==J_EDETACHED&&!env;
    if((*vm)->AttachCurrentThread(vm,&env,NULL)!=J_OK||!env)ok=0;
    if(mock_JNI_OnLoad(vm,NULL)!=J_VERSION_1_6)ok=0;
    if(tls_find_denials!=1)ok=0;
    if((*vm)->GetEnv(vm,&env,0x00010008)!=J_EVERSION||env)ok=0;
    if((*vm)->GetEnv(vm,&env,J_VERSION_1_4)!=J_OK||!env)ok=0;
    if((*vm)->AttachCurrentThreadAsDaemon(vm,&env,NULL)!=J_OK||!env)ok=0;
    WorkerState worker={.vm=vm,.passed=0};
    pthread_t t;
    if(pthread_create(&t,NULL,secondary_thread,&worker)!=0)ok=0;
    else if(pthread_join(t,NULL)!=0||!worker.passed)ok=0;
    if((*vm)->DetachCurrentThread(vm)!=J_OK)ok=0;
    if((*vm)->GetEnv(vm,&env,J_VERSION_1_6)!=J_EDETACHED||env)ok=0;
    if((*vm)->DestroyJavaVM(vm)!=J_ERR)ok=0;
    printf("[lenneth phase27] JNI_VM_SELFTEST=%s synthetic_JNI_OnLoad=PASS "
           "JavaVM_GetEnv=PASS thread_attach_detach=PASS "
           "JNIEnv_thread_local=PASS FindClass=DENIED "
           "Android_ART=NOT_IMPLEMENTED app_ClassLoader=NOT_IMPLEMENTED "
           "commercial_JNI_OnLoad=NOT_CALLED engine_executed=NO\n",
           ok?"PASS":"FAIL");
    return ok?0:27;
}


/* Phase30 startup replay with only the marker-verified synthetic ELF.
 * No part of this is installed as a JNI bridge for the commercial engine. */
int lenneth_jni27_replay_mock(Lenneth30MockOnload onload,
                              Lenneth30MockInit init,
                              Lenneth30MockRender render){
    if(!onload||!init||!render)return 30;
    JavaVM27 vm_pointer=&vm_functions;
    JavaVM27 *vm=&vm_pointer;
    void *env=NULL;
    int attached=(*vm)->AttachCurrentThread(vm,&env,NULL)==J_OK && env;
    int returned=attached?onload(vm,NULL):J_ERR;
    if(attached&&returned==J_VERSION_1_6){
        init(env,NULL,640,480);
        render(env,NULL);
    }
    int detached=attached && (*vm)->DetachCurrentThread(vm)==J_OK;
    int ok=attached && detached && returned==J_VERSION_1_6;
    printf("[lenneth phase30] SYNTHETIC_JNI=%s onload_version=0x%x "
           "nativeInit=nativeRender=MOCK_ONLY threads_detached=%d "
           "guest_commercial_executed=NO\n",
           ok?"PASS":"FAIL",returned,detached);
    return ok?0:30;
}
