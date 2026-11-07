#ifndef LENNETH_BIONIC_FORTIFY_H
#define LENNETH_BIONIC_FORTIFY_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
/* Standalone Bionic FORTIFY and errno bridge, independently written under MIT.
 * No guest engine code is executed in the phase10 diagnostic.
 */
int lenneth_bionic_fortify_lookup(const char *name,uint64_t *out);
int lenneth_bionic_fortify_selftest(void);
#endif
