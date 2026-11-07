/*
 * Lenneth R36S: experimental OpenSL ES 1.0.1 ABI surface (phase 15).
 * Independently written; MIT. Definitions/order follow the Khronos
 * OpenSLES 1.0.1 C API (Object, Engine, Play, BufferQueue, Volume).
 *
 * IMPORTANT: This is a *non-playing*, purely synthetic ABI test backend.
 * A queued PCM buffer is held in a bounded local buffer; test-only pump
 * consumes one buffer and fires a registered callback. No real ALSA/SDL
 * PCM playback or guest callbacks occur in the normal linker preflight.
 * This is NOT a complete Android OpenSL ES runtime.
 */
#include "opensl_bridge.h"
#include <elf.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t SLresult;
typedef uint32_t SLuint32;
typedef uint32_t SLboolean;
typedef int16_t SLmillibel;
typedef struct SLUUID {
    uint32_t low;
    uint16_t middle,version,clock;
    uint8_t node[6];
} SLUUID;
typedef const SLUUID *SLInterfaceID;
/* These are exported POINTER DATA variables, as in the Android NDK ABI.
 * Their addresses, NOT their UUID addresses, are used for ELF relocations.
 * Only pointer identities are currently used for interface lookup.
 */
static const SLUUID uuid_engine={0x31534c01,0x534c,0x1001,0x7001,{1,2,3,4,5,1}};
static const SLUUID uuid_play  ={0x31534c02,0x534c,0x1001,0x7002,{1,2,3,4,5,2}};
static const SLUUID uuid_queue ={0x31534c03,0x534c,0x1001,0x7003,{1,2,3,4,5,3}};
static const SLUUID uuid_volume={0x31534c04,0x534c,0x1001,0x7004,{1,2,3,4,5,4}};
const SLInterfaceID SL_IID_ENGINE=&uuid_engine;
const SLInterfaceID SL_IID_PLAY=&uuid_play;
const SLInterfaceID SL_IID_BUFFERQUEUE=&uuid_queue;
const SLInterfaceID SL_IID_VOLUME=&uuid_volume;

enum { SL_OK=0,SL_PRECONDITION=1,SL_INVALID=2,SL_NO_MEMORY=3,
       SL_BUFFER_INSUFFICIENT=7,SL_UNSUPPORTED=12 };
enum { OBJ_UNREALIZED=1,OBJ_REALIZED=2,PLAY_STOPPED=1,PLAY_PAUSED=2,
       PLAY_PLAYING=3,LOC_OUTPUT_MIX=4,LOC_BUFFER_QUEUE=6,FMT_PCM=2 };
enum { MAX_OBJECTS=64,MAX_QUEUE=16,MAX_PCM_BUFFER=262144 };
struct ObjectV;
struct EngineV;
struct PlayV;
struct QueueV;
struct VolumeV;
typedef const struct ObjectV * const *ObjectItf;
typedef const struct EngineV * const *EngineItf;
typedef const struct PlayV * const *PlayItf;
typedef const struct QueueV * const *QueueItf;
typedef const struct VolumeV * const *VolumeItf;
typedef void(*BufferCallback)(QueueItf,void*);
typedef struct {
    SLuint32 locatorType,numBuffers;
} QueueLocator;
typedef struct {
    SLuint32 formatType,channels,sampleRate,bits,container,mask,endianness;
} PcmFormat;
typedef struct {
    SLuint32 locatorType;
    ObjectItf mix;
} MixLocator;
typedef struct {void *locator,*format;} DataEndpoint;
typedef struct {
    SLuint32 count,playIndex;
} QueueState;
typedef struct {
    uint32_t id,data;
} EngineOption;
typedef struct OpenSLObject {
    const struct ObjectV *objectV;
    const struct EngineV *engineV;
    const struct PlayV *playV;
    const struct QueueV *queueV;
    const struct VolumeV *volumeV;
    int active,kind;
    SLuint32 state,playState,capacity,head,count,index;
    SLmillibel volume;
    SLboolean muted;
    void *buffers[MAX_QUEUE];
    uint32_t sizes[MAX_QUEUE];
    BufferCallback callback;
    void *callbackCtx;
} OpenSLObject;
enum { KIND_ENGINE=1,KIND_MIX,KIND_PLAYER };

