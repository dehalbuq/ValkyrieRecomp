/*
 * Lenneth Phase32 — isolated JNI C ABI service fixture (MIT).
 * Based on Phase31 observed class/method literals; this is NOT Android ART.
 *
 * Only independently authored test code calls these JNI function pointers.
 * Commercial ELF modules stay mapped non-executable; no game constructors,
 * JNI_OnLoad, nativeInit, nativeRender or asset access occurs here.
 *
 * Unsupported requests fail closed; the mock never reaches a filesystem,
 * Java bytecode, Android service or real class loader.
 */
#include "jni_services32.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { P32_JNI_1_6=0x00010006, P32_ERR=-1 };
typedef const struct JniFns32 *JNIEnv32;
typedef JNIEnv32 *Env32;

/* Exact function-table indices in the C JNIEnv ABI, including padding.
 * Typed members avoid casting data pointers into function pointers. */
typedef struct JniFns32 {
    void *reserved0_3[4];
    int (*GetVersion)(Env32 *);
    void *reserved5;
    void *(*FindClass)(Env32 *, const char *);
    void *reserved7_14[8];
    void *(*ExceptionOccurred)(Env32 *);
    void *reserved16;
    void (*ExceptionClear)(Env32 *);
    void *reserved18_112[95];
    void *(*GetStaticMethodID)(Env32 *, void *, const char *, const char *);
    void *reserved114_115[2];
    void *(*CallStaticObjectMethodA)(Env32 *, void *, void *, const void *);
    void *reserved117_166[50];
    void *(*NewStringUTF)(Env32 *, const char *);
    void *reserved168;
    const char *(*GetStringUTFChars)(Env32 *, void *, unsigned char *);
    void (*ReleaseStringUTFChars)(Env32 *, void *, const char *);
    void *reserved171_214[44];
    int (*RegisterNatives)(Env32 *, void *, const void *, int);
    void *reserved216_227[12];
    unsigned char (*ExceptionCheck)(Env32 *);
} JniFns32;

#define JNI_SLOT32(name,index) _Static_assert(offsetof(JniFns32,name)==(index)*sizeof(void *), "JNI slot mismatch: " #name)
JNI_SLOT32(GetVersion,4);
JNI_SLOT32(FindClass,6);
JNI_SLOT32(ExceptionOccurred,15);
JNI_SLOT32(ExceptionClear,17);
JNI_SLOT32(GetStaticMethodID,113);
JNI_SLOT32(CallStaticObjectMethodA,116);
JNI_SLOT32(NewStringUTF,167);
JNI_SLOT32(GetStringUTFChars,169);
JNI_SLOT32(ReleaseStringUTFChars,170);
JNI_SLOT32(RegisterNatives,215);
JNI_SLOT32(ExceptionCheck,228);
_Static_assert(sizeof(void *)==8, "Phase32 requires 64-bit JNI pointer ABI");

/* Addresses identify handles; never dereference external/unknown jobjects. */
typedef struct { const char *name; } Class32;
typedef struct { const char *name, *signature; } Method32;
typedef struct { const char *text; } String32;
static const Class32 classes32[]={
    {"org/cocos2dx/lib/Cocos2dxRenderer"},
    {"org/cocos2dx/lib/Cocos2dxHelper"},
    {"org/cocos2dx/lib/Cocos2dxActivity"},
    {"org/cocos2dx/lib/Cocos2dxBitmap"},
    {"org/cocos2dx/lib/Cocos2dxGLSurfaceView"},
    {"android/content/Context"},
    {"java/lang/ClassLoader"},
    {"android/view/Surface"},
    {"android/media/AudioManager"}
};
static const Method32 methods32[]={
    {"getCocos2dxWritablePath","()Ljava/lang/String;"},
    {"getCurrentLanguage","()Ljava/lang/String;"}
};
/* Synthetic sentinels, NOT app-private paths or game assets. */
static const String32 results32[]={
    {"/synthetic-phase32-only/no-filesystem-access/"},
    {"pt"}
};
static _Thread_local int exception32;
static _Thread_local unsigned borrowed32;
static _Thread_local char user_chars32[64];
static _Thread_local String32 user_string32;
static _Thread_local int user_active32;
static const int exception_sentinel32=32;

