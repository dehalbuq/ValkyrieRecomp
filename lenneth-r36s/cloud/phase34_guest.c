/*
 * Phase34 deliberately synthetic JNI guest. Compiled as an independent
 * AArch64 ELF against a real JNI 1.6 header; never commercial game code.
 * The exported class/method names here are fixtures, NOT verified APK ABI.
 */
#include <jni.h>
#include <string.h>

static unsigned initialized, frames;
static void JNICALL fixture_init(JNIEnv *env, jobject obj, jint width, jint height) {
    (void)env; (void)obj;
    if (width == 640 && height == 480) initialized = 1;
}
static void JNICALL fixture_render(JNIEnv *env, jobject obj) {
    (void)env; (void)obj;
    if (initialized) frames++;
}
JNIEXPORT jint JNICALL lenneth_phase34_fixture_marker(void) {
    return 0x34344a4e;
}
JNIEXPORT jint JNICALL lenneth_phase34_fixture_state(void) {
    return (jint)(initialized * 10u + frames);
}
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)reserved;
    JNIEnv *env = NULL;
    if (!vm || (*vm)->GetEnv(vm,(void **)&env,JNI_VERSION_1_6)!=JNI_OK ||
        !env || (*env)->GetVersion(env)!=JNI_VERSION_1_6) return JNI_ERR;

    jclass renderer=(*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxRenderer");
    if (!renderer || (*env)->ExceptionCheck(env)) return JNI_ERR;
    JNINativeMethod registration[2] = {
        {"nativeFixtureInit","(II)V",(void *)fixture_init},
        {"nativeFixtureRender","()V",(void *)fixture_render}
    };
    if ((*env)->RegisterNatives(env,renderer,registration,2)!=JNI_OK ||
        (*env)->ExceptionCheck(env)) return JNI_ERR;

    jclass helper=(*env)->FindClass(env,"org/cocos2dx/lib/Cocos2dxHelper");
    if (!helper || (*env)->ExceptionCheck(env)) return JNI_ERR;
    jmethodID path=(*env)->GetStaticMethodID(env,helper,
        "getCocos2dxWritablePath","()Ljava/lang/String;");
    if (!path || (*env)->ExceptionCheck(env)) return JNI_ERR;
    jstring s=(jstring)(*env)->CallStaticObjectMethod(env,helper,path);
    if (!s || (*env)->ExceptionCheck(env)) return JNI_ERR;
    const char *chars=(*env)->GetStringUTFChars(env,s,NULL);
    if (!chars || strcmp(chars,"/sandbox/phase34/saves")) return JNI_ERR;
    (*env)->ReleaseStringUTFChars(env,s,chars);
    if ((*env)->ExceptionCheck(env)) return JNI_ERR;
    return JNI_VERSION_1_6;
}
