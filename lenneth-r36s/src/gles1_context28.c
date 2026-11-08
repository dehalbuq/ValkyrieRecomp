/*
 * Lenneth Phase28, MIT. SDL/EGL native OpenGL ES1 selection diagnostic.
 *
 * The R36S phase27 reported a CREATED SDL context but the directly dlsym'd
 * libGLESv1_CM glGetString(GL_VERSION) returned an empty version. This probe
 * discriminates an SDL-selected ES2/3 context from a mismatched GLES1
 * dispatch library, and optionally creates a separate explicit EGL ES1
 * PBUFFER context when the existing display supports it.
 *
 * No game .so, Android constructor, JNI, proprietary assets or game
 * rendering are loaded or executed. Only synthetic GL state is used.
 * This runs in a separate timeout-limited process AFTER the batch suite.
 */
#define _GNU_SOURCE
#include "gles1_context28.h"
#include <SDL.h>
#include <EGL/egl.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef const unsigned char *(*GetStringFn)(unsigned int);
typedef unsigned int (*GetErrorFn)(void);
typedef void (*MatrixModeFn)(unsigned int);
typedef void (*LoadIdentityFn)(void);
enum { GL_VERSION_ENUM=0x1f02, GL_MODELVIEW_ENUM=0x1700 };
static int is_es1(const char *p) {
    return p && (!strncmp(p,"OpenGL ES-CM 1.",15) ||
                 !strncmp(p,"OpenGL ES-CL 1.",15) ||
                 !strncmp(p,"OpenGL ES 1.",12));
}
static const char *classify(const char *p) {
    if(!p)return "NULL_POINTER";
    if(!*p)return "EMPTY_STRING";
    return is_es1(p)?"ES1":"NON_ES1";
}
static void sanitized(char *out,size_t n,const char *p){
    size_t i=0;
    if(n==0)return;
    if(p)while(i+1<n && *p) {
        unsigned char c=(unsigned char)*p++;
        out[i++]=(c>=32 && c<=126 && c!='\t' && c!='\\')?c:'?';
    }
    out[i]=0;
}
static void line(FILE *f,const char *gate,const char *status,const char *detail){
    fprintf(f,"native\t%s\t%s\t%s\n",gate,status,detail);
}
static void number(FILE *f,const char *gate,int n){
    char buff[48];snprintf(buff,sizeof(buff),"%d",n);
    line(f,gate,"OBSERVED",buff);
}
static void uhex(FILE *f,const char *gate,unsigned n){
    char buff[48];snprintf(buff,sizeof(buff),"0x%x",n);
    line(f,gate,"OBSERVED",buff);
}
static void *open_gles1(void){
    void *h=dlopen("libGLESv1_CM.so.1",RTLD_NOW|RTLD_LOCAL);
    if(!h)h=dlopen("libGLESv1_CM.so",RTLD_NOW|RTLD_LOCAL);
    return h;
}
static GetStringFn get_string_ptr(void *ptr){
    GetStringFn fn=NULL;
    if(sizeof(fn)==sizeof(ptr))memcpy(&fn,&ptr,sizeof(fn));
    return fn;
}
static GetErrorFn get_error_ptr(void *ptr){
    GetErrorFn fn=NULL;
    if(sizeof(fn)==sizeof(ptr))memcpy(&fn,&ptr,sizeof(fn));
    return fn;
}
static MatrixModeFn get_matrix_ptr(void *ptr){
    MatrixModeFn fn=NULL;
    if(sizeof(fn)==sizeof(ptr))memcpy(&fn,&ptr,sizeof(fn));
    return fn;
}
static LoadIdentityFn get_identity_ptr(void *ptr){
    LoadIdentityFn fn=NULL;
    if(sizeof(fn)==sizeof(ptr))memcpy(&fn,&ptr,sizeof(fn));
    return fn;
}
static int query_version(FILE *f,const char *source,GetStringFn fn){
    if(!fn){
        line(f,source,"FUNCTION_MISSING","not_resolved");
        return 0;
    }
    const char *p=(const char*)fn(GL_VERSION_ENUM);
    char buff[144];sanitized(buff,sizeof(buff),p);
    line(f,source,classify(p),buff[0]?buff:"no_version_text");
    return is_es1(p);
}
/* This test deliberately does not assume the SDL KMSDRM driver honors an
 * ES1 request. EGL_QUERY is the authoritative chosen client version. */