static int version32(Env32 *env);
static void *class32(Env32 *env,const char *name);
static void *exception_occurred32(Env32 *env);
static void exception_clear32(Env32 *env);
static void *static_method32(Env32 *env,void *clazz,const char *name,const char *sig);
static void *call_static32(Env32 *env,void *clazz,void *method,const void *args);
static void *new_string32(Env32 *env,const char *value);
static const char *string_chars32(Env32 *env,void *value,unsigned char *copy);
static void release_chars32(Env32 *env,void *value,const char *chars);
static int register_natives32(Env32 *env,void *clazz,const void *methods,int count);
static unsigned char check_exception32(Env32 *env);
static const JniFns32 funcs32={
    .GetVersion=version32, .FindClass=class32,
    .ExceptionOccurred=exception_occurred32,
    .ExceptionClear=exception_clear32,
    .GetStaticMethodID=static_method32,
    .CallStaticObjectMethodA=call_static32,
    .NewStringUTF=new_string32, .GetStringUTFChars=string_chars32,
    .ReleaseStringUTFChars=release_chars32,
    .RegisterNatives=register_natives32,
    .ExceptionCheck=check_exception32
};
static JNIEnv32 env_pointer32=&funcs32;
static int valid_env32(Env32 *env){
    return env==&env_pointer32 && *env==&funcs32;
}
static int version32(Env32 *env){return valid_env32(env)?P32_JNI_1_6:P32_ERR;}
static void *class32(Env32 *env,const char *name){
    if(!valid_env32(env)||exception32)return NULL;
    if(name)for(size_t i=0;i<sizeof(classes32)/sizeof(classes32[0]);i++)
        if(!strcmp(name,classes32[i].name))return (void *)&classes32[i];
    exception32=1;
    return NULL;
}
static void *exception_occurred32(Env32 *env){
    return valid_env32(env)&&exception32?(void *)&exception_sentinel32:NULL;
}
static void exception_clear32(Env32 *env){if(valid_env32(env))exception32=0;}
static void *static_method32(Env32 *env,void *clazz,const char *name,const char *sig){
    if(!valid_env32(env)||exception32)return NULL;
    if(clazz==(void *)&classes32[1] && name && sig)
        for(size_t i=0;i<sizeof(methods32)/sizeof(methods32[0]);i++)
            if(!strcmp(name,methods32[i].name)&&!strcmp(sig,methods32[i].signature))
                return (void *)&methods32[i];
    exception32=1;
    return NULL;
}
static void *call_static32(Env32 *env,void *clazz,void *method,const void *args){
    (void)args;
    if(!valid_env32(env)||exception32)return NULL;
    if(clazz==(void *)&classes32[1])
        for(size_t i=0;i<sizeof(methods32)/sizeof(methods32[0]);i++)
            if(method==(void *)&methods32[i])return (void *)&results32[i];
    exception32=1;
    return NULL;
}
static void *new_string32(Env32 *env,const char *value){
    if(!valid_env32(env)||exception32)return NULL;
    /* Enforce bounded, single-fixture-local string storage. */
    size_t n=value?strnlen(value,sizeof(user_chars32)):sizeof(user_chars32);
    if(n>=sizeof(user_chars32)||user_active32||borrowed32){
        exception32=1;return NULL;
    }
    memcpy(user_chars32,value,n+1);
    user_string32.text=user_chars32;
    user_active32=1;
    return (void *)&user_string32;
}
static const char *string_value32(void *value){
    for(size_t i=0;i<sizeof(results32)/sizeof(results32[0]);i++)
        if(value==(void *)&results32[i])return results32[i].text;
    if(user_active32&&value==(void *)&user_string32)return user_string32.text;
    return NULL;
}
static const char *string_chars32(Env32 *env,void *value,unsigned char *copy){
    if(!valid_env32(env)||exception32)return NULL;
    const char *s=string_value32(value);
    if(!s){exception32=1;return NULL;}
    borrowed32++;
    if(copy)*copy=0; /* direct immutable storage; not a copied allocation */
    return s;
}
static void release_chars32(Env32 *env,void *value,const char *chars){
    if(!valid_env32(env)||exception32)return;
    if(!borrowed32||!chars||chars!=string_value32(value))exception32=1;
    else borrowed32--;
}
static int register_natives32(Env32 *env,void *clazz,const void *methods,int count){
    (void)clazz;(void)methods;(void)count;
    if(valid_env32(env))exception32=1;
    return P32_ERR; /* dynamic registration is unsafe and not supported */
}
static unsigned char check_exception32(Env32 *env){
    return valid_env32(env)&&exception32?1:0;
}
static void report32(FILE *f,const char *gate,const char *status,const char *details){
    fprintf(f,"synthetic\t%s\t%s\t%s\n",gate,status,details);
}
int lenneth_jni_services32_selftest(void){
    Env32 *env=&env_pointer32;
    int ok=((*env)->GetVersion(env)==P32_JNI_1_6);
    unsigned known=0;
    for(size_t i=0;i<sizeof(classes32)/sizeof(classes32[0]);i++)
        known+=((*env)->FindClass(env,classes32[i].name)==(void *)&classes32[i]);
    ok=ok && known==9;
    void *helper=(*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxHelper");
    void *method=(*env)->GetStaticMethodID(env,helper,
                      "getCocos2dxWritablePath","()Ljava/lang/String;");
    void *result=(*env)->CallStaticObjectMethodA(env,helper,method,NULL);
    unsigned char is_copy=255;
    const char *value=(*env)->GetStringUTFChars(env,result,&is_copy);
    int writable_ok=value&&strcmp(value,"/synthetic-phase32-only/no-filesystem-access/")==0&&
                    is_copy==0;
    (*env)->ReleaseStringUTFChars(env,result,value);
    method=(*env)->GetStaticMethodID(env,helper,
                      "getCurrentLanguage","()Ljava/lang/String;");
    result=(*env)->CallStaticObjectMethodA(env,helper,method,NULL);
    value=(*env)->GetStringUTFChars(env,result,&is_copy);
    int lang_ok=value&&strcmp(value,"pt")==0;
    (*env)->ReleaseStringUTFChars(env,result,value);
    ok=ok&&writable_ok&&lang_ok&&borrowed32==0;
    void *str=(*env)->NewStringUTF(env,"synthetic text");
    value=(*env)->GetStringUTFChars(env,str,&is_copy);
    int text_ok=value&&!strcmp(value,"synthetic text");
    (*env)->ReleaseStringUTFChars(env,str,value);
    user_active32=0; /* fixture-owned local reference expires */
    ok=ok&&text_ok&&borrowed32==0;
    int unknown_class=(*env)->FindClass(env,"com/unknown/NotInAllowlist")==NULL &&
                      (*env)->ExceptionCheck(env)==1 &&
                      (*env)->ExceptionOccurred(env)!=NULL;
    (*env)->ExceptionClear(env);
    int bad_sig=(*env)->GetStaticMethodID(env,helper,
                "getCurrentLanguage","(I)Ljava/lang/String;")==NULL &&
                (*env)->ExceptionCheck(env)==1;
    (*env)->ExceptionClear(env);
    int denied_register=(*env)->RegisterNatives(env,helper,NULL,0)==P32_ERR &&
                        (*env)->ExceptionCheck(env)==1;
    (*env)->ExceptionClear(env);
    ok=ok&&unknown_class&&bad_sig&&denied_register&&
       (*env)->ExceptionCheck(env)==0;
    FILE *f=fopen("lenneth-jni-services32.tsv","w");
    if(!f)ok=0;
    else {
        fputs("module\tgate\tstatus\tdetail\n",f);
        report32(f,"JNI_C_ABI_LAYOUT",ok?"PASS":"FAIL",
                 "slots=4,6,15,17,113,116,167,169,170,215,228");
        report32(f,"ALLOWLIST_CLASS_LOOKUP",known==9&&unknown_class?"PASS":"FAIL",
                 "nine_synthetic_classes;unknown_class_denied");
        report32(f,"STATIC_METHOD_SIGNATURES",writable_ok&&lang_ok&&bad_sig?"PASS":"FAIL",
                 "two_mock_Cocos2dxHelper_methods;signature_checked");
        report32(f,"STRING_UTF8_LIFETIME",text_ok&&borrowed32==0?"PASS":"FAIL",
                 "bounded_fixture_strings;borrow_and_release");
        report32(f,"JNI_NATIVE_REGISTRATION",denied_register?"DENIED":"FAIL",
                 "RegisterNatives_not_implemented");
        report32(f,"ANDROID_CONTEXT_AND_ASSETS","BLOCKED",
                 "no_real_ClassLoader_AssetManager_Activity_or_service");
        report32(f,"COMMERCIAL_ENGINE","BLOCKED",
                 "no_original_constructors_JNI_or_native_methods_invoked");
        if(ferror(f)||fclose(f)!=0)ok=0;
    }
    printf("[lenneth phase32] JNI_SERVICES_SELFTEST=%s classes=%u/9 "
           "static_methods=%d/2 string_utf8=%s class_exception=%s "
           "invalid_signature=%s RegisterNatives=DENIED "
           "report=lenneth-jni-services32.tsv commercial_executed=NO\n",
           ok?"PASS":"FAIL",known,(writable_ok?1:0)+(lang_ok?1:0),
           text_ok&&borrowed32==0?"PASS":"FAIL",
           unknown_class?"PASS":"FAIL",bad_sig?"PASS":"FAIL");
    return ok?0:32;
}
