/* SPDX-License-Identifier: MIT
 * Android Bionic FORTIFY entry points on AArch64 Linux.
 * Every write is checked before use. On a detected bound violation,
 * terminate the current process just like an Android FORTIFY failure.
 * This module is tested using synthetic buffers, never commercial code.
 */
#ifndef VPLNX_BIONIC_FORTIFY_H
#define VPLNX_BIONIC_FORTIFY_H
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

char *vpl_bf_strcat(char *dst, const char *src, size_t dst_size);
char *vpl_bf_strncpy(char *dst, const char *src, size_t copy_len, size_t dst_size);
char *vpl_bf_strchr(const char *src, int ch, size_t src_size);
ssize_t vpl_bf_read(int fd, void *dst, size_t len, size_t dst_size);
int vpl_bf_vsnprintf(char *dst, size_t dst_size_arg, int flags,
                     size_t dst_object_size, const char *fmt, va_list ap);
int vpl_bf_vsprintf(char *dst, int flags, size_t dst_object_size,
                    const char *fmt, va_list ap);
#endif
