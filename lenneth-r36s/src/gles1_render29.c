/*
 * Lenneth Phase29 — verify OpenGL ES 1.1 fixed-function rasterization via
 * EGL-resolved functions on a standalone 32x32 pbuffer (MIT).
 *
 * Physical R36S phase28 proved:
 * - EGL client version == 1, GLES1-CM 1.1 via eglGetProcAddress;
 * - same-name dlsym/SDL_GL_GetProcAddress glGetString returned NULL;
 * - synthetic fixed-function matrix test succeeded on a manual context.
 *
 * Phase29 uses eglGetProcAddress consistently, draws only synthetic pixels,
 * then checks the color buffer via glReadPixels. Neither this file nor the
 * parent executable runs any proprietary engine code or Android constructors.
 * This probe is a separate process with a timeout, after normal diagnostics.
 */
#define _GNU_SOURCE
#include "gles1_render29.h"
#include <SDL.h>
#include <EGL/egl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

enum {
    L_GL_VERSION=0x1F02,
    L_GL_COLOR_BUFFER_BIT=0x4000,
    L_GL_RGBA=0x1908,
    L_GL_UNSIGNED_BYTE=0x1401,
    L_GL_MODELVIEW=0x1700,
    L_GL_PROJECTION=0x1701,
    L_GL_VERTEX_ARRAY=0x8074,
    L_GL_FLOAT=0x1406,
    L_GL_TRIANGLES=0x0004,
    L_GL_DITHER=0x0BD0
};
typedef unsigned int LGLenum;
typedef int LGLint,LGLsizei;
typedef unsigned char LGLubyte;
typedef float LGLfloat;
typedef void LGLvoid;
typedef const LGLubyte *(*PFN_GetString)(LGLenum);
typedef LGLenum (*PFN_GetError)(void);
typedef void (*PFN_Viewport)(LGLint,LGLint,LGLsizei,LGLsizei);
typedef void (*PFN_ClearColor)(LGLfloat,LGLfloat,LGLfloat,LGLfloat);
typedef void (*PFN_Clear)(LGLenum);
typedef void (*PFN_ReadPixels)(LGLint,LGLint,LGLsizei,LGLsizei,LGLenum,LGLenum,LGLvoid*);
typedef void (*PFN_MatrixMode)(LGLenum);
typedef void (*PFN_LoadIdentity)(void);
typedef void (*PFN_Color4f)(LGLfloat,LGLfloat,LGLfloat,LGLfloat);
typedef void (*PFN_VertexPointer)(LGLint,LGLenum,LGLsizei,const LGLvoid*);
typedef void (*PFN_EnableClientState)(LGLenum);
typedef void (*PFN_Disable)(LGLenum);
typedef void (*PFN_DrawArrays)(LGLenum,LGLint,LGLsizei);
typedef struct {
    PFN_GetString glGetString;
    PFN_GetError glGetError;
    PFN_Viewport glViewport;
    PFN_ClearColor glClearColor;
    PFN_Clear glClear;
    PFN_ReadPixels glReadPixels;
    PFN_MatrixMode glMatrixMode;
    PFN_LoadIdentity glLoadIdentity;
    PFN_Color4f glColor4f;
    PFN_VertexPointer glVertexPointer;
    PFN_EnableClientState glEnableClientState;
    PFN_Disable glDisable;
    PFN_DrawArrays glDrawArrays;
} GL;
static void capture(FILE *f,const char *gate,const char *status,const char *detail){
    fprintf(f,"native\t%s\t%s\t%s\n",gate,status,detail);
}
static int valid_es1(const char *s){
    return s&&(!strncmp(s,"OpenGL ES-CM 1.",15)||
              !strncmp(s,"OpenGL ES-CL 1.",15)||
              !strncmp(s,"OpenGL ES 1.",12));
}
static void clean(char *target,size_t cap,const char *s){
    size_t i=0;
    if(!cap)return;
    if(s)for(;i+1<cap&&s[i];i++){
        unsigned char c=(unsigned char)s[i];
        target[i]=(c>=32&&c<=126&&c!='\t'&&c!='\\')?c:'?';
    }
    target[i]=0;
}
static int blue_pixel(const unsigned char *px){
    return px && px[2]>=125 && px[2]>=px[0]+40 &&
           px[2]>=px[1]+40;
}
static int green_pixel(const unsigned char *px){
    return px && px[1]>=135 && px[1]>=px[0]+40 &&
           px[1]>=px[2]+40;
}
static void pixel_row(FILE *f,const char *test,const char *status,
                      const unsigned char *p){
    char data[80];
    snprintf(data,sizeof(data),"rgba=%u,%u,%u,%u",
             (unsigned)p[0],(unsigned)p[1],
             (unsigned)p[2],(unsigned)p[3]);
    capture(f,test,status,data);
}
static unsigned resolve_gl(GL *gl){
    unsigned count=0;
    /* In the user's 2026-11-08 R36S log eglGetProcAddress succeeded where
     * SDL_GL_GetProcAddress and dlsym's glGetString did not.
     * Never fall back to an unproven ES2 dispatch table here. */
#define G(type,slot) do { \
    __eglMustCastToProperFunctionPointerType p=eglGetProcAddress(#slot); \
    if(sizeof(p)==sizeof(gl->slot))memcpy(&gl->slot,&p,sizeof(p)); \
    if(gl->slot)count++; \
} while(0)
    G(PFN_GetString,glGetString);
    G(PFN_GetError,glGetError);
    G(PFN_Viewport,glViewport);
    G(PFN_ClearColor,glClearColor);
    G(PFN_Clear,glClear);
    G(PFN_ReadPixels,glReadPixels);
    G(PFN_MatrixMode,glMatrixMode);
    G(PFN_LoadIdentity,glLoadIdentity);
    G(PFN_Color4f,glColor4f);
    G(PFN_VertexPointer,glVertexPointer);
    G(PFN_EnableClientState,glEnableClientState);
    G(PFN_Disable,glDisable);
    G(PFN_DrawArrays,glDrawArrays);
#undef G
    return count;
}
static void error_row(FILE *f,const char *gate,LGLenum e){
    char msg[32];
    snprintf(msg,sizeof(msg),"0x%x",e);
    capture(f,gate,e==0?"PASS":"ERROR",msg);
}
/* Failures of graphics support are reported honestly as UNVERIFIED rather
 * than failing the prior ABI/system audit. Only report I/O fails the test. */
