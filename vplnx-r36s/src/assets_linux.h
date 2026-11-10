/* SPDX-License-Identifier: MIT
 * Linux port of the Android NDK asset API interface used by vpl_nx.
 * Reads archives on demand; does not preload .cpk content into RAM.
 */
#ifndef VPLNX_ASSETS_LINUX_H
#define VPLNX_ASSETS_LINUX_H
#include <stdint.h>
#include <stddef.h>
typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
typedef struct AAssetDir AAssetDir;
int vpl_assets_set_root(const char *assets_directory);
void vpl_assets_shutdown(void);
AAssetManager *asset_manager(void);
AAssetManager *AAssetManager_fromJava_fake(void *env, void *obj);
AAsset *AAssetManager_open_fake(AAssetManager *mgr, const char *filename, int mode);
int AAsset_read_fake(AAsset *asset, void *buffer, size_t count);
int64_t AAsset_seek_fake(AAsset *asset, int64_t off, int whence);
void AAsset_close_fake(AAsset *asset);
int64_t AAsset_getLength_fake(AAsset *asset);
int64_t AAsset_getLength64_fake(AAsset *asset);
int AAsset_openFileDescriptor64_fake(AAsset *asset, int64_t *start, int64_t *length);
AAssetDir *AAssetManager_openDir_fake(AAssetManager *mgr, const char *name);
const char *AAssetDir_getNextFileName_fake(AAssetDir *dir);
void AAssetDir_close_fake(AAssetDir *dir);
#endif
