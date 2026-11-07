#ifndef LENNETH_PORTABLE_PROVIDER_H
#define LENNETH_PORTABLE_PROVIDER_H
#include <stdint.h>
/* Explicitly allowlisted portable libc/libm functions; NEVER call game code.
 * Android Bionic pthread, TLS, FILE*, locale, fortify and C++ ABI not routed
 * to glibc by name. */
void lenneth_portable_open(void);
void lenneth_portable_close(void);
int lenneth_portable_lookup(const char *name,uint64_t *address);
#endif
