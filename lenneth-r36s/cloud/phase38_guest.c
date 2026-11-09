/*
 * Phase38 synthetic AArch64 Cocos JNI guest: noncommercial test fixture.
 * Method names match offline DEX/ELF metadata; behavior is independently
 * written, and not an implementation of the Lenneth game engine.
 */
#include <jni.h>
#include <stdint.h>
#include <string.h>

typedef struct AAsset AAsset;
typedef struct AAssetManager AAssetManager;
extern AAssetManager *AAssetManager_fromJava(JNIEnv *, jobject);
extern AAsset *AAssetManager_open(AAssetManager *, const char *, int);
extern int AAsset_read(AAsset *,void *,size_t);
extern int64_t AAsset_getLength64(AAsset *);
extern int64_t AAsset_seek64(AAsset *,int64_t,int);
extern void AAsset_close(AAsset *);

enum { BOOT_MAGIC=0x38384c45 };
enum {
  ONLOAD=1, CONTEXT=2, INITIALIZED=3, FRAMES=4, PAUSED=5,
  INIT_DENIED=6, BAD_CONTEXT=7, ASSET_FAILURE=8, FRAME_BLOCKED=9
};
static struct {
    int onload, context, initialized, frames, paused, init_denied;
    int bad_context, asset_failure, frame_blocked;
} state;
static jobject context_global,assets_global;

static int valid_env(JNIEnv *env) {
    return env && (*env) && (*env)->GetVersion &&
       (*env)->GetVersion(env)==JNI_VERSION_1_6 &&
       !(*env)->ExceptionCheck(env);
}
static int is_type(JNIEnv *env,jobject obj,const char *klass) {
    if(!valid_env(env)||!obj)return 0;
    jclass actual=(*env)->GetObjectClass(env,obj);
    jclass expected=(*env)->FindClass(env,klass);
    return actual && expected && !(*env)->ExceptionCheck(env) &&
           (*env)->IsSameObject(env,actual,expected)==JNI_TRUE;
}
JNIEXPORT jint JNICALL lenneth_phase38_fixture_marker(void) { return BOOT_MAGIC; }
JNIEXPORT jint JNICALL lenneth_phase38_fixture_state(jint which) {
    switch(which) {
      case ONLOAD:return state.onload;
      case CONTEXT:return state.context;
      case INITIALIZED:return state.initialized;
      case FRAMES:return state.frames;
      case PAUSED:return state.paused;
      case INIT_DENIED:return state.init_denied;
      case BAD_CONTEXT:return state.bad_context;
      case ASSET_FAILURE:return state.asset_failure;
      case FRAME_BLOCKED:return state.frame_blocked;
      default:return -1;
    }
}
JNIEXPORT void JNICALL lenneth_phase38_fixture_reset(JNIEnv *env) {
    if(!valid_env(env))return;
    if(context_global)(*env)->DeleteGlobalRef(env,context_global);
    if(assets_global)(*env)->DeleteGlobalRef(env,assets_global);
    context_global=NULL;assets_global=NULL;
    memset(&state,0,sizeof(state));
}
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm,void *unused) {
    (void)unused;
    JNIEnv *env=NULL;
    if(!vm || state.onload ||
       (*vm)->GetEnv(vm,(void **)&env,JNI_VERSION_1_6)!=JNI_OK ||
       !valid_env(env))return JNI_ERR;
    if(!(*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxRenderer") ||
       !(*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxHelper"))return JNI_ERR;
    state.onload=1;
    return JNI_VERSION_1_6;
}
/* Signature verified by Phase35 DEX: (Context;AssetManager;)V. */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext(
    JNIEnv *env,jobject self,jobject context,jobject assets) {
    (void)self;
    if(!state.onload || state.context ||
       !is_type(env,context,"android/content/Context") ||
       !is_type(env,assets,"android/content/res/AssetManager")) {
        state.bad_context++;return;
    }
    jobject c=(*env)->NewGlobalRef(env,context);
    if(!c||(*env)->ExceptionCheck(env)){state.bad_context++;return;}
    jobject a=(*env)->NewGlobalRef(env,assets);
    if(!a||(*env)->ExceptionCheck(env)){
        (*env)->DeleteGlobalRef(env,c);
        state.bad_context++;return;
    }
    context_global=c;assets_global=a;state.context=1;
}
/* NDK AAssetManager is called only AFTER Java context/asset handoff. */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit(
    JNIEnv *env,jobject self,jint width,jint height) {
    (void)self;
    if(!valid_env(env)||!state.onload||!state.context||state.initialized ||
       width<=0||height<=0||width>4096||height>4096) {
        state.init_denied++;return;
    }
    AAssetManager *mgr=AAssetManager_fromJava(env,assets_global);
    if(!mgr){state.asset_failure++;return;}
    AAsset *asset=AAssetManager_open(mgr,"config/boot.txt",2);
    if(!asset){state.asset_failure++;return;}
    static const char marker[]="PHASE38_BOOT_READY\n";
    char actual[sizeof(marker)]={0};
    int verified=AAsset_getLength64(asset)==(int64_t)(sizeof(marker)-1) &&
        AAsset_read(asset,actual,5)==5 &&
        AAsset_read(asset,actual+5,(sizeof(marker)-1)-5)==(int)(sizeof(marker)-1-5) &&
        !memcmp(actual,marker,sizeof(marker)-1) &&
        AAsset_seek64(asset,0,0)==0;
    AAsset_close(asset);
    if(!verified){state.asset_failure++;return;}
    state.initialized=1;
}
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender(JNIEnv *env,jobject self) {
    (void)self;
    if(!valid_env(env)||!state.initialized||state.paused) {
        state.frame_blocked++;return;
    }
    state.frames++;
}
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause(JNIEnv *env,jobject self) {
    (void)self;
    if(valid_env(env)&&state.initialized)state.paused=1;
}
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume(JNIEnv *env,jobject self) {
    (void)self;
    if(valid_env(env)&&state.initialized)state.paused=0;
}
