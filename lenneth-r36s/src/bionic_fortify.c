/*
 * Lenneth Android ARM64: independently implemented Bionic FORTIFY ABI bridge.
 * MIT. API prototypes cross-checked with AOSP Bionic NDK declarations.
 *
 * Conservative: fail-closed on malformed size/buffer bounds. Source/memory
 * checked pointers MUST still be valid; this is not a memory sanitizer.
 * No direct Bionic FILE*, thread objects, JNI, dynamic library or data import
 * bridging. No commercial code executed by phase10.
 */
#define _GNU_SOURCE
#include "bionic_fortify.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fail_size(const char *fn){
    fprintf(stderr,"[lenneth fortify] abort invalid bounds for %s\n",fn);
    abort();
}
static size_t bounded_len(const char *s,size_t n,const char *func) {
    if(!s)fail_size(func);
    const char *e=memchr(s,'\0',n);
    if(!e)fail_size(func);
    return (size_t)(e-s);
}
static size_t guest_strlen_chk(const char *s,size_t len) {
    return bounded_len(s,len,"__strlen_chk");
}
static void *guest_memcpy_chk(void *dst,const void *src,size_t n,size_t capacity){
    if(n>capacity)fail_size("__memcpy_chk");
    return memcpy(dst,src,n);
}
static void *guest_memmove_chk(void *dst,const void *src,size_t n,size_t capacity){
    if(n>capacity)fail_size("__memmove_chk");
    return memmove(dst,src,n);
}
static char *guest_strcat_chk(char *dst,const char *src,size_t capacity) {
    size_t used=bounded_len(dst,capacity,"__strcat_chk");
    size_t need=strlen(src);
    if(need>=capacity-used)fail_size("__strcat_chk");
    return strcat(dst,src);
}
static char *guest_strncpy_chk(char *dst,const char *src,size_t count,size_t capacity) {
    if(count>capacity)fail_size("__strncpy_chk");
    return strncpy(dst,src,count);
}
static char *guest_strchr_chk(const char *s,int ch,size_t capacity) {
    if(!s)fail_size("__strchr_chk");
    for(size_t i=0;i<capacity;i++){
        if(s[i]==(char)ch)return (char *)(s+i);
        if(s[i]=='\0')return NULL;
    }
    fail_size("__strchr_chk");
}
static ssize_t guest_read_chk(int fd,void *buf,size_t count,size_t cap) {
    if(count>cap || count>(size_t)SSIZE_MAX)fail_size("__read_chk");
    return read(fd,buf,count);
}
static int guest_vsnprintf_chk(char *dst,size_t max,int flags,
                               size_t cap,const char *fmt,va_list va) {
    (void)flags;
    if(max>cap)fail_size("__vsnprintf_chk");
    return vsnprintf(dst,max,fmt,va);
}
/* Bionic __vsprintf_chk uses a destination object-size bound.
 * Use bounded formatting rather than silently accepting overflows. */
static int guest_vsprintf_chk(char *dst,int flags,size_t cap,
                             const char *fmt,va_list ap){
    (void)flags;
    if(!dst||!fmt||cap==0)fail_size("__vsprintf_chk");
    va_list copy;
    va_copy(copy,ap);
    int len=vsnprintf(dst,cap,fmt,copy);
    va_end(copy);
    if(len<0 || (size_t)len>=cap)fail_size("__vsprintf_chk");
    return len;
}
static int guest_open_2(const char *name,int flags){
    if(flags&O_CREAT)fail_size("__open_2 (O_CREAT lacks mode)");
#ifdef O_TMPFILE
    if((flags&O_TMPFILE)==O_TMPFILE)fail_size("__open_2 (O_TMPFILE lacks mode)");
#endif
    return open(name,flags);
}
static void guest_stack_chk_fail(void) {
    fprintf(stderr,"[lenneth fortify] stack check failed\n");
    abort();
}
static int *guest_errno(void) {return &errno;}

