/*
 * Synthetic AArch64 game-like JNI fixture only. MIT.
 * Uses a mock shared state and a tiny JNI table subset. Contains no
 * proprietary game code, assets, third-party libraries or media.
 */
#include <stdint.h>
__attribute__((visibility("default")))
const char lenneth_phase30_mock_marker[] =
    "LENNETH_PHASE30_SYNTHETIC_LIFECYCLE_ONLY_2026";

extern volatile int phase30_shared_progress;

__attribute__((constructor(101)))
static void phase30_game_ctor_first(void) {
    if(phase30_shared_progress == 10)phase30_shared_progress = 11;
}
__attribute__((constructor(102)))
static void phase30_game_ctor_second(void) {
    if(phase30_shared_progress == 11)phase30_shared_progress = 13;
}

/* C JNI pointer-to-table layout: JavaVM GetEnv has index 6 and JNIEnv
 * GetVersion has index 4. Native fixture does not resolve Android classes. */
typedef int (*GetEnvFn)(void *, void **, int);
typedef int (*GetVersionFn)(void *);

__attribute__((visibility("default")))
int JNI_OnLoad(void *vm,void *reserved){
    if(!vm || reserved || phase30_shared_progress != 13)return -1;
    void **vmtable=*(void ***)vm;
    if(!vmtable || !vmtable[6])return -1;
    GetEnvFn get_env=(GetEnvFn)(uintptr_t)vmtable[6];
    void *env=0;
    if(get_env(vm,&env,0x00010006)!=0||!env)return -1;
    void **envtable=*(void ***)env;
    if(!envtable || !envtable[4])return -1;
    GetVersionFn get_version=(GetVersionFn)(uintptr_t)envtable[4];
    if(get_version(env)!=0x00010006)return -1;
    phase30_shared_progress=14;
    return 0x00010006;
}
__attribute__((visibility("default")))
void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit(
    void *env,void *renderer,int width,int height){
    if(env && !renderer && width==640 && height==480 &&
       phase30_shared_progress==14)
        phase30_shared_progress=15;
}
__attribute__((visibility("default")))
void Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender(
    void *env,void *renderer){
    if(env && !renderer && phase30_shared_progress==15)
        phase30_shared_progress=16;
}
__attribute__((visibility("default")))
int lenneth_phase30_get_stage(void){
    return phase30_shared_progress;
}
