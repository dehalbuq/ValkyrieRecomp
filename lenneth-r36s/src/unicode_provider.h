#ifndef LENNETH_UNICODE_PROVIDER_H
#define LENNETH_UNICODE_PROVIDER_H
#include <stdint.h>
/* Only array/32-bit-wchar operations; Bionic locale_t and mbstate_t
 * structures are deliberately NOT bridged to glibc. */
int lenneth_unicode_provider_lookup(const char *symbol,uint64_t *address);
int lenneth_unicode_provider_selftest(void);
#endif
