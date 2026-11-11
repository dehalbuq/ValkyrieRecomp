/* SPDX-License-Identifier: MIT
 * Created specifically for native AArch64 cloud tests.
 * This is NOT a commercial library, Android APK or game engine.
 */
#include <stdint.h>
__attribute__((visibility("default")))
const uint64_t vplnx_synthetic_partner_magic = UINT64_C(0x5054525f43495f31);
__attribute__((visibility("default")))
int vplnx_synthetic_partner_checksum(int x) {
    return x + 11;
}