struct ObjectV {
    SLresult (*Realize)(ObjectItf,SLboolean);
    void (*Resume_unused)(void);
    SLresult (*GetState)(ObjectItf,SLuint32*);
    SLresult (*GetInterface)(ObjectItf,SLInterfaceID,void*);
    void (*RegisterCallback_unused)(void);
    void (*AbortAsyncOperation_unused)(void);
    void (*Destroy)(ObjectItf);
    void (*SetPriority_unused)(void);
    void (*GetPriority_unused)(void);
    void (*SetLossOfControlInterfaces_unused)(void);
};
/* Engine position 2 is CreateAudioPlayer; position 7 CreateOutputMix. */
struct EngineV {
    void (*CreateLEDDevice_unused)(void);
    void (*CreateVibraDevice_unused)(void);
    SLresult (*CreateAudioPlayer)(EngineItf,ObjectItf*,DataEndpoint*,DataEndpoint*,
                                  SLuint32,const SLInterfaceID*,const SLboolean*);
    void (*CreateAudioRecorder_unused)(void);
    void (*CreateMidiPlayer_unused)(void);
    void (*CreateListener_unused)(void);
    void (*Create3DGroup_unused)(void);
    SLresult (*CreateOutputMix)(EngineItf,ObjectItf*,SLuint32,const SLInterfaceID*,
                                 const SLboolean*);
    void (*CreateMetadataExtractor_unused)(void);
    void (*CreateExtensionObject_unused)(void);
    void (*QueryNumSupportedInterfaces_unused)(void);
    void (*QuerySupportedInterfaces_unused)(void);
    void (*QueryNumSupportedExtensions_unused)(void);
    void (*QuerySupportedExtension_unused)(void);
    void (*IsExtensionSupported_unused)(void);
};
struct PlayV {
    SLresult (*SetPlayState)(PlayItf,SLuint32);
    SLresult (*GetPlayState)(PlayItf,SLuint32*);
    void (*GetDuration_unused)(void);
    void (*GetPosition_unused)(void);
    void (*RegisterCallback_unused)(void);
    void (*SetCallbackEventsMask_unused)(void);
    void (*GetCallbackEventsMask_unused)(void);
    void (*SetMarkerPosition_unused)(void);
    void (*ClearMarkerPosition_unused)(void);
    void (*GetMarkerPosition_unused)(void);
    void (*SetPositionUpdatePeriod_unused)(void);
    void (*GetPositionUpdatePeriod_unused)(void);
};
struct QueueV {
    SLresult (*Enqueue)(QueueItf,const void*,SLuint32);
    SLresult (*Clear)(QueueItf);
    SLresult (*GetState)(QueueItf,QueueState*);
    SLresult (*RegisterCallback)(QueueItf,BufferCallback,void*);
};
struct VolumeV {
    SLresult (*SetVolumeLevel)(VolumeItf,SLmillibel);
    SLresult (*GetVolumeLevel)(VolumeItf,SLmillibel*);
    SLresult (*GetMaxVolumeLevel)(VolumeItf,SLmillibel*);
    SLresult (*SetMute)(VolumeItf,SLboolean);
    SLresult (*GetMute)(VolumeItf,SLboolean*);
    void (*EnableStereoPosition_unused)(void);
    void (*IsEnabledStereoPosition_unused)(void);
    void (*SetStereoPosition_unused)(void);
    void (*GetStereoPosition_unused)(void);
};

