/* SPDX-License-Identifier: MIT
 * Linux ARM64 host provider allowlist for the MIT vpl_nx adaptation.
 * Do not route guest pthread_mutex_t, FILE*, DIR*, stat or locale_t directly
 * to glibc: those Android Bionic object layouts are not ABI-compatible.
 */
#define _GNU_SOURCE 1
extern void __stack_chk_fail(void) __attribute__((noreturn));
#include "host_imports_linux.h"
#include "assets_linux.h"
#include "bionic_pthread_linux.h"
#include "../upstream/opensles.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <errno.h>
#include <ctype.h>
#include <malloc.h>
#include <strings.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wchar.h>
#include <wctype.h>

static int *android_errno(void) {
    return &errno;
}

/* A modest Bionic FORTIFY subset. Detect invalid declared buffer lengths
 * rather than bypassing checks and silently overflowing host memory. */
static void *android_memcpy_chk(void *dst, const void *src,
                                size_t n, size_t capacity) {
    if (!dst || (!src && n) || n > capacity) {
        errno = ERANGE;
        return NULL;
    }
    return memcpy(dst, src, n);
}
static void *android_memmove_chk(void *dst, const void *src,
                                 size_t n, size_t capacity) {
    if (!dst || (!src && n) || n > capacity) {
        errno = ERANGE;
        return NULL;
    }
    return memmove(dst, src, n);
}
static size_t android_strlen_chk(const char *s, size_t capacity) {
    if (!s) {
        errno = EFAULT;
        return 0;
    }
    const char *end = memchr(s, 0, capacity);
    if (!end) {
        errno = ERANGE;
        return 0;
    }
    return (size_t)(end - s);
}

/* Stable Android log ABI for diagnostics, without calling logcat. */
static int android_log_write(int priority, const char *tag, const char *msg) {
    if (priority < 0) priority = 0;
    if (priority > 8) priority = 8;
    if (fprintf(stderr, "[android:%d:%.*s] %.*s\n",
                priority, 64, tag ? tag : "?",
                2048, msg ? msg : "") < 0)
        return -1;
    return 1;
}
static int android_log_print(int priority, const char *tag, const char *fmt, ...) {
    if (!fmt) return -1;
    char buffer[2049];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (n < 0) return -1;
    android_log_write(priority,tag,buffer);
    return n;
}
/* Explicit no-op for diagnostic metadata; not a signal/abort handler. */
static void android_abort_message(const char *msg) {
    if (msg) android_log_write(6,"abort-message",msg);
}


/* GL_OES_mapbuffer is optional: resolve extension dispatch only when an EGL
 * context exists. No GL calls happen during ELF relocation itself. */
static void *android_gl_map_buffer_oes(GLenum target, GLenum access) {
    typedef void *(*MapFn)(GLenum,GLenum);
    MapFn f=(MapFn)eglGetProcAddress("glMapBufferOES");
    return f?f(target,access):NULL;
}
static GLboolean android_gl_unmap_buffer_oes(GLenum target) {
    typedef GLboolean (*UnmapFn)(GLenum);
    UnmapFn f=(UnmapFn)eglGetProcAddress("glUnmapBufferOES");
    return f?f(target):(GLboolean)0;
}

typedef struct { const char *name; uintptr_t addr; } Import;
#define FN(sym, target) { sym, (uintptr_t)&target }
#define FN_SELF(name) FN(#name, name)
#define DATA(name) { #name, (uintptr_t)&name }

