#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Independent Linux ARM64 bootstrap. Does not run the commercial game. */
int lenneth_elf_audit(const char *filename);
int lenneth_elf_map_preflight(const char *filename);
int lenneth_link_preflight(const char *game_path,const char *cxx_path);
int lenneth_host_plan(const char *game_path,const char *cxx_path);
int lenneth_portable_plan(const char *game_path,const char *cxx_path);

static const char *vs_src =
    "attribute vec2 a_pos; void main(){gl_Position=vec4(a_pos,0.0,1.0);}";
static const char *fs_src =
    "precision mediump float; uniform vec4 u_color;"
    "void main(){gl_FragColor=u_color;}";

static GLuint make_shader(GLenum kind,const char *source) {
    GLint ok=0;
    GLuint shader=glCreateShader(kind);
    glShaderSource(shader,1,&source,NULL);
    glCompileShader(shader);
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        GLchar log[2048]={0};
        glGetShaderInfoLog(shader,sizeof(log),NULL,log);
        fprintf(stderr,"[lenneth probe] GLSL error: %s\n",log);
        glDeleteShader(shader);return 0;
    }
    return shader;
}
static GLuint make_program(void) {
    GLuint v=make_shader(GL_VERTEX_SHADER,vs_src);
    GLuint f=make_shader(GL_FRAGMENT_SHADER,fs_src);
    if(!v||!f) {if(v)glDeleteShader(v);if(f)glDeleteShader(f);return 0;}
    GLuint p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);
    glBindAttribLocation(p,0,"a_pos");
    glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok) {
        char log[2048]={0};glGetProgramInfoLog(p,sizeof(log),NULL,log);
        fprintf(stderr,"[lenneth probe] program link: %s\n",log);
        glDeleteProgram(p);p=0;
    }
    glDeleteShader(v);glDeleteShader(f);
    return p;
}