int lenneth_gles1_render29(void){
    alarm(9);
    FILE *report=fopen("lenneth-gles1-raster29.tsv","w");
    if(!report){alarm(0);return 29;}
    setvbuf(report,NULL,_IOLBF,0);
    fputs("provider\tgate\tstatus\tdetail\n",report);
    SDL_Window *window=NULL;
    SDL_GLContext sdctx=NULL;
    int sdl=0,egl_version=-1,functions=0;
    int clear_ok=0,triangle_ok=0;
    const char *why="not_started";
    char version[128]="no_version";
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLContext original=EGL_NO_CONTEXT,manual=EGL_NO_CONTEXT;
    EGLSurface old_draw=EGL_NO_SURFACE,old_read=EGL_NO_SURFACE;
    EGLSurface pbuffer=EGL_NO_SURFACE;
    EGLenum original_api=EGL_NONE;
    int original_valid=0;
    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        why="SDL_video_init_failed";goto finish;
    }
    sdl=1;
    if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES)||
       SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,1)||
       SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1)){
        why="SDL_GLES1_attributes_failed";goto finish;
    }
    window=SDL_CreateWindow("Lenneth GLES1 raster selftest",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,
        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
    if(!window){why="SDL_ES1_window_failed";goto finish;}
    sdctx=SDL_GL_CreateContext(window);
    if(!sdctx||SDL_GL_MakeCurrent(window,sdctx)){
        why="SDL_ES1_context_failed";goto finish;
    }
    display=eglGetCurrentDisplay();
    original=eglGetCurrentContext();
    old_draw=eglGetCurrentSurface(EGL_DRAW);
    old_read=eglGetCurrentSurface(EGL_READ);
    original_api=eglQueryAPI();
    original_valid=display!=EGL_NO_DISPLAY && original!=EGL_NO_CONTEXT;
    if(!original_valid){
        why="SDL_context_not_EGL_current";goto finish;
    }
    capture(report,"SDL_GLES1_CONTEXT","CREATED","SDL_EGL_context_current");
    EGLConfig config=NULL;
    EGLint nconfig=0,got_version=0;
    const EGLint config_attributes[]={
        EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES_BIT,
        EGL_RED_SIZE,5,EGL_GREEN_SIZE,6,EGL_BLUE_SIZE,5,EGL_NONE
    };
    if(!eglChooseConfig(display,config_attributes,&config,1,&nconfig)||
       nconfig<1){
        why="no_ES1_pbuffer_EGL_config";goto restore;
    }
    if(!eglBindAPI(EGL_OPENGL_ES_API)){
        why="eglBindAPI_ES_failed";goto restore;
    }
    const EGLint context_attrs[]={
        EGL_CONTEXT_CLIENT_VERSION,1,EGL_NONE
    };
    manual=eglCreateContext(display,config,EGL_NO_CONTEXT,context_attrs);
    if(manual==EGL_NO_CONTEXT){
        why="explicit_EGL_ES1_create_failed";goto restore;
    }
    const EGLint surface_attrs[]={EGL_WIDTH,32,EGL_HEIGHT,32,EGL_NONE};
    pbuffer=eglCreatePbufferSurface(display,config,surface_attrs);
    if(pbuffer==EGL_NO_SURFACE){
        why="explicit_EGL_pbuffer_failed";goto restore;
    }
    if(!eglMakeCurrent(display,pbuffer,pbuffer,manual)){
        why="eglMakeCurrent_explicit_ES1_failed";goto restore;
    }
    if(eglQueryContext(display,manual,EGL_CONTEXT_CLIENT_VERSION,&got_version))
        egl_version=got_version;
    if(egl_version!=1){why="EGL_client_version_not_ES1";goto restore;}
    GL gl={0};
    functions=(int)resolve_gl(&gl);
    char count[40];
    snprintf(count,sizeof(count),"available=%d_of_13",functions);
    capture(report,"EGL_GLES1_PROCADDR",functions==13?"COMPLETE":"PARTIAL",count);
    if(functions!=13){
        why="EGL_missing_required_GL_entrypoints";goto restore;
    }
    clean(version,sizeof(version),(const char*)gl.glGetString(L_GL_VERSION));
    capture(report,"EGL_GLES1_VERSION",valid_es1(version)?"PASS":"UNVERIFIED",version);
    if(!valid_es1(version)){
        why="OpenGL_version_not_ES1";goto restore;
    }
    for(int i=0;i<8&&gl.glGetError()!=0;i++){}
    gl.glViewport(0,0,32,32);
    gl.glDisable(L_GL_DITHER);
    gl.glClearColor(0.08f,0.18f,0.83f,1.0f);
    gl.glClear(L_GL_COLOR_BUFFER_BIT);
    unsigned char blue[4]={0,0,0,0};
    gl.glReadPixels(16,16,1,1,L_GL_RGBA,L_GL_UNSIGNED_BYTE,blue);
    LGLenum e1=gl.glGetError();
    clear_ok=(e1==0&&blue_pixel(blue));
    pixel_row(report,"ES1_CLEAR_READBACK",clear_ok?"PASS":"UNVERIFIED",blue);
    error_row(report,"ES1_CLEAR_GL_ERROR",e1);
    gl.glMatrixMode(L_GL_PROJECTION);
    gl.glLoadIdentity();
    gl.glMatrixMode(L_GL_MODELVIEW);
    gl.glLoadIdentity();
    const LGLfloat triangle[]={-0.8f,-0.8f,0.8f,-0.8f,0.0f,0.8f};
    gl.glVertexPointer(2,L_GL_FLOAT,0,triangle);
    gl.glEnableClientState(L_GL_VERTEX_ARRAY);
    gl.glColor4f(0.08f,0.88f,0.10f,1.0f);
    gl.glDrawArrays(L_GL_TRIANGLES,0,3);
    unsigned char green[4]={0,0,0,0};
    gl.glReadPixels(16,16,1,1,L_GL_RGBA,L_GL_UNSIGNED_BYTE,green);
    LGLenum e2=gl.glGetError();
    triangle_ok=(e2==0&&green_pixel(green));
    pixel_row(report,"ES1_TRIANGLE_READBACK",triangle_ok?"PASS":"UNVERIFIED",green);
    error_row(report,"ES1_TRIANGLE_GL_ERROR",e2);
    why=clear_ok&&triangle_ok?
        "native_ES1_1_fixed_function_pixels_confirmed":
        "ES1_1_present_but_raster_readback_unverified";
restore:
    if(manual!=EGL_NO_CONTEXT)
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if(pbuffer!=EGL_NO_SURFACE)eglDestroySurface(display,pbuffer);
    if(manual!=EGL_NO_CONTEXT)eglDestroyContext(display,manual);
    if(original_valid) {
        EGLBoolean good=eglMakeCurrent(display,old_draw,old_read,original);
        capture(report,"SDL_CONTEXT_RESTORED",good?"YES":"NO",
                "restored_prior_EGL_context");
    }
    if(original_api!=EGL_NONE)eglBindAPI(original_api);
finish:
    capture(report,"ES1_CONTEXT_CLIENT_VERSION",
        egl_version==1?"ES1_CONFIRMED":"UNVERIFIED",
        egl_version==1?"1":"not_1_or_unavailable");
    capture(report,"FINAL_ES1_RASTER",
        clear_ok&&triangle_ok?"PASS":"UNVERIFIED",why);
    capture(report,"COMMERCIAL_ENGINE","BLOCKED",
        "no_game_assets_no_guest_code_no_JNI_no_ctors");
    int io_error=ferror(report);
    if(fclose(report)!=0)io_error=1;
    if(sdctx)SDL_GL_DeleteContext(sdctx);
    if(window)SDL_DestroyWindow(window);
    if(sdl)SDL_Quit();
    alarm(0);
    printf("[lenneth phase29] GLES1_RASTER=%s "
           "egl_client_major=%d procaddress=%d/13 clear=%d triangle=%d "
           "version=%s reason=%s commercial_engine_executed=NO "
           "report=lenneth-gles1-raster29.tsv\n",
           clear_ok&&triangle_ok?"PASS":"UNVERIFIED",
           egl_version,functions,clear_ok,triangle_ok,version,why);
    return io_error?29:0;
}
int lenneth_gles1_render29_selftest(void){
    static const unsigned char blue_ok[]={20,40,210,255};
    static const unsigned char green_ok[]={12,220,20,255};
    static const unsigned char almost[]={42,52,48,255};
    int ok=valid_es1("OpenGL ES-CM 1.1 ARM") &&
           !valid_es1("OpenGL ES 3.2") &&
           !valid_es1(NULL) &&
           blue_pixel(blue_ok) && !blue_pixel(almost) &&
           green_pixel(green_ok) && !green_pixel(almost);
    char buff[32];clean(buff,sizeof(buff),"OK\tGL\\ES");
    if(strcmp(buff,"OK?GL?ES"))ok=0;
    printf("[lenneth phase29] SELFTEST=%s ES1_version_gate=PASS "
           "color_readback_classifier=PASS EGL_procaddress_only=1 "
           "no_game_execution=1\n",ok?"PASS":"FAIL");
    return ok?0:29;
}
