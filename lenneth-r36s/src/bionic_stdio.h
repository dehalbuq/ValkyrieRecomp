#ifndef LENNETH_BIONIC_STDIO_H
#define LENNETH_BIONIC_STDIO_H
#include <stdint.h>
/*
 * Phase14 experimental Bionic stdio proxy API. Guest FILE* is always an
 * opaque LENNETH token; host glibc FILE* never crosses the guest boundary.
 * Phase22 implements a bounded __sF[3] data array for standard streams only.
 */
int lenneth_bionic_stdio_lookup(const char *name,uint64_t *out);
/* Phase22: Bionic legacy __sF data (3 x 152 bytes, never glibc FILE). */
int lenneth_bionic_stdio_data_lookup(const char *name,uint64_t *out);
int lenneth_bionic_stdio_legacy_selftest(void);
int lenneth_bionic_stdio_selftest(void);
#endif
