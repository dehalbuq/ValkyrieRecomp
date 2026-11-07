/*
 * ValkyrieRecomp R36S graphics capability/profiling tool.
 * Original GLES2 benchmark code, no DuckStation or commercial game sources.
 * Opens the real KMSDRM/EGL display to query the driver and exercise a
 * texture-sampling triangle raster pipeline. Separate from gameplay:
 * success here does NOT mean PS1 GPU primitives have been migrated.
 */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VRAM_W = 1024, VRAM_H = 512, TILES = 160, FRAMES = 90, REPEATS = 6 };
static const char *vs_src =
    "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;\n"
    "void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); }\n";
static const char *fs_src =
    "precision mediump float; varying vec2 vUV; uniform sampler2D uTex;\n"
    "void main(){ vec4 p=texture2D(uTex,vUV); gl_FragColor=vec4(p.rgb,1.0); }\n";
static GLuint make_shader(GLenum type, const char *src) {
    GLuint shader=glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader,1,&src,NULL); glCompileShader(shader);
    GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[2048]={0};
        glGetShaderInfoLog(shader,sizeof(log),NULL,log);
        fprintf(stderr,"[probe] shader failed: %s\n",log);
        glDeleteShader(shader);return 0;
    }
    return shader;
}
static GLuint program(void) {
    GLuint v=make_shader(GL_VERTEX_SHADER,vs_src), f=make_shader(GL_FRAGMENT_SHADER,fs_src);
    if (!v || !f) {if(v)glDeleteShader(v);if(f)glDeleteShader(f);return 0;}
    GLuint p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);
    glBindAttribLocation(p,0,"aPos");glBindAttribLocation(p,1,"aUV");
    glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]={0};glGetProgramInfoLog(p,sizeof(log),NULL,log);
        fprintf(stderr,"[probe] link failed: %s\n",log);glDeleteProgram(p);p=0;}
    glDeleteShader(v);glDeleteShader(f);return p;
}
static void emit(float *v,int *count,int i) {
    const int col=i%20,row=i/20;
    const float x=-1.0f+(float)col*0.098f;
    const float y=-1.0f+(float)row*0.23f;
    const float w=0.09f,h=0.20f;
    const float quad[6][4] = {
        {x,y,0,0},{x+w,y,1,0},{x,y+h,0,1},
        {x+w,y,1,0},{x+w,y+h,1,1},{x,y+h,0,1}
    };
    memcpy(v+(*count)*4,quad,sizeof(quad));*count+=6;
}
static int benchmark(void) {
    GLuint p=program(),texture=0,fbtex=0,fbo=0,vbo=0;
    int success=0;
    if(!p)return 0;
    static unsigned char pixels[256*256*4];
    for(int i=0;i<256*256;i++){
        pixels[i*4]= (unsigned char)(i&255);
        pixels[i*4+1]=(unsigned char)((i/256)&255);
        pixels[i*4+2]=(unsigned char)((i^((i/256)<<2))&255);
        pixels[i*4+3]=255;
    }
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenTextures(1,&fbtex);glBindTexture(GL_TEXTURE_2D,fbtex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,VRAM_W,VRAM_H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,fbtex,0);
    GLenum status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status!=GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr,"[probe] RGBA VRAM FBO incomplete: 0x%x\n",(unsigned)status);goto cleanup;
    }
    float verts[TILES*6*4];int count=0;
    for(int i=0;i<TILES;i++)emit(verts,&count,i);
    glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)sizeof(verts),verts,GL_STATIC_DRAW);
    glUseProgram(p);glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(const void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(const void*)(2*sizeof(float)));
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
    glUniform1i(glGetUniformLocation(p,"uTex"),0);
    glViewport(0,0,VRAM_W,VRAM_H);
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES,0,count);glFinish();
    const Uint64 t0=SDL_GetPerformanceCounter();
    for(int frame=0;frame<FRAMES;frame++){
        glClear(GL_COLOR_BUFFER_BIT);
        for(int rep=0;rep<REPEATS;rep++)
            glDrawArrays(GL_TRIANGLES,0,count);
    }
    glFinish();
    const Uint64 t1=SDL_GetPerformanceCounter(), freq=SDL_GetPerformanceFrequency();
    const double ms=freq?(double)(t1-t0)*1000.0/(double)freq:0.0;
    printf("[probe] GPU textured triangles=%d total; vramsize=%dx%d; %.3f ms total, %.3f ms/frame; %.1f synthetic frames/s\n",
        count/3*REPEATS*FRAMES, VRAM_W, VRAM_H, ms, ms/FRAMES, ms>0.0?(FRAMES*1000.0/ms):0.0);
    printf("[probe] NOTE: artificial GPU raster test only; NOT a Valkyrie Profile FPS figure.\n");
    fflush(stdout);
    success=1;
