/* SPDX-License-Identifier: MIT
 * Synthetic C++ ABI lifecycle test: destructor ordering, DSO isolation,
 * reentrancy, exactly-once callbacks and random canary.
 * No real game code or APK dependency.
 */
#include "bionic_cxa_linux.h"
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int order[64], n=0;
static int dso1=1, dso2=2;
static void append(void *arg) {
    if(n<(int)(sizeof(order)/sizeof(order[0])))
        order[n++]=(int)(intptr_t)arg;
}
static void nested(void *arg) {
    append(arg);
    vpl_bcxa_finalize(&dso2);
}
static int fail(const char *what) {
    fprintf(stderr,"VPLNX_CXA=FAIL case=%s\n",what);
    return 1;
}
int main(void) {
    uintptr_t addr=vpl_bionic_stack_guard_address();
    uintptr_t same=vpl_bionic_stack_guard_address();
    if(!addr || same!=addr || !*(uintptr_t*)addr ||
       *(uintptr_t*)addr==UINT64_MAX)
        return fail("stack guard entropy");
    if(vpl_bcxa_atexit(NULL,NULL,NULL)==0)
        return fail("null destructor accepted");
    if(vpl_bcxa_atexit(append,(void*)(intptr_t)1,&dso1) ||
       vpl_bcxa_atexit(append,(void*)(intptr_t)2,&dso2) ||
       vpl_bcxa_atexit(nested,(void*)(intptr_t)3,&dso1))
        return fail("registration");
    if(vpl_bcxa_pending()!=3)
        return fail("pending registration");
    vpl_bcxa_finalize(&dso1);
    /* DSO1 executes callback #3 and recursively finalizes DSO2 #2,
     * then resumes DSO1 callback #1. */
    if(n!=3 || order[0]!=3 || order[1]!=2 || order[2]!=1)
        return fail("order/reentrance");
    if(vpl_bcxa_pending()!=0)
        return fail("dtor called more than once");
    vpl_bcxa_finalize(NULL);
    vpl_bcxa_finalize(&dso1);
    if(n!=3)
        return fail("double finalization");
    if(vpl_bcxa_atexit(append,(void*)(intptr_t)4,&dso1) ||
       vpl_bcxa_atexit(append,(void*)(intptr_t)5,&dso2))
        return fail("second registration");
    vpl_bcxa_finalize(NULL);
    if(n!=5 || order[3]!=5 || order[4]!=4)
        return fail("process finalization order");
    if(vpl_bcxa_pending()!=0)
        return fail("pending after all");
    printf("VPLNX_CXA_ABI=PASS dso_isolation=1 reverse_order=1 reentrant=1 exactly_once=1\n");
    printf("VPLNX_BIONIC_STACK_GUARD=PASS random_nonzero=1 stable_process_address=1\n");
    puts("COMMERCIAL_ENGINE_EXECUTED=NO");
    return 0;
}
