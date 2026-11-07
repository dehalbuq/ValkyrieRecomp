/*
 * Lenneth R36S - Android AAssetManager read-only filesystem shim (phase 6).
 * MIT. Independently authored, no game code/assets included.
 *
 * Maps *extracted, local files* under an allowlisted root directory to
 * Android NDK AAsset methods. This is NOT a ZIP/APK reader and NOT a JNI
 * implementation. Uses openat(O_NOFOLLOW) for each component to prevent
 * traversal/symlink escape. ZIP-compressed APK entries are not supported.
 * All functions are initially tested ONLY with original synthetic fixtures.
 */
#define _GNU_SOURCE
#include "asset_provider.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#error "Openat O_NOFOLLOW support is required"
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
struct AAssetManager {unsigned magic;};
struct AAsset {
    unsigned magic;
    int fd;
    off_t size;
    const void *mapped;
    size_t map_length;
};
struct AAssetDir {
    unsigned magic;
    DIR *dir;
};
enum { MANAGER_MAGIC=0x4c41534d, ASSET_MAGIC=0x4c415354,
       DIR_MAGIC=0x4c415344 };
static struct AAssetManager manager={MANAGER_MAGIC};
static int root_fd=-1;

int lenneth_asset_set_root(const char *path) {
    if(!path || !*path)return 0;
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0){
        fprintf(stderr,"[lenneth asset] cannot open root: %s\n",strerror(errno));
        return 0;
    }
    if(root_fd>=0)close(root_fd);
    root_fd=fd;
    printf("[lenneth asset] root=ready policy=read-only,no-symlink,no-traversal\n");
    return 1;
}
void lenneth_asset_close_root(void) {
    if(root_fd>=0)close(root_fd);
    root_fd=-1;
}
static int open_relative(const char *name,int want_directory) {
    if(root_fd<0 || !name || *name=='/' || strlen(name)>=PATH_MAX)
        return -1;
    if(strchr(name,'\\') || strstr(name,"//"))return -1;
    int fd=dup(root_fd);
    if(fd<0)return -1;
    if(!*name){
        if(want_directory)return fd;
        close(fd);return -1;
    }
    char path[PATH_MAX];
    memcpy(path,name,strlen(name)+1);
    char *save=NULL;
    char *part=strtok_r(path,"/",&save);
    while(part){
        if(strcmp(part,".")==0 || strcmp(part,"..")==0 || !*part){
            close(fd);return -1;
        }
        char *next=strtok_r(NULL,"/",&save);
        int flags=O_RDONLY|O_NOFOLLOW|O_CLOEXEC;
        if(next || want_directory)flags|=O_DIRECTORY;
        int child=openat(fd,part,flags);
        close(fd);
        if(child<0)return -1;
        fd=child;
        part=next;
    }
    return fd;
}