/* Each registration is explicit. Never fall through to arbitrary glibc. */
typedef struct {const char *name;void (*fn)(void);} Symbol;
#define EXPORT(name,fn) {name,(void(*)(void))(fn)}
static const Symbol symbols[]={
    EXPORT("__strlen_chk",guest_strlen_chk),
    EXPORT("__memcpy_chk",guest_memcpy_chk),
    EXPORT("__memmove_chk",guest_memmove_chk),
    EXPORT("__strcat_chk",guest_strcat_chk),
    EXPORT("__strncpy_chk",guest_strncpy_chk),
    EXPORT("__strchr_chk",guest_strchr_chk),
    EXPORT("__read_chk",guest_read_chk),
    EXPORT("__vsnprintf_chk",guest_vsnprintf_chk),
    EXPORT("__vsprintf_chk",guest_vsprintf_chk),
    EXPORT("__open_2",guest_open_2),
    EXPORT("__stack_chk_fail",guest_stack_chk_fail),
    EXPORT("__errno",guest_errno)
};
int lenneth_bionic_fortify_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++){
        if(strcmp(symbols[i].name,name)!=0)continue;
        void (*fn)(void)=symbols[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));
        return 1;
    }
    return 0;
}
/* No game library is loaded or executed by this synthetic test. */
static int format_test(char *dst,size_t n,int flags,size_t cap,
                       const char *fmt,...){
    va_list va;va_start(va,fmt);
    int result=guest_vsnprintf_chk(dst,n,flags,cap,fmt,va);
    va_end(va);
    return result;
}
static int run_vsprintf_fixture(char *dst,int flags,size_t cap,
                               const char *fmt,...){
    va_list args;va_start(args,fmt);
    int ret=guest_vsprintf_chk(dst,flags,cap,fmt,args);
    va_end(args);return ret;
}
int lenneth_bionic_fortify_selftest(void){
    int ok=1;
    const char *sample="Lenneth";
    char dst[32]={0}, buffer[32]={0};
    if(guest_strlen_chk(sample,8)!=7)ok=0;
    if(guest_memcpy_chk(dst,sample,8,sizeof(dst))!=dst ||
       strcmp(dst,sample)!=0)ok=0;
    if(guest_memmove_chk(dst+1,dst,8,sizeof(dst)-1)!=dst+1 ||
       strcmp(dst+1,sample)!=0)ok=0;
    if(guest_strcat_chk(dst+1,"!",sizeof(dst)-1)!=dst+1 ||
       strcmp(dst+1,"Lenneth!")!=0)ok=0;
    if(guest_strncpy_chk(buffer,sample,7,sizeof(buffer))!=buffer ||
       memcmp(buffer,sample,7)!=0)ok=0;
    if(guest_strchr_chk(sample,'n',8)!=sample+2)ok=0;
    if(run_vsprintf_fixture(buffer,0,sizeof(buffer),"%s-%d","SYN",64)!=6 ||
       strcmp(buffer,"SYN-64")!=0)ok=0;
    if(format_test(buffer,sizeof(buffer),0,sizeof(buffer),"%s-%d","ARM64",32)!=8 ||
       strcmp(buffer,"ARM64-32")!=0)ok=0;
    int pipes[2];
    if(pipe(pipes)==0){
        const char payload[]="ABCDE";
        if(write(pipes[1],payload,sizeof(payload))!=(ssize_t)sizeof(payload))ok=0;
        char readbuf[16]={0};
        if(guest_read_chk(pipes[0],readbuf,sizeof(payload),sizeof(readbuf))!=
           (ssize_t)sizeof(payload) || memcmp(readbuf,payload,sizeof(payload)))ok=0;
        close(pipes[0]);close(pipes[1]);
    }else ok=0;
    int *p=guest_errno();
    *p=ERANGE;
    if(errno!=ERANGE)ok=0;
    uint64_t addr=0;
    if(!lenneth_bionic_fortify_lookup("__memcpy_chk",&addr)||!addr ||
       lenneth_bionic_fortify_lookup("__sF",&addr) ||
       lenneth_bionic_fortify_lookup("dlopen",&addr))ok=0;
    printf("[lenneth fortify] SELFTEST=%s allowlisted_functions=%zu "
           "Bionic_FILE_dynloader=NOT_MAPPED engine=not_executed\n",
           ok?"PASS":"FAIL",sizeof(symbols)/sizeof(symbols[0]));
    return ok?0:7;
}
