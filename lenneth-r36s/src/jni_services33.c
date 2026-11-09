/*
 * Lenneth Phase33 (MIT): isolated Android JNI service graph fixture.
 * No commercial ELF execution and no actual ART, JVM, Context, or assets.
 * Test-only return values/signatures MUST NOT be mistaken for game ABI.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Ref33 Ref33;
typedef struct Method33 Method33;
typedef const struct EnvFns33 *JniEnv33;
struct Ref33 { unsigned kind; const char *name; };
struct Method33 { unsigned id; const char *name; const char *sig; };
enum { CLASS=1, OBJECT=2, STRING=3, VM_1_6=0x00010006 };
enum { M_PATH=1,M_LANG,M_LOADER,M_ASSETS,M_SERVICE,M_LOAD_CLASS };
enum { MAX_REF_NAME=160 };
typedef struct EnvFns33 {
    void *r0_3[4];
    int (*GetVersion)(JniEnv33 *);
    void *r5;
    Ref33 *(*FindClass)(JniEnv33 *,const char *);
    void *r7_14[8];
    Ref33 *(*ExceptionOccurred)(JniEnv33 *);
    void *r16;
    void (*ExceptionClear)(JniEnv33 *);
    void *r18_20[3];
    Ref33 *(*NewGlobalRef)(JniEnv33 *,Ref33 *);
    void (*DeleteGlobalRef)(JniEnv33 *,Ref33 *);
    void *r23;
    unsigned char (*IsSameObject)(JniEnv33 *,Ref33 *,Ref33 *);
    void *r25_30[6];
    Ref33 *(*GetObjectClass)(JniEnv33 *,Ref33 *);
    void *r32;
    Method33 *(*GetMethodID)(JniEnv33 *,Ref33 *,const char *,const char *);
    Ref33 *(*CallObjectMethod)(JniEnv33 *,Ref33 *,Method33 *,...);
    void *r35_112[78];
    Method33 *(*GetStaticMethodID)(JniEnv33 *,Ref33 *,const char *,const char *);
    Ref33 *(*CallStaticObjectMethod)(JniEnv33 *,Ref33 *,Method33 *,...);
    void *r115_166[52];
    Ref33 *(*NewStringUTF)(JniEnv33 *,const char *);
    void *r168;
    const char *(*GetStringUTFChars)(JniEnv33 *,Ref33 *,unsigned char *);
    void (*ReleaseStringUTFChars)(JniEnv33 *,Ref33 *,const char *);
    void *r171_227[57];
    unsigned char (*ExceptionCheck)(JniEnv33 *);
} EnvFns33;
#define SLOT(m,n) _Static_assert(offsetof(EnvFns33,m)==(n)*sizeof(void *),"wrong JNI index " #m)
_Static_assert(sizeof(void*)==8,"phase33 requires AArch64");
SLOT(GetVersion,4);SLOT(FindClass,6);SLOT(ExceptionOccurred,15);
SLOT(ExceptionClear,17);SLOT(NewGlobalRef,21);SLOT(DeleteGlobalRef,22);
SLOT(IsSameObject,24);SLOT(GetObjectClass,31);SLOT(GetMethodID,33);
SLOT(CallObjectMethod,34);SLOT(GetStaticMethodID,113);
SLOT(CallStaticObjectMethod,114);SLOT(NewStringUTF,167);
SLOT(GetStringUTFChars,169);SLOT(ReleaseStringUTFChars,170);
SLOT(ExceptionCheck,228);

static Ref33 c_helper={CLASS,"org/cocos2dx/lib/Cocos2dxHelper"};
static Ref33 c_renderer={CLASS,"org/cocos2dx/lib/Cocos2dxRenderer"};
static Ref33 c_context={CLASS,"android/content/Context"};
static Ref33 c_loader={CLASS,"java/lang/ClassLoader"};
static Ref33 c_assets={CLASS,"android/content/res/AssetManager"};
static Ref33 c_audio={CLASS,"android/media/AudioManager"};
static Ref33 c_string={CLASS,"java/lang/String"};
static Ref33 o_context={OBJECT,"fixture_context"};
static Ref33 o_loader={OBJECT,"fixture_loader"};
static Ref33 o_assets={OBJECT,"fixture_assets"};
static Ref33 o_audio={OBJECT,"fixture_audio"};
static Ref33 s_path={STRING,"/sandbox/lenneth-phase33/saves"};
static Ref33 s_lang={STRING,"en"};
static Ref33 s_audio={STRING,"audio"};
static Ref33 s_renderer={STRING,"org.cocos2dx.lib.Cocos2dxRenderer"};
static Ref33 o_error={OBJECT,"fixture_exception"};
static Ref33 *const refs[]={
    &c_helper,&c_renderer,&c_context,&c_loader,&c_assets,&c_audio,&c_string,
    &o_context,&o_loader,&o_assets,&o_audio,&s_path,&s_lang,&s_audio,&s_renderer,&o_error
};
static Method33 methods[]={
    {M_PATH,"getCocos2dxWritablePath","()Ljava/lang/String;"},
    {M_LANG,"getCurrentLanguage","()Ljava/lang/String;"},
    {M_LOADER,"getClassLoader","()Ljava/lang/ClassLoader;"},
    {M_ASSETS,"getAssets","()Landroid/content/res/AssetManager;"},
    {M_SERVICE,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;"},
    {M_LOAD_CLASS,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;"}
};
static _Thread_local unsigned exception_pending,global_refs,released_strings;
static JniEnv33 table_pointer;
static int env_ok(JniEnv33 *e){return e==&table_pointer&&*e!=NULL;}
static int ref_ok(Ref33 *r){
    if(!r)return 1;
    for(size_t i=0;i<sizeof(refs)/sizeof(refs[0]);i++)if(r==refs[i])return 1;
    return 0;
}
static void deny(JniEnv33 *e){if(env_ok(e))exception_pending=1;}
static int get_version(JniEnv33 *e){return env_ok(e)?VM_1_6:0;}
static Ref33 *find_class(JniEnv33 *e,const char *name){
    if(!env_ok(e))return NULL;
    if(!name||strnlen(name,MAX_REF_NAME+1)>MAX_REF_NAME){deny(e);return NULL;}
    Ref33 *const classes[]={&c_helper,&c_renderer,&c_context,&c_loader,&c_assets,&c_audio,&c_string};
    for(size_t i=0;i<sizeof(classes)/sizeof(classes[0]);i++)
        if(!strcmp(name,classes[i]->name))return classes[i];
    deny(e);return NULL;
}
static Ref33 *exception_occurred(JniEnv33 *e){
    return env_ok(e)&&exception_pending?&o_error:NULL;
}
static void exception_clear(JniEnv33 *e){if(env_ok(e))exception_pending=0;}
static unsigned char exception_check(JniEnv33 *e){return env_ok(e)&&exception_pending?1:0;}
static Ref33 *new_global(JniEnv33 *e,Ref33 *r){
    if(!env_ok(e)||!ref_ok(r)||!r){deny(e);return NULL;}
    global_refs++;return r; /* static mock handle, not ART ownership */
}
static void delete_global(JniEnv33 *e,Ref33 *r){
    if(!env_ok(e)||!ref_ok(r)||!r||!global_refs){deny(e);return;}
    global_refs--;
}
static unsigned char same_object(JniEnv33 *e,Ref33 *a,Ref33 *b){
    if(!env_ok(e)||!ref_ok(a)||!ref_ok(b)){deny(e);return 0;}
    return a==b;
}
static Ref33 *get_object_class(JniEnv33 *e,Ref33 *obj){
    if(!env_ok(e))return NULL;
    if(obj==&o_context)return &c_context;
    if(obj==&o_loader)return &c_loader;
    if(obj==&o_assets)return &c_assets;
    if(obj==&o_audio)return &c_audio;
    if(obj==&s_path||obj==&s_lang||obj==&s_audio||obj==&s_renderer)return &c_string;
    deny(e);return NULL;
}
static Method33 *method_by(JniEnv33 *e,Ref33 *c,const char *n,const char *s,int is_static){
    if(!env_ok(e))return NULL;
    if(!n||!s||strnlen(n,MAX_REF_NAME+1)>MAX_REF_NAME||
       strnlen(s,MAX_REF_NAME+1)>MAX_REF_NAME){deny(e);return NULL;}
    unsigned start=is_static?0:2,end=is_static?2:6;
    for(unsigned i=start;i<end;i++){
        Ref33 *owner=i<2?&c_helper:i<5?&c_context:&c_loader;
        if(c==owner&&!strcmp(n,methods[i].name)&&!strcmp(s,methods[i].sig))
            return &methods[i];
    }
    deny(e);return NULL;
}
static Method33 *get_method(JniEnv33 *e,Ref33 *c,const char *n,const char *s){
    return method_by(e,c,n,s,0);
}
static Method33 *get_static_method(JniEnv33 *e,Ref33 *c,const char *n,const char *s){
    return method_by(e,c,n,s,1);
}
static Ref33 *call_static(JniEnv33 *e,Ref33 *c,Method33 *m,...){
    if(!env_ok(e))return NULL;
    if(c==&c_helper&&m==&methods[0])return &s_path;
    if(c==&c_helper&&m==&methods[1])return &s_lang;
    deny(e);return NULL;
}
static Ref33 *call_instance(JniEnv33 *e,Ref33 *obj,Method33 *m,...){
    if(!env_ok(e))return NULL;
    if(obj==&o_context&&m==&methods[2])return &o_loader;
    if(obj==&o_context&&m==&methods[3])return &o_assets;
    va_list args;
    if(obj==&o_context&&m==&methods[4]){
        va_start(args,m);Ref33 *a=va_arg(args,Ref33 *);va_end(args);
        if(a==&s_audio)return &o_audio;
    }
    if(obj==&o_loader&&m==&methods[5]){
        va_start(args,m);Ref33 *a=va_arg(args,Ref33 *);va_end(args);
        if(a==&s_renderer)return &c_renderer;
    }
    deny(e);return NULL;
}
static Ref33 *new_string(JniEnv33 *e,const char *str){
    if(!env_ok(e))return NULL;
    if(!str||strnlen(str,MAX_REF_NAME+1)>MAX_REF_NAME){deny(e);return NULL;}
    Ref33 *const strings[]={&s_path,&s_lang,&s_audio,&s_renderer};
    for(size_t i=0;i<sizeof(strings)/sizeof(strings[0]);i++)
        if(!strcmp(str,strings[i]->name))return strings[i];
    deny(e);return NULL;
}
static const char *get_utf(JniEnv33 *e,Ref33 *r,unsigned char *copy){
    if(!env_ok(e))return NULL;
    if(r==&s_path||r==&s_lang||r==&s_audio||r==&s_renderer){
        if(copy)*copy=0;return r->name;
    }
    deny(e);return NULL;
}
static void release_utf(JniEnv33 *e,Ref33 *r,const char *v){
    if(!env_ok(e))return;
    if((r==&s_path||r==&s_lang||r==&s_audio||r==&s_renderer)&&v==r->name)
        released_strings++;
    else deny(e);
}
static const EnvFns33 functions={
    .GetVersion=get_version,.FindClass=find_class,
    .ExceptionOccurred=exception_occurred,.ExceptionClear=exception_clear,
    .NewGlobalRef=new_global,.DeleteGlobalRef=delete_global,
    .IsSameObject=same_object,.GetObjectClass=get_object_class,
    .GetMethodID=get_method,.CallObjectMethod=call_instance,
    .GetStaticMethodID=get_static_method,.CallStaticObjectMethod=call_static,
    .NewStringUTF=new_string,.GetStringUTFChars=get_utf,
    .ReleaseStringUTFChars=release_utf,.ExceptionCheck=exception_check
};

