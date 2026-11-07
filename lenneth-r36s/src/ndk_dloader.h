#ifndef LENNETH_NDK_DLOADER_H
#define LENNETH_NDK_DLOADER_H
#include <stdint.h>
/* Narrow, audited NDK libdl bridge. Only host GLES/EGL sonames are
 * accessible; never loads arbitrary guest paths or Android commercial libs.
 * The guest engine is not executed by this diagnostic. */
int lenneth_ndk_dl_lookup(const char *name,uint64_t *address);
int lenneth_ndk_dl_selftest(void);
void lenneth_ndk_dl_close_all(void);
#endif
