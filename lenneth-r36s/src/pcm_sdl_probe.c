/* Lenneth R36S Phase 16: native SDL2 PCM preflight, MIT.
 * Independent fixture only; never executes proprietary Android code.
 * Playback queue draining does not prove audible speaker output. */
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
int lenneth_pcm_sdl_selftest(void) {
    const int own_audio=SDL_WasInit(SDL_INIT_AUDIO)==0;
    SDL_AudioDeviceID dev=0;
    int16_t *samples=NULL;
    int pass=1,opened=0,submitted=0,drained=0;
    const int frames=11025;
    if (own_audio && SDL_InitSubSystem(SDL_INIT_AUDIO)!=0) {
        fprintf(stderr,"[lenneth pcm] init: %s\n",SDL_GetError());
        pass=0;goto done;
    }
    SDL_AudioSpec request,received;
    SDL_zero(request);SDL_zero(received);
    request.freq=44100;request.format=AUDIO_S16SYS;
    request.channels=2;request.samples=512;
    dev=SDL_OpenAudioDevice(NULL,0,&request,&received,0);
    if (!dev) {
        fprintf(stderr,"[lenneth pcm] open: %s\n",SDL_GetError());
        pass=0;goto done;
    }
    opened=1;
    if(received.freq!=44100 || received.format!=AUDIO_S16SYS ||
       received.channels!=2) {pass=0;goto done;}
    samples=(int16_t*)malloc(frames*2*sizeof(int16_t));
    if(!samples) {pass=0;goto done;}
    SDL_PauseAudioDevice(dev,1);
    for(int burst=0;burst<2;burst++) {
        int period=burst?67:100;
        for(int i=0;i<frames;i++) {
            int fade=i<256?i:(i>=frames-256?frames-1-i:256);
            int wave=(i%period)<(period/2)?5000:-5000;
            int16_t v=(int16_t)(wave*fade/256);
            samples[2*i]=v;samples[2*i+1]=v;
        }
        if(SDL_QueueAudio(dev,samples,(Uint32)(frames*2*sizeof(int16_t)))!=0){
            fprintf(stderr,"[lenneth pcm] enqueue: %s\n",SDL_GetError());
            pass=0;goto done;
        }
        submitted++;
    }
    if(SDL_GetQueuedAudioSize(dev)!=(Uint32)(2*frames*2*sizeof(int16_t))) {
        pass=0;goto done;
    }
    SDL_PauseAudioDevice(dev,0);
    Uint32 start=SDL_GetTicks();
    while(SDL_GetTicks()-start<3000) {
        if(SDL_GetQueuedAudioSize(dev)==0) {drained=1;break;}
        SDL_Delay(10);
    }
    if(!drained)pass=0;
done:
    printf("[lenneth pcm] PCM_SDL_SELFTEST=%s audio_driver=%s "
           "device=%s buffers_queued=%d queue_drained=%d "
           "mode=SDL_QUEUED commercial_execution=NO\n",
           pass?"PASS":"FAIL",
           SDL_GetCurrentAudioDriver()?SDL_GetCurrentAudioDriver():"(none)",
           opened?"opened":"unavailable",submitted,drained);
    if(dev)SDL_CloseAudioDevice(dev);
    free(samples);
    if(own_audio)SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return pass?0:16;
}