static const Import native_imports[] = {
    /* Basic C interfaces whose AArch64 calling convention and object types
     * are compatible. These never receive opaque Bionic FILE/mutex objects. */
    FN_SELF(malloc), FN_SELF(calloc), FN_SELF(realloc), FN_SELF(free),
    FN_SELF(memcpy), FN_SELF(memmove), FN_SELF(memset), FN_SELF(memcmp),
    FN_SELF(strlen), FN_SELF(strcmp), FN_SELF(strncmp), FN_SELF(strchr),
    FN_SELF(strrchr), FN_SELF(strstr),
    FN_SELF(strncpy), FN_SELF(strnlen), FN_SELF(strdup),
    FN_SELF(atoi), FN_SELF(atol), FN_SELF(strtol), FN_SELF(strtoul),
    FN_SELF(sin), FN_SELF(sinf), FN_SELF(cos), FN_SELF(cosf),
    FN_SELF(sqrt), FN_SELF(sqrtf), FN_SELF(fabs), FN_SELF(fabsf),
    FN_SELF(floor), FN_SELF(floorf), FN_SELF(ceil), FN_SELF(ceilf),
    FN_SELF(fmod), FN_SELF(fmodf), FN_SELF(pow), FN_SELF(powf),
    /* ABI-compatible scalar math, text and allocation functions imported by
     * this Lenneth build. No opaque structs, process spawning or callbacks. */
    FN_SELF(acos), FN_SELF(acosf), FN_SELF(asinf),
    FN_SELF(atan2), FN_SELF(atan2f), FN_SELF(exp2f), FN_SELF(expf),
    FN_SELF(frexp), FN_SELF(log10), FN_SELF(log10f), FN_SELF(logf),
    FN_SELF(modf), FN_SELF(tan), FN_SELF(tanf),
    FN_SELF(sincos), FN_SELF(sincosf),
    FN_SELF(atof), FN_SELF(strtod), FN_SELF(strtoull),
    FN_SELF(memchr), FN_SELF(memalign),
    FN_SELF(strcasecmp), FN_SELF(strcat), FN_SELF(strcpy),
    FN_SELF(strncat), FN_SELF(isalnum), FN_SELF(isalpha),
    FN_SELF(isspace), FN_SELF(tolower), FN_SELF(toupper),
    FN_SELF(rand), FN_SELF(srand),
    /* Straight, scalar/byte-buffer LP64 Linux/Bionic ABI-compatible calls.
     * fopen/FILE*, pthread_create/pthread_t and stat layouts remain BLOCKED. */
    FN_SELF(time), FN_SELF(clock_gettime), FN_SELF(clock_nanosleep),
    FN_SELF(gettimeofday), FN_SELF(nanosleep), FN_SELF(usleep),
    FN_SELF(sched_yield),
    FN_SELF(read), FN_SELF(write), FN_SELF(close), FN_SELF(lseek),
    /* open/rename/remove/mkdir must use sandboxed save/asset path adapters,
       not unrestricted glibc calls. */
    FN_SELF(getenv), FN_SELF(getpid), FN_SELF(gettid),
    FN_SELF(getpriority), FN_SELF(setpriority), FN_SELF(sysconf),
    FN_SELF(abort), FN_SELF(qsort), FN_SELF(posix_memalign),
    FN_SELF(strtoll), FN_SELF(strtof),
    FN_SELF(snprintf), FN_SELF(vsnprintf),
    FN_SELF(sscanf), FN_SELF(vsscanf),
    FN_SELF(printf), FN_SELF(sprintf), FN_SELF(vsprintf),
    FN_SELF(strerror), FN_SELF(div),
    FN_SELF(isxdigit), FN_SELF(islower), FN_SELF(isupper),
    FN_SELF(wcslen), FN_SELF(wmemchr), FN_SELF(wmemcpy),
    FN_SELF(wmemmove), FN_SELF(wmemcmp), FN_SELF(wmemset),
    FN_SELF(iswcntrl), FN_SELF(iswpunct), FN_SELF(iswdigit),
    FN_SELF(iswupper), FN_SELF(iswblank), FN_SELF(iswprint),
    FN_SELF(iswlower), FN_SELF(iswalpha), FN_SELF(iswspace),
    FN_SELF(iswxdigit), FN_SELF(towlower), FN_SELF(towupper),



    FN("__stack_chk_fail",__stack_chk_fail),
    FN("__errno",android_errno),
    FN("__memcpy_chk",android_memcpy_chk),
    FN("__memmove_chk",android_memmove_chk),
    FN("__strlen_chk",android_strlen_chk),
    FN("__android_log_write",android_log_write),
    FN("__android_log_print",android_log_print),
    FN("android_set_abort_message",android_abort_message),

    /* Android NDK AAsset interface - rooted, no absolute paths or symlinks. */

    /* Android LP64 pthread opaque-object translation: guest structs are
     * NEVER passed straight to glibc. Android sizes: mutex=40, cond=48. */
    FN("pthread_mutex_init",vpl_bp_mutex_init),
    FN("pthread_mutex_lock",vpl_bp_mutex_lock),
    FN("pthread_mutex_trylock",vpl_bp_mutex_trylock),
    FN("pthread_mutex_unlock",vpl_bp_mutex_unlock),
    FN("pthread_mutex_destroy",vpl_bp_mutex_destroy),
    FN("pthread_mutexattr_init",vpl_bp_mutexattr_init),
    FN("pthread_mutexattr_settype",vpl_bp_mutexattr_settype),
    FN("pthread_mutexattr_destroy",vpl_bp_mutexattr_destroy),
    FN("pthread_cond_init",vpl_bp_cond_init),
    FN("pthread_cond_signal",vpl_bp_cond_signal),
    FN("pthread_cond_broadcast",vpl_bp_cond_broadcast),
    FN("pthread_cond_wait",vpl_bp_cond_wait),
    FN("pthread_cond_timedwait",vpl_bp_cond_timedwait),
    FN("pthread_cond_destroy",vpl_bp_cond_destroy),
    FN("AAssetManager_fromJava",AAssetManager_fromJava_fake),
    FN("AAssetManager_open",AAssetManager_open_fake),
    FN("AAssetManager_openDir",AAssetManager_openDir_fake),
    FN("AAssetDir_getNextFileName",AAssetDir_getNextFileName_fake),
    FN("AAssetDir_close",AAssetDir_close_fake),
    FN("AAsset_read",AAsset_read_fake),
    FN("AAsset_seek",AAsset_seek_fake),
    FN("AAsset_getLength",AAsset_getLength_fake),
    FN("AAsset_getLength64",AAsset_getLength64_fake),
    FN("AAsset_close",AAsset_close_fake),
    FN("AAsset_openFileDescriptor64",AAsset_openFileDescriptor64_fake),

    /* Android OpenSL ES backed by SDL2, already tested separately. */
    FN_SELF(slCreateEngine),
    DATA(SL_IID_ENGINE), DATA(SL_IID_PLAY), DATA(SL_IID_VOLUME),
    DATA(SL_IID_BUFFERQUEUE), DATA(SL_IID_ANDROIDSIMPLEBUFFERQUEUE),
    DATA(SL_IID_OUTPUTMIX),

    /* GLES2/EGL functions: exposed after an SDL EGL context has been made.
     * Shader variants/extension entry points are intentionally not guessed. */
    FN_SELF(glGetString), FN_SELF(glGetIntegerv), FN_SELF(glGetError),
    FN_SELF(glClear), FN_SELF(glClearColor), FN_SELF(glViewport),
    FN_SELF(glEnable), FN_SELF(glDisable),
    FN_SELF(glBlendFunc), FN_SELF(glBlendFuncSeparate),
    FN_SELF(glCreateShader), FN_SELF(glShaderSource),
    FN_SELF(glCompileShader), FN_SELF(glGetShaderiv),
    FN_SELF(glGetShaderInfoLog), FN_SELF(glCreateProgram),
    FN_SELF(glAttachShader), FN_SELF(glLinkProgram),
    FN_SELF(glUseProgram), FN_SELF(glGetProgramiv),
    FN_SELF(glGetUniformLocation), FN_SELF(glGetAttribLocation),
    FN_SELF(glUniform1i), FN_SELF(glUniform1f), FN_SELF(glUniformMatrix4fv),
    FN_SELF(glVertexAttribPointer), FN_SELF(glEnableVertexAttribArray),
    FN_SELF(glDisableVertexAttribArray),
    FN_SELF(glActiveTexture), FN_SELF(glBindTexture), FN_SELF(glGenTextures),
    FN_SELF(glTexImage2D), FN_SELF(glTexParameteri), FN_SELF(glTexSubImage2D),
    FN_SELF(glBindBuffer), FN_SELF(glGenBuffers), FN_SELF(glBufferData),
    FN_SELF(glBufferSubData), FN_SELF(glDrawArrays), FN_SELF(glDrawElements),

    /* Exact GLES2 symbols imported by this build of Lenneth; explicitly
     * enumerated rather than resolving arbitrary addresses via dlsym(). */
    FN_SELF(glBindAttribLocation), FN_SELF(glBindFramebuffer),
    FN_SELF(glBindRenderbuffer), FN_SELF(glBlendEquation),
    FN_SELF(glCheckFramebufferStatus), FN_SELF(glClearDepthf),
    FN_SELF(glClearStencil), FN_SELF(glColorMask),
    FN_SELF(glCompressedTexImage2D), FN_SELF(glCullFace),
    FN_SELF(glDeleteBuffers), FN_SELF(glDeleteFramebuffers),
    FN_SELF(glDeleteProgram), FN_SELF(glDeleteRenderbuffers),
    FN_SELF(glDeleteShader), FN_SELF(glDeleteTextures),
    FN_SELF(glDepthFunc), FN_SELF(glDepthMask),
    FN_SELF(glFramebufferRenderbuffer), FN_SELF(glFramebufferTexture2D),
    FN_SELF(glFrontFace), FN_SELF(glGenFramebuffers),
    FN_SELF(glGenRenderbuffers), FN_SELF(glGenerateMipmap),
    FN_SELF(glGetActiveAttrib), FN_SELF(glGetActiveUniform),
    FN_SELF(glGetBooleanv), FN_SELF(glGetFloatv),
    FN_SELF(glGetProgramInfoLog), FN_SELF(glGetShaderSource),
    FN_SELF(glIsBuffer), FN_SELF(glIsEnabled), FN_SELF(glIsRenderbuffer),
    FN_SELF(glLineWidth), FN_SELF(glPixelStorei), FN_SELF(glReadPixels),
    FN_SELF(glRenderbufferStorage), FN_SELF(glScissor),
    FN_SELF(glStencilMask), FN_SELF(glTexParameterf),
    FN_SELF(glUniform1fv), FN_SELF(glUniform2f), FN_SELF(glUniform2fv),
    FN_SELF(glUniform2i), FN_SELF(glUniform2iv),
    FN_SELF(glUniform3f), FN_SELF(glUniform3fv),
    FN_SELF(glUniform3i), FN_SELF(glUniform3iv),
    FN_SELF(glUniform4f), FN_SELF(glUniform4fv),
    FN_SELF(glUniform4i), FN_SELF(glUniform4iv),
    FN_SELF(glUniformMatrix2fv), FN_SELF(glUniformMatrix3fv),
    FN("glMapBufferOES",android_gl_map_buffer_oes),
    FN("glUnmapBufferOES",android_gl_unmap_buffer_oes),
    FN_SELF(eglGetProcAddress)
};