AAssetManager *AAssetManager_fromJava(void *env,void *java_asset_mgr) {
    /* A full JNI runtime needs to validate Java asset-manager objects.
     * This sentinel merely exposes an already configured private root. */
    (void)env;(void)java_asset_mgr;
    return root_fd>=0?&manager:NULL;
}
AAsset *AAssetManager_open(AAssetManager *mgr,const char *filename,int mode) {
    (void)mode;
    if(root_fd<0 || mgr!=&manager || !filename)return NULL;
    int fd=open_relative(filename,0);
    if(fd<0)return NULL;
    struct stat sb;
    if(fstat(fd,&sb)!=0 || !S_ISREG(sb.st_mode) || sb.st_size<0) {
        close(fd);return NULL;
    }
    AAsset *a=(AAsset*)calloc(1,sizeof(*a));
    if(!a){close(fd);return NULL;}
    a->magic=ASSET_MAGIC;
    a->fd=fd;
    a->size=sb.st_size;
    return a;
}
AAssetDir *AAssetManager_openDir(AAssetManager *mgr,const char *name) {
    if(root_fd<0 || mgr!=&manager)return NULL;
    int fd=open_relative(name?name:"",1);
    if(fd<0)return NULL;
    DIR *d=fdopendir(fd);
    if(!d){close(fd);return NULL;}
    AAssetDir *a=(AAssetDir*)calloc(1,sizeof(*a));
    if(!a){closedir(d);return NULL;}
    a->magic=DIR_MAGIC;a->dir=d;
    return a;
}
int AAsset_read(AAsset *a,void *buf,size_t count) {
    if(!a||a->magic!=ASSET_MAGIC||(!buf&&count))return -1;
    if(count>INT_MAX)count=INT_MAX;
    ssize_t n=read(a->fd,buf,count);
    return n<0?-1:(int)n;
}
int64_t AAsset_seek64(AAsset *a,int64_t off,int whence) {
    if(!a||a->magic!=ASSET_MAGIC ||
       (whence!=SEEK_SET && whence!=SEEK_CUR && whence!=SEEK_END))
        return -1;
    off_t n=lseek(a->fd,(off_t)off,whence);
    return n<0?-1:(int64_t)n;
}
off_t AAsset_seek(AAsset *a,off_t off,int whence) {
    return (off_t)AAsset_seek64(a,(int64_t)off,whence);
}
int64_t AAsset_getLength64(AAsset *a) {
    return a&&a->magic==ASSET_MAGIC?(int64_t)a->size:-1;
}
off_t AAsset_getLength(AAsset *a) {
    return (off_t)AAsset_getLength64(a);
}
int64_t AAsset_getRemainingLength64(AAsset *a) {
    if(!a||a->magic!=ASSET_MAGIC)return -1;
    off_t pos=lseek(a->fd,0,SEEK_CUR);
    if(pos<0)return -1;
    return pos>=a->size?0:(int64_t)(a->size-pos);
}
off_t AAsset_getRemainingLength(AAsset *a) {
    return (off_t)AAsset_getRemainingLength64(a);
}
const void *AAsset_getBuffer(AAsset *a) {
    if(!a||a->magic!=ASSET_MAGIC)return NULL;
    if(a->mapped)return a->mapped;
    if(a->size<=0 || (uint64_t)a->size>SIZE_MAX)return NULL;
    /* mmap file privately; do not copy giant CPK files into physical RAM. */
    void *buf=mmap(NULL,(size_t)a->size,PROT_READ,MAP_PRIVATE,a->fd,0);
    if(buf==MAP_FAILED)return NULL;
    a->mapped=buf;
    a->map_length=(size_t)a->size;
    return a->mapped;
}
int AAsset_isAllocated(AAsset *a) {
    return a && a->magic==ASSET_MAGIC ? 0 : 0;
}
void AAsset_close(AAsset *a) {
    if(!a || a->magic!=ASSET_MAGIC)return;
    a->magic=0;
    if(a->mapped)munmap((void*)a->mapped,a->map_length);
    close(a->fd);
    free(a);
}
const char *AAssetDir_getNextFileName(AAssetDir *a) {
    if(!a||a->magic!=DIR_MAGIC)return NULL;
    struct dirent *ent;
    while((ent=readdir(a->dir))!=NULL){
        if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)
            continue;
        if(ent->d_type==DT_DIR || ent->d_type==DT_LNK)continue;
        /* Readdir pointer stays valid until the next directory call. */
        return ent->d_name;
    }
    return NULL;
}
void AAssetDir_rewind(AAssetDir *a) {
    if(a && a->magic==DIR_MAGIC)rewinddir(a->dir);
}
void AAssetDir_close(AAssetDir *a) {
    if(!a || a->magic!=DIR_MAGIC)return;
    a->magic=0;
    closedir(a->dir);
    free(a);
}
int AAsset_openFileDescriptor64(AAsset *a,int64_t *start,int64_t *length) {
    if(!a || a->magic!=ASSET_MAGIC || !start || !length)return -1;
    int fd=dup(a->fd);
    if(fd<0)return -1;
    *start=0;*length=(int64_t)a->size;
    return fd;
}
int AAsset_openFileDescriptor(AAsset *a,off_t *start,off_t *length) {
    if(!start||!length)return -1;
    int64_t st=0,len=0;
    int fd=AAsset_openFileDescriptor64(a,&st,&len);
    if(fd>=0){*start=(off_t)st;*length=(off_t)len;}
    return fd;
}

