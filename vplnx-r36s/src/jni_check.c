/* SPDX-License-Identifier: MIT
 * JNI C ABI fixture for Linux AArch64. Calls the LGPL/MIT-compatible
 * open-source shim only; does NOT load Android commercial native code.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jni_fake.h"
#include "assets_linux.h"

typedef int32_t jint;
typedef jint (*FnVersion)(void *);
typedef void *(*FnClass)(void *, const char *);
typedef void *(*FnID)(void *, void *, const char *, const char *);
typedef void *(*FnObject)(void *, void *, void *, ...);
typedef jint (*FnInt)(void *, void *, void *, ...);
typedef void *(*FnNewStr)(void *, const char *);
typedef const char *(*FnGetStr)(void *, void *, unsigned char *);
typedef void (*FnReleaseStr)(void *, void *, const char *);
typedef jint (*FnLength)(void *, void *);
typedef void *(*FnPriData)(void *, void *, unsigned char *);
typedef jint (*FnRegNatives)(void *, void *, void *, int);
typedef jint (*FnVMGetEnv)(void *, void **, jint);
typedef void (*FnLocalDelete)(void *, void *);
typedef jint (*FnPushLocal)(void *, jint);
typedef void *(*FnPopLocal)(void *, void *);
typedef void (*FnReleasePri)(void *, void *, void *, int);
typedef void *(*FnNewIntArray)(void *, int);
typedef void (*FnSetRegion)(void *, void *, int, int, const void *);

static void fail(const char *why) {
    fprintf(stderr,"VPLNX_JNI=FAIL reason=%s\n",why);
    exit(1);
}
int main(int argc, char **argv) {
    if (argc!=2 || vpl_assets_set_root(argv[1])!=0)
        fail("must supply test assets root");
    jni_init();
    if(!fake_env || !fake_vm) fail("nil VM or JNIEnv");

    void **env=*(void ***)fake_env;
    void **vm=*(void ***)fake_vm;
    if(!env || !vm) fail("bad JNI table pointers");

    FnVersion version=(FnVersion)env[4];
    FnClass find_class=(FnClass)env[6];
    FnID static_id=(FnID)env[113];
    FnObject object=(FnObject)env[114];
    FnInt integer=(FnInt)env[129];
    FnNewStr newstr=(FnNewStr)env[167];
    FnGetStr getstr=(FnGetStr)env[169];
    FnReleaseStr releasestr=(FnReleaseStr)env[170];
    FnLength length=(FnLength)env[171];
    FnPriData get_data=(FnPriData)env[188];
    FnRegNatives register_natives=(FnRegNatives)env[215];
    FnVMGetEnv get_vm_env=(FnVMGetEnv)vm[6];
    FnLocalDelete delete_ref=(FnLocalDelete)env[23];
    FnPushLocal push=(FnPushLocal)env[19];
    FnPopLocal pop=(FnPopLocal)env[20];
    FnReleasePri release_data=(FnReleasePri)env[196];
    FnNewIntArray new_int_array=(FnNewIntArray)env[179];
    FnSetRegion set_int_region=(FnSetRegion)env[211];
    if(!version || !find_class || !static_id || !object ||
       !integer || !get_vm_env || !get_data || !register_natives)
        fail("JNI method slot missing");
    if(version(fake_env)!=0x00010006) fail("JNI 1.6 version");
    void *attached=NULL;
    if(get_vm_env(fake_vm,&attached,0x00010006)!=0 || attached!=fake_env)
        fail("GetEnv ABI");
    void *clazz=find_class(fake_env,"org/cocos2dx/lib/Cocos2dxHelper");
    if(!clazz) fail("FindClass");
    void *method=static_id(fake_env,clazz,"getCocos2dxWritablePath","()Ljava/lang/String;");
    void *path=object(fake_env,clazz,method);
    if(!path) fail("writable path object");
    const char *path_text=getstr(fake_env,path,NULL);
    if(!path_text || !path_text[0]) fail("writable path string");
    releasestr(fake_env,path,path_text);
    delete_ref(fake_env,path);

    method=static_id(fake_env,clazz,"getDPI","()I");
    if(integer(fake_env,clazz,method)!=320) fail("integer method");
    method=static_id(fake_env,clazz,"isNetworkConnected","()Z");
    if(integer(fake_env,clazz,method)!=0) fail("network spoofing not disabled");
    method=static_id(fake_env,clazz,"checkPermission","(Ljava/lang/String;)Z");
    if(integer(fake_env,clazz,method)!=0) fail("permission spoofing not disabled");
    if(register_natives(fake_env,clazz,NULL,0)!=-1)
        fail("native registration falsely approved");

    if(push(fake_env,16)!=0) fail("PushLocalFrame");
    void *hello=newstr(fake_env,"Cocos2d-x");
    if(strcmp(getstr(fake_env,hello,NULL),"Cocos2d-x")!=0)
        fail("UTF8 string mismatch");
    if(pop(fake_env,NULL)!=NULL) fail("PopLocalFrame");

    void *a=new_int_array(fake_env,2);
    int32_t ints[]={42,7};
    set_int_region(fake_env,a,0,2,ints);
    if(length(fake_env,a)!=2) fail("int array length");
    int32_t *elements=get_data(fake_env,a,NULL);
    if(!elements || elements[0]!=42 || elements[1]!=7)
        fail("int array contents");
    release_data(fake_env,a,elements,0);
    delete_ref(fake_env,a);

    method=static_id(fake_env,clazz,"getObbAssetFileDescriptor","(Ljava/lang/String;)[J");
    void *name=newstr(fake_env,"sample.bin");
    void *range=object(fake_env,clazz,method,name);
    if(!range || length(fake_env,range)!=3)
        fail("AAsset file descriptor range");
    int64_t *elems=get_data(fake_env,range,NULL);
    if(!elems || elems[0]<0 || elems[1]!=0 || elems[2]!=8)
        fail("bad 64bit asset descriptor");
    char buf[9]={0};
    if(read((int)elems[0],buf,8)!=8 ||
       strcmp(buf,"SYNTHJNI")!=0)
        fail("asset data read");
    close((int)elems[0]);
    release_data(fake_env,range,elems,0);
    delete_ref(fake_env,range);
    delete_ref(fake_env,name);

    vpl_assets_shutdown();
    puts("VPLNX_JNI_ABI=PASS jni_version=1.6 GetEnv=PASS local_refs=PASS");
    puts("VPLNX_JNI_ASSETS=PASS file_descriptor=PASS");
    puts("VPLNX_JNI_FAIL_CLOSED=PASS network=DENIED permissions=DENIED RegisterNatives=UNIMPLEMENTED");
    puts("VPLNX_JNI_GAME_EXECUTED=NO");
    return 0;
}
