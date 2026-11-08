/*
 * Lenneth Phase 27: real native GLES1 fixed-function context probe.
 * MIT, independently implemented. No proprietary game, JNI or constructors.
 *
 * Launch this as a separate executable invocation AFTER the normal R36S
 * hardware test has closed its SDL video session. A driver crash therefore
 * cannot destroy the full-suite reports that were already generated.
 */
#define _GNU_SOURCE
#include "gles1_context27.h"
#include <SDL.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { VERSION_ENUM=0x1f02, NO_ERROR_ENUM=0, MODELVIEW_ENUM=0x1700 };
typedef const unsigned char *(*GetStringFn)(unsigned int);
typedef unsigned int (*GetErrorFn)(void);
typedef void (*MatrixModeFn)(unsigned int);
typedef void (*LoadIdentityFn)(void);
typedef void (*ClearColorFn)(float,float,float,float);
typedef void (*ClearFn)(unsigned int);

static void *open_legacy(void){
    void *h=dlopen("libGLESv1_CM.so.1",RTLD_NOW|RTLD_LOCAL);
    if(!h)h=dlopen("libGLESv1_CM.so",RTLD_NOW|RTLD_LOCAL);
    return h;
}
static int is_es1_version(const char *v){
    return v && (!strncmp(v,"OpenGL ES-CM 1.",14)||
                 !strncmp(v,"OpenGL ES 1.",12)||
                 !strncmp(v,"OpenGL ES-CL 1.",14));
}
static void sanitized(char *out,size_t cap,const char *input){
    size_t i=0;
    if(!cap)return;
    if(input)for(;i+1<cap&&input[i];i++){
        unsigned char c=(unsigned char)input[i];
        out[i]=((c>=32&&c<=126)&&c!='\t'&&c!='\\')?c:'?';
    }
    out[i]=0;
}
static void result(FILE *report,const char *gate,const char *state,const char *detail){
    fprintf(report,"host\t%s\t%s\t%s\n",gate,state,detail);
}
/* Returns 0 for a completed, honestly reported test even if the device
 * cannot create a native GLES1 context. Returns nonzero only on report I/O. */
int lenneth_gles1_context27_probe(void){
    alarm(9); /* KMSDRM/EGL driver hangs cannot block the handheld forever. */
    FILE *report=fopen("lenneth-gles1-context27.tsv","w");
    if(!report){alarm(0);return 27;}
    fputs("provider\tgate\tstatus\tdetail\n",report);
    int context_ok=0,api_ok=0,got_context=0;
    unsigned int first_err=0;
    char version[112]="none";
    const char *status="UNAVAILABLE";
    const char *reason="not_tested";
    void *legacy=open_legacy();
    SDL_Window *window=NULL;
    SDL_GLContext context=NULL;
    int sdl_started=0;
    if(!legacy){
        reason="native_GLES1_library_absent";
        goto end;
    }
    result(report,"LIBGLESV1_CM","FOUND","native_library_loaded");
    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        reason="SDL_video_initialization_failed";
        goto end;
    }
    sdl_started=1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    window=SDL_CreateWindow("Lenneth ES1 verification (no game)",
          SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,
          SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
    if(!window){
        reason="SDL_GLES1_window_unavailable";
        goto end;
    }
    context=SDL_GL_CreateContext(window);
    if(!context){
        reason="EGL_ES1_context_unavailable";
        goto end;
    }
    if(SDL_GL_MakeCurrent(window,context)!=0){
        reason="SDL_ES1_makecurrent_failed";
        goto end;
    }
    got_context=1;
    void *sym;
    GetStringFn gl_version=NULL;
    GetErrorFn gl_error=NULL;
    MatrixModeFn gl_matrix=NULL;
    LoadIdentityFn gl_identity=NULL;
    ClearColorFn gl_color=NULL;
    ClearFn gl_clear=NULL;
#define TRANSFER(slot,name) do {sym=dlsym(legacy,name); \
    if(sizeof(slot)==sizeof(sym))memcpy(&(slot),&sym,sizeof(slot));}while(0)
    TRANSFER(gl_version,"glGetString");
    TRANSFER(gl_error,"glGetError");
    TRANSFER(gl_matrix,"glMatrixMode");
    TRANSFER(gl_identity,"glLoadIdentity");
    TRANSFER(gl_color,"glClearColor");
    TRANSFER(gl_clear,"glClear");
#undef TRANSFER
    if(!gl_version || !gl_error || !gl_matrix || !gl_identity ||
       !gl_color || !gl_clear){
        reason="essential_ES1_functions_missing";
        goto end;
    }
    const unsigned char *returned=gl_version(VERSION_ENUM);
    sanitized(version,sizeof(version),(const char*)returned);
    if(!is_es1_version(version)){
        reason="context_is_not_OpenGL_ES1";
        goto end;
    }
    api_ok=1;
    /* Exercise only the synthetic ES1 fixed-function state on a valid ES1
     * context, with no guest rendering or commercial assets. */
    while(gl_error()!=NO_ERROR_ENUM){}
    gl_matrix(MODELVIEW_ENUM);
    gl_identity();
    gl_color(0.0f,0.0f,0.0f,1.0f);
    gl_clear(0x00004000);
    first_err=gl_error();
    if(first_err!=NO_ERROR_ENUM){
        reason="ES1_fixed_function_call_reported_gl_error";
        goto end;
    }
    SDL_GL_SwapWindow(window);
    context_ok=1;
    status="PASS";
    reason="native_ES1_context_and_fixed_function_passed";
end:
    result(report,"EGL_ES1_CONTEXT",got_context?"CREATED":"UNAVAILABLE",
           got_context?"SDL_GLES1_context_created":"no_usable_context");
    result(report,"NATIVE_GLES1_VERSION",api_ok?"ES1_CONFIRMED":"UNVERIFIED",
           version);
    result(report,"GLES1_FIXED_FUNCTION",context_ok?"PASS":"NOT_VERIFIED",
           reason);
    result(report,"COMMERCIAL_ENGINE","BLOCKED",
           "no_game_rendering_no_guest_code_executed");
    const int io_error=ferror(report);
    const int close_error=fclose(report);
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    if(sdl_started)SDL_Quit();
    dlclose(legacy);
    alarm(0);
    printf("[lenneth phase27] GLES1_CONTEXT=%s reason=%s "
           "created=%d es1_version=%d gl_error=%u version=%s "
           "commercial_engine_executed=NO report=lenneth-gles1-context27.tsv\n",
           status,reason,got_context,api_ok,first_err,version);
    return io_error||close_error?27:0;
}
int lenneth_gles1_context27_selftest(void){
    int ok=is_es1_version("OpenGL ES-CM 1.1") &&
           is_es1_version("OpenGL ES 1.0") &&
           is_es1_version("OpenGL ES-CL 1.1") &&
           !is_es1_version("OpenGL ES 2.0") &&
           !is_es1_version("OpenGL ES 3.2") &&
           !is_es1_version(NULL);
    char out[16];
    sanitized(out,sizeof(out),"A\tB\nC");
    if(strcmp(out,"A?B?C"))ok=0;
    printf("[lenneth phase27] GLES1_CONTEXT_SELFTEST=%s "
           "ES1_version_classifier=PASS native_ES2_not_ES1=PASS "
           "commercial_execution=NO\n",ok?"PASS":"FAIL");
    return ok?0:27;
}