/* Explicitly exported name/address pairs for the non-executing ELF plan.
 * It is intentionally a whitelist; it does not route JNI or unrelated
 * dynamic symbols and cannot make the full Android runtime executable. */
typedef struct {const char *name;void (*fn)(void);} ShimEntry;
#define ENTRY(name) {#name, (void (*)(void))(name)}
static const ShimEntry api[]={
    ENTRY(AAssetManager_fromJava),
    ENTRY(AAssetManager_open), ENTRY(AAssetManager_openDir),
    ENTRY(AAsset_read), ENTRY(AAsset_seek),ENTRY(AAsset_seek64),
    ENTRY(AAsset_getLength),ENTRY(AAsset_getLength64),
    ENTRY(AAsset_getRemainingLength),ENTRY(AAsset_getRemainingLength64),
    ENTRY(AAsset_getBuffer),ENTRY(AAsset_isAllocated),ENTRY(AAsset_close),
    ENTRY(AAssetDir_getNextFileName),ENTRY(AAssetDir_rewind),
    ENTRY(AAssetDir_close),ENTRY(AAsset_openFileDescriptor),
    ENTRY(AAsset_openFileDescriptor64)
};
int lenneth_asset_lookup(const char *name,uint64_t *address) {
    if(!name || !address)return 0;
    for(size_t i=0;i<sizeof(api)/sizeof(api[0]);i++){
        if(strcmp(name,api[i].name)==0) {
            void (*fn)(void)=api[i].fn;
            if(sizeof(fn)!=sizeof(*address))return 0;
            memcpy(address,&fn,sizeof(fn));
            return 1;
        }
    }
    return 0;
}

/* Tiny fixture test: no game files, no JNI and no engine execution. */
int lenneth_asset_selftest(const char *root,const char *relative_path) {
    if(!lenneth_asset_set_root(root))return 3;
    AAssetManager *m=AAssetManager_fromJava(NULL,NULL);
    AAsset *a=AAssetManager_open(m,relative_path,2);
    if(!a){fprintf(stderr,"[lenneth asset] test file missing\n");
        lenneth_asset_close_root();return 4;}
    char data[128]={0}, again[128]={0};
    int n=AAsset_read(a,data,sizeof(data));
    int ok=n>0 && AAsset_seek(a,0,SEEK_SET)==0 &&
           AAsset_read(a,again,(size_t)n)==n &&
           memcmp(data,again,(size_t)n)==0 &&
           AAsset_getLength(a)>=n &&
           AAsset_getRemainingLength(a)>=0 &&
           AAsset_getBuffer(a)!=NULL;
    AAsset_close(a);
    AAsset *escape=AAssetManager_open(m,"../etc/passwd",2);
    if(escape){AAsset_close(escape);ok=0;}
    escape=AAssetManager_open(m,"/etc/passwd",2);
    if(escape){AAsset_close(escape);ok=0;}
    AAssetDir *d=AAssetManager_openDir(m,"");
    if(!d)ok=0;
    else {
        int seen=0;
        while(AAssetDir_getNextFileName(d) && ++seen<1000){}
        AAssetDir_rewind(d);
        AAssetDir_close(d);
    }
    printf("[lenneth asset] SELFTEST=%s bytes_read=%d security_traversal=blocked "
           "engine=not_executed\n",ok?"PASS":"FAIL",n);
    lenneth_asset_close_root();
    return ok?0:5;
}
