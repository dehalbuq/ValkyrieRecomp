/*
 * Lenneth Phase32 (MIT): isolated, fail-closed JNI table and object fixture.
 * This module is NEVER installed as the commercial libMyGame.so JNI bridge.
 * Its only purpose is testing AArch64 JNI C ABI slot layout and synthetic
 * class/method/string lifecycles. No ART, game constructors, or game code.
 *
 * Method signatures, object lifetimes and Android services of the real APK
 * remain UNVERIFIED. Deliberately allow exactly one fixture-only method.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Ref32 Ref32;
typedef struct Method32 Method32;
typedef const struct JniFns32 *JniEnv32;
struct Ref32 { uint32_t magic; unsigned kind; const char *name; };
struct Method32 { uint32_t magic; const char *name; const char *sig; };
enum { REF_MAGIC=0x4c4e3332u, METHOD_MAGIC=0x4d333246u,
       CLASS_KIND=1, STRING_KIND=2, EXCEPTION_KIND=3,
       JNI_VERSION_1_6=0x00010006, JNI_FALSE=0, JNI_TRUE=1 };

/* Only selected entries of the published JNI C ABI are implemented.
 * All unimplemented slots are NULL and MUST NOT be used by game code. */
typedef struct JniFns32 {
    void *r0_3[4];
    int (*GetVersion)(JniEnv32 *);
    void *r5;
    Ref32 *(*FindClass)(JniEnv32 *,const char *);
    void *r7_14[8];
    Ref32 *(*ExceptionOccurred)(JniEnv32 *);
    void *r16;
    void (*ExceptionClear)(JniEnv32 *);
    void *r18_20[3];
    Ref32 *(*NewGlobalRef)(JniEnv32 *,Ref32 *);
    void (*DeleteGlobalRef)(JniEnv32 *,Ref32 *);
    void *r23;
    unsigned char (*IsSameObject)(JniEnv32 *,Ref32 *,Ref32 *);
    void *r25_30[6];
    Ref32 *(*GetObjectClass)(JniEnv32 *,Ref32 *);
    void *r32;
    Method32 *(*GetMethodID)(JniEnv32 *,Ref32 *,const char *,const char *);
    void *r34_112[79];
    Method32 *(*GetStaticMethodID)(JniEnv32 *,Ref32 *,const char *,const char *);
    Ref32 *(*CallStaticObjectMethod)(JniEnv32 *,Ref32 *,Method32 *,...);
    void *r115_166[52];
    Ref32 *(*NewStringUTF)(JniEnv32 *,const char *);
    void *r168;
    const char *(*GetStringUTFChars)(JniEnv32 *,Ref32 *,unsigned char *);
    void (*ReleaseStringUTFChars)(JniEnv32 *,Ref32 *,const char *);
    void *r171_214[44];
    int (*RegisterNatives)(JniEnv32 *,Ref32 *,const void *,int);
    void *r216_227[12];
    unsigned char (*ExceptionCheck)(JniEnv32 *);
} JniFns32;

#define SLOT(member,number) _Static_assert(offsetof(JniFns32,member)==(number)*sizeof(void *),"JNI slot mismatch: " #member)
SLOT(GetVersion,4); SLOT(FindClass,6); SLOT(ExceptionOccurred,15);
SLOT(ExceptionClear,17); SLOT(NewGlobalRef,21); SLOT(DeleteGlobalRef,22);
SLOT(IsSameObject,24); SLOT(GetObjectClass,31); SLOT(GetMethodID,33);
SLOT(GetStaticMethodID,113); SLOT(CallStaticObjectMethod,114);
SLOT(NewStringUTF,167); SLOT(GetStringUTFChars,169);
SLOT(ReleaseStringUTFChars,170); SLOT(RegisterNatives,215);
SLOT(ExceptionCheck,228);
_Static_assert(sizeof(void *)==8,"Phase32 tests 64-bit JNI only");

