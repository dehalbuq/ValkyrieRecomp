#ifndef LENNETH_OPENSL_BRIDGE_H
#define LENNETH_OPENSL_BRIDGE_H
#include <stdint.h>
/* Phase15: OpenSL ES 1.0.1 interface-layout / object-lifecycle prototype.
 * The four IID symbols are DATA objects containing pointers to UUIDs,
 * never function pointers. PCM playback hardware is NOT implemented here.
 * All tests call synthetic host-defined functions, not commercial game code.
 */
int lenneth_opensl_lookup(const char *name, uint8_t elf_symbol_type,
                         uint64_t *address);
int lenneth_opensl_selftest(void);
#endif
