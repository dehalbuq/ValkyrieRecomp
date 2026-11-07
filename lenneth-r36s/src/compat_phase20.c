/* Lenneth phase20: restricted process priority/syscall compatibility.
 * MIT; independently authored for AArch64 Bionic->Linux preflight.
 * Unknown syscalls return ENOSYS. No system(), shell, setjmp or phdr emulation.
 * No commercial game code invoked. */
#define _GNU_SOURCE
#include "compat_phase20.h"
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

/* Priority changes would affect the handheld's runtime. Accept only a
 * request that leaves this process's existing priority unchanged. */
static int guest_setpriority(int which,id_t who,int priority){
    if(which!=PRIO_PROCESS || (who!=0 && who!=(id_t)getpid())){
        errno=EPERM;return -1;
    }
    errno=0;
    int current=getpriority(PRIO_PROCESS,0);
    if(current==-1 && errno)return -1;
    if(current!=priority){errno=EPERM;return -1;}
    return 0;
}
/* Linux AArch64 and Android arm64 share kernel syscall numbers, but not
 * arbitrary libc syscall ABI contracts. Only zero-argument, scalar,
 * non-mutating calls are allowed. No varargs read for rejected calls. */
static long guest_syscall(long number,...){
    switch(number){
#ifdef SYS_gettid
        case SYS_gettid:return (long)syscall(SYS_gettid);
#endif
#ifdef SYS_getpid
        case SYS_getpid:return (long)getpid();
#endif
#ifdef SYS_getppid
        case SYS_getppid:return (long)getppid();
#endif
#ifdef SYS_getuid
        case SYS_getuid:return (long)getuid();
#endif
#ifdef SYS_geteuid
        case SYS_geteuid:return (long)geteuid();
#endif
#ifdef SYS_getgid
        case SYS_getgid:return (long)getgid();
#endif
#ifdef SYS_getegid
        case SYS_getegid:return (long)getegid();
#endif
        default:errno=ENOSYS;return -1;
    }
}
typedef struct {const char *name;void(*fn)(void);} Entry;
#define X(name,fn) {name,(void(*)(void))(fn)}
static const Entry entries[]={
    X("setpriority",guest_setpriority),X("syscall",guest_syscall)
};
int lenneth_phase20_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
        if(strcmp(name,entries[i].name)!=0)continue;
        void(*fn)(void)=entries[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));return 1;
    }
    return 0;
}
int lenneth_phase20_selftest(void){
    int ok=1;
    errno=0;int nice=getpriority(PRIO_PROCESS,0);
    if(nice==-1 && errno!=0)ok=0;
    if(guest_setpriority(PRIO_PROCESS,0,nice)!=0)ok=0;
    errno=0;
    if(guest_setpriority(PRIO_PROCESS,0,nice==19?18:nice+1)!=-1 ||
       errno!=EPERM)ok=0;
    errno=0;
    if(guest_setpriority(PRIO_PGRP,0,nice)!=-1 || errno!=EPERM)ok=0;
    errno=0;
    if(guest_setpriority(PRIO_PROCESS,(id_t)getpid()+1,nice)!=-1 ||
       errno!=EPERM)ok=0;
#ifdef SYS_gettid
    if(guest_syscall(SYS_gettid)!=(long)syscall(SYS_gettid))ok=0;
#endif
#ifdef SYS_getpid
    if(guest_syscall(SYS_getpid)!=(long)getpid())ok=0;
#endif
#ifdef SYS_getuid
    if(guest_syscall(SYS_getuid)!=(long)getuid())ok=0;
#endif
    errno=0;
    if(guest_syscall(0x7fffffffL)!=-1 || errno!=ENOSYS)ok=0;
    uint64_t p=0;
    if(!lenneth_phase20_lookup("setpriority",&p)||!p ||
       !lenneth_phase20_lookup("syscall",&p)||!p ||
       lenneth_phase20_lookup("__sF",&p) ||
       lenneth_phase20_lookup("system",&p) ||
       lenneth_phase20_lookup("dl_iterate_phdr",&p) ||
       lenneth_phase20_lookup("setjmp",&p))ok=0;
    printf("[lenneth phase20] SELFTEST=%s process_priority=UNCHANGED "
           "syscall_policy=READONLY_ALLOWLIST unknown_syscalls=ENOSYS "
           "shim_symbols=%zu commercial_execution=NO\n",ok?"PASS":"FAIL",
           sizeof(entries)/sizeof(entries[0]));
    return ok?0:20;
}
