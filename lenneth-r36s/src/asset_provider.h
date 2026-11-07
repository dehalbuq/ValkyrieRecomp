#ifndef LENNETH_ASSET_PROVIDER_H
#define LENNETH_ASSET_PROVIDER_H
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
/* Android NDK AAsset API bridge for an EXTRACTED, local private assets/ folder.
 * This is NOT an APK/ZIP reader, virtual Android Java object, or JNI runtime.
 * The probe never invokes the proprietary engine.
 */
typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
typedef struct AAssetDir AAssetDir;
int lenneth_asset_set_root(const char *path);
void lenneth_asset_close_root(void);
int lenneth_asset_lookup(const char *symbol,uint64_t *address);
int lenneth_asset_selftest(const char *root,const char *relative_path);
AAssetManager *AAssetManager_fromJava(void *env,void *asset_manager_java);
AAsset *AAssetManager_open(AAssetManager *mgr,const char *filename,int mode);
AAssetDir *AAssetManager_openDir(AAssetManager *mgr,const char *dirname);
int AAsset_read(AAsset *asset,void *buffer,size_t count);
off_t AAsset_seek(AAsset *asset,off_t offset,int whence);
int64_t AAsset_seek64(AAsset *asset,int64_t offset,int whence);
off_t AAsset_getLength(AAsset *asset);
int64_t AAsset_getLength64(AAsset *asset);
off_t AAsset_getRemainingLength(AAsset *asset);
int64_t AAsset_getRemainingLength64(AAsset *asset);
const void *AAsset_getBuffer(AAsset *asset);
int AAsset_isAllocated(AAsset *asset);
void AAsset_close(AAsset *asset);
const char *AAssetDir_getNextFileName(AAssetDir *dir);
void AAssetDir_rewind(AAssetDir *dir);
void AAssetDir_close(AAssetDir *dir);
int AAsset_openFileDescriptor(AAsset *asset,off_t *start,off_t *length);
int AAsset_openFileDescriptor64(AAsset *asset,int64_t *start,int64_t *length);
#endif
