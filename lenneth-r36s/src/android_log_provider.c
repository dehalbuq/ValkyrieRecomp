/*
 * Linux Android NDK __android_log_* ABI adapter for Lenneth R36S.
 * MIT; written independently. No original game data or code included.
 *
 * Strict whitelist, no android_log_* dynamic resolution or JNI emulation.
 * This shim is tested directly with synthetic messages. No guest engine code
 * is ever called. __android_log_assert aborts, consistent with NDK semantics;
 * selftest intentionally does NOT invoke it.
 */
#define _GNU_SOURCE
#include "android_log_provider.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static char abort_message[512];
static const char *severity(int n) {
    switch(n) {
        case 2:return "V";
        case 3:return "D";
        case 4:return "I";
        case 5:return "W";
        case 6:return "E";
        case 7:return "F";
        default:return "?";
    }
}
static void clean(char *dst,size_t dstsize,const char *source) {
    if(!dstsize)return;
    if(!source)source="";
    size_t i=0;
    for(;source[i] && i+1<dstsize;i++){
        unsigned char c=(unsigned char)source[i];
        /* Log only; strip terminal controls; cap size to protect RAM. */
        dst[i]=(c<32 || c==127)?' ':source[i];
    }
    dst[i]='\0';
}
static int print_line(int level,const char *tag,const char *value) {
    char t[96],message[2049];
    clean(t,sizeof(t),tag);
    clean(message,sizeof(message),value);
    flockfile(stderr);
    fprintf(stderr,"[lenneth android-log][%s][%s] %s\n",severity(level),t,message);
    funlockfile(stderr);
    return (int)strlen(message);
}
int __android_log_write(int priority,const char *tag,const char *text) {
    if(!text)return -1;
    return print_line(priority,tag,text);
}
int __android_log_vprint(int priority,const char *tag,
                         const char *format,va_list ap) {
    if(!format)return -1;
    char message[2049];
    va_list copy;
    va_copy(copy,ap);
    int len=vsnprintf(message,sizeof(message),format,copy);
    va_end(copy);
    if(len<0)return len;
    print_line(priority,tag,message);
    return len;
}
int __android_log_print(int priority,const char *tag,const char *format,...) {
    if(!format)return -1;
    va_list ap;
    va_start(ap,format);
    int result=__android_log_vprint(priority,tag,format,ap);
    va_end(ap);
    return result;
}
void android_set_abort_message(const char *msg) {
    clean(abort_message,sizeof(abort_message),msg);
    if(abort_message[0])
        print_line(7,"abort-message",abort_message);
}
void __android_log_assert(const char *condition,const char *tag,
                          const char *format,...) {
    char message[2049]="";
    if(format){
        va_list ap;
        va_start(ap,format);
        vsnprintf(message,sizeof(message),format,ap);
        va_end(ap);
    }
    char prefix[2300];
    snprintf(prefix,sizeof(prefix),"Assertion failed (%s): %s",
             condition?condition:"?",message);
    print_line(7,tag,prefix);
    abort();
}
typedef struct {const char *name; void (*fn)(void);} Symbol;
#define EXPORT(name) {#name,(void (*)(void))(name)}
static const Symbol exports[]={
    EXPORT(__android_log_write),
    EXPORT(__android_log_print),
    EXPORT(__android_log_vprint),
    EXPORT(__android_log_assert),
    EXPORT(android_set_abort_message)
};
int lenneth_android_log_lookup(const char *name,uint64_t *out) {
    if(!name || !out)return 0;
    for(size_t i=0;i<sizeof(exports)/sizeof(exports[0]);i++){
        if(strcmp(name,exports[i].name))continue;
        void (*fn)(void)=exports[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));
        return 1;
    }
    return 0;
}
int lenneth_android_log_selftest(void) {
    int a=__android_log_write(4,"Phase7","synthetic file-backed logger");
    int b=__android_log_print(3,"Phase7","synthetic number %d",42);
    android_set_abort_message("synthetic message: no abort invoked");
    uint64_t address=0;
    int found=lenneth_android_log_lookup("__android_log_write",&address);
    int blocked=!lenneth_android_log_lookup("pthread_create",&address);
    int ok=a>0&&b>0&&found&&blocked;
    printf("[lenneth android-log] SELFTEST=%s provider_symbols=%zu "
           "unknown-symbol-blocked=%d engine=not_executed\n",
           ok?"PASS":"FAIL",sizeof(exports)/sizeof(exports[0]),
           blocked);
    return ok?0:1;
}
