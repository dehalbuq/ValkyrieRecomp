/* SPDX-License-Identifier: MIT
 * Android/Bionic C++ static object lifecycle bridge for Linux AArch64.
 * Destructors are registered in a private table and explicitly finalized,
 * never handed to glibc's process atexit list, which could call guest code
 * after its ELF module has been unmapped.
 */
#ifndef VPLNX_BIONIC_CXA_LINUX_H
#define VPLNX_BIONIC_CXA_LINUX_H
#include <stddef.h>
#include <stdint.h>
int vpl_bcxa_atexit(void (*destructor)(void *), void *arg, void *dso);
void vpl_bcxa_finalize(void *dso);
size_t vpl_bcxa_pending(void);
/* Use Linux getrandom(2) for the guest's Android global stack canary.
 * Zero return indicates no usable source of entropy; loader must block.
 */
uintptr_t vpl_bionic_stack_guard_address(void);
#endif
