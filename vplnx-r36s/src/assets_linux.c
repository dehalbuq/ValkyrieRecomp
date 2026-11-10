/* SPDX-License-Identifier: MIT
 * Sandboxed Linux implementation of the vpl_nx asset_shim API. Unlike the
 * Switch original, absolute paths and symlink traversal are forbidden.
 * Large CPK files are streamed via a file descriptor and never read wholesale.
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include "assets_linux.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
struct AAssetManager { int initialized; };
struct AAsset { FILE *fp; int64_t size; char name[512]; };
struct AAssetDir { DIR *dir; char prefix[512]; char name[512]; };
static struct AAssetManager manager;
static int rootfd = -1;

static int normalize(const char *path, char out[512], int allow_empty) {
    if (!path) return -1;
    while (strncmp(path,"assets/",7)==0) path+=7;
    size_t n=strlen(path);
    if ((!n && !allow_empty) || n>=512 || path[0]=='/' ||
        strchr(path,'\\') || strchr(path,':')) return -1;
    if (!n) {out[0]='\0';return 0;}
    size_t component_start=0;
    for(size_t i=0;i<=n;i++) {
        if(path[i]=='/' || path[i]=='\0') {
            size_t len=i-component_start;
            if(!len || (len==1 && path[component_start]=='.') ||
               (len==2 && path[component_start]=='.' && path[component_start+1]=='.'))
                return -1;
            component_start=i+1;
        }
    }
    memcpy(out,path,n+1);
    return 0;
}
static int open_relative(const char *relative, int directory) {
    if(rootfd<0) {errno=ENOENT;return -1;}
    if(!*relative) {
        if(!directory) {errno=EINVAL;return -1;}
        return dup(rootfd);
    }
    int dir=dup(rootfd);
    if(dir<0) return -1;
    const char *p=relative;
    for(;;) {
        const char *slash=strchr(p,'/');
        int last=(slash==NULL);
        size_t length=last?strlen(p):(size_t)(slash-p);
        char component[512];
        if(length==0 || length>=sizeof(component)) {
            close(dir);errno=EINVAL;return -1;
        }
        memcpy(component,p,length);
        component[length]=0;
        int flags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW;
        if(!last || directory) flags|=O_DIRECTORY;
        int next=openat(dir,component,flags);
        close(dir);
        if(next<0) return -1;
        if(last) return next;
        dir=next;
        p=slash+1;
    }
}
int vpl_assets_set_root(const char *assets_directory) {
    if(!assets_directory) return -1;
    int fd=open(assets_directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0) return -1;
    if(rootfd>=0) close(rootfd);
    rootfd=fd;
    manager.initialized=1;
    return 0;
}
void vpl_assets_shutdown(void) {
    if(rootfd>=0) close(rootfd);
    rootfd=-1;
    manager.initialized=0;
}
AAssetManager *asset_manager(void) {
    return rootfd<0?NULL:&manager;
}
AAssetManager *AAssetManager_fromJava_fake(void *env,void *obj) {
    (void)env;(void)obj;
    return asset_manager();
}
AAsset *AAssetManager_open_fake(AAssetManager *mgr,const char *filename,int mode) {
    (void)mode;
    char path[512];
    if(!mgr || mgr!=&manager || normalize(filename,path,0)!=0) return NULL;
    int fd=open_relative(path,0);
    if(fd<0) return NULL;
    struct stat st;
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0) {
        close(fd);return NULL;
    }
    FILE *fp=fdopen(fd,"rb");
    if(!fp) {close(fd);return NULL;}
    AAsset *asset=calloc(1,sizeof(*asset));
    if(!asset) {fclose(fp);return NULL;}
    asset->fp=fp;
    asset->size=(int64_t)st.st_size;
    snprintf(asset->name,sizeof(asset->name),"%s",path);
    return asset;
}
int AAsset_read_fake(AAsset *asset,void *buffer,size_t count) {
    if(!asset || !asset->fp || (!buffer && count)) return -1;
    if(count>(size_t)INT_MAX) count=INT_MAX;
    size_t n=fread(buffer,1,count,asset->fp);
    if(n==0 && count && ferror(asset->fp)) return -1;
    return (int)n;
}
int64_t AAsset_seek_fake(AAsset *asset,int64_t off,int whence) {
    if(!asset || !asset->fp) return -1;
    if(fseeko(asset->fp,(off_t)off,whence)!=0) return -1;
    off_t pos=ftello(asset->fp);
    return pos<0?-1:(int64_t)pos;
}
void AAsset_close_fake(AAsset *asset) {
    if(!asset) return;
    if(asset->fp) fclose(asset->fp);
    free(asset);
}
int64_t AAsset_getLength_fake(AAsset *asset) {return asset?asset->size:0;}
int64_t AAsset_getLength64_fake(AAsset *asset) {return asset?asset->size:0;}
int AAsset_openFileDescriptor64_fake(AAsset *asset,int64_t *start,int64_t *length) {
    if(!asset || !asset->fp) return -1;
    int fd=open_relative(asset->name,0);
    if(fd<0) return -1;
    if(start) *start=0;
    if(length) *length=asset->size;
    return fd;
}
AAssetDir *AAssetManager_openDir_fake(AAssetManager *mgr,const char *name) {
    char path[512];
    if(!mgr || mgr!=&manager || normalize(name?name:"",path,1)!=0) return NULL;
    int fd=open_relative(path,1);
    if(fd<0) return NULL;
    DIR *dir=fdopendir(fd);
    if(!dir) {close(fd);return NULL;}
    AAssetDir *result=calloc(1,sizeof(*result));
    if(!result) {closedir(dir);return NULL;}
    result->dir=dir;
    if(*path) {
        size_t n=strlen(path);
        if(n+1 >= sizeof(result->prefix)) {
            AAssetDir_close_fake(result);
            return NULL;
        }
        memcpy(result->prefix,path,n);
        result->prefix[n]='/';
        result->prefix[n+1]='\0';
    }
    return result;
}
const char *AAssetDir_getNextFileName_fake(AAssetDir *ad) {
    if(!ad || !ad->dir) return NULL;
    struct dirent *entry;
    while((entry=readdir(ad->dir))!=NULL) {
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
        struct stat st;
        if(fstatat(dirfd(ad->dir),entry->d_name,&st,AT_SYMLINK_NOFOLLOW)!=0 ||
           !S_ISREG(st.st_mode)) continue;
        int n=snprintf(ad->name,sizeof(ad->name),"%s%s",ad->prefix,entry->d_name);
        if(n<0 || (size_t)n>=sizeof(ad->name)) continue;
        return ad->name;
    }
    return NULL;
}
void AAssetDir_close_fake(AAssetDir *ad) {
    if(!ad) return;
    if(ad->dir) closedir(ad->dir);
    free(ad);
}
