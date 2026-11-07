/*
 * MIT - safe guest POSIX path mutation on owned local saves/ tree.
 * Portable primitive rename, mkdir and remove with restricted path
 * policy; all parent directories opened O_NOFOLLOW from a fixed root.
 * No Android/commercial guest code is executed by this probe.
 */
#define _GNU_SOURCE
#include "saveops_provider.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int save_root=-1;

int lenneth_saveops_open(const char *root){
    if(!root||!*root)return 0;
    int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return 0;
    if(save_root>=0)close(save_root);
    save_root=fd;return 1;
}
void lenneth_saveops_close(void){
    if(save_root>=0)close(save_root);
    save_root=-1;
}
static int is_bad_component(const char *p) {
    return !p || !*p || strcmp(p,".")==0 || strcmp(p,"..")==0;
}
/* Split a guest path into a no-symlink parent fd and leaf basename.
 * Both are returned to the calling operation; the caller closes the fd.
 * The last name is NOT followed by renameat/unlinkat/mkdirat. */
static int parent_of(const char *path,int *parent,char *leaf,size_t cap){
    if(save_root<0||!path||strncmp(path,"saves/",6)!=0){
        errno=EACCES;return 0;
    }
    const char *relative=path+6;
    if(!*relative || strlen(relative)>=PATH_MAX ||
       *relative=='/' || strchr(relative,'\\') || strstr(relative,"//") ||
       relative[strlen(relative)-1]=='/'){
        errno=EACCES;return 0;
    }
    char scratch[PATH_MAX];
    strcpy(scratch,relative);
    int current=dup(save_root);
    if(current<0)return 0;
    char *save=NULL;
    char *part=strtok_r(scratch,"/",&save);
    while(part){
        if(is_bad_component(part))goto deny;
        char *next=strtok_r(NULL,"/",&save);
        if(!next){
            if(strlen(part)+1>cap){errno=ENAMETOOLONG;goto fail;}
            strcpy(leaf,part);
            *parent=current;
            return 1;
        }
        int child=openat(current,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(child<0)goto fail;
        close(current);current=child;part=next;
    }
deny: errno=EACCES;
fail: {
    int previous=errno;
    close(current);
    errno=previous;
    return 0;
}
}
static int guest_rename(const char *from,const char *to){
    int oldfd,newfd;
    char oldleaf[NAME_MAX+1],newleaf[NAME_MAX+1];
    if(!parent_of(from,&oldfd,oldleaf,sizeof(oldleaf)))return -1;
    if(!parent_of(to,&newfd,newleaf,sizeof(newleaf))){
        int e=errno;close(oldfd);errno=e;return -1;
    }
    int ret=renameat(oldfd,oldleaf,newfd,newleaf);
    int e=errno;close(oldfd);close(newfd);errno=e;return ret;
}
static int guest_mkdir(const char *path,mode_t mode){
    int fd;char leaf[NAME_MAX+1];
    if(!parent_of(path,&fd,leaf,sizeof(leaf)))return -1;
    int rc=mkdirat(fd,leaf,mode&0777);
    int e=errno;close(fd);errno=e;return rc;
}
static int guest_remove(const char *path){
    int fd;char leaf[NAME_MAX+1];
    if(!parent_of(path,&fd,leaf,sizeof(leaf)))return -1;
    int rc=unlinkat(fd,leaf,0);
    if(rc<0 && (errno==EISDIR || errno==EPERM))
        rc=unlinkat(fd,leaf,AT_REMOVEDIR);
    int e=errno;close(fd);errno=e;return rc;
}
typedef struct {const char *name;void(*fn)(void);} Entry;
static const Entry functions[]={
    {"rename",(void(*)(void))guest_rename},
    {"mkdir",(void(*)(void))guest_mkdir},
    {"remove",(void(*)(void))guest_remove}
};
int lenneth_saveops_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(functions)/sizeof(functions[0]);i++){
        if(strcmp(name,functions[i].name))continue;
        void(*fn)(void)=functions[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));return 1;
    }
    return 0;
}
int lenneth_saveops_selftest(void){
    char root[]="/tmp/lenneth-saveops-XXXXXX";
    char *dir=mkdtemp(root);
    if(!dir)return 9;
    int ok=lenneth_saveops_open(root);
    int fd=-1;
    if(guest_mkdir("saves/progress",0700)!=0)ok=0;
    if(guest_mkdir("saves/../outside",0700)==0)ok=0;
    char path[PATH_MAX];
    snprintf(path,sizeof(path),"%s/progress/old.dat",dir);
    fd=open(path,O_CREAT|O_WRONLY|O_TRUNC,0600);
    if(fd<0)ok=0;
    else{if(write(fd,"OK",2)!=2)ok=0;close(fd);}
    if(guest_rename("saves/progress/old.dat","saves/progress/new.dat")!=0)ok=0;
    if(guest_rename("saves/progress/new.dat","/tmp/leak.dat")==0)ok=0;
    if(guest_remove("saves/progress/new.dat")!=0)ok=0;
    if(guest_remove("assets/anything")==0)ok=0;
    if(guest_remove("saves/progress")!=0)ok=0;
    char linkname[PATH_MAX];
    snprintf(linkname,sizeof(linkname),"%s/external",dir);
    if(symlink("/etc",linkname)==0 &&
       guest_remove("saves/external/passwd")==0)ok=0;
    if(guest_remove("saves/external")!=0)ok=0;
    uint64_t addr=0;
    if(!lenneth_saveops_lookup("rename",&addr)||!addr ||
       lenneth_saveops_lookup("system",&addr))ok=0;
    lenneth_saveops_close();
    rmdir(dir);
    printf("[lenneth saveops] SELFTEST=%s rename_mkdir_remove=covered "
           "sandbox_escape=DENIED no_host_absolute_paths=1 "
           "engine_executed=NO\n",ok?"PASS":"FAIL");
    return ok?0:8;
}