/* Fixture-only in-memory asset. No real game CPK or APK is opened. */
static int safe_relative(const char *p){
    if(!p||!*p||strlen(p)>96||*p=='/')return 0;
    const char *seg=p;
    for(const char *s=p;;s++){
        unsigned char c=(unsigned char)*s;
        if(c=='/'||!c){
            size_t n=(size_t)(s-seg);
            if(n==0||(n==1&&seg[0]=='.')||
               (n==2&&seg[0]=='.'&&seg[1]=='.'))return 0;
            if(!c)break;
            seg=s+1;
        }else if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                    (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
    }
    return 1;
}
static int fixture_asset(Ref33 *manager,const char *path,char *out,size_t size){
    static const char payload[]="LennethPhase33Only";
    if(manager!=&o_assets||!safe_relative(path)||
       strcmp(path,"config/fixture.txt")||size<sizeof(payload)||!out)return 0;
    memcpy(out,payload,sizeof(payload));return 1;
}
static void report(FILE *f,const char *gate,const char *status,const char *detail){
    fprintf(f,"fixture\t%s\t%s\t%s\n",gate,status,detail);
}
int lenneth_jni_services33_selftest(void){
    table_pointer=&functions;
    JniEnv33 *e=&table_pointer;
    exception_pending=0;global_refs=0;released_strings=0;
    int ok=(*e)->GetVersion(e)==VM_1_6;
    Ref33 *helper=(*e)->FindClass(e,"org/cocos2dx/lib/Cocos2dxHelper");
    Ref33 *context=(*e)->FindClass(e,"android/content/Context");
    Ref33 *loader=(*e)->FindClass(e,"java/lang/ClassLoader");
    ok=ok&&helper==&c_helper&&context==&c_context&&loader==&c_loader;
    Method33 *path=(*e)->GetStaticMethodID(e,helper,
        "getCocos2dxWritablePath","()Ljava/lang/String;");
    Method33 *lang=(*e)->GetStaticMethodID(e,helper,
        "getCurrentLanguage","()Ljava/lang/String;");
    Ref33 *path_result=(*e)->CallStaticObjectMethod(e,helper,path);
    Ref33 *lang_result=(*e)->CallStaticObjectMethod(e,helper,lang);
    ok=ok&&path==&methods[0]&&lang==&methods[1]&&
       path_result==&s_path&&lang_result==&s_lang;
    unsigned char copy=1;
    const char *bytes=(*e)->GetStringUTFChars(e,path_result,&copy);
    ok=ok&&bytes&&!strcmp(bytes,"/sandbox/lenneth-phase33/saves")&&copy==0;
    (*e)->ReleaseStringUTFChars(e,path_result,bytes);

    Method33 *get_loader=(*e)->GetMethodID(e,context,
        "getClassLoader","()Ljava/lang/ClassLoader;");
    Method33 *get_assets=(*e)->GetMethodID(e,context,
        "getAssets","()Landroid/content/res/AssetManager;");
    Method33 *get_service=(*e)->GetMethodID(e,context,
        "getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
    Ref33 *got_loader=(*e)->CallObjectMethod(e,&o_context,get_loader);
    Ref33 *got_assets=(*e)->CallObjectMethod(e,&o_context,get_assets);
    Ref33 *got_audio=(*e)->CallObjectMethod(e,&o_context,get_service,&s_audio);
    ok=ok&&got_loader==&o_loader&&got_assets==&o_assets&&
       got_audio==&o_audio&&
       (*e)->GetObjectClass(e,got_assets)==&c_assets;

    Method33 *load_class=(*e)->GetMethodID(e,loader,
        "loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
    Ref33 *renderer=(*e)->CallObjectMethod(e,got_loader,load_class,&s_renderer);
    ok=ok&&renderer==&c_renderer;
    Ref33 *global=(*e)->NewGlobalRef(e,got_assets);
    ok=ok&&global==got_assets&&(*e)->IsSameObject(e,global,got_assets);
    (*e)->DeleteGlobalRef(e,global);
    ok=ok&&global_refs==0&&released_strings==1;
    char buf[64]={0};
    ok=ok&&fixture_asset(got_assets,"config/fixture.txt",buf,sizeof(buf))&&
       !strcmp(buf,"LennethPhase33Only");
    int traversal_denied=!fixture_asset(got_assets,"../config/fixture.txt",buf,sizeof(buf))&&
       !fixture_asset(got_assets,"/etc/passwd",buf,sizeof(buf))&&
       !fixture_asset(got_assets,"config\\fixture.txt",buf,sizeof(buf))&&
       !fixture_asset(got_assets,"config//fixture.txt",buf,sizeof(buf))&&
       !fixture_asset(got_assets,"config/./fixture.txt",buf,sizeof(buf));
    ok=ok&&traversal_denied;
    int unknown_denied=!(*e)->GetMethodID(e,context,
        "getFilesDir","()Ljava/io/File;")&&
        (*e)->ExceptionCheck(e)&&(*e)->ExceptionOccurred(e)==&o_error;
    (*e)->ExceptionClear(e);
    unknown_denied=unknown_denied&&!(*e)->FindClass(e,"android/secret/Other")&&
        (*e)->ExceptionCheck(e);
    (*e)->ExceptionClear(e);
    unknown_denied=unknown_denied&&
        !(*e)->CallObjectMethod(e,&o_context,get_service,&s_renderer)&&
        (*e)->ExceptionCheck(e);
    (*e)->ExceptionClear(e);
    ok=ok&&unknown_denied&&!(*e)->ExceptionCheck(e);
    FILE *f=fopen("lenneth-jni-services33.tsv","w");
    if(!f)ok=0;
    if(f){
        fputs("component\tgate\tstatus\tdetail\n",f);
        report(f,"JNI_SERVICE_GRAPH",ok?"PASS":"FAIL","synthetic_Context_ClassLoader_CocosHelper_only");
        report(f,"JNI_METHOD_DISPATCH",ok?"PASS":"FAIL","fixture_signatures_and_handles_only");
        report(f,"SANDBOX_ASSET_FIXTURE",ok?"PASS":"FAIL","in_memory_asset_path_validation");
        report(f,"TRAVERSAL_DENIAL",traversal_denied?"PASS":"FAIL","no_guest_path_escape");
        report(f,"UNIMPLEMENTED_DENIAL",unknown_denied?"PASS":"FAIL","unknown_method_class_service_rejected");
        fputs("runtime\tREAL_ART_JAVA_VM\tBLOCKED\tno_actual_jvm_or_android_context\n",f);
        fputs("runtime\tREAL_ASSET_MANAGER\tBLOCKED\tno_game_assets_opened\n",f);
        fputs("runtime\tGAME_METHOD_SIGNATURES\tUNVERIFIED\tall_signatures_are_fixture_only\n",f);
        fputs("runtime\tCOMMERCIAL_ENGINE\tBLOCKED\tno_game_constructors_or_JNI_called\n",f);
        if(ferror(f)||fclose(f)!=0)ok=0;
    }
    printf("[lenneth phase33] JNI_SERVICES=%s service_graph=FIXTURE "
           "classloader=FIXTURE assets=IN_MEMORY_ONLY traversal=%s "
           "unknown_requests=%s game_signatures=UNVERIFIED engine_executed=NO "
           "report=lenneth-jni-services33.tsv\n",
           ok?"PASS":"FAIL",traversal_denied?"DENIED":"FAIL",
           unknown_denied?"DENIED":"FAIL");
    return ok?0:33;
}