/* Record each unique unresolved import once, preserving ELF .dynstr strings
 * until the command prints its report and unloads the mapped modules. */
static void note_missing(VplHostStats *stats, const char *symbol) {
    if (!stats) return;
    stats->unknown++;
    if (!symbol || !*symbol) return;
    for (size_t i=0; i<stats->missing_unique; i++)
        if (strcmp(stats->missing_names[i],symbol)==0) return;
    if (stats->missing_unique >= VPL_HOST_MAX_MISSING) {
        stats->missing_overflow++;
        return;
    }
    stats->missing_names[stats->missing_unique++]=symbol;
}
uintptr_t vpl_linux_host_lookup(const char *symbol, void *context) {
    VplHostStats *stats = context;
    if (!symbol || !*symbol) {
        note_missing(stats,symbol);
        return 0;
    }
    for (size_t i=0; i<sizeof(native_imports)/sizeof(native_imports[0]); i++)
        if (strcmp(symbol,native_imports[i].name)==0) {
            if (stats) stats->found++;
            return native_imports[i].addr;
        }
    note_missing(stats,symbol);
    return 0;
}
int vpl_linux_host_selftest(void) {
    const char *present[] = {
        "malloc","memcpy","__errno","__memcpy_chk","__android_log_write",
        "AAssetManager_open","AAsset_read","AAsset_openFileDescriptor64",
        "slCreateEngine","SL_IID_ENGINE","SL_IID_PLAY","glClear","glTexImage2D",
        "atan2f","strtod","memchr","glMapBufferOES","glDeleteFramebuffers",
        "pthread_mutex_lock","pthread_cond_wait","pthread_mutexattr_settype",
        "time","clock_gettime","nanosleep","read","write","close",
        "posix_memalign","snprintf","sscanf","__stack_chk_fail"
    };
    const char *forbidden[] = {
        "pthread_create","pthread_rwlock_init","fopen","fread","fclose",
        "opendir","stat","fopen","open","rename","remove","mkdir",
        "system","dlopen","unknown_android_symbol",
        "ExecuteProgram","__stack_chk_guard"
    };
    VplHostStats stats={0};
    for(size_t i=0;i<sizeof(present)/sizeof(present[0]);i++)
        if(!vpl_linux_host_lookup(present[i],&stats)) return 1;
    for(size_t i=0;i<sizeof(forbidden)/sizeof(forbidden[0]);i++)
        if(vpl_linux_host_lookup(forbidden[i],&stats)) return 2;
    int *e=(int *)((int *(*)(void))vpl_linux_host_lookup("__errno",NULL))();
    if(e != &errno) return 3;
    char dst[8]={0};
    typedef void *(*CheckFn)(void *,const void *,size_t,size_t);
    CheckFn check=(CheckFn)vpl_linux_host_lookup("__memcpy_chk",NULL);
    if(!check || check(dst,"ABC",4,sizeof(dst))!=dst ||
       strcmp(dst,"ABC")!=0) return 4;
    errno=0;
    if(check(dst,"ABCDE",6,4)!=NULL || errno!=ERANGE) return 5;
    if(stats.found!=sizeof(present)/sizeof(present[0]) ||
       stats.unknown!=sizeof(forbidden)/sizeof(forbidden[0])) return 6;
    return 0;
}
