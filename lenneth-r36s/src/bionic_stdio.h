#ifndef LENNETH_BIONIC_STDIO_H
#define LENNETH_BIONIC_STDIO_H
#include <stdint.h>
/*
 * Phase14 experimental Bionic stdio proxy API. Guest FILE* is always an
 * opaque LENNETH token; host glibc FILE* never crosses the guest boundary.
 * __sF and raw legacy Bionic FILE object layouts are NOT implemented.
 */
int lenneth_bionic_stdio_lookup(const char *name,uint64_t *out);
int lenneth_bionic_stdio_selftest(void);
#endif
