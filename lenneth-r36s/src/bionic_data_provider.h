#ifndef LENNETH_BIONIC_DATA_PROVIDER_H
#define LENNETH_BIONIC_DATA_PROVIDER_H
#include <stdint.h>
/* Only one independently created process data object is supported:
   __stack_chk_guard. Never fabricate Bionic __sF or OpenSL IID objects. */
int lenneth_bionic_data_lookup(const char *symbol,uint64_t *address);
int lenneth_bionic_data_selftest(void);
#endif
