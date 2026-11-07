/*
 * Lenneth R36S - Bionic stdio opaque proxy (MIT, independent implementation).
 *
 * Adapter pointers are NOT glibc FILE*; host FILE* remain private. This
 * covers only API-mediated streams opened under the sandboxed assets/ and
 * saves/ paths. Android __sF / FILE struct and the standard-stream layout
 * are NOT yet compatible; these must be implemented and verified separately.
 * Original commercial game code is never executed in the diagnostic.
 */
#define _GNU_SOURCE
#include "bionic_stdio.h"
#include "posix_fd_provider.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_STREAMS 128
typedef struct {
    FILE *host;
    uint64_t generation;
    int active;
} GuestStream;
/* Static slots provide finite, address-identity tokens, not host objects. */
static GuestStream streams[MAX_STREAMS];
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static uint64_t next_generation=1;
static size_t opened_total;

static GuestStream *get_slot(void *guest){
    if(!guest)return NULL;
    for(size_t i=0;i<MAX_STREAMS;i++){
        if(guest==&streams[i] && streams[i].active && streams[i].host)
            return &streams[i];
    }
    errno=EBADF;return NULL;
}
static void *allocate_slot(FILE *stream){
    if(!stream)return NULL;
    pthread_mutex_lock(&lock);
    for(size_t i=0;i<MAX_STREAMS;i++){
        if(streams[i].active)continue;
        streams[i].active=1;
        streams[i].host=stream;
        streams[i].generation=next_generation++;
        opened_total++;
        pthread_mutex_unlock(&lock);
        return &streams[i];
    }
    pthread_mutex_unlock(&lock);
    errno=EMFILE;
    fclose(stream);
    return NULL;
}
/* File mode parsing deliberately rejects modes with unknown semantics. */
static int parse_mode(const char *m,int *flags){
    if(!m || !*m || !flags){errno=EINVAL;return 0;}
    int plus=0,exclusive=0;
    for(const char *p=m+1;*p;p++){
        if(*p=='+'){if(plus++)goto invalid;}
        else if(*p=='b'||*p=='e')continue;
        else if(*p=='x'){if(exclusive++)goto invalid;}
        else goto invalid;
    }
    switch(*m){
        case 'r':if(exclusive)goto invalid;
                 *flags=plus?O_RDWR:O_RDONLY;return 1;
        case 'w':*flags=(plus?O_RDWR:O_WRONLY)|O_CREAT|O_TRUNC|
                      (exclusive?O_EXCL:0);return 1;
        case 'a':*flags=(plus?O_RDWR:O_WRONLY)|O_CREAT|O_APPEND|
                      (exclusive?O_EXCL:0);return 1;
        default:break;
    }
invalid:
    errno=EINVAL;return 0;
}
static void *guest_fopen(const char *path,const char *mode){
    int flags=0;
    if(!parse_mode(mode,&flags))return NULL;
    int fd=lenneth_fd_open_guest_path(path,flags,0600);
    if(fd<0)return NULL;
    FILE *host=fdopen(fd,mode);
    if(!host){int e=errno;close(fd);errno=e;return NULL;}
    return allocate_slot(host);
}
/* Phase20: fdopen is mediated by an existing sandboxed descriptor; no glibc
 * FILE* crosses the guest ABI. We preserve POSIX ownership on success. */
static void *guest_fdopen(int fd,const char *mode){
    int flags=0;
    if(!parse_mode(mode,&flags) || !lenneth_fd_is_guest_regular(fd))return NULL;
    /* Existing fdopen should not accept create-exclusive semantics. */
    if(strchr(mode,'x')){errno=EINVAL;return NULL;}
    int current=fcntl(fd,F_GETFL);
    if(current<0)return NULL;
    int access=current&O_ACCMODE;
    int requested=flags&O_ACCMODE;
    if((requested==O_RDONLY && access==O_WRONLY) ||
       (requested==O_WRONLY && access==O_RDONLY) ||
       (requested==O_RDWR && access!=O_RDWR)){
        errno=EBADF;return NULL;
    }
    FILE *host=fdopen(fd,mode);
    if(!host)return NULL;
    return allocate_slot(host);
}

