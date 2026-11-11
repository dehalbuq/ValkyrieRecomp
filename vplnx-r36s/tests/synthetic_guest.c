/* SPDX-License-Identifier: MIT
 * Synthetic ELF guest which exercises JavaVM/JNIEnv + host imports +
 * cross-module relocation through executable ARM64 code in Linux cloud.
 * No copyrighted engine code, no DRM, no external assets.
 */
#include <stddef.h>
#include <stdint.h>
__attribute__((visibility("default")))
const uint64_t vplnx_synthetic_guest_magic = UINT64_C(0x475553545f43495f);
typedef struct { uint32_t word; unsigned char reserved[36]; } AndroidMutex;
_Static_assert(sizeof(AndroidMutex) == 40, "Android ARM64 mutex layout");
static AndroidMutex s_lock;
extern int pthread_mutex_lock(AndroidMutex *);
extern int pthread_mutex_unlock(AndroidMutex *);
extern int vplnx_synthetic_partner_checksum(int);
extern size_t strlen(const char *);
extern void *malloc(size_t);
extern void free(void *);

__attribute__((visibility("default")))
int JNI_OnLoad(void *vm, void *reserved) {
    (void)reserved;
    if (!vm) return -1;
    void **vtab = *(void ***)vm;
    if (!vtab || !vtab[6]) return -2;
    int (*GetEnv)(void *, void **, int) = (int (*)(void *, void **, int))vtab[6];
    void *env = NULL;
    if (GetEnv(vm, &env, 0x00010006) != 0 || !env) return -3;
    void **etab = *(void ***)env;
    if (!etab || !etab[4]) return -4;
    int (*GetVersion)(void *) = (int (*)(void *))etab[4];
    if (GetVersion(env) != 0x00010006) return -5;
    if (pthread_mutex_lock(&s_lock) != 0) return -6;
    int *number = malloc(sizeof(*number));
    if (!number) return -7;
    *number = vplnx_synthetic_partner_checksum(31);
    int good = *number == 42 && strlen("vplnx") == 5;
    free(number);
    if (pthread_mutex_unlock(&s_lock) != 0) return -8;
    return good ? 0x00010006 : -9;
}