static GLint color_uniform=-1;
static void draw_rect(float x,float y,float w,float h,
                      float r,float g,float b,float alpha) {
    GLfloat v[]={x,y,x+w,y,x,y+h,x+w,y,x+w,y+h,x,y+h};
    glUniform4f(color_uniform,r,g,b,alpha);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v);
    glDrawArrays(GL_TRIANGLES,0,6);
}
typedef struct {char ch;uint8_t rows[7];} Glyph;
static const Glyph glyphs[]={
    {' ',{0,0,0,0,0,0,0}},
    {'A',{14,17,17,31,17,17,17}},
    {'B',{30,17,17,30,17,17,30}},
    {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}},
    {'G',{14,17,16,23,17,17,14}},
    {'I',{31,4,4,4,4,4,31}},
    {'J',{7,2,2,2,18,18,12}},
    {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}},
    {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}},
    {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}},
    {'T',{31,4,4,4,4,4,4}},
    {'V',{17,17,17,17,17,10,4}},
    {'?',{14,17,1,2,4,0,4}},
    {'=',{0,0,31,0,31,0,0}}
};
static void draw_text(const char *str,float x,float y,float pixel) {
    for(;*str;str++) {
        const Glyph *gl=&glyphs[0];
        for(size_t i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);i++)
            if(glyphs[i].ch==*str){gl=&glyphs[i];break;}
        for(int iy=0;iy<7;iy++)for(int ix=0;ix<5;ix++)
            if(gl->rows[iy]&(1u<<(4-ix)))
                draw_rect(x+ix*pixel,y-iy*pixel,pixel,pixel,
                          0.98f,0.98f,0.93f,1.f);
        x+=pixel*6;
    }
}
static int raw_button(SDL_Joystick *joy,int button) {
    return joy && SDL_JoystickGetAttached(joy) &&
           SDL_JoystickNumButtons(joy)>button &&
           SDL_JoystickGetButton(joy,button)!=0;
}
static int mapped_button(SDL_GameController *pad,SDL_GameControllerButton b) {
    return pad && SDL_GameControllerGetAttached(pad) &&
           SDL_GameControllerGetButton(pad,b)!=0;
}
static void log_memory(void) {
    FILE *f=fopen("/proc/meminfo","r");
    if(!f)return;
    char buf[200];
    for(int i=0;i<4&&fgets(buf,sizeof(buf),f);i++)
        fprintf(stdout,"[lenneth memory] %s",buf);
    fclose(f);
}
static int video_probe(int seconds) {
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|
                SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO)!=0) {
        fprintf(stderr,"[lenneth probe] SDL_Init: %s\n",SDL_GetError());
        return 2;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window *win=SDL_CreateWindow("Lenneth ARM64 hardware probe",
       SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,
       SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
    if(!win){
        fprintf(stderr,"[lenneth probe] SDL_CreateWindow: %s\n",SDL_GetError());
        SDL_Quit();return 3;
    }
    SDL_GLContext glctx=SDL_GL_CreateContext(win);
    if(!glctx){
        fprintf(stderr,"[lenneth probe] SDL_GL_CreateContext: %s\n",SDL_GetError());
        SDL_DestroyWindow(win);SDL_Quit();return 4;
    }
    SDL_GL_MakeCurrent(win,glctx);
    SDL_GL_SetSwapInterval(0);
    fprintf(stdout,"[lenneth video] sdl_driver=%s gl_version=%s\n",
       SDL_GetCurrentVideoDriver()?SDL_GetCurrentVideoDriver():"(none)",
       glGetString(GL_VERSION)?(const char*)glGetString(GL_VERSION):"(null)");
    fprintf(stdout,"[lenneth video] vendor=%s renderer=%s glsl=%s\n",
       glGetString(GL_VENDOR)?(const char*)glGetString(GL_VENDOR):"(null)",
       glGetString(GL_RENDERER)?(const char*)glGetString(GL_RENDERER):"(null)",
       glGetString(GL_SHADING_LANGUAGE_VERSION)?
       (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION):"(null)");
    fflush(stdout);
    GLuint shader=make_program();
    if(!shader){
        SDL_GL_DeleteContext(glctx);SDL_DestroyWindow(win);SDL_Quit();
        return 5;
    }
    glUseProgram(shader);
    color_uniform=glGetUniformLocation(shader,"u_color");
    glEnableVertexAttribArray(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    SDL_GameController *pad=NULL;
    SDL_Joystick *raw=NULL;
    int n=SDL_NumJoysticks();
    fprintf(stdout,"[lenneth input] joysticks=%d\n",n);
    for(int i=0;i<n;i++){
        if(SDL_IsGameController(i)){
            pad=SDL_GameControllerOpen(i);
            if(pad){raw=SDL_GameControllerGetJoystick(pad);break;}
        }
        if(!raw){raw=SDL_JoystickOpen(i);if(raw)break;}
    }
    if(raw)fprintf(stdout,
        "[lenneth input] name=%s buttons=%d axes=%d mapped=%d\n",
        SDL_JoystickName(raw),SDL_JoystickNumButtons(raw),
        SDL_JoystickNumAxes(raw),pad!=NULL);
    SDL_AudioSpec desired;SDL_zero(desired);
    desired.freq=44100;desired.format=AUDIO_S16SYS;
    desired.channels=2;desired.samples=1024;
    SDL_AudioDeviceID audio=SDL_OpenAudioDevice(NULL,0,&desired,NULL,0);
    fprintf(stdout,"[lenneth audio] driver=%s device=%u %s\n",
       SDL_GetCurrentAudioDriver()?SDL_GetCurrentAudioDriver():"(none)",
       (unsigned)audio,audio?"opened":SDL_GetError());
    log_memory();
    fprintf(stdout,"[lenneth probe] Renderer/controls only. Commercial Android engine NOT loaded.\n");
    fprintf(stdout,"[lenneth probe] Hold START+SELECT 2500ms; A=exit; B=resume. Timeout=%d s.\n",seconds);
    fflush(stdout);
    Uint32 t0=SDL_GetTicks(),held=0;
    int menu=0,release=0,quit=0,previous_a=0,previous_b=0;
    int last_seconds=-1;
    while(!quit) {
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            if(ev.type==SDL_QUIT)quit=1;
            if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_F10) {
                menu=1;release=1;held=0;
            }
        }
        if(seconds>0 && SDL_GetTicks()-t0>=(Uint32)seconds*1000u)break;
        if(raw&&!SDL_JoystickGetAttached(raw)){
            fprintf(stdout,"[lenneth input] pad detached\n");
            raw=NULL;pad=NULL;
        }
        SDL_GameControllerUpdate();SDL_JoystickUpdate();
        const Uint8 *keys=SDL_GetKeyboardState(NULL);
        const int combo=(mapped_button(pad,SDL_CONTROLLER_BUTTON_START) &&
                         mapped_button(pad,SDL_CONTROLLER_BUTTON_BACK)) ||
                        (raw_button(raw,7)&&raw_button(raw,6)) ||
                        (raw_button(raw,9)&&raw_button(raw,8)) ||
                        (keys[SDL_SCANCODE_RETURN]&&keys[SDL_SCANCODE_BACKSPACE]);
        const int a=mapped_button(pad,SDL_CONTROLLER_BUTTON_A) ||
                    raw_button(raw,0) || keys[SDL_SCANCODE_RETURN];
        const int b=mapped_button(pad,SDL_CONTROLLER_BUTTON_B) ||
                    raw_button(raw,1) || keys[SDL_SCANCODE_ESCAPE];
        if(!menu) {
            if(combo){
                if(!held)held=SDL_GetTicks();
                if(SDL_GetTicks()-held>=2500){menu=1;release=1;
                    fprintf(stdout,"[lenneth exit] menu opened\n");}
            } else held=0;
        } else if(release){
            if(!combo&&!a&&!b)release=0;
        } else if(a&&!previous_a) {
            fprintf(stdout,"[lenneth exit] confirmed\n");quit=1;
        } else if(b&&!previous_b) {
            fprintf(stdout,"[lenneth exit] resume\n");menu=0;held=0;
        }
        previous_a=a;previous_b=b;
        int outw=640,outh=480;
        SDL_GL_GetDrawableSize(win,&outw,&outh);
        glViewport(0,0,outw,outh);
        glClearColor(0.035f,0.065f,0.11f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        /* Triangle rasterized by GLES2 GPU, not by a software framebuffer. */
        GLfloat triangle[]={-0.6f,-0.4f,0.6f,-0.4f,0.f,0.55f};
        glUniform4f(color_uniform,0.1f,0.7f,0.55f,1.f);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,triangle);
        glDrawArrays(GL_TRIANGLES,0,3);
        draw_rect(-0.98f,0.89f,1.96f,0.08f,0.11f,0.28f,0.55f,1.f);
        if(menu){
            draw_rect(-0.86f,-0.33f,1.72f,0.70f,0.03f,0.03f,0.08f,0.95f);
            draw_text("SAIR DO JOGO?",-0.73f,0.24f,0.015f);
            draw_text("A = SAIR",-0.73f,0.02f,0.015f);
            draw_text("B = VOLTAR",-0.73f,-0.18f,0.015f);
        }
        SDL_GL_SwapWindow(win);
        int elapsed=(int)((SDL_GetTicks()-t0)/1000u);
        if(elapsed!=last_seconds && elapsed%5==0){
            fprintf(stdout,"[lenneth probe] runtime=%d s menu=%d\n",elapsed,menu);
            last_seconds=elapsed;
        }
        SDL_Delay(16);
    }
    if(audio)SDL_CloseAudioDevice(audio);
    if(pad)SDL_GameControllerClose(pad);
    else if(raw)SDL_JoystickClose(raw);
    glDeleteProgram(shader);
    SDL_GL_DeleteContext(glctx);SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc==3 && strcmp(argv[1],"--inspect")==0)
        return lenneth_elf_audit(argv[2]);
    if(argc==3 && strcmp(argv[1],"--preflight")==0)
        return lenneth_elf_map_preflight(argv[2]);
    if(argc==4 && strcmp(argv[1],"--link-plan")==0)
        return lenneth_link_preflight(argv[2],argv[3]);
    if(argc==4 && strcmp(argv[1],"--host-plan")==0)
        return lenneth_host_plan(argv[2],argv[3]);
    if(argc==4 && strcmp(argv[1],"--portable-plan")==0)
        return lenneth_portable_plan(argv[2],argv[3]);
    if(argc>=2 && strcmp(argv[1],"--video")==0) {
        int duration=20;
        if(argc==4 && strcmp(argv[2],"--seconds")==0){
            duration=atoi(argv[3]);
            if(duration<0||duration>600)duration=20;
        }
        return video_probe(duration);
    }
    fprintf(stderr,"Usage: %s --inspect libMyGame.so | --preflight libMyGame.so | --link-plan libMyGame.so libc++_shared.so | --host-plan libMyGame.so libc++_shared.so | --portable-plan libMyGame.so libc++_shared.so | --video [--seconds 20]\n",argv[0]);
    return 2;
}