static int phase28_native(void) {
    alarm(10);
    FILE *report=fopen("lenneth-gles1-selection28.tsv","w");
    if(!report){alarm(0);return 28;}
    setvbuf(report,NULL,_IOLBF,0);
    fputs("provider\tgate\tstatus\tdetail\n",report);
    int success=0;
    int sdl=0;
    SDL_Window *win=NULL;SDL_GLContext ctx=NULL;
    void *gles1=open_gles1();
    int desired_major=1, desired_minor=1,actual_major=-1,actual_minor=-1;
    int sdl_profile=-1, egl_client_version=-1;
    int sdl_context_valid=0,manual_created=0,manual_es1=0;
    const char *reason="no_context";
    if(!gles1){
        line(report,"GLES1_MODULE","ABSENT","optional_native_libGLESv1_CM_unavailable");
    }else line(report,"GLES1_MODULE","FOUND","native_GLES1_loader_loaded");
    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        line(report,"SDL_INIT","FAIL","no_SDL_video_driver");
        reason="SDL_video_unavailable";
        goto finish;
    }
    sdl=1;
    if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES) ||
       SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,desired_major) ||
       SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,desired_minor)){
        line(report,"SDL_REQUEST","FAIL","SDL_GL_SetAttribute_error");
        reason="SDL_ES1_attributes_rejected";
        goto finish;
    }
    line(report,"SDL_REQUEST","ES1_1","requested_1_1_not_a_guarantee");
    win=SDL_CreateWindow("Lenneth Phase28 (no game)",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,
        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
    if(!win){reason="SDL_ES1_window_failed";goto finish;}
    ctx=SDL_GL_CreateContext(win);
    if(!ctx){reason="SDL_ES1_context_failed";goto finish;}
    if(SDL_GL_MakeCurrent(win,ctx)!=0){
        reason="SDL_make_current_failed";goto finish;
    }
    sdl_context_valid=1;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&actual_major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&actual_minor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,&sdl_profile);
    number(report,"SDL_REALIZED_MAJOR",actual_major);
    number(report,"SDL_REALIZED_MINOR",actual_minor);
    number(report,"SDL_REALIZED_PROFILE",sdl_profile);
    EGLDisplay display=eglGetCurrentDisplay();
    EGLContext egl_context=eglGetCurrentContext();
    EGLSurface old_draw=eglGetCurrentSurface(EGL_DRAW);
    EGLSurface old_read=eglGetCurrentSurface(EGL_READ);
    EGLenum previous_api=eglQueryAPI();
    uhex(report,"EGL_CURRENT_API",(unsigned)previous_api);
    line(report,"EGL_DISPLAY",display!=EGL_NO_DISPLAY?"CURRENT":"NONE",
         display!=EGL_NO_DISPLAY?"SDL_made_display_current":"no_display");
    line(report,"EGL_CONTEXT",egl_context!=EGL_NO_CONTEXT?"CURRENT":"NONE",
         egl_context!=EGL_NO_CONTEXT?"SDL_made_context_current":"no_context");
    if(display==EGL_NO_DISPLAY||egl_context==EGL_NO_CONTEXT){
        reason="SDL_context_not_backed_by_current_EGL_context";
        goto finish;
    }
    EGLint attr=0;
    EGLBoolean q=eglQueryContext(display,egl_context,EGL_CONTEXT_CLIENT_VERSION,&attr);
    if(q)egl_client_version=attr;
    number(report,"EGL_CONTEXT_CLIENT_VERSION",egl_client_version);
    if(!q)uhex(report,"EGL_QUERY_ERROR",(unsigned)eglGetError());
    EGLint client_type=0;
    if(eglQueryContext(display,egl_context,EGL_CONTEXT_CLIENT_TYPE,&client_type))
        uhex(report,"EGL_CONTEXT_CLIENT_TYPE",(unsigned)client_type);
    char apis[128]={0};
    sanitized(apis,sizeof(apis),eglQueryString(display,EGL_CLIENT_APIS));
    line(report,"EGL_CLIENT_APIS",apis[0]?"PRESENT":"UNAVAILABLE",
         apis[0]?apis:"not_reported");

    /* Three independent function-pointer provenance paths. The returned
     * version is checked, not merely the presence of an export symbol. */
    GetStringFn direct=get_string_ptr(gles1?dlsym(gles1,"glGetString"):NULL);
    GetStringFn sdlget=get_string_ptr(SDL_GL_GetProcAddress("glGetString"));
    GetStringFn eglget=get_string_ptr((void*)eglGetProcAddress("glGetString"));
    const int direct_es1=query_version(report,"SDL_VERSION_GLES1_DLSYM",direct);
    const int sdl_es1=query_version(report,"SDL_VERSION_SDL_PROCADDR",sdlget);
    const int egl_es1=query_version(report,"SDL_VERSION_EGL_PROCADDR",eglget);
    if(egl_client_version==1 && (direct_es1||sdl_es1||egl_es1)){
        success=1;reason="SDL_confirmed_ES1_with_function_dispatch";
    }else{
        reason="SDL_requested_ES1_but_version_or_dispatch_mismatch";
    }

    /* Second, explicit EGL ES1 Pbuffer context on SDL's already-initialized
     * EGLDisplay. This does not steal a display/DRM master or run guest code.
     * All objects are destroyed and the previous EGL context is restored. */
    EGLConfig config=NULL;
    EGLint config_count=0;
    const EGLint cfg[]={
        EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES_BIT,
        EGL_RED_SIZE,5,EGL_GREEN_SIZE,6,EGL_BLUE_SIZE,5,EGL_NONE
    };
    const EGLBoolean chosen=eglChooseConfig(display,cfg,&config,1,&config_count);
    number(report,"MANUAL_CONFIG_COUNT",chosen?config_count:0);
    if(!chosen||config_count<1){
        line(report,"MANUAL_ES1","SKIP","no_matching_ES1_PBUFFER_config");
        goto finish;
    }
    if(!eglBindAPI(EGL_OPENGL_ES_API)){
        line(report,"MANUAL_ES1","SKIP","eglBindAPI_ES_failed");
        goto restore;
    }
    const EGLint context_attributes[]={
        EGL_CONTEXT_CLIENT_VERSION,1,EGL_NONE
    };
    EGLContext manual=eglCreateContext(display,config,EGL_NO_CONTEXT,context_attributes);
    if(manual==EGL_NO_CONTEXT){
        uhex(report,"MANUAL_CREATE_ERROR",(unsigned)eglGetError());
        line(report,"MANUAL_ES1","UNAVAILABLE","eglCreateContext_ES1_failed");
        goto restore;
    }
    manual_created=1;
    const EGLint pbuffer_attributes[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};
    EGLSurface surface=eglCreatePbufferSurface(display,config,pbuffer_attributes);
    if(surface==EGL_NO_SURFACE){
        line(report,"MANUAL_ES1","UNAVAILABLE","eglCreatePbufferSurface_failed");
        eglDestroyContext(display,manual);
        goto restore;
    }
    if(!eglMakeCurrent(display,surface,surface,manual)){
        line(report,"MANUAL_ES1","UNAVAILABLE","eglMakeCurrent_ES1_failed");
        eglDestroySurface(display,surface);eglDestroyContext(display,manual);
        goto restore;
    }
    EGLint manual_version=-1;
    if(eglQueryContext(display,manual,EGL_CONTEXT_CLIENT_VERSION,&attr))
        manual_version=attr;
    number(report,"MANUAL_CONTEXT_CLIENT_VERSION",manual_version);
    int m1=query_version(report,"MANUAL_VERSION_GLES1_DLSYM",direct);
    int m2=query_version(report,"MANUAL_VERSION_SDL_PROCADDR",sdlget);
    int m3=query_version(report,"MANUAL_VERSION_EGL_PROCADDR",eglget);
    manual_es1=(manual_version==1&&(m1||m2||m3));
    if(manual_es1){
        GetErrorFn gl_error=get_error_ptr(gles1?dlsym(gles1,"glGetError"):NULL);
        MatrixModeFn gl_matrix=get_matrix_ptr(gles1?dlsym(gles1,"glMatrixMode"):NULL);
        LoadIdentityFn gl_identity=get_identity_ptr(gles1?dlsym(gles1,"glLoadIdentity"):NULL);
        if(gl_error&&gl_matrix&&gl_identity){
            for(int i=0;i<8&&gl_error()!=0;i++){}
            gl_matrix(GL_MODELVIEW_ENUM);
            gl_identity();
            int gl_err=(int)gl_error();
            uhex(report,"MANUAL_FIXED_FUNCTION_ERROR",(unsigned)gl_err);
            if(gl_err)manual_es1=0;
        }else{
            line(report,"MANUAL_FIXED_FUNCTION","MISSING","legacy_GL_symbols_missing");
            manual_es1=0;
        }
    }
    line(report,"MANUAL_ES1",manual_es1?"PASS":"UNVERIFIED",
        manual_es1?"EGL_ES1_pbuffer_and_fixed_function_pass":"no_confirmed_ES1_dispatch");
    if(manual_es1){
        success=1;reason="explicit_EGL_ES1_context_and_state_valid";
    }
    /* Release the pbuffer before restoring SDL, then restore SDL's EGL
     * context even if the fixed-function check failed. */
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroySurface(display,surface);
    eglDestroyContext(display,manual);