cleanup:
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(vbo)glDeleteBuffers(1,&vbo);
    if(fbo)glDeleteFramebuffers(1,&fbo);
    if(texture)glDeleteTextures(1,&texture);
    if(fbtex)glDeleteTextures(1,&fbtex);
    glDeleteProgram(p);
    return success;
}
static int try_context(int major,int minor) {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,minor);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window *win=SDL_CreateWindow("ValkyrieRecomp GLES probe",0,0,640,480,
                                     SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
    if(!win){fprintf(stderr,"[probe] ES%d.%d SDL_CreateWindow failed: %s\n",
                     major,minor,SDL_GetError());return 0;}
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    if(!ctx){fprintf(stderr,"[probe] ES%d.%d context failed: %s\n",
                    major,minor,SDL_GetError());SDL_DestroyWindow(win);return 0;}
    SDL_GL_MakeCurrent(win,ctx);SDL_GL_SetSwapInterval(0);
    printf("[probe] requested ES %d.%d; actual GL_VERSION=%s\n",major,minor,
        glGetString(GL_VERSION)?(const char*)glGetString(GL_VERSION):"(null)");
    printf("[probe] GL_VENDOR=%s GL_RENDERER=%s GL_SHADING_LANGUAGE_VERSION=%s\n",
        glGetString(GL_VENDOR)?(const char*)glGetString(GL_VENDOR):"(null)",
        glGetString(GL_RENDERER)?(const char*)glGetString(GL_RENDERER):"(null)",
        glGetString(GL_SHADING_LANGUAGE_VERSION)?(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION):"(null)");
    GLint max_tex=0,max_uniform=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_tex);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,&max_uniform);
    printf("[probe] MAX_TEXTURE_SIZE=%d MAX_FRAGMENT_UNIFORM_VECTORS=%d\n",max_tex,max_uniform);
    printf("[probe] ES%d.%d FBO test and triangle benchmark starting...\n",major,minor);
    const int ok=benchmark();
    SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(win);
    return ok;
}
int main(int argc,char**argv) {
    (void)argc;(void)argv;
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR,"1");
    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        fprintf(stderr,"[probe] SDL_Init VIDEO failed: %s\n",SDL_GetError());return 2;
    }
    printf("[probe] SDL_VIDEO_DRIVER=%s ENV SDL_RENDER_DRIVER=%s\n",
        SDL_GetCurrentVideoDriver()?SDL_GetCurrentVideoDriver():"(null)",
        getenv("SDL_RENDER_DRIVER")?getenv("SDL_RENDER_DRIVER"):"(unset)");
    printf("[probe] Driver software/hardware is taken from actual GL_RENDERER, not the SDL renderer hint.\n");
    int ok31=try_context(3,1);
    int ok30=try_context(3,0);
    int ok20=try_context(2,0);
    printf("[probe] ES3.1=%s ES3.0=%s ES2.0=%s\n",
        ok31?"benchmark-ok":"unavailable-or-failed",
        ok30?"benchmark-ok":"unavailable-or-failed",
        ok20?"benchmark-ok":"unavailable-or-failed");
    SDL_Quit();
    return (ok31||ok30||ok20)?0:3;
}
