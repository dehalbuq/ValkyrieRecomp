/*
 * Phase37 MIT: JNI 1.6 class/asset lifecycle harness using real DEX-derived
 * public entry-point identifiers, but a NEW independently authored test ELF.
 * Fail closed on missing exports. No dynamic loading of game libraries.
 */
#define _GNU_SOURCE
#include <jni.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { const char *name; int kind; } Mock37;
static Mock37 cls_helper={"org/cocos2dx/lib/Cocos2dxHelper",1};
static Mock37 cls_renderer={"org/cocos2dx/lib/Cocos2dxRenderer",1};
static Mock37 cls_context={"android/content/Context",1};
static Mock37 cls_assets={"android/content/res/AssetManager",1};
static Mock37 obj_context={"fixture_context",2};
static Mock37 obj_assets={"fixture_assets",2};
static Mock37 obj_invalid={"fixture_wrong_type",2};
static Mock37 obj_exception={"fixture_exception",3};
static int exception_pending,global_refs,global_releases,denied;

static const struct JNINativeInterface_ env_api;
static const struct JNIInvokeInterface_ vm_api;
static JNIEnv env_ptr=&env_api;
static JavaVM vm_ptr=&vm_api;

static int env_ok(JNIEnv *env){ return env==&env_ptr && *env==&env_api; }
static int vm_ok(JavaVM *vm){return vm==&vm_ptr && *vm==&vm_api;}
static void deny(JNIEnv *env){if(env_ok(env)){exception_pending=1;denied++;}}
static jint JNICALL get_version(JNIEnv *env){
    return env_ok(env)?JNI_VERSION_1_6:JNI_ERR;
}
static jboolean JNICALL exception_check(JNIEnv *env){
    return env_ok(env) && exception_pending ? JNI_TRUE : JNI_FALSE;
}
static jthrowable JNICALL exception_occurred(JNIEnv *env){
    return env_ok(env) && exception_pending?(jthrowable)&obj_exception:NULL;
}
static void JNICALL exception_clear(JNIEnv *env){
    if(env_ok(env))exception_pending=0;
}
static jclass JNICALL find_class(JNIEnv *env,const char *name){
    if(!env_ok(env)||!name)return NULL;
    Mock37 *classes[]={&cls_helper,&cls_renderer,&cls_context,&cls_assets};
    for(unsigned i=0;i<4;i++)
        if(!strcmp(name,classes[i]->name))return (jclass)classes[i];
    deny(env);return NULL;
}
static jclass JNICALL get_object_class(JNIEnv *env,jobject obj){
    if(!env_ok(env))return NULL;
    if(obj==(jobject)&obj_context)return (jclass)&cls_context;
    if(obj==(jobject)&obj_assets)return (jclass)&cls_assets;
    deny(env);return NULL;
}
static jboolean JNICALL is_same(JNIEnv *env,jobject a,jobject b){
    if(!env_ok(env))return JNI_FALSE;
    return a==b ? JNI_TRUE : JNI_FALSE;
}
static jobject JNICALL new_global(JNIEnv *env,jobject ref){
    if(env_ok(env) && (ref==(jobject)&obj_context || ref==(jobject)&obj_assets)){
        global_refs++;return ref;
    }
    deny(env);return NULL;
}
static void JNICALL delete_global(JNIEnv *env,jobject ref){
    if(env_ok(env) && global_refs>0 &&
       (ref==(jobject)&obj_context || ref==(jobject)&obj_assets)){
        global_refs--;global_releases++;return;
    }
    deny(env);
}
static jint JNICALL vm_getenv(JavaVM *vm,void **out,jint version){
    if(!vm_ok(vm)||!out)return JNI_ERR;
    *out=NULL;
    if(version!=JNI_VERSION_1_6)return JNI_EVERSION;
    *out=&env_ptr;return JNI_OK;
}
static const struct JNINativeInterface_ env_api = {
    .GetVersion=get_version,.FindClass=find_class,.GetObjectClass=get_object_class,
    .IsSameObject=is_same,.ExceptionCheck=exception_check,
    .ExceptionOccurred=exception_occurred,.ExceptionClear=exception_clear,
    .NewGlobalRef=new_global,.DeleteGlobalRef=delete_global
};
static const struct JNIInvokeInterface_ vm_api = {
    .GetEnv=vm_getenv
};
typedef jint (JNICALL *OnLoad)(JavaVM *,void *);
typedef jint (JNICALL *StateFn)(jint);
typedef jint (JNICALL *MarkerFn)(void);
typedef void (JNICALL *ResetFn)(JNIEnv *);
typedef void (JNICALL *SetContextFn)(JNIEnv *,jobject,jobject,jobject);
typedef void (JNICALL *InitFn)(JNIEnv *,jobject,jint,jint);
typedef void (JNICALL *RenderFn)(JNIEnv *,jobject);
typedef void (JNICALL *SurfaceFn)(JNIEnv *,jobject,jint,jint);

