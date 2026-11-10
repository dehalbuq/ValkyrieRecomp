/* SPDX-License-Identifier: MIT
 * Android/Bionic-compatible host import subset for Linux ARM64.
 *
 * Only explicitly reviewed ABI-compatible functions are exposed.
 * No dynamic dlsym search, blanket libc forwarding, pthread object aliasing,
 * guest engine execution, or proprietary contents.
 */
#ifndef VPLNX_HOST_IMPORTS_LINUX_H
#define VPLNX_HOST_IMPORTS_LINUX_H
#include <stdint.h>
#include <stddef.h>

#define VPL_HOST_MAX_MISSING 256
typedef struct {
    size_t found;
    size_t unknown;
    size_t missing_unique;
    size_t missing_overflow;
    /* Names point to read-only .dynstr while ELF modules are open. */
    const char *missing_names[VPL_HOST_MAX_MISSING];
} VplHostStats;

/* Return pointer to a host routine or (for data imports) the actual data
 * address; 0 means a genuine unresolved import that must block execution.
 */
uintptr_t vpl_linux_host_lookup(const char *symbol, void *context);

/* Self-tests call only native host code; the Android engine is not loaded. */
int vpl_linux_host_selftest(void);
#endif
