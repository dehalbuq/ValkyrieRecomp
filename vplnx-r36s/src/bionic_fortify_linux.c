/* SPDX-License-Identifier: MIT
 * Explicit Android Bionic LP64 FORTIFY wrappers for Linux.
 * Distinct from direct glibc imports: enforce Android guest byte counts.
 * Violations abort immediately; the user cannot continue with corrupted
 * assets or overwritten JNI/ELF state.
 */
#define _GNU_SOURCE 1
#include "bionic_fortify_linux.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fatal_fortify(const char *func) __attribute__((noreturn));
static void fatal_fortify(const char *func) {
    fprintf(stderr,"VPLNX_FORTIFY=FAIL function=%s bounds_or_arguments\n",func);
    fflush(stderr);
    abort();
}

char *vpl_bf_strcat(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0 ||
        dst_size == (size_t)-1) fatal_fortify("__strcat_chk");
    const char *end = memchr(dst,0,dst_size);
    if (!end) fatal_fortify("__strcat_chk");
    size_t offset=(size_t)(end-dst);
    size_t remain=dst_size-offset;
    if (!memchr(src,0,remain)) fatal_fortify("__strcat_chk");
    return strcat(dst,src);
}
char *vpl_bf_strncpy(char *dst, const char *src,
                      size_t len, size_t dst_size) {
    if (!dst || !src || len>dst_size) fatal_fortify("__strncpy_chk");
    return strncpy(dst,src,len);
}
char *vpl_bf_strchr(const char *src, int ch, size_t src_size) {
    if (!src || !src_size || src_size==(size_t)-1 ||
        !memchr(src,0,src_size)) fatal_fortify("__strchr_chk");
    return strchr(src,ch);
}
ssize_t vpl_bf_read(int fd, void *dst, size_t len, size_t dst_size) {
    if ((len && !dst) || len>dst_size ||
        len>(size_t)SSIZE_MAX) fatal_fortify("__read_chk");
    return read(fd,dst,len);
}
int vpl_bf_vsnprintf(char *dst, size_t dst_size_arg, int flags,
                     size_t dst_object_size, const char *fmt, va_list ap) {
    if (flags!=0 || !fmt ||
        (dst_size_arg && !dst) ||
        dst_size_arg>dst_object_size ||
        dst_size_arg>(size_t)INT_MAX)
        fatal_fortify("__vsnprintf_chk");
    /* A C99 size query with size=0 may pass dst=NULL. */
    return vsnprintf(dst,dst_size_arg,fmt,ap);
}
int vpl_bf_vsprintf(char *dst, int flags, size_t dst_object_size,
                    const char *fmt, va_list ap) {
    if (flags!=0 || !dst || !fmt ||
        dst_object_size==(size_t)-1 ||
        dst_object_size==0 || dst_object_size>(size_t)INT_MAX)
        fatal_fortify("__vsprintf_chk");
    va_list copy;
    va_copy(copy,ap);
    int needed=vsnprintf(NULL,0,fmt,copy);
    va_end(copy);
    if (needed < 0) return needed;
    if ((size_t)needed>=dst_object_size)
        fatal_fortify("__vsprintf_chk");
    return vsnprintf(dst,dst_object_size,fmt,ap);
}
