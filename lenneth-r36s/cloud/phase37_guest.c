/*
 * Phase37 MIT: independent AArch64 JNI lifecycle guest using names and
 * descriptors observed in the user's DEX metadata and libMyGame.so export
 * inventory. NO game code, assets, ART, graphics or APK execution.
 * Java_ functions are stand-ins: this is not the commercial engine.
 */
#include <jni.h>
#include <stdint.h>
#include <string.h>

enum {
    S_ONLOAD=1, S_CONTEXT=2, S_INITIALIZED=3, S_PAUSED=4,
    S_WIDTH=5, S_HEIGHT=6, S_FRAMES=7, S_REJECT_CONTEXT=8,
    S_REJECT_INIT=9, S_BLOCK_RENDER=10, S_REJECT_SURFACE=11
};
typedef struct {
    int onload, context, initialized, paused, width, height, frames;
    int bad_context, bad_init, blocked_render, bad_surface;
} Lifecycle37;
static Lifecycle37 state;
static jobject context_ref, asset_ref;

static int valid_env(JNIEnv *env) {
    return env && (*env) && (*env)->GetVersion &&
           (*env)->GetVersion(env) == JNI_VERSION_1_6 &&
           !(*env)->ExceptionCheck(env);
}
static int type_is(JNIEnv *env,jobject object,const char *class_name) {
    if (!object || !valid_env(env)) return 0;
    jclass actual=(*env)->GetObjectClass(env,object);
    if (!actual || (*env)->ExceptionCheck(env)) return 0;
    jclass expected=(*env)->FindClass(env,class_name);
    if (!expected || (*env)->ExceptionCheck(env)) return 0;
    return (*env)->IsSameObject(env,actual,expected)==JNI_TRUE &&
           !(*env)->ExceptionCheck(env);
}
JNIEXPORT jint JNICALL lenneth_phase37_fixture_marker(void) {
    return (jint)0x37374a4e;
}
JNIEXPORT jint JNICALL lenneth_phase37_fixture_state(jint field) {
    switch(field) {
        case S_ONLOAD:return state.onload;
        case S_CONTEXT:return state.context;
        case S_INITIALIZED:return state.initialized;
        case S_PAUSED:return state.paused;
        case S_WIDTH:return state.width;
        case S_HEIGHT:return state.height;
        case S_FRAMES:return state.frames;
        case S_REJECT_CONTEXT:return state.bad_context;
        case S_REJECT_INIT:return state.bad_init;
        case S_BLOCK_RENDER:return state.blocked_render;
        case S_REJECT_SURFACE:return state.bad_surface;
        default:return -1;
    }
}
JNIEXPORT void JNICALL lenneth_phase37_fixture_reset(JNIEnv *env) {
    if (valid_env(env)) {
        if(context_ref)(*env)->DeleteGlobalRef(env,context_ref);
        if(asset_ref)(*env)->DeleteGlobalRef(env,asset_ref);
        context_ref=NULL;asset_ref=NULL;
        memset(&state,0,sizeof(state));
    }
}
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm,void *reserved) {
    (void)reserved;
    JNIEnv *env=NULL;
    if (!vm || (*vm)->GetEnv(vm,(void **)&env,JNI_VERSION_1_6)!=JNI_OK ||
        !valid_env(env) || state.onload) return JNI_ERR;
    jclass helper=(*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxHelper");
    jclass renderer=(*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxRenderer");
    if(!helper || !renderer || (*env)->ExceptionCheck(env)) return JNI_ERR;
    state.onload=1;
    return JNI_VERSION_1_6;
}

/* DEX signature:
   (Landroid/content/Context;Landroid/content/res/AssetManager;)V */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext(
    JNIEnv *env,jobject receiver,jobject context,jobject assets) {
    (void)receiver;
    if (!state.onload || state.context ||
        !type_is(env,context,"android/content/Context") ||
        !type_is(env,assets,"android/content/res/AssetManager")) {
        state.bad_context++;return;
    }
    jobject c=(*env)->NewGlobalRef(env,context);
    if(!c || (*env)->ExceptionCheck(env)) {
        state.bad_context++;return;
    }
    jobject a=(*env)->NewGlobalRef(env,assets);
    if(!a || (*env)->ExceptionCheck(env)) {
        (*env)->DeleteGlobalRef(env,c);
        state.bad_context++;return;
    }
    context_ref=c;asset_ref=a;state.context=1;
}
/* DEX signature (II)V */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit(
    JNIEnv *env,jobject receiver,jint width,jint height) {
    (void)receiver;
    if(!state.onload||!state.context||state.initialized||
       !valid_env(env)||width<=0||height<=0||width>4096||height>4096) {
        state.bad_init++;return;
    }
    state.width=width;state.height=height;state.initialized=1;
}
/* DEX signature ()V */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender(JNIEnv *env,jobject receiver) {
    (void)receiver;
    if(!state.initialized||state.paused||!valid_env(env)) {
        state.blocked_render++;return;
    }
    state.frames++;
}
/* DEX signature ()V */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause(JNIEnv *env,jobject receiver) {
    (void)receiver;
    if(state.initialized && valid_env(env))state.paused=1;
}
/* DEX signature ()V */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume(JNIEnv *env,jobject receiver) {
    (void)receiver;
    if(state.initialized && valid_env(env))state.paused=0;
}
/* DEX signature (II)V */
JNIEXPORT void JNICALL
Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnSurfaceChanged(
    JNIEnv *env,jobject receiver,jint width,jint height) {
    (void)receiver;
    if(!state.initialized||!valid_env(env)||
       width<=0||height<=0||width>4096||height>4096) {
        state.bad_surface++;return;
    }
    state.width=width;state.height=height;
}