static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static OpenSLObject *objects[MAX_OBJECTS];
static size_t object_count;
static const struct ObjectV object_methods;
static const struct EngineV engine_methods;
static const struct PlayV play_methods;
static const struct QueueV queue_methods;
static const struct VolumeV volume_methods;
static OpenSLObject *find_handle(const void *handle,int kind){
    if(!handle)return NULL;
    for(size_t i=0;i<object_count;i++){
        OpenSLObject *o=objects[i];
        if(!o||!o->active)continue;
        if(kind==0 && handle==&o->objectV)return o;
        if(kind==KIND_ENGINE && handle==&o->engineV && o->kind==KIND_ENGINE)return o;
        if(kind==KIND_PLAYER && o->kind==KIND_PLAYER &&
           (handle==&o->playV || handle==&o->queueV || handle==&o->volumeV))
            return o;
    }
    return NULL;
}
static void queue_clear(OpenSLObject *o){
    for(int i=0;i<MAX_QUEUE;i++){free(o->buffers[i]);o->buffers[i]=NULL;o->sizes[i]=0;}
    o->head=o->count=0;
}
static OpenSLObject *make_object(int kind){
    pthread_mutex_lock(&mutex);
    if(object_count>=MAX_OBJECTS){pthread_mutex_unlock(&mutex);return NULL;}
    OpenSLObject *o=(OpenSLObject*)calloc(1,sizeof(*o));
    if(o){
        o->objectV=&object_methods;
        o->engineV=&engine_methods;
        o->playV=&play_methods;
        o->queueV=&queue_methods;
        o->volumeV=&volume_methods;
        o->active=1;o->kind=kind;o->state=OBJ_UNREALIZED;
        o->playState=PLAY_STOPPED;o->capacity=MAX_QUEUE;
        o->volume=0;objects[object_count++]=o;
    }
    pthread_mutex_unlock(&mutex);
    return o;
}
static SLresult obj_realize(ObjectItf itf,SLboolean async){
    (void)async;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,0);
    if(!o){pthread_mutex_unlock(&mutex);return SL_INVALID;}
    o->state=OBJ_REALIZED;
    pthread_mutex_unlock(&mutex);
    return SL_OK;
}
static SLresult obj_state(ObjectItf itf,SLuint32 *state){
    if(!state)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,0);
    if(o)*state=o->state;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult obj_interface(ObjectItf itf,SLInterfaceID iid,void *result){
    if(!result || !iid)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,0);
    if(!o){pthread_mutex_unlock(&mutex);return SL_INVALID;}
    if(o->state!=OBJ_REALIZED){pthread_mutex_unlock(&mutex);return SL_PRECONDITION;}
    SLresult rc=SL_OK;
    if(o->kind==KIND_ENGINE && iid==SL_IID_ENGINE)
        *(EngineItf*)result=&o->engineV;
    else if(o->kind==KIND_PLAYER && iid==SL_IID_PLAY)
        *(PlayItf*)result=&o->playV;
    else if(o->kind==KIND_PLAYER && iid==SL_IID_BUFFERQUEUE)
        *(QueueItf*)result=&o->queueV;
    else if(o->kind==KIND_PLAYER && iid==SL_IID_VOLUME)
        *(VolumeItf*)result=&o->volumeV;
    else rc=SL_UNSUPPORTED;
    pthread_mutex_unlock(&mutex);
    return rc;
}
static void obj_destroy(ObjectItf itf){
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,0);
    if(o){o->active=0;queue_clear(o);}
    pthread_mutex_unlock(&mutex);
    /* Keep token addresses alive to make later stale handles invalid;
       no commercial guest has ever called these interfaces. */
}
static SLresult eng_mix(EngineItf itf,ObjectItf *out,SLuint32 n,
                       const SLInterfaceID *ids,const SLboolean *required){
    (void)ids;(void)required;
    if(!out || n>0)return n?SL_UNSUPPORTED:SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *eng=find_handle(itf,KIND_ENGINE);
    int ready=eng&&eng->state==OBJ_REALIZED;
    pthread_mutex_unlock(&mutex);
    if(!ready)return SL_PRECONDITION;
    OpenSLObject *mix=make_object(KIND_MIX);
    if(!mix)return SL_NO_MEMORY;
    *out=&mix->objectV;
    return SL_OK;
}
static SLresult eng_player(EngineItf itf,ObjectItf *out,DataEndpoint *source,
                            DataEndpoint *sink,SLuint32 n,
                            const SLInterfaceID *ids,const SLboolean *req){
    (void)ids;(void)req;
    if(!out||!source||!sink||!source->locator||!source->format||!sink->locator)
        return SL_INVALID;
    if(n>3)return SL_UNSUPPORTED;
    pthread_mutex_lock(&mutex);
    OpenSLObject *eng=find_handle(itf,KIND_ENGINE);
    int ready=eng&&eng->state==OBJ_REALIZED;
    pthread_mutex_unlock(&mutex);
    if(!ready)return SL_PRECONDITION;
    const QueueLocator *ql=(const QueueLocator*)source->locator;
    const PcmFormat *pcm=(const PcmFormat*)source->format;
    const MixLocator *mix=(const MixLocator*)sink->locator;
    if(ql->locatorType!=LOC_BUFFER_QUEUE || !ql->numBuffers ||
       ql->numBuffers>MAX_QUEUE || pcm->formatType!=FMT_PCM ||
       (pcm->channels!=1 && pcm->channels!=2) ||
       pcm->bits!=16 || pcm->container!=16 ||
       pcm->sampleRate<8000000 || pcm->sampleRate>48000000 ||
       pcm->endianness!=2 || mix->locatorType!=LOC_OUTPUT_MIX)
        return SL_UNSUPPORTED;
    pthread_mutex_lock(&mutex);
    OpenSLObject *mixobj=find_handle(mix->mix,0);
    int mix_ready=mixobj && mixobj->kind==KIND_MIX &&
                  mixobj->state==OBJ_REALIZED;
    pthread_mutex_unlock(&mutex);
    if(!mix_ready)return SL_PRECONDITION;
    OpenSLObject *player=make_object(KIND_PLAYER);
    if(!player)return SL_NO_MEMORY;
    player->capacity=ql->numBuffers;
    *out=&player->objectV;
    return SL_OK;
}
static SLresult play_set(PlayItf itf,SLuint32 state){
    if(state!=PLAY_STOPPED && state!=PLAY_PAUSED &&
       state!=PLAY_PLAYING)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)o->playState=state;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult play_get(PlayItf itf,SLuint32 *state){
    if(!state)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)*state=o->playState;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult queue_enqueue(QueueItf itf,const void *buffer,SLuint32 size){
    if(!buffer || !size || size>MAX_PCM_BUFFER)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(!o){pthread_mutex_unlock(&mutex);return SL_INVALID;}
    if(o->count>=o->capacity){pthread_mutex_unlock(&mutex);return SL_BUFFER_INSUFFICIENT;}
    void *copy=malloc(size);
    if(!copy){pthread_mutex_unlock(&mutex);return SL_NO_MEMORY;}
    memcpy(copy,buffer,size);
    unsigned pos=(o->head+o->count)%MAX_QUEUE;
    o->buffers[pos]=copy;
    o->sizes[pos]=size;o->count++;
    pthread_mutex_unlock(&mutex);return SL_OK;
}
static SLresult queue_clear_method(QueueItf itf){
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)queue_clear(o);
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult queue_state(QueueItf itf,QueueState *state){
    if(!state)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o){state->count=o->count;state->playIndex=o->index;}
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult queue_callback(QueueItf itf,BufferCallback callback,void *context){
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o){o->callback=callback;o->callbackCtx=context;}
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
/* Synthetic-only pump, NOT a playback callback or a real audio engine. */
static int synthetic_consume(QueueItf itf){
    BufferCallback cb=NULL;void *ctx=NULL;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(!o || !o->count){pthread_mutex_unlock(&mutex);return 0;}
    free(o->buffers[o->head]);o->buffers[o->head]=NULL;
    o->head=(o->head+1)%MAX_QUEUE;o->count--;o->index++;
    cb=o->callback;ctx=o->callbackCtx;
    pthread_mutex_unlock(&mutex);
    if(cb)cb(itf,ctx);
    return 1;
}
static SLresult vol_set(VolumeItf itf,SLmillibel value){
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)o->volume=value;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult vol_get(VolumeItf itf,SLmillibel *value){
    if(!value)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)*value=o->volume;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult vol_max(VolumeItf itf,SLmillibel *value){
    if(!value)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)*value=0;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult vol_mute(VolumeItf itf,SLboolean value){
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)o->muted=value?1:0;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static SLresult vol_getmute(VolumeItf itf,SLboolean *value){
    if(!value)return SL_INVALID;
    pthread_mutex_lock(&mutex);
    OpenSLObject *o=find_handle(itf,KIND_PLAYER);
    if(o)*value=o->muted;
    pthread_mutex_unlock(&mutex);
    return o?SL_OK:SL_INVALID;
}
static const struct ObjectV object_methods={
    .Realize=obj_realize,.GetState=obj_state,.GetInterface=obj_interface,
    .Destroy=obj_destroy
};
static const struct EngineV engine_methods={
    .CreateAudioPlayer=eng_player,.CreateOutputMix=eng_mix
};
static const struct PlayV play_methods={
    .SetPlayState=play_set,.GetPlayState=play_get
};
static const struct QueueV queue_methods={
    .Enqueue=queue_enqueue,.Clear=queue_clear_method,
    .GetState=queue_state,.RegisterCallback=queue_callback
};
static const struct VolumeV volume_methods={
    .SetVolumeLevel=vol_set,.GetVolumeLevel=vol_get,
    .GetMaxVolumeLevel=vol_max,.SetMute=vol_mute,.GetMute=vol_getmute
};
static SLresult bridge_create_engine(ObjectItf *out,SLuint32 numOptions,
                                     const EngineOption *options,
                                     SLuint32 numInterfaces,
                                     const SLInterfaceID *ids,
                                     const SLboolean *required){
    (void)options;(void)ids;(void)required;
    if(!out)return SL_INVALID;
    *out=NULL;
    if(numOptions>0 || numInterfaces>0)return SL_UNSUPPORTED;
    OpenSLObject *o=make_object(KIND_ENGINE);
    if(!o)return SL_NO_MEMORY;
    *out=&o->objectV;
    return SL_OK;
}
int lenneth_opensl_lookup(const char *name,uint8_t symbol_type,uint64_t *out){
    if(!name||!out)return 0;
    /* An ELF STT_OBJECT/NOTYPE import addresses a DATA variable
       containing a SLInterfaceID UUID pointer, NOT a UUID or function. */
    if(symbol_type==STT_OBJECT || symbol_type==STT_NOTYPE){
        if(!strcmp(name,"SL_IID_ENGINE")) {*out=(uint64_t)(uintptr_t)&SL_IID_ENGINE;return 1;}
        if(!strcmp(name,"SL_IID_PLAY")) {*out=(uint64_t)(uintptr_t)&SL_IID_PLAY;return 1;}
        if(!strcmp(name,"SL_IID_BUFFERQUEUE")) {*out=(uint64_t)(uintptr_t)&SL_IID_BUFFERQUEUE;return 1;}
        if(!strcmp(name,"SL_IID_VOLUME")) {*out=(uint64_t)(uintptr_t)&SL_IID_VOLUME;return 1;}
    }
    if((symbol_type==STT_FUNC || symbol_type==STT_NOTYPE) &&
       !strcmp(name,"slCreateEngine")){
        SLresult(*f)(ObjectItf*,SLuint32,const EngineOption*,SLuint32,
                     const SLInterfaceID*,const SLboolean*)=bridge_create_engine;
        if(sizeof(f)!=sizeof(*out))return 0;
        memcpy(out,&f,sizeof(f));return 1;
    }
    return 0;
}
static void test_callback(QueueItf itf,void *ctx){
    (void)itf;
    (*(int*)ctx)++;
}
int lenneth_opensl_selftest(void){
    int ok=1,calls=0;
    uint64_t addr=0;
    if(!lenneth_opensl_lookup("SL_IID_ENGINE",STT_OBJECT,&addr) ||
       *(const SLInterfaceID*)(uintptr_t)addr != SL_IID_ENGINE)ok=0;
    if(!lenneth_opensl_lookup("SL_IID_PLAY",STT_OBJECT,&addr) ||
       !lenneth_opensl_lookup("SL_IID_BUFFERQUEUE",STT_OBJECT,&addr) ||
       !lenneth_opensl_lookup("SL_IID_VOLUME",STT_OBJECT,&addr) ||
       !lenneth_opensl_lookup("slCreateEngine",STT_FUNC,&addr) ||
       lenneth_opensl_lookup("SL_IID_PLAY",STT_FUNC,&addr) ||
       lenneth_opensl_lookup("slCreateEngine",STT_OBJECT,&addr) ||
       lenneth_opensl_lookup("SL_IID_RECORDER",STT_OBJECT,&addr))ok=0;
    ObjectItf engine=NULL,mix=NULL,player=NULL;
    EngineItf engine_itf=NULL;
    PlayItf play=NULL;QueueItf queue=NULL;VolumeItf vol=NULL;
    if(bridge_create_engine(&engine,0,NULL,0,NULL,NULL)!=SL_OK)ok=0;
    if(!engine)return 10;
    if((*engine)->Realize(engine,0)!=SL_OK ||
       (*engine)->GetInterface(engine,SL_IID_ENGINE,&engine_itf)!=SL_OK ||
       !engine_itf)ok=0;
    if(!engine_itf)return 10;
    if((*engine_itf)->CreateOutputMix(engine_itf,&mix,0,NULL,NULL)!=SL_OK ||
       !mix)ok=0;
    if(!mix)return 10;
    if((*mix)->Realize(mix,0)!=SL_OK)ok=0;
    QueueLocator loc={LOC_BUFFER_QUEUE,4};
    PcmFormat pcm={FMT_PCM,2,44100000,16,16,3,2};
    MixLocator sinkloc={LOC_OUTPUT_MIX,mix};
    DataEndpoint source={&loc,&pcm},sink={&sinkloc,NULL};
    if((*engine_itf)->CreateAudioPlayer(engine_itf,&player,&source,&sink,
                                        0,NULL,NULL)!=SL_OK||!player)ok=0;
    if(!player)return 10;
    if((*player)->Realize(player,0)!=SL_OK ||
       (*player)->GetInterface(player,SL_IID_PLAY,&play)!=SL_OK ||
       (*player)->GetInterface(player,SL_IID_BUFFERQUEUE,&queue)!=SL_OK ||
       (*player)->GetInterface(player,SL_IID_VOLUME,&vol)!=SL_OK ||
       !play||!queue||!vol)ok=0;
    if(!play||!queue||!vol)return 10;
    if((*play)->SetPlayState(play,PLAY_PLAYING)!=SL_OK)ok=0;
    SLuint32 state=0;
    if((*play)->GetPlayState(play,&state)!=SL_OK || state!=PLAY_PLAYING)ok=0;
    const int16_t pcm_samples[8]={0,8192,16384,8192,0,-8192,-16384,-8192};
    if((*queue)->RegisterCallback(queue,test_callback,&calls)!=SL_OK ||
       (*queue)->Enqueue(queue,pcm_samples,sizeof(pcm_samples))!=SL_OK)ok=0;
    QueueState qstate={0};
    if((*queue)->GetState(queue,&qstate)!=SL_OK||qstate.count!=1)ok=0;
    if(synthetic_consume(queue)!=1 || calls!=1)ok=0;
    if((*queue)->GetState(queue,&qstate)!=SL_OK ||
       qstate.count!=0 || qstate.playIndex!=1)ok=0;
    if((*vol)->SetVolumeLevel(vol,-1000)!=SL_OK)ok=0;
    SLmillibel level=0;
    if((*vol)->GetVolumeLevel(vol,&level)!=SL_OK||level!=-1000)ok=0;
    (*player)->Destroy(player);
    (*mix)->Destroy(mix);
    (*engine)->Destroy(engine);
    if((*engine)->Realize(engine,0)==SL_OK)ok=0;
    printf("[lenneth opensl] SELFTEST=%s engine_mix_player_vtables=covered "
           "iid_data_symbols=4 pcm_queues=bounded callbacks_mock=%d "
           "real_pcm_output=NOT_IMPLEMENTED commercial_execution=NO\n",
           ok?"PASS":"FAIL",calls);
    return ok?0:9;
}
