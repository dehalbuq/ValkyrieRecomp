/*
 * Lenneth phase23, early AArch64 Bionic nonlocal jump bridge. MIT.
 * This module implements a narrow adapter for the Android 5-era 256-byte
 * jmp_buf with 0x53657200/1 signature. It must NEVER invoke glibc setjmp.
 * Shell/Game engine never executed in this diagnostic.
 *
 * The assembly preserves x19..x30, SP and d8..d15 in the legacy arrangement.
 * Newer Android Bionic uses randomized cookies / mangling / checksums, which
 * this phase refuses to claim it supports.
 */
#define _GNU_SOURCE
#include "compat_phase23.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>

typedef struct {uint32_t magic, saved_mask;uint64_t slots[31];} LegacyBionicJmp;
_Static_assert(sizeof(LegacyBionicJmp)==256,"legacy Android arm64 jmp_buf=256");

extern int lenneth_legacy_setjmp_arm64(void*) __attribute__((returns_twice));
extern void lenneth_legacy_longjmp_arm64(void*,int) __attribute__((noreturn));

/* These helpers only run on the current thread, never modify other threads.
 * Legacy Bionic sigblock()/sigsetmask() stored the low 32 mask bits. */
uint32_t lenneth_phase23_get_mask(void){
    sigset_t mask;
    if(sigprocmask(SIG_SETMASK,NULL,&mask)!=0)abort();
    uint32_t low=0;
    memcpy(&low,&mask,sizeof(low));
    return low;
}
void lenneth_phase23_restore_mask(uint32_t old_low){
    sigset_t mask;
    if(sigprocmask(SIG_SETMASK,NULL,&mask)!=0)abort();
    memcpy(&mask,&old_low,sizeof(old_low));
    if(sigprocmask(SIG_SETMASK,&mask,NULL)!=0)abort();
}
/* A corrupt saved environment is fatal rather than producing a guest
 * controllable stack pointer or return address. */
void lenneth_phase23_bad_jump(void){
    fputs("[lenneth phase23] fatal invalid legacy Bionic jump context\n",stderr);
    abort();
}
typedef struct {const char *name;void (*fn)(void);} Entry;
#define SYM(name,fn) {name,(void (*)(void))(fn)}
static const Entry entries[]={
    SYM("setjmp",lenneth_legacy_setjmp_arm64),
    SYM("longjmp",lenneth_legacy_longjmp_arm64)
};
int lenneth_phase23_legacy_jmp_lookup(const char *name,uint64_t *value){
    if(!name||!value)return 0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
        if(strcmp(name,entries[i].name)!=0)continue;
        void(*fn)(void)=entries[i].fn;
        if(sizeof(fn)!=sizeof(*value))return 0;
        memcpy(value,&fn,sizeof(fn));
        return 1;
    }
    return 0;
}
/* Real nonlocal jump from a distinct stack frame, never game code. */
__attribute__((noinline))
static void nested_jump(void *buffer,int value){
    lenneth_legacy_longjmp_arm64(buffer,value);
}
static int masked_signal(int signal){
    sigset_t mask;
    if(sigprocmask(SIG_SETMASK,NULL,&mask)!=0)return -1;
    return sigismember(&mask,signal);
}
static int test_jump_value(int jump_value){
    _Alignas(16) LegacyBionicJmp buffer;
    memset(&buffer,0xa6,sizeof(buffer));
    volatile int count=0;
    int result=lenneth_legacy_setjmp_arm64(&buffer);
    if(result==0){
        count++;
        nested_jump(&buffer,jump_value);
    }
    if(result!=(jump_value?jump_value:1) || count!=1 ||
       buffer.magic!=0x53657201u)return 0;
    return 1;
}
/* Check the saved thread mask has been restored after longjmp. */
static int test_mask_restore(void){
    _Alignas(16) LegacyBionicJmp buffer;
    sigset_t before,add,original;
    if(sigprocmask(SIG_SETMASK,NULL,&original)!=0)return 0;
    if(sigemptyset(&before)||sigaddset(&before,SIGUSR1))return 0;
    if(sigprocmask(SIG_UNBLOCK,&before,NULL)!=0)return 0;
    int expected=masked_signal(SIGUSR1);
    if(expected!=0){sigprocmask(SIG_SETMASK,&original,NULL);return 0;}
    volatile int jumped=0;
    int value=lenneth_legacy_setjmp_arm64(&buffer);
    if(value==0){
        sigemptyset(&add);
        sigaddset(&add,SIGUSR1);
        if(sigprocmask(SIG_BLOCK,&add,NULL)!=0)return 0;
        if(masked_signal(SIGUSR1)!=1)return 0;
        jumped=1;
        nested_jump(&buffer,7);
    }
    int ok=(value==7 && jumped==1 && masked_signal(SIGUSR1)==0);
    if(sigprocmask(SIG_SETMASK,&original,NULL)!=0)ok=0;
    return ok;
}
static int test_bad_magic(void){
    pid_t child=fork();
    if(child<0)return 0;
    if(child==0){
        /* Device safety: abort remains confined to this child, without
         * creating a large crash dump on the handheld's memory card. */
        struct rlimit no_core={.rlim_cur=0,.rlim_max=0};
        (void)setrlimit(RLIMIT_CORE,&no_core);
        struct sigaction ignore={0};
        ignore.sa_handler=SIG_DFL;
        sigaction(SIGABRT,&ignore,NULL);
        _Alignas(16) LegacyBionicJmp bad={0};
        lenneth_legacy_longjmp_arm64(&bad,1);
        _exit(111);
    }
    int code=0;
    if(waitpid(child,&code,0)!=child)return 0;
    return WIFSIGNALED(code) && WTERMSIG(code)==SIGABRT;
}
int lenneth_phase23_legacy_probe(void){
    /* All these are standalone tests on our own program's stack. */
    return test_jump_value(19) && test_jump_value(0) &&
           test_mask_restore() && test_bad_magic();
}
int lenneth_phase23_legacy_jmp_selftest(void){
    uint64_t addr=0;
    int ok=lenneth_phase23_legacy_probe();
    if(!lenneth_phase23_legacy_jmp_lookup("setjmp",&addr)||!addr ||
       !lenneth_phase23_legacy_jmp_lookup("longjmp",&addr)||!addr ||
       lenneth_phase23_legacy_jmp_lookup("__sF",&addr) ||
       lenneth_phase23_legacy_jmp_lookup("sigsetjmp",&addr) ||
       lenneth_phase23_legacy_jmp_lookup("system",&addr))ok=0;
    printf("[lenneth phase23] SELFTEST=%s bionic_jmpbuf_bytes=256 "
           "architecture=arm64 legacy_magic=0x53657201 "
           "nested_jump=TESTED zero_returns_one=TESTED "
           "signal_mask_restore=TESTED malformed_magic=ABORTED "
           "modern_cookie_checksum=UNMAPPED commercial_execution=NO\n",
           ok?"PASS":"FAIL");
    return ok?0:23;
}
