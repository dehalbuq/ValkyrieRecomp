/*
 * Lenneth phase26 — host GLES1 ABI availability and ELF dependency audit.
 * MIT; independent implementation. Never call any OpenGL routine or game code.
 * Presence of a .so or symbol does not prove a usable EGL ES1 context.
 */
#define _GNU_SOURCE
#include "gles1_probe26.h"
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned needed, fixed, all;
    int found;
} GameGL;
static int parse_details(const char *s,GameGL *g){
    unsigned needed=0,fixed=0,all=0;
    if(!s||!g)return 0;
    if(sscanf(s,"needs_es1=%u fixed_function_imports=%u total_gl_imports=%u",
              &needed,&fixed,&all)!=3)return 0;
    if(needed>100||fixed>100000||all>100000||fixed>all)return 0;
    g->needed=needed;g->fixed=fixed;g->all=all;g->found=1;
    return 1;
}
static int parse_phase25_plan(GameGL *game,GameGL *cxx){
    FILE *f=fopen("lenneth-phase25-startup-plan.tsv","rb");
    if(!f)return 0;
    char line[512];
    int ok=1;
    while(fgets(line,sizeof(line),f)){
        const char *module=NULL;
        GameGL *target=NULL;
        if(strncmp(line,"libMyGame.so\tGLES1_COMPATIBILITY\t",33)==0){
            module="libMyGame.so";target=game;
        }else if(strncmp(line,"libc++_shared.so\tGLES1_COMPATIBILITY\t",37)==0){
            module="libc++_shared.so";target=cxx;
        }
        if(!target)continue;
        if(target->found){ok=0;break;}
        const char *detail=strchr(line,'\t');
        if(!detail){ok=0;break;}
        detail=strchr(detail+1,'\t');
        if(!detail){ok=0;break;}
        detail=strchr(detail+1,'\t');
        if(!detail){ok=0;break;}
        detail++;
        if(!parse_details(detail,target)){ok=0;break;}
        (void)module;
    }
    if(ferror(f))ok=0;
    fclose(f);
    return ok&&game->found&&cxx->found;
}
static void row(FILE *f,const char *provider,const char *test,
                const char *status,const char *detail){
    fprintf(f,"%s\t%s\t%s\t%s\n",provider,test,status,detail);
}
static void *open_provider(const char *a,const char *b){
    void *handle=dlopen(a,RTLD_NOW|RTLD_LOCAL);
    if(!handle)handle=dlopen(b,RTLD_NOW|RTLD_LOCAL);
    return handle;
}
int lenneth_gles1_probe26(void){
    GameGL game={0},cxx={0};
    int has_plan=parse_phase25_plan(&game,&cxx);
    FILE *out=fopen("lenneth-gles1-runtime26.tsv","wb");
    if(!out){perror("lenneth-gles1-runtime26.tsv");return 26;}
    fputs("provider\tcapability\tstatus\tdetail\n",out);
    int available=0,complete=0;
    void *g1=open_provider("libGLESv1_CM.so.1","libGLESv1_CM.so");
    void *g2=open_provider("libGLESv2.so.2","libGLESv2.so");
    void *eg=open_provider("libEGL.so.1","libEGL.so");
    static const char *const sym[]={
        "glMatrixMode","glLoadIdentity","glVertexPointer",
        "glEnableClientState"
    };
    if(g1){
        available=1;
        row(out,"host","GLES1_SONAME","FOUND","libGLESv1_CM.so");
        for(size_t i=0;i<sizeof(sym)/sizeof(sym[0]);i++){
            int yes=dlsym(g1,sym[i])!=NULL;
            complete+=yes;
            row(out,"host",sym[i],yes?"FOUND":"MISSING",
                yes?"exported_function_pointer_only":"host_export_unavailable");
        }
    }else{
        row(out,"host","GLES1_SONAME","ABSENT","no_system_libGLESv1_CM_found");
        for(size_t i=0;i<sizeof(sym)/sizeof(sym[0]);i++)
            row(out,"host",sym[i],"NOT_TESTED","GLES1_library_missing");
    }
    row(out,"host","GLES2_SYMBOLS",
        g2&&dlsym(g2,"glCreateShader")&&dlsym(g2,"glDrawArrays")?"FOUND":"MISSING",
        "glCreateShader_and_glDrawArrays_without_call");
    row(out,"host","EGL_LOADER",
        eg&&dlsym(eg,"eglGetProcAddress")?"FOUND":"MISSING",
        "eglGetProcAddress_without_call");
    row(out,"host","GLES1_EGL_CONTEXT","NOT_TESTED",
        "host_soname_exports_do_not_prove_usable_ES1_context");
    char details[160];
    if(has_plan){
        snprintf(details,sizeof(details),"needed=%u fixed_imports=%u gl_imports=%u",
                 game.needed+ cxx.needed,game.fixed+cxx.fixed,
                 game.all+cxx.all);
        row(out,"guest","GLES1_ELF_IMPORTS","INVENTORIED",details);
        row(out,"guest","DYNAMIC_GLES1_SYMBOL_USAGE","UNKNOWN",
            "no_direct_fixed_imports_does_not_exclude_dlsym_calls");
    }else{
        row(out,"guest","GLES1_ELF_IMPORTS","INVALID",
            "phase25_plan_not_found_or_malformed");
    }
    row(out,"runtime","GLES1_ENGINE_BINDING","BLOCKED",
        "native_ES1_adapter_conditional_and_commercial_engine_NOT_EXECUTED");
    row(out,"runtime","JNI_CONSTRUCTORS","BLOCKED",
        "196_early_native_constructors_not_called");
    row(out,"runtime","COMMERCIAL_ENGINE","BLOCKED","no_guest_code_executed");
    int io_error=ferror(out);
    if(fclose(out)!=0)io_error=1;
    if(g1)dlclose(g1);
    if(g2)dlclose(g2);
    if(eg)dlclose(eg);
    const int passed=has_plan&&!io_error;
    printf("[lenneth phase26] GLES1_PROBE=%s gles1_available=%d "
           "essential_symbols=%d/4 game_needs_gles1=%u game_fixed_imports=%u "
           "host_context=NOT_TESTED jni=NOT_READY engine_executed=NO "
           "report=lenneth-gles1-runtime26.tsv\n",
           passed?"PASS":"FAIL",available,complete,
           has_plan?game.needed:0u,has_plan?game.fixed:0u);
    return passed?0:26;
}
int lenneth_gles1_probe26_selftest(void){
    GameGL g={0};
    int ok=parse_details("needs_es1=1 fixed_function_imports=0 total_gl_imports=96",&g) &&
           g.needed==1 && g.fixed==0 && g.all==96;
    if(parse_details("needs_es1=1 fixed_function_imports=7 total_gl_imports=2",&g)||
       parse_details("evil; rm -rf /",&g)||
       parse_details("needs_es1=1 fixed_function_imports=1 total_gl_imports=100001",&g))
        ok=0;
    printf("[lenneth phase26] SELFTEST=%s phase25_plan_parser=PASS "
           "allowlisted_sonames_only=PASS no_GL_calls=1 no_guest_execution=1\n",
           ok?"PASS":"FAIL");
    return ok?0:26;
}
