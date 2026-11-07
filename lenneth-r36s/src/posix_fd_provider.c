/*
 * MIT, original code. Bionic <-> glibc POSIX file-descriptor adapter.
 * No commercial code executed; synthetic file fixtures only.
 *
 * IMPORTANT: Only file-descriptor API; Bionic FILE* / struct stat /
 * Bionic directory-entry ABI are not glibc-compatible and NOT mapped.
 * Restricted paths: "assets/<relative>" read-only,
 * "saves/<relative>" for app-private reads/writes,
 * bare filenames are interpreted under assets/.
 * Unknown Android absolute paths are explicitly denied.
 */
#define _GNU_SOURCE
#include "posix_fd_provider.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int assets_fd=-1,saves_fd=-1;

void lenneth_fd_provider_close(void){
    if(assets_fd>=0)close(assets_fd);
    if(saves_fd>=0)close(saves_fd);
    assets_fd=saves_fd=-1;
}
int lenneth_fd_provider_open(const char *asset_dir,const char *save_dir) {
    if(!asset_dir||!save_dir)return 0;
    int a=open(asset_dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    int s=open(save_dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(a<0||s<0){
        if(a>=0)close(a);
        if(s>=0)close(s);
        printf("[lenneth fd] roots unavailable; require local assets/ and saves/\n");
        return 0;
    }
    lenneth_fd_provider_close();
    assets_fd=a;saves_fd=s;
    printf("[lenneth fd] provider=READY paths=assets_readonly,saves_private "
           "absolute_and_symlink_paths=DENIED\n");
    return 1;
}
static int open_beneath(int root,const char *path,int flags,mode_t mode) {
    if(root<0||!path||!*path||*path=='/'||strchr(path,'\\')||
       strstr(path,"//")||strlen(path)>=PATH_MAX){
        errno=EACCES;return -1;
    }
    char copy[PATH_MAX];
    strcpy(copy,path);
    int fd=dup(root);
    if(fd<0)return -1;
    char *save=NULL;
    char *part=strtok_r(copy,"/",&save);
    while(part){
        if(!strcmp(part,"..")||!strcmp(part,".")||!*part){
            close(fd);errno=EACCES;return -1;
        }
        char *next=strtok_r(NULL,"/",&save);
        int child=next?
            openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC):
            openat(fd,part,flags|O_NOFOLLOW|O_CLOEXEC,mode);
        int error=errno;
        close(fd);
        if(child<0){errno=error;return -1;}
        fd=child;
        part=next;
    }
    return fd;
}
/* POSIX open is variadic; Android and Linux arm64 follow the same AAPCS64
 * value passing convention for const char*, int, and mode_t. */
static int guest_open(const char *path,int flags,...) {
    if(!path){errno=EFAULT;return -1;}
    mode_t mode=0600;
    if(flags&O_CREAT){
        va_list ap;va_start(ap,flags);
        mode=(mode_t)va_arg(ap,int);
        va_end(ap);
    }
#ifdef O_TMPFILE
    if((flags&O_TMPFILE)==O_TMPFILE){errno=EOPNOTSUPP;return -1;}
#endif
    int root;
    const char *rel=path;
    int writing=(flags&(O_WRONLY|O_RDWR|O_CREAT|O_TRUNC|O_APPEND))!=0;
    if(!strncmp(path,"assets/",7)){
        root=assets_fd;rel=path+7;
    }else if(!strncmp(path,"saves/",6)){
        root=saves_fd;rel=path+6;
    }else {
        /* Default to read-only local assets for relative guest paths. */
        root=assets_fd;
    }
    if(root<0){errno=ENOENT;return -1;}
    if(root==assets_fd && writing){errno=EROFS;return -1;}
    return open_beneath(root,rel,flags,mode);
}
int lenneth_fd_open_guest_path(const char *path,int flags,mode_t mode) {
    return guest_open(path,flags,(int)mode);
}
/* Phase20: verify an existing descriptor's current kernel target is a
 * regular file below a configured sandbox root. This is used by the fdopen
 * opaque FILE adapter; it never trusts arbitrary host file descriptors. */
int lenneth_fd_is_guest_regular(int fd){
    if(fd<0 || assets_fd<0 || saves_fd<0){errno=EBADF;return 0;}
    struct stat st;
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode)){errno=EACCES;return 0;}
    char link_name[80],target[PATH_MAX],asset_root[PATH_MAX],save_root[PATH_MAX];
    snprintf(link_name,sizeof(link_name),"/proc/self/fd/%d",fd);
    ssize_t len=readlink(link_name,target,sizeof(target)-1);
    if(len<1 || len>=(ssize_t)sizeof(target)-1){errno=EACCES;return 0;}
    target[len]=0;
    snprintf(link_name,sizeof(link_name),"/proc/self/fd/%d",assets_fd);
    len=readlink(link_name,asset_root,sizeof(asset_root)-1);
    if(len<1 || len>=(ssize_t)sizeof(asset_root)-1){errno=EACCES;return 0;}
    asset_root[len]=0;
    snprintf(link_name,sizeof(link_name),"/proc/self/fd/%d",saves_fd);
    len=readlink(link_name,save_root,sizeof(save_root)-1);
    if(len<1 || len>=(ssize_t)sizeof(save_root)-1){errno=EACCES;return 0;}
    save_root[len]=0;
    size_t a=strlen(asset_root),b=strlen(save_root);
    int allowed=(strncmp(target,asset_root,a)==0 && target[a]=='/') ||
                (strncmp(target,save_root,b)==0 && target[b]=='/');
    if(!allowed)errno=EACCES;
    return allowed;
}