static const char *const known_classes[]={
    "org/cocos2dx/lib/Cocos2dxRenderer",
    "org/cocos2dx/lib/Cocos2dxHelper",
    "org/cocos2dx/lib/Cocos2dxActivity",
    "org/cocos2dx/lib/Cocos2dxBitmap",
    "org/cocos2dx/lib/Cocos2dxGLSurfaceView",
    "android/content/Context",
    "java/lang/ClassLoader",
    "android/view/Surface",
    "android/media/AudioManager",
    "java/lang/String" /* JNI fixture class; absent as Phase31 literal */
};
#define NCLASS (sizeof(known_classes)/sizeof(known_classes[0]))
static Ref32 classes[NCLASS];
static Ref32 writable_path={REF_MAGIC,STRING_KIND,"/sandbox/phase32/saves"};
static Ref32 synthetic_exception={REF_MAGIC,EXCEPTION_KIND,"synthetic/JNIException"};
static Method32 fixture_path_method={METHOD_MAGIC,"phase32FixturePath","()Ljava/lang/String;"};
static _Thread_local int pending_exception;
static _Thread_local unsigned global_ref_count;
static _Thread_local unsigned released_utf_count;
static JniEnv32 env_table;
static int valid_env(JniEnv32 *e){return e==&env_table && *e!=NULL;}
static int valid_ref(Ref32 *r){
    if(r==&writable_path || r==&synthetic_exception)return 1;
    for(size_t i=0;i<NCLASS;i++)if(r==&classes[i])return 1;
    return 0;
}
static int version(JniEnv32 *e){return valid_env(e)?JNI_VERSION_1_6:0;}
static Ref32 *find_class(JniEnv32 *e,const char *name){
    if(!valid_env(e))return NULL;
    if(!name || strlen(name)>128){pending_exception=1;return NULL;}
    for(size_t i=0;i<NCLASS;i++)
        if(!strcmp(name,known_classes[i]))return &classes[i];
    pending_exception=1;return NULL;
}
static Ref32 *exception_occurred(JniEnv32 *e){
    return valid_env(e)&&pending_exception?&synthetic_exception:NULL;
}
static void exception_clear(JniEnv32 *e){if(valid_env(e))pending_exception=0;}
static unsigned char exception_check(JniEnv32 *e){
    return valid_env(e)&&pending_exception?JNI_TRUE:JNI_FALSE;
}
static Ref32 *new_global(JniEnv32 *e,Ref32 *r){
    if(!valid_env(e)||!valid_ref(r)){pending_exception=1;return NULL;}
    global_ref_count++;return r; /* token only, NOT real Java ref ownership */
}
static void delete_global(JniEnv32 *e,Ref32 *r){
    if(!valid_env(e)||!valid_ref(r)||!global_ref_count){
        pending_exception=1;return;
    }
    global_ref_count--;
}
static unsigned char same_object(JniEnv32 *e,Ref32 *a,Ref32 *b){
    if(!valid_env(e) || (a&&!valid_ref(a)) || (b&&!valid_ref(b))){
        pending_exception=1;return JNI_FALSE;
    }
    return a==b?JNI_TRUE:JNI_FALSE;
}
static Ref32 *get_object_class(JniEnv32 *e,Ref32 *r){
    if(!valid_env(e))return NULL;
    if(r==&writable_path)return &classes[9];
    pending_exception=1;return NULL;
}
static Method32 *get_method(JniEnv32 *e,Ref32 *c,const char *name,const char *sig){
    (void)c;(void)name;(void)sig;
    if(valid_env(e))pending_exception=1;
    return NULL; /* instance Java methods are NOT implemented */
}
static Method32 *get_static_method(JniEnv32 *e,Ref32 *c,
                                    const char *name,const char *sig){
    if(!valid_env(e))return NULL;
    if(c==&classes[1] && name && sig &&
       !strcmp(name,fixture_path_method.name) &&
       !strcmp(sig,fixture_path_method.sig))return &fixture_path_method;
    pending_exception=1;return NULL;
}
static Ref32 *call_static_obj(JniEnv32 *e,Ref32 *c,Method32 *m,...){
    if(valid_env(e) && c==&classes[1] && m==&fixture_path_method)
        return &writable_path;
    if(valid_env(e))pending_exception=1;
    return NULL;
}
static Ref32 *new_utf(JniEnv32 *e,const char *value){
    if(valid_env(e) && value && !strcmp(value,writable_path.name))
        return &writable_path;
    if(valid_env(e))pending_exception=1;
    return NULL;
}
static const char *get_utf(JniEnv32 *e,Ref32 *r,unsigned char *copy){
    if(valid_env(e)&&r==&writable_path){if(copy)*copy=JNI_FALSE;return r->name;}
    if(valid_env(e))pending_exception=1;
    return NULL;
}
static void release_utf(JniEnv32 *e,Ref32 *r,const char *value){
    if(valid_env(e)&&r==&writable_path&&value==r->name)released_utf_count++;
    else if(valid_env(e))pending_exception=1;
}
static int register_natives(JniEnv32 *e,Ref32 *c,const void *methods,int count){
    (void)c;(void)methods;(void)count;
    if(valid_env(e))pending_exception=1;
    return -1; /* registration deliberately denied */
}
static const JniFns32 api={
    .GetVersion=version,.FindClass=find_class,
    .ExceptionOccurred=exception_occurred,.ExceptionClear=exception_clear,
    .NewGlobalRef=new_global,.DeleteGlobalRef=delete_global,
    .IsSameObject=same_object,.GetObjectClass=get_object_class,
    .GetMethodID=get_method,.GetStaticMethodID=get_static_method,
    .CallStaticObjectMethod=call_static_obj,.NewStringUTF=new_utf,
    .GetStringUTFChars=get_utf,.ReleaseStringUTFChars=release_utf,
    .RegisterNatives=register_natives,.ExceptionCheck=exception_check
};

