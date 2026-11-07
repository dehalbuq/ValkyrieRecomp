/*
 * Phase 5 conservative, explicit ABI-safe native Linux libc/libm providers.
 * License: MIT. Written independently; contains no game data.
 *
 * The JNI/Bionic runtime is NOT implemented. This table is just an audited
 * set of individual C functions whose AArch64 calling signatures are
 * compatible between Android and Linux for their supported inputs.
 *
 * High-risk entries are intentionally absent: pthread_* (Bionic object's
 * representation differs), setjmp/longjmp, __errno, FILE* APIs,
 * fopen/fread/printf, sysconf, stat, locale internals, __sF, TLS, C++
 * exception/constructor ABI, filesystem permission or JNI APIs.
 *
 * This phase links data relocation slots only in RW non-executable mappings.
 */
#include "portable_provider.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static struct {
    void *libc_handle;
    void *libm_handle;
    unsigned ready;
} hp;

static void *open_one(const char *major,const char *minor) {
    void *h=dlopen(major,RTLD_NOW|RTLD_LOCAL);
    if(!h)h=dlopen(minor,RTLD_NOW|RTLD_LOCAL);
    return h;
}

void lenneth_portable_open(void) {
    memset(&hp,0,sizeof(hp));
    hp.libc_handle=open_one("libc.so.6","libc.so");
    hp.libm_handle=open_one("libm.so.6","libm.so");
    hp.ready=1;
    printf("[lenneth portable] libc_provider=%s libm_provider=%s "
           "policy=conservative-function-allowlist\n",
           hp.libc_handle?"loaded":"missing",
           hp.libm_handle?"loaded":"missing");
}
void lenneth_portable_close(void) {
    if(hp.libm_handle)dlclose(hp.libm_handle);
    if(hp.libc_handle)dlclose(hp.libc_handle);
    memset(&hp,0,sizeof(hp));
}

/* Keep list finite and individually audited. C function signatures must
 * be checked before guest execution; this diagnostic only binds slots.
 * Extra phase9 POSIX functions use standard timespec/timeval and varargs;
 * none of these addresses have been invoked by the original Android motor.
 * These use only numeric, basic
 * string, buffer or allocator ABI types. No Bionic C structs or pointers
 * to private libc state can cross this boundary.
 *
 * Do NOT add an arbitrary glibc dlsym fallback.
 */
static const char *const vetted_functions[]={
    "acos", "acosf", "asin", "asinf", "atan", "atan2", "atan2f",
    "atanf", "atof", "atoi", "atol", "atoll", "calloc",
    "ceil", "ceilf", "cos", "cosf",
    "exp", "exp2", "exp2f", "expf",
    "fabs", "fabsf", "floor", "floorf",
    "fmod", "fmodf", "free", "frexp", "frexpf",
    "getenv", "labs", "log", "logf", "malloc",
    "memchr", "memcmp", "memcpy", "memmove", "memset", "modf", "modff",
    "posix_memalign", "pow", "powf", "realloc",
    "sin", "sinf", "sqrt", "sqrtf", "strcat", "strchr",
    "strcmp", "strcpy", "strdup", "strlen", "strncat", "strncmp",
    "strncpy", "strrchr", "strstr", "strtod", "strtof",
    "strtol", "strtoll", "strtoul", "strtoull",
    "tan", "tanf", "tolower", "toupper",
    /* Phase 9: C numeric/time/ctype ABI with only plain C/POSIX types.
     * Still DO NOT bind FILE*, pthread objects, locale structs or guest paths. */
    "log10", "log10f", "sincos", "sincosf", "div", "qsort",
    "rand", "srand", "abort", "exit",
    "isalpha", "isalnum", "isspace", "islower", "isupper", "isxdigit",
    "time", "gettimeofday", "clock_gettime", "nanosleep",
    "usleep", "sched_yield", "strerror", "strcasecmp",
    "snprintf", "vsnprintf", "sprintf", "vsprintf",
    "printf", "sscanf", "vsscanf"
};
static int vetted(const char *name) {
    for(size_t i=0;i<sizeof(vetted_functions)/sizeof(vetted_functions[0]);i++)
        if(strcmp(name,vetted_functions[i])==0)return 1;
    return 0;
}
int lenneth_portable_lookup(const char *name,uint64_t *address) {
    if(!hp.ready || !name || !address || !vetted(name))
        return 0;
    /* libm first: this is safe for allowlisted C math functions. */
    void *p=hp.libm_handle?dlsym(hp.libm_handle,name):NULL;
    if(!p && hp.libc_handle)p=dlsym(hp.libc_handle,name);
    if(!p)return 0;
    *address=(uint64_t)(uintptr_t)p;
    return 1;
}
