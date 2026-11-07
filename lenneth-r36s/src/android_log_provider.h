#ifndef LENNETH_ANDROID_LOG_PROVIDER_H
#define LENNETH_ANDROID_LOG_PROVIDER_H
#include <stdint.h>
#include <stdarg.h>
/* Minimal Linux-backed Android NDK logging ABI. No Java/JNI runtime. */
int __android_log_write(int priority, const char *tag, const char *text);
int __android_log_print(int priority, const char *tag, const char *format, ...);
int __android_log_vprint(int priority, const char *tag, const char *format, va_list args);
void __android_log_assert(const char *condition, const char *tag, const char *format, ...);
void android_set_abort_message(const char *message);
int lenneth_android_log_lookup(const char *name,uint64_t *address);
int lenneth_android_log_selftest(void);
#endif