int lenneth_jni_contract32_selftest(void){
    for(size_t i=0;i<NCLASS;i++){
        classes[i].magic=REF_MAGIC;classes[i].kind=CLASS_KIND;
        classes[i].name=known_classes[i];
    }
    env_table=&api;JniEnv32 *e=&env_table;
    pending_exception=0;global_ref_count=0;released_utf_count=0;
    int ok=(*e)->GetVersion(e)==JNI_VERSION_1_6;
    Ref32 *helper=(*e)->FindClass(e,"org/cocos2dx/lib/Cocos2dxHelper");
    ok=ok && helper==&classes[1];
    Method32 *method=(*e)->GetStaticMethodID(e,helper,
                             "phase32FixturePath","()Ljava/lang/String;");
    ok=ok && method==&fixture_path_method;
    Ref32 *s=(*e)->CallStaticObjectMethod(e,helper,method);
    unsigned char iscopy=1;
    const char *utf=(*e)->GetStringUTFChars(e,s,&iscopy);
    ok=ok && s==&writable_path && utf &&
       !strcmp(utf,"/sandbox/phase32/saves") && iscopy==JNI_FALSE;
    (*e)->ReleaseStringUTFChars(e,s,utf);
    Ref32 *global=(*e)->NewGlobalRef(e,s);
    ok=ok && global==s && (*e)->IsSameObject(e,global,s)==JNI_TRUE;
    (*e)->DeleteGlobalRef(e,global);
    ok=ok && !global_ref_count && released_utf_count==1 &&
       (*e)->GetObjectClass(e,s)==&classes[9];
    ok=ok && !(*e)->FindClass(e,"android/secret/Unsupported") &&
       (*e)->ExceptionCheck(e)==JNI_TRUE &&
       (*e)->ExceptionOccurred(e)==&synthetic_exception;
    (*e)->ExceptionClear(e);
    ok=ok && (*e)->ExceptionCheck(e)==JNI_FALSE;
    ok=ok && !(*e)->GetStaticMethodID(e,helper,"unknown","()V") &&
       (*e)->ExceptionCheck(e)==JNI_TRUE;
    (*e)->ExceptionClear(e);
    ok=ok && (*e)->RegisterNatives(e,helper,NULL,0)==-1 &&
       (*e)->ExceptionCheck(e)==JNI_TRUE;
    (*e)->ExceptionClear(e);
    ok=ok && !(*e)->GetMethodID(e,helper,"getAssets","()V") &&
       (*e)->ExceptionCheck(e)==JNI_TRUE;
    (*e)->ExceptionClear(e);
    ok=ok && (*e)->NewStringUTF(e,"/sandbox/phase32/saves")==s &&
       (*e)->ExceptionCheck(e)==JNI_FALSE;

    FILE *out=fopen("lenneth-jni-contract32.tsv","w");
    if(!out)ok=0;
    if(out){
        fputs("component\tgate\tstatus\tdetail\n",out);
        fprintf(out,"abi\tJNI_SLOT_LAYOUT\t%s\tselected_native_interface_indices_4_to_228\n",ok?"PASS":"FAIL");
        fprintf(out,"fixture\tCLASS_AND_METHOD\t%s\tknown_Cocos_class_fixture_only\n",ok?"PASS":"FAIL");
        fprintf(out,"fixture\tSTRING_AND_GLOBAL_REFS\t%s\tsynthetic_token_lifecycle_only\n",ok?"PASS":"FAIL");
        fprintf(out,"safety\tUNKNOWN_JAVA_REQUESTS\t%s\tdenied_with_synthetic_exception\n",ok?"PASS":"FAIL");
        fputs("runtime\tREGISTER_NATIVES\tBLOCKED\tdynamic_native_registration_not_implemented\n",out);
        fputs("runtime\tJAVA_VM_ART_CLASSLOADER\tBLOCKED\tno_real_Android_VM_or_java_objects\n",out);
        fputs("runtime\tMETHOD_SIGNATURES\tUNVERIFIED\tfixture_signature_is_NOT_game_evidence\n",out);
        fputs("runtime\tANDROID_ASSET_CONTEXT\tBLOCKED\tfilesystem_adapter_not_Java_AssetManager\n",out);
        fputs("runtime\tCOMMERCIAL_ENGINE\tBLOCKED\tno_guest_constructor_or_JNI_called\n",out);
        if(ferror(out)||fclose(out)!=0)ok=0;
    }
    printf("[lenneth phase32] JNI_CONTRACT=%s abi_slots=PASS "
           "class_lookup=FIXTURE method_dispatch=FIXTURE utf_refs=FIXTURE "
           "unknown_java=DENIED RegisterNatives=DENIED "
           "real_ART=NO engine_executed=NO report=lenneth-jni-contract32.tsv\n",
           ok?"PASS":"FAIL");
    return ok?0:32;
}