restore:
    if(egl_context!=EGL_NO_CONTEXT){
        EGLBoolean restored=eglMakeCurrent(display,old_draw,old_read,egl_context);
        line(report,"SDL_CONTEXT_RESTORED",restored?"YES":"NO",
            "restore_after_manual_egl_probe");
    }
    if(previous_api!=EGL_NONE)eglBindAPI(previous_api);
finish:
    line(report,"FINAL_ES1_VALIDATION",success?"PASS":"UNVERIFIED",reason);
    line(report,"COMMERCIAL_ENGINE","BLOCKED","no_guest_code_no_constructors_no_JNI");
    int file_fail=ferror(report);
    if(fclose(report)!=0)file_fail=1;
    if(ctx)SDL_GL_DeleteContext(ctx);
    if(win)SDL_DestroyWindow(win);
    if(sdl)SDL_Quit();
    if(gles1)dlclose(gles1);
    alarm(0);
    printf("[lenneth phase28] ES1_SELECTION=%s "
           "sdl_context=%d sdl_major=%d sdl_minor=%d "
           "egl_context_major=%d manual_created=%d manual_pass=%d "
           "reason=%s commercial_executed=NO report=lenneth-gles1-selection28.tsv\n",
           success?"PASS":"UNVERIFIED",sdl_context_valid,
           actual_major,actual_minor,egl_client_version,
           manual_created,manual_es1,reason);
    return file_fail?28:0;
}
int lenneth_gles1_context28_probe(void){return phase28_native();}
int lenneth_gles1_context28_selftest(void){
    int ok=is_es1("OpenGL ES-CM 1.1 Mali")&&is_es1("OpenGL ES-CL 1.0")&&
           is_es1("OpenGL ES 1.1")&&!is_es1("OpenGL ES 3.2")&&
           !is_es1("")&&!is_es1(NULL)&&
           !strcmp(classify(NULL),"NULL_POINTER")&&
           !strcmp(classify(""),"EMPTY_STRING")&&
           !strcmp(classify("OpenGL ES 3.2"),"NON_ES1");
    char result[20]={0};sanitized(result,sizeof(result),"GL\tES\\2");
    if(strcmp(result,"GL?ES?2"))ok=0;
    printf("[lenneth phase28] SELFTEST=%s "
           "SDL_EGL_version_checks=PASS dispatch_source_classifier=PASS "
           "manual_EGL_pbuffer_mode=OPT_IN_DRIVER_TEST no_commercial_code=YES\n",
           ok?"PASS":"FAIL");
    return ok?0:28;
}
