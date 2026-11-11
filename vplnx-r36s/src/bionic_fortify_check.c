/* SPDX-License-Identifier: MIT
 * Synthetic FORTIFY ABI tests. No game code or proprietary libraries.
 */
#define _GNU_SOURCE 1
#include "bionic_fortify_linux.h"
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int chk_vsnprintf(char *buf,size_t len,int flags,size_t cap,const char *fmt,...) {
    va_list ap;
    va_start(ap,fmt);
    int rc=vpl_bf_vsnprintf(buf,len,flags,cap,fmt,ap);
    va_end(ap);
    return rc;
}
static int chk_vsprintf(char *buf,int flags,size_t cap,const char *fmt,...) {
    va_list ap;
    va_start(ap,fmt);
    int rc=vpl_bf_vsprintf(buf,flags,cap,fmt,ap);
    va_end(ap);
    return rc;
}
static void bad_overflow_strcat(void) {
    char dst[4]="abc";
    vpl_bf_strcat(dst,"d",sizeof(dst));
}
static void bad_overflow_strncpy(void) {
    char dst[4]={0};
    vpl_bf_strncpy(dst,"payload",8,sizeof(dst));
}
static void bad_strchr_unterminated(void) {
    const char a[3]={'A','B','C'};
    (void)vpl_bf_strchr(a,'B',sizeof(a));
}
static void bad_overflow_vsprintf(void) {
    char dst[5];
    (void)chk_vsprintf(dst,0,sizeof(dst),"%s","too-long");
}
static int verify_abort(void (*action)(void)) {
    pid_t pid=fork();
    if(pid<0) return -1;
    if(pid==0) {
        struct rlimit no_core={0,0};
        setrlimit(RLIMIT_CORE,&no_core);
        action();
        _exit(42); /* should be impossible */
    }
    int status=0;
    if(waitpid(pid,&status,0)!=pid)
        return -1;
    return WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT ? 0 : -1;
}
int main(void) {
    char dst[32]="hello";
    if (vpl_bf_strcat(dst," world",sizeof(dst))!=dst ||
        strcmp(dst,"hello world")!=0)
        return 1;
    if (vpl_bf_strncpy(dst,"ok",2,sizeof(dst))!=dst ||
        dst[0]!='o' || dst[1]!='k')
        return 2;
    if (vpl_bf_strchr("a:b",':',4)!=("a:b")+1) {
        /* The compiler need not merge identical literals; compare content. */
        if (!vpl_bf_strchr("a:b",':',4) ||
            *vpl_bf_strchr("a:b",':',4)!=':')
            return 3;
    }
    if(chk_vsnprintf(dst,sizeof(dst),0,sizeof(dst),"%d %s",42,"ok")!=5 ||
       strcmp(dst,"42 ok")!=0)
        return 4;
    if(chk_vsprintf(dst,0,sizeof(dst),"%s-%d","fmt",10)!=6 ||
       strcmp(dst,"fmt-10")!=0)
        return 5;
    int fds[2]={-1,-1};
    if(pipe(fds)!=0)
        return 6;
    if(write(fds[1],"123",3)!=3) return 7;
    close(fds[1]);
    if(vpl_bf_read(fds[0],dst,3,sizeof(dst))!=3 ||
       memcmp(dst,"123",3)!=0) return 8;
    close(fds[0]);

    if(verify_abort(bad_overflow_strcat) ||
       verify_abort(bad_overflow_strncpy) ||
       verify_abort(bad_strchr_unterminated) ||
       verify_abort(bad_overflow_vsprintf))
        return 9;
    puts("VPLNX_BIONIC_FORTIFY=PASS string_checks=PASS read_checks=PASS formatted_buffers=PASS violations=ABORT");
    puts("COMMERCIAL_ENGINE_EXECUTED=NO");
    return 0;
}