#define REQUIRED(sym,var) do {\
    void *raw=dlsym(lib,sym); \
    if(!raw || sizeof(raw)!=sizeof(var)){ \
        fprintf(stderr,"[phase37] missing fixture export %s\n",sym); \
        dlclose(lib);return 37; \
    } \
    memcpy(&(var),&raw,sizeof(raw)); \
}while(0)
#define CHECK(expr) do {\
    if(!(expr)){fprintf(stderr,"[phase37] assertion failed: %s (%d)\n", \
             #expr,__LINE__);ok=0;goto report;} \
}while(0)
#define STATE(n) state(n)

int main(void) {
    int ok=1, negative=0, sequences=0;
    /* Only our independently authored, marker-verified CI fixture. */
    void *lib=dlopen("/results/lenneth-phase37-fixture.so",RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"[phase37] no test ELF: %s\n",dlerror());return 37;}
    MarkerFn marker=NULL;StateFn state=NULL;ResetFn reset=NULL;
    OnLoad onload=NULL;SetContextFn set_context=NULL;InitFn init=NULL;
    RenderFn render=NULL,pause_game=NULL,resume_game=NULL;
    SurfaceFn surface=NULL;
    REQUIRED("lenneth_phase37_fixture_marker",marker);
    REQUIRED("lenneth_phase37_fixture_state",state);
    REQUIRED("lenneth_phase37_fixture_reset",reset);
    REQUIRED("JNI_OnLoad",onload);
    REQUIRED("Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext",set_context);
    REQUIRED("Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit",init);
    REQUIRED("Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender",render);
    REQUIRED("Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause",pause_game);
    REQUIRED("Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume",resume_game);
    REQUIRED("Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnSurfaceChanged",surface);
    CHECK(marker()==0x37374a4e);
    CHECK(STATE(1)==0);
    render(&env_ptr,(jobject)&cls_renderer);
    init(&env_ptr,(jobject)&cls_renderer,640,480);
    set_context(&env_ptr,(jobject)&cls_helper,(jobject)&obj_context,
                (jobject)&obj_assets);
    CHECK(STATE(10)==1 && STATE(9)==1 && STATE(8)==1);
    CHECK(STATE(2)==0 && global_refs==0);
    CHECK(onload(NULL,NULL)==JNI_ERR && STATE(1)==0);
    CHECK(onload(&vm_ptr,NULL)==JNI_VERSION_1_6);
    CHECK(STATE(1)==1 && onload(&vm_ptr,NULL)==JNI_ERR);

    set_context(&env_ptr,(jobject)&cls_helper,(jobject)&obj_context,
                (jobject)&obj_invalid);
    CHECK(STATE(8)==2 && STATE(2)==0 && global_refs==0);
    CHECK(exception_pending && env_ptr->ExceptionCheck(&env_ptr));
    env_ptr->ExceptionClear(&env_ptr);
    set_context(&env_ptr,(jobject)&cls_helper,(jobject)&obj_context,
                (jobject)&obj_assets);
    CHECK(STATE(2)==1 && global_refs==2 && !exception_pending);
    set_context(&env_ptr,(jobject)&cls_helper,(jobject)&obj_context,
                (jobject)&obj_assets);
    CHECK(STATE(8)==3 && global_refs==2);
    init(&env_ptr,(jobject)&cls_renderer,0,480);
    CHECK(STATE(9)==2 && STATE(3)==0);
    render(&env_ptr,(jobject)&cls_renderer);
    CHECK(STATE(10)==2 && STATE(7)==0);
    negative=1;

    init(&env_ptr,(jobject)&cls_renderer,640,480);
    CHECK(STATE(3)==1 && STATE(5)==640 && STATE(6)==480);
    render(&env_ptr,(jobject)&cls_renderer);
    CHECK(STATE(7)==1);
    pause_game(&env_ptr,(jobject)&cls_renderer);
    render(&env_ptr,(jobject)&cls_renderer);
    CHECK(STATE(4)==1 && STATE(7)==1 && STATE(10)==3);
    surface(&env_ptr,(jobject)&cls_renderer,0,200);
    CHECK(STATE(11)==1 && STATE(5)==640);
    surface(&env_ptr,(jobject)&cls_renderer,320,240);
    CHECK(STATE(5)==320 && STATE(6)==240);
    resume_game(&env_ptr,(jobject)&cls_renderer);
    render(&env_ptr,(jobject)&cls_renderer);
    CHECK(STATE(4)==0 && STATE(7)==2);
    CHECK(global_refs==2 && denied==1);
    reset(&env_ptr);
    CHECK(global_refs==0 && global_releases==2 &&
          STATE(1)==0 && STATE(2)==0 && STATE(7)==0);
    render(&env_ptr,(jobject)&cls_renderer);
    CHECK(STATE(10)==1);
    sequences=1;
report:
    {
        FILE *f=fopen("lenneth-jni-lifecycle37.tsv","w");
        if(!f)ok=0;
        else {
            fprintf(f,"component\tgate\tstatus\tdetail\n");
            fprintf(f,"fixture\tJNI_REAL_NAMED_EXPORTS\t%s\t6_Cocos_method_names_from_DEX_ELF_audit\n",ok?"PASS":"FAIL");
            fprintf(f,"fixture\tJNI_ONLOAD_1_6\t%s\tmock_JavaVM_in_independent_guest\n",ok?"PASS":"FAIL");
            fprintf(f,"fixture\tCONTEXT_ASSET_GLOBAL_REFS\t%s\tmanaged_context_and_asset_handles_only\n",ok?"PASS":"FAIL");
            fprintf(f,"fixture\tINIT_RENDER_PAUSE_RESUME\t%s\t640x480_then_320x240_mock_frames\n",sequences?"PASS":"FAIL");
            fprintf(f,"safety\tINVALID_ORDER_AND_ASSET\t%s\tdenied_preinit_wrong_type_duplicate_pause\n",negative?"PASS":"FAIL");
            fprintf(f,"safety\tNO_REF_LEAK_AFTER_RESET\t%s\tglobal_refs_released\n",sequences?"PASS":"FAIL");
            fprintf(f,"runtime\tACTUAL_ART_ANDROID_CONTEXT\tBLOCKED\tsynthetic_pointer_handles_only\n");
            fprintf(f,"runtime\tREAL_APK_ASSETS_AND_GRAPHICS\tBLOCKED\tno_game_assets_no_Mali_G31_render\n");
            fprintf(f,"runtime\tCOMMERCIAL_CONSTRUCTORS_AND_JNI\tBLOCKED\tgame_binary_not_loaded\n");
            if(ferror(f)||fclose(f)!=0)ok=0;
        }
    }
    printf("PHASE37_JNI_LIFECYCLE=%s onload=%s context=%s frames=%s "
           "negative=%s globals_released=%d game_executed=NO\n",
           ok?"PASS":"FAIL",sequences?"PASS":"FAIL",
           sequences?"PASS":"FAIL",sequences?"2":"0",
           negative?"PASS":"FAIL",global_releases);
    dlclose(lib);
    return ok?0:37;
}
