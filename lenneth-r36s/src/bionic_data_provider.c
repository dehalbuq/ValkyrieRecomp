/*
 * MIT: independent random AArch64 stack-guard data import for Bionic.
 * A true Linux process-wide random u64 object is used rather than an
 * Android-vs-glibc object layout cast. No guest code is executed.
 */
#define _GNU_SOURCE
#include "bionic_data_provider.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/random.h>

static uint64_t stack_guard;
static int stack_guard_ready;
static pthread_once_t init_once=PTHREAD_ONCE_INIT;
static void initialize(void){
    ssize_t n=getrandom(&stack_guard,sizeof(stack_guard),0);
    if(n!=(ssize_t)sizeof(stack_guard)){
        int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
        if(fd>=0){
            n=read(fd,&stack_guard,sizeof(stack_guard));
            close(fd);
        }
    }
    stack_guard_ready=(n==(ssize_t)sizeof(stack_guard)&&stack_guard!=0);
}
int lenneth_bionic_data_lookup(const char *name,uint64_t *out){
    if(!name||!out || strcmp(name,"__stack_chk_guard")!=0)return 0;
    pthread_once(&init_once,initialize);
    if(!stack_guard_ready)return 0;
    *out=(uint64_t)(uintptr_t)&stack_guard;
    return 1;
}
int lenneth_bionic_data_selftest(void){
    uint64_t p=0;
    int found=lenneth_bionic_data_lookup("__stack_chk_guard",&p);
    int blocked=!lenneth_bionic_data_lookup("__sF",&p) &&
                !lenneth_bionic_data_lookup("SL_IID_ENGINE",&p);
    uint64_t q=0;
    int stable=lenneth_bionic_data_lookup("__stack_chk_guard",&q)&&q!=0;
    int ok=found&&blocked&&stable&&q==(uint64_t)(uintptr_t)&stack_guard
            &&stack_guard!=0;
    printf("[lenneth bionic-data] SELFTEST=%s entropy=nonzero "
           "stack_guard=process_local Bionic_FILE_and_OpenSLIID=NOT_MAPPED "
           "commercial_execution=NO\n",ok?"PASS":"FAIL");
    return ok?0:7;
}
