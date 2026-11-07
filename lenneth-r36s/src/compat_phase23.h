#ifndef LENNETH_COMPAT_PHASE23_H
#define LENNETH_COMPAT_PHASE23_H
#include <stdint.h>
/* Only the early Android/Bionic AArch64 jmp_buf layout, 256 bytes. 
 * Modern Android cookie/checksum layouts are not interchangeable. */
int lenneth_phase23_legacy_jmp_lookup(const char *name,uint64_t *address);
int lenneth_phase23_legacy_jmp_selftest(void);
int lenneth_phase23_legacy_probe(void);
#endif
