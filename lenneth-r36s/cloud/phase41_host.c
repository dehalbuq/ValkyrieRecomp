/* Integration test of C++ compiler-generated guards through --wrap.
 * No commercial ELF, Bionic runtime or game constructor is loaded. */
#define _GNU_SOURCE
#include "cxx_guard41.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
typedef int (*Fn)(void);
typedef struct {Fn fn;pthread_barrier_t *barrier;int result;} Job;
static void *run(void *arg){
    Job *p=arg;
    pthread_barrier_wait(p->barrier);
    p->result=p->fn();
    return NULL;
}
int main(void){
    int ok=(lenneth_guard41_selftest()==0);
    void *lib=dlopen("./phase41_fixture.so",RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"Fixture dlopen: %s\n",dlerror());return 41;}
    void *sm=dlsym(lib,"phase41_fixture_marker"),
         *sv=dlsym(lib,"phase41_cpp_static"),
         *sc=dlsym(lib,"phase41_cpp_calls");
    Fn marker=NULL,value=NULL,calls=NULL;
    if(!sm||!sv||!sc || sizeof(sm)!=sizeof(marker))return 41;
    memcpy(&marker,&sm,sizeof(marker));
    memcpy(&value,&sv,sizeof(value));
    memcpy(&calls,&sc,sizeof(calls));
    if(marker()!=0x41424341)ok=0;
    enum { N=8 };
    Job jobs[N];pthread_t threads[N];pthread_barrier_t bar;
    if(pthread_barrier_init(&bar,NULL,N))return 41;
    for(int i=0;i<N;i++){
        jobs[i]=(Job){.fn=value,.barrier=&bar,.result=-1};
        if(pthread_create(&threads[i],NULL,run,&jobs[i]))return 41;
    }
    for(int i=0;i<N;i++){
        if(pthread_join(threads[i],NULL)!=0||jobs[i].result!=411)ok=0;
    }
    pthread_barrier_destroy(&bar);
    if(calls()!=1||value()!=411)ok=0;
    FILE *f=fopen("phase41_guard.tsv","w");
    if(!f)return 41;
    fprintf(f,"gate\tstatus\tdetail\n");
    fprintf(f,"CXX_GUARD_LIFETIME\t%s\titanium_64bit_guard_fixture\n",ok?"PASS":"FAIL");
    fprintf(f,"COMPILER_GENERATED_CXX_GUARD\t%s\tgnu_ld_wrap_no_host_interpose\n",ok?"PASS":"FAIL");
    fprintf(f,"CONCURRENT_STATIC_INITIALIZER\t%s\t8_threads_once\n",ok?"PASS":"FAIL");
    fprintf(f,"BIONIC_ANDROID_GUARD_LAYOUT\tUNVERIFIED\tcommercial_abi_version_unknown\n");
    fprintf(f,"COMMERCIAL_CONSTRUCTORS\tBLOCKED\tno_original_game_code\n");
    if(fclose(f)!=0)ok=0;
    printf("PHASE41_CPP_GUARD=%s concurrency=%s static_init_calls=%d "
           "game_executed=NO\n",ok?"PASS":"FAIL",ok?"PASS":"FAIL",calls());
    dlclose(lib);
    return ok?0:41;
}