static ssize_t guest_read(int fd,void *dst,size_t n){return read(fd,dst,n);}
static ssize_t guest_write(int fd,const void *src,size_t n){return write(fd,src,n);}
static int guest_close(int fd){return close(fd);}
static off_t guest_lseek(int fd,off_t offset,int origin) {
    return lseek(fd,offset,origin);
}
static pid_t guest_gettid(void){return (pid_t)syscall(SYS_gettid);}
static int guest_clock_nanosleep(clockid_t clock_id,int flags,
                                 const struct timespec *request,
                                 struct timespec *remain) {
    return clock_nanosleep(clock_id,flags,request,remain);
}
static void *guest_memalign(size_t alignment,size_t size) {
    void *ptr=NULL;
    if(alignment<sizeof(void*) || (alignment&(alignment-1)))
        return NULL;
    return posix_memalign(&ptr,alignment,size)==0?ptr:NULL;
}
typedef struct {const char *name;void(*fn)(void);} AbiEntry;
#define X(name,fn) {name,(void(*)(void))(fn)}
static const AbiEntry entries[]={
    X("open",guest_open),X("read",guest_read),X("write",guest_write),
    X("close",guest_close),X("lseek",guest_lseek),
    X("gettid",guest_gettid),X("clock_nanosleep",guest_clock_nanosleep),
    X("memalign",guest_memalign)
};
int lenneth_fd_provider_lookup(const char *name,uint64_t *value){
    if(!name||!value)return 0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
        if(strcmp(name,entries[i].name))continue;
        void(*fn)(void)=entries[i].fn;
        if(sizeof(fn)!=sizeof(*value))return 0;
        memcpy(value,&fn,sizeof(fn));return 1;
    }
    return 0;
}
static int create_dir(const char *path) {
    if(!mkdir(path,0700))return 1;
    return errno==EEXIST;
}
int lenneth_fd_provider_selftest(void) {
    char base[]="/tmp/lenneth-io-XXXXXX";
    char *root=mkdtemp(base);
    if(!root)return 7;
    char assets[PATH_MAX],saves[PATH_MAX],fixture[PATH_MAX],symlink_path[PATH_MAX];
    snprintf(assets,sizeof(assets),"%s/assets",root);
    snprintf(saves,sizeof(saves),"%s/saves",root);
    snprintf(fixture,sizeof(fixture),"%s/assets/fixture.bin",root);
    snprintf(symlink_path,sizeof(symlink_path),"%s/assets/link",root);
    int ok=create_dir(assets)&&create_dir(saves);
    const char secret[]="LENNETH-SYNTHETIC-IO";
    int src=open(fixture,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(src<0)ok=0;
    else {
        if(write(src,secret,sizeof(secret))!=(ssize_t)sizeof(secret))ok=0;
        close(src);
    }
    if(!lenneth_fd_provider_open(assets,saves))ok=0;
    char out[sizeof(secret)]={0};
    int fd=guest_open("assets/fixture.bin",O_RDONLY);
    if(fd<0)ok=0;
    else {
        if(guest_read(fd,out,sizeof(out))!=(ssize_t)sizeof(out) ||
           memcmp(out,secret,sizeof(secret)))ok=0;
        if(guest_lseek(fd,0,SEEK_SET)!=0)ok=0;
        guest_close(fd);
    }
    int escape=guest_open("assets/../saves/foo",O_RDONLY);
    if(escape>=0){guest_close(escape);ok=0;}
    escape=guest_open("/etc/passwd",O_RDONLY);
    if(escape>=0){guest_close(escape);ok=0;}
    if(symlink("/etc/passwd",symlink_path)==0){
        escape=guest_open("assets/link",O_RDONLY);
        if(escape>=0){guest_close(escape);ok=0;}
    }
    escape=guest_open("assets/fixture.bin",O_WRONLY|O_TRUNC);
    if(escape>=0){guest_close(escape);ok=0;}
    int save=guest_open("saves/test.dat",O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(save<0)ok=0;
    else {
        if(guest_write(save,secret,sizeof(secret))!=(ssize_t)sizeof(secret))
            ok=0;
        guest_close(save);
    }
    save=guest_open("saves/test.dat",O_RDONLY);
    if(save<0)ok=0;
    else {
        memset(out,0,sizeof(out));
        if(guest_read(save,out,sizeof(out))!=(ssize_t)sizeof(out) ||
           memcmp(out,secret,sizeof(secret)))ok=0;
        guest_close(save);
    }
    if(guest_gettid()<=0)ok=0;
    void *aligned=guest_memalign(64,128);
    if(!aligned || (uintptr_t)aligned%64)ok=0;
    free(aligned);
    uint64_t address=0;
    if(!lenneth_fd_provider_lookup("open",&address)||!address ||
       lenneth_fd_provider_lookup("fopen",&address) ||
       lenneth_fd_provider_lookup("dlopen",&address))ok=0;
    lenneth_fd_provider_close();
    char save_file[PATH_MAX];
    snprintf(save_file,sizeof(save_file),"%s/saves/test.dat",root);
    unlink(save_file);unlink(symlink_path);unlink(fixture);
    rmdir(assets);rmdir(saves);rmdir(root);
    printf("[lenneth fd] SELFTEST=%s read_write_seek_save=covered "
           "traversal_and_symlink=DENIED guest_FILE=NOT_MAPPED "
           "commercial_execution=NO\n",ok?"PASS":"FAIL");
    return ok?0:8;
}