static int guest_fclose(void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *slot=get_slot(stream);
    if(!slot){pthread_mutex_unlock(&lock);return EOF;}
    FILE *host=slot->host;
    slot->active=0;slot->host=NULL;
    int rc=fclose(host);
    pthread_mutex_unlock(&lock);
    return rc;
}
static size_t guest_fread(void *ptr,size_t size,size_t n,void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    size_t ret=s?fread(ptr,size,n,s->host):0;
    pthread_mutex_unlock(&lock);return ret;
}
static size_t guest_fwrite(const void *ptr,size_t size,size_t n,void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    size_t ret=s?fwrite(ptr,size,n,s->host):0;
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_fseek(void *stream,long off,int whence){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    int rc=s?fseek(s->host,off,whence):-1;
    pthread_mutex_unlock(&lock);return rc;
}
static long guest_ftell(void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    long ret=s?ftell(s->host):-1;
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_ferror(void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    int ret=s?ferror(s->host):1;
    pthread_mutex_unlock(&lock);return ret;
}
static void guest_clearerr(void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    if(s)clearerr(s->host);
    pthread_mutex_unlock(&lock);
}
static int guest_fflush(void *stream){
    pthread_mutex_lock(&lock);
    int ret=0;
    if(!stream){
        for(size_t i=0;i<MAX_STREAMS;i++){
            if(streams[i].active && streams[i].host && fflush(streams[i].host)!=0)
                ret=EOF;
        }
    }else{
        GuestStream *s=get_slot(stream);
        ret=s?fflush(s->host):EOF;
    }
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_fputc(int ch,void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    int ret=s?fputc(ch,s->host):EOF;
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_getc(void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    int ret=s?getc(s->host):EOF;
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_ungetc(int ch,void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    int ret=s?ungetc(ch,s->host):EOF;
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_fileno(void *stream){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    int ret=s?fileno(s->host):-1;
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_vfprintf(void *stream,const char *fmt,va_list va){
    pthread_mutex_lock(&lock);
    GuestStream *s=get_slot(stream);
    int ret=s&&fmt?vfprintf(s->host,fmt,va):-1;
    pthread_mutex_unlock(&lock);return ret;
}
static int guest_fprintf(void *stream,const char *fmt,...){
    va_list va;va_start(va,fmt);
    int ret=guest_vfprintf(stream,fmt,va);
    va_end(va);return ret;
}
typedef struct {const char *name;void(*fn)(void);} Symbol;
#define X(name,fn) {name,(void(*)(void))(fn)}
static const Symbol api[]={
    X("fopen",guest_fopen),X("fdopen",guest_fdopen),X("fclose",guest_fclose),
    X("fread",guest_fread),X("fwrite",guest_fwrite),
    X("fseek",guest_fseek),X("ftell",guest_ftell),
    X("ferror",guest_ferror),X("clearerr",guest_clearerr),
    X("fflush",guest_fflush),X("fputc",guest_fputc),
    X("getc",guest_getc),X("ungetc",guest_ungetc),
    X("fileno",guest_fileno),X("vfprintf",guest_vfprintf),
    X("fprintf",guest_fprintf)
};
int lenneth_bionic_stdio_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(api)/sizeof(api[0]);i++){
        if(strcmp(name,api[i].name))continue;
        void(*fn)(void)=api[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));return 1;
    }
    return 0;
}
int lenneth_bionic_stdio_selftest(void){
    char tmp[]="/tmp/lenneth-stdio-XXXXXX";
    char *base=mkdtemp(tmp);
    if(!base)return 7;
    char assets[512],saves[512],input[512],output[512];
    snprintf(assets,sizeof(assets),"%s/assets",base);
    snprintf(saves,sizeof(saves),"%s/saves",base);
    snprintf(input,sizeof(input),"%s/assets/input.txt",base);
    snprintf(output,sizeof(output),"%s/saves/progress.txt",base);
    int ok=(mkdir(assets,0700)==0 && mkdir(saves,0700)==0);
    FILE *file=fopen(input,"wb");
    if(!file)ok=0;
    else {
        if(fwrite("SYNTHETIC",1,9,file)!=9)ok=0;
        fclose(file);
    }
    if(!lenneth_fd_provider_open(assets,saves))ok=0;
    void *guest=guest_fopen("assets/input.txt","rb");
    if(!guest)ok=0;
    else {
        char buf[16]={0};
        if(guest_fread(buf,1,9,guest)!=9||memcmp(buf,"SYNTHETIC",9))ok=0;
        if(guest_ftell(guest)!=9 || guest_fseek(guest,0,SEEK_SET)!=0)ok=0;
        if(guest_getc(guest)!='S' || guest_ungetc('S',guest)!='S' ||
           guest_getc(guest)!='S')ok=0;
        if(guest_fileno(guest)<0 || guest_ferror(guest))ok=0;
        if(guest_fclose(guest)!=0 || guest_fclose(guest)!=EOF)ok=0;
    }
    /* Phase20: permitted FD, foreign FD, wrong mode and invalid descriptor. */
    int fd=lenneth_fd_open_guest_path("assets/input.txt",O_RDONLY,0);
    if(fd<0)ok=0;
    else{
        guest=guest_fdopen(fd,"rb");
        if(!guest) {ok=0;close(fd);}
        else {
            char buf[16]={0};
            if(guest_fread(buf,1,9,guest)!=9 || memcmp(buf,"SYNTHETIC",9) ||
               guest_fileno(guest)!=fd || guest_fclose(guest)!=0)ok=0;
        }
    }
    fd=open("/etc/passwd",O_RDONLY|O_CLOEXEC);
    if(fd>=0){
        if(guest_fdopen(fd,"rb")!=NULL)ok=0;
        close(fd);
    }
    fd=lenneth_fd_open_guest_path("assets/input.txt",O_RDONLY,0);
    if(fd<0)ok=0;
    else{
        if(guest_fdopen(fd,"wb")!=NULL)ok=0;
        if(guest_fdopen(fd,"rbx")!=NULL)ok=0;
        close(fd);
    }
    if(guest_fdopen(-1,"rb")!=NULL)ok=0;
    if(guest_fopen("/etc/passwd","rb")!=NULL)ok=0;
    if(guest_fopen("assets/../saves/progress.txt","wb")!=NULL)ok=0;
    if(guest_fopen("assets/input.txt","wb")!=NULL)ok=0;
    guest=guest_fopen("saves/progress.txt","wb+");
    if(!guest)ok=0;
    else {
        if(guest_fwrite("AB",1,2,guest)!=2 ||
           guest_fprintf(guest,"%s-%d","C",42)!=4 ||
           guest_fflush(guest)!=0 || guest_fseek(guest,0,SEEK_SET)!=0)
            ok=0;
        char buf[32]={0};
        if(guest_fread(buf,1,6,guest)!=6 || memcmp(buf,"ABC-42",6))ok=0;
        guest_clearerr(guest);
        if(guest_fclose(guest)!=0)ok=0;
    }
    if(guest_fread(NULL,1,3,(void*)(uintptr_t)1)!=0)ok=0;
    uint64_t addr=0;
    if(!lenneth_bionic_stdio_lookup("fopen",&addr) || !addr ||
       !lenneth_bionic_stdio_lookup("fdopen",&addr) || !addr ||
       lenneth_bionic_stdio_lookup("__sF",&addr) ||
       lenneth_bionic_stdio_lookup("system",&addr))ok=0;
    lenneth_fd_provider_close();
    unlink(input);unlink(output);rmdir(assets);rmdir(saves);rmdir(base);
    printf("[lenneth stdio] SELFTEST=%s shimmed_api=%zu opened_streams=%zu "
           "host_FILE_opaque=1 legacy___sF_NOT_MAPPED=1 "
           "commercial_execution=NO\n",ok?"PASS":"FAIL",
           sizeof(api)/sizeof(api[0]),opened_total);
    return ok?0:8;
}
