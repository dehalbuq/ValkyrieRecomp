/*
 * Phase34 MIT - JNI v1.6 ABI integration harness for an independently
 * compiled synthetic ELF only. This is NOT ART, Bionic, the Android NDK
 * runtime or an executable commercial-game bridge.
 *
 * Uses the real platform JNI header layout rather than hand-offset tables.
 * There is intentionally NO command-line path to run arbitrary game .so.
 */
#define _GNU_SOURCE
#include <jni.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { const char *name; int kind; } FixtureObject;
static FixtureObject renderer={"org/cocos2dx/lib/Cocos2dxRenderer",1};
static FixtureObject helper={"org/cocos2dx/lib/Cocos2dxHelper",1};
static FixtureObject path_obj={"/sandbox/phase34/saves",2};
static FixtureObject exception_obj={"fixture/Exception",3};
static FixtureObject path_method={"getCocos2dxWritablePath",4};

static _Thread_local int attached=0, pending=0;
static unsigned registered=0, utf_releases=0, globals=0, denied=0;
static JNINativeMethod native_init, native_render;
static const struct JNINativeInterface_ env_api;
static const struct JNIInvokeInterface_ vm_api;
static JNIEnv env_val=&env_api;
static JavaVM vm_val=&vm_api;

static int valid_env(JNIEnv *e) {
    return e==&env_val && attached && *e==&env_api;
}
static int valid_vm(JavaVM *v) {
    return v==&vm_val && *v==&vm_api;
}
static void fail(JNIEnv *env) {
    if(valid_env(env)) { pending=1; denied++; }
}
static jint JNICALL env_version(JNIEnv *env) {
    return valid_env(env)?JNI_VERSION_1_6:JNI_ERR;
}
static jclass JNICALL env_find(JNIEnv *env,const char *name) {
    if (!valid_env(env)) return NULL;
    if (name && !strcmp(name,renderer.name)) return (jclass)&renderer;
    if (name && !strcmp(name,helper.name)) return (jclass)&helper;
    fail(env);return NULL;
}
static jboolean JNICALL env_exception_check(JNIEnv *env) {
    return valid_env(env) && pending ? JNI_TRUE : JNI_FALSE;
}
static jthrowable JNICALL env_exception_occurred(JNIEnv *env) {
    return valid_env(env) && pending ? (jthrowable)&exception_obj : NULL;
}
static void JNICALL env_exception_clear(JNIEnv *env) {
    if (valid_env(env)) pending=0;
}
static jmethodID JNICALL env_get_static_method(
    JNIEnv *env,jclass klass,const char *name,const char *signature) {
    if (valid_env(env) && klass==(jclass)&helper && name && signature &&
        !strcmp(name,path_method.name) &&
        !strcmp(signature,"()Ljava/lang/String;"))
        return (jmethodID)&path_method;
    fail(env);return NULL;
}
static jobject JNICALL env_call_static_object(
    JNIEnv *env,jclass klass,jmethodID method,...) {
    if(valid_env(env) && klass==(jclass)&helper &&
       method==(jmethodID)&path_method) return (jobject)&path_obj;
    fail(env);return NULL;
}
static const char *JNICALL env_get_string(JNIEnv *env,jstring str,jboolean *copy) {
    if(valid_env(env) && str==(jstring)&path_obj) {
        if(copy)*copy=JNI_FALSE;
        return path_obj.name;
    }
    fail(env);return NULL;
}
static void JNICALL env_release_string(JNIEnv *env,jstring str,const char *chars) {
    if(valid_env(env) && str==(jstring)&path_obj && chars==path_obj.name)
        utf_releases++;
    else fail(env);
}
static jobject JNICALL env_new_global(JNIEnv *env,jobject ref) {
    if(valid_env(env) && (ref==(jobject)&renderer ||
                          ref==(jobject)&helper || ref==(jobject)&path_obj)) {
        globals++;return ref;
    }
    fail(env);return NULL;
}
static void JNICALL env_delete_global(JNIEnv *env,jobject ref) {
    if(valid_env(env) && globals &&
        (ref==(jobject)&renderer || ref==(jobject)&helper ||
         ref==(jobject)&path_obj)) { globals--;return; }
    fail(env);
}
static jint JNICALL env_register(JNIEnv *env,jclass klass,
                                 const JNINativeMethod *list,jint count) {
    /* Atomic, exact allowlist: do not partially modify registry on failure. */
    if(!valid_env(env) || klass!=(jclass)&renderer ||
       count!=2 || !list || !list[0].name || !list[0].signature ||
       !list[1].name || !list[1].signature ||
       strcmp(list[0].name,"nativeFixtureInit") ||
       strcmp(list[0].signature,"(II)V") ||
       strcmp(list[1].name,"nativeFixtureRender") ||
       strcmp(list[1].signature,"()V") ||
       !list[0].fnPtr || !list[1].fnPtr || registered) {
        fail(env);return JNI_ERR;
    }
    native_init=list[0];native_render=list[1];registered=2;
    return JNI_OK;
}
static jint JNICALL vm_getenv(JavaVM *vm,void **out,jint version) {
    if(!valid_vm(vm)||!out)return JNI_ERR;
    *out=NULL;
    if(version!=JNI_VERSION_1_6)return JNI_EVERSION;
    if(!attached)return JNI_EDETACHED;
    *out=&env_val;return JNI_OK;
}
static jint JNICALL vm_attach(JavaVM *vm,void **out,void *args) {
    (void)args;
    if(!valid_vm(vm)||!out)return JNI_ERR;
    attached=1;*out=&env_val;return JNI_OK;
}
static jint JNICALL vm_detach(JavaVM *vm) {
    if(!valid_vm(vm)||!attached)return JNI_ERR;
    attached=0;return JNI_OK;
}
static const struct JNINativeInterface_ env_api = {
    .GetVersion=env_version,
    .FindClass=env_find,
    .ExceptionCheck=env_exception_check,
    .ExceptionOccurred=env_exception_occurred,
    .ExceptionClear=env_exception_clear,
    .GetStaticMethodID=env_get_static_method,
    .CallStaticObjectMethod=env_call_static_object,
    .GetStringUTFChars=env_get_string,
    .ReleaseStringUTFChars=env_release_string,
    .NewGlobalRef=env_new_global,
    .DeleteGlobalRef=env_delete_global,
    .RegisterNatives=env_register
};
static const struct JNIInvokeInterface_ vm_api = {
    .AttachCurrentThread=vm_attach,
    .AttachCurrentThreadAsDaemon=vm_attach,
    .DetachCurrentThread=vm_detach,
    .GetEnv=vm_getenv
};
typedef jint (*OnLoad)(JavaVM *,void *);
typedef jint (*StateFn)(void);
typedef void (JNICALL *InitFn)(JNIEnv *,jobject,jint,jint);
typedef void (JNICALL *RenderFn)(JNIEnv *,jobject);
typedef struct {int ok;} ThreadResult;
static void *thread_check(void *p) {
    ThreadResult *r=p;
    void *env=(void *)1;
    int detached_ok=vm_val->GetEnv(&vm_val,&env,JNI_VERSION_1_6)==JNI_EDETACHED
                     && env==NULL;
    int attached_ok=vm_val->AttachCurrentThread(&vm_val,&env,NULL)==JNI_OK
                    && env==&env_val;
    int version_ok=vm_val->GetEnv(&vm_val,&env,0x00120000)==JNI_EVERSION;
    int detach_ok=vm_val->DetachCurrentThread(&vm_val)==JNI_OK;
    r->ok=detached_ok&&attached_ok&&version_ok&&detach_ok;
    return NULL;
}
int main(void) {
    /* The app/main thread begins attached; new pthreads do not. */
    attached=1;
    int ok=1, thread_ok=0, deny_ok=0;
    const char *fixture="/results/lenneth-phase34-fixture.so";
    /* Path is fixed to our independently compiled synthetic test guest. */
    void *lib=dlopen(fixture,RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"phase34 fixture unavailable: %s\n",dlerror());return 34;}
    void *marker_sym=dlsym(lib,"lenneth_phase34_fixture_marker");
    void *state_sym=dlsym(lib,"lenneth_phase34_fixture_state");
    void *onload_sym=dlsym(lib,"JNI_OnLoad");
    StateFn marker=NULL,state=NULL;OnLoad onload=NULL;
    if(sizeof(marker)!=sizeof(marker_sym)||sizeof(state)!=sizeof(state_sym)||
       sizeof(onload)!=sizeof(onload_sym) ||
       !marker_sym||!state_sym||!onload_sym) {
        dlclose(lib);return 34;
    }
    memcpy(&marker,&marker_sym,sizeof(marker));
    memcpy(&state,&state_sym,sizeof(state));
    memcpy(&onload,&onload_sym,sizeof(onload));
    if(marker()!=0x34344a4e){dlclose(lib);return 34;}
    jint returned=onload(&vm_val,NULL);
    ok=returned==JNI_VERSION_1_6 && registered==2 && utf_releases==1 &&
       !pending && native_init.fnPtr && native_render.fnPtr;
    InitFn init=NULL;RenderFn render=NULL;
    if(sizeof(init)!=sizeof(native_init.fnPtr) ||
       sizeof(render)!=sizeof(native_render.fnPtr))ok=0;
    if(ok){
        memcpy(&init,&native_init.fnPtr,sizeof(init));
        memcpy(&render,&native_render.fnPtr,sizeof(render));
        init(&env_val,(jobject)&renderer,640,480);
        render(&env_val,(jobject)&renderer);
        ok=state()==11;
    }
    jobject global=env_val->NewGlobalRef(&env_val,(jobject)&path_obj);
    ok=ok && global==(jobject)&path_obj && globals==1;
    env_val->DeleteGlobalRef(&env_val,global);
    ok=ok && globals==0;

    jclass bad=env_val->FindClass(&env_val,"android/permission/Unknown");
    deny_ok=bad==NULL && env_val->ExceptionCheck(&env_val)==JNI_TRUE &&
            env_val->ExceptionOccurred(&env_val)==(jthrowable)&exception_obj;
    env_val->ExceptionClear(&env_val);
    JNINativeMethod fake={"unexpected","()V",(void *)onload};
    deny_ok=deny_ok && env_val->RegisterNatives(&env_val,(jclass)&renderer,
                                                    &fake,1)==JNI_ERR &&
            env_val->ExceptionCheck(&env_val)==JNI_TRUE &&
            registered==2;
    env_val->ExceptionClear(&env_val);
    deny_ok=deny_ok && env_val->GetStaticMethodID(&env_val,(jclass)&helper,
                                       "unsupported","()V")==NULL &&
            env_val->ExceptionCheck(&env_val)==JNI_TRUE;
    env_val->ExceptionClear(&env_val);
    ok=ok && deny_ok && denied==3 && !pending;

    ThreadResult result={0};
    pthread_t thread;
    if(pthread_create(&thread,NULL,thread_check,&result)==0) {
        if(pthread_join(thread,NULL)==0)thread_ok=result.ok;
    }
    ok=ok && thread_ok;
    FILE *f=fopen("lenneth-jni-integration34.tsv","w");
    if(!f)ok=0;
    else {
        fprintf(f,"component\tgate\tstatus\tdetail\n");
        fprintf(f,"abi\tSTANDARD_JNI_HEADER\t%s\tOpenJDK_JNI_1_6_compatible_slots\n",ok?"PASS":"FAIL");
        fprintf(f,"fixture\tJNI_ONLOAD_REGISTER_NATIVES\t%s\ttwo_allowlisted_mock_callbacks\n",ok?"PASS":"FAIL");
        fprintf(f,"fixture\tCOCOS_WRITABLE_PATH\t%s\tmock_string_only\n",ok?"PASS":"FAIL");
        fprintf(f,"fixture\tJNI_NATIVE_INIT_RENDER\t%s\tsynthetic_lifecycle_only\n",ok?"PASS":"FAIL");
        fprintf(f,"fixture\tJNI_THREAD_ATTACH_DETACH\t%s\tmock_thread_local_vm\n",thread_ok?"PASS":"FAIL");
        fprintf(f,"safety\tUNKNOWN_CLASS_METHOD_REGISTRATION\t%s\tfail_closed\n",deny_ok?"PASS":"FAIL");
        fprintf(f,"runtime\tACTUAL_ART_CLASSLOADER\tBLOCKED\tno_real_Android_VM\n");
        fprintf(f,"runtime\tAPK_METHOD_SIGNATURES\tUNVERIFIED\tfixtures_not_evidence\n");
        fprintf(f,"runtime\tCOMMERCIAL_ENGINE\tBLOCKED\tgame_not_loaded_or_executed\n");
        if(ferror(f)||fclose(f)!=0)ok=0;
    }
    printf("[lenneth phase34] JNI_INTEGRATION=%s JNI_OnLoad=%s "
           "RegisterNatives=%s nativeInit_render=%s mock_threads=%s "
           "unknown_requests=%s game_executed=NO\n",
           ok?"PASS":"FAIL",returned==JNI_VERSION_1_6?"PASS":"FAIL",
           registered==2?"PASS":"FAIL",state()==11?"PASS":"FAIL",
           thread_ok?"PASS":"FAIL",deny_ok?"DENIED":"FAIL");
    dlclose(lib);
    return ok?0:34;
}
