#ifndef LENNETH_CXX_RUNTIME_H
#define LENNETH_CXX_RUNTIME_H
#include <stdint.h>
/* Independently authored guest C++ destructor lifecycle adapter.
 * It only registers functions during linker inspection; guest code is never
 * executed in the phase-11 multitest. Synthetic selftest uses local callbacks.
 */
int lenneth_cxx_runtime_lookup(const char *name,uint64_t *address);
int lenneth_cxx_runtime_selftest(void);
#endif
