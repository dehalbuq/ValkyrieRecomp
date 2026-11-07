/* Lenneth Phase 17: reviewed POSIX adapters, independent MIT implementation.
 * Non-executing commercial preflight. Bionic stat/FILE, locale_t, mbstate_t,
 * setjmp, syscall, and dl_iterate_phdr remain intentionally UNMAPPED. */
#define _GNU_SOURCE
#include "compat_phase17.h"
#include "posix_fd_provider.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define MAX_GUEST_DIRS 32
typedef struct { DIR *dir; int active; } GuestDir;
static GuestDir dirs[MAX_GUEST_DIRS];
static pthread_mutex_t dir_lock=PTHREAD_MUTEX_INITIALIZER;
/* Opaque guest DIR tokens: host DIR* is not exposed, double close rejected. */
static void *compat_opendir(const char *path){
    if(!path){errno=EINVAL;return NULL;}
    int fd=lenneth_fd_open_guest_path(path,O_RDONLY|O_DIRECTORY,0);
    if(fd<0)return NULL;
    DIR *d=fdopendir(fd);
    if(!d){int e=errno;close(fd);errno=e;return NULL;}
    pthread_mutex_lock(&dir_lock);
    for(size_t i=0;i<MAX_GUEST_DIRS;i++){
        if(dirs[i].active)continue;
        dirs[i].dir=d;dirs[i].active=1;
        pthread_mutex_unlock(&dir_lock);
        return &dirs[i];
    }
    pthread_mutex_unlock(&dir_lock);
    closedir(d);errno=EMFILE;return NULL;
}
static int compat_closedir(void *opaque){
    pthread_mutex_lock(&dir_lock);
    for(size_t i=0;i<MAX_GUEST_DIRS;i++){
        if(opaque!=&dirs[i] || !dirs[i].active)continue;
        DIR *d=dirs[i].dir;
        dirs[i].active=0;dirs[i].dir=NULL;
        int ret=closedir(d);
        pthread_mutex_unlock(&dir_lock);return ret;
    }
    pthread_mutex_unlock(&dir_lock);
    errno=EBADF;return -1;
}
/* time_t=64 on ARM64; keep public struct tm in thread-local host storage. */
static struct tm *compat_gmtime(const time_t *t){
    static _Thread_local struct tm result;
    if(!t){errno=EINVAL;return NULL;}
    return gmtime_r(t,&result);
}
static struct tm *compat_localtime(const time_t *t){
    static _Thread_local struct tm result;
    if(!t){errno=EINVAL;return NULL;}
    return localtime_r(t,&result);
}
/* Android/Bionic expects the POSIX int strerror_r convention, not GNU char*. */
static int compat_strerror_r(int error,char *out,size_t cap){
    if(!out || !cap)return ERANGE;
    const char *msg=strerror(error);
    if(!msg)return EINVAL;
    size_t len=strlen(msg);
    if(len>=cap){memcpy(out,msg,cap-1);out[cap-1]=0;return ERANGE;}
    memcpy(out,msg,len+1);return 0;
}
/* Guest syslog writes to diagnostic stderr, never host system log. */
static void compat_openlog(const char *id,int option,int facility){
    (void)id;(void)option;(void)facility;
}
static void compat_closelog(void){}
static void compat_syslog(int priority,const char *fmt,...){
    if(!fmt)return;
    char line[384];
    va_list ap;va_start(ap,fmt);
    vsnprintf(line,sizeof(line),fmt,ap);
    va_end(ap);
    fprintf(stderr,"[lenneth syslog] priority=%d %s\n",priority,line);
}
/* Restrict priority queries to this host process. setpriority remains missing. */
static int compat_getpriority(int which,id_t who){
    if(which!=PRIO_PROCESS || (who!=0 && who!=(id_t)getpid())){
        errno=EPERM;return -1;
    }
    return getpriority(PRIO_PROCESS,0);
}
/* The following are plain scalar/character buffer ABI functions; wchar_t
 * is 32-bit on both Android and Linux AArch64. */
typedef struct {const char *name;void(*fn)(void);} Entry;
#define ENTRY(s,f) {s,(void(*)(void))(f)}
static const Entry entries[]={
    ENTRY("opendir",compat_opendir),ENTRY("closedir",compat_closedir),
    ENTRY("gmtime",compat_gmtime),ENTRY("localtime",compat_localtime),
    ENTRY("strftime",strftime),ENTRY("strcoll",strcoll),
    ENTRY("strxfrm",strxfrm),ENTRY("wcscoll",wcscoll),
    ENTRY("wcsxfrm",wcsxfrm),ENTRY("strerror_r",compat_strerror_r),
    ENTRY("vasprintf",vasprintf),ENTRY("syslog",compat_syslog),
    ENTRY("openlog",compat_openlog),ENTRY("closelog",compat_closelog),
    ENTRY("getpriority",compat_getpriority)
};
int lenneth_phase17_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
        if(strcmp(name,entries[i].name)!=0)continue;
        void(*fn)(void)=entries[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));return 1;
    }
    return 0;
}
static int fixture_vasprintf(char **out,const char *fmt,...){
    va_list args;va_start(args,fmt);
    int count=vasprintf(out,fmt,args);
    va_end(args);return count;
}
int lenneth_phase17_selftest(void){
    int ok=1;
    char base[]="/tmp/lenneth-phase17-XXXXXX";
    char *root=mkdtemp(base);
    if(!root)return 13;
    char assets[512],saves[512],sub[512],link[512];
    snprintf(assets,sizeof(assets),"%s/assets",root);
    snprintf(saves,sizeof(saves),"%s/saves",root);
    snprintf(sub,sizeof(sub),"%s/assets/levels",root);
    snprintf(link,sizeof(link),"%s/assets/link",root);
    if(mkdir(assets,0700)||mkdir(saves,0700)||mkdir(sub,0700))ok=0;
    if(symlink("/etc",link)!=0)ok=0;
    if(!lenneth_fd_provider_open(assets,saves))ok=0;
    void *d=compat_opendir("assets/levels");
    if(!d)ok=0;
    else if(compat_closedir(d)!=0 || compat_closedir(d)!=-1)ok=0;
    if(compat_opendir("/etc")||compat_opendir("assets/../saves")||
       compat_opendir("assets/link"))ok=0;
    const time_t epoch=0;
    const struct tm *utc=compat_gmtime(&epoch);
    if(!utc||utc->tm_year!=70||utc->tm_mon!=0||utc->tm_mday!=1)ok=0;
    if(!compat_localtime(&epoch))ok=0;
    char fmt[32]={0},xfrm[32]={0};
    if(strftime(fmt,sizeof(fmt),"%Y",utc)!=4 || strcmp(fmt,"1970"))ok=0;
    if(strcoll("a","b")>=0 || strxfrm(xfrm,"abc",sizeof(xfrm))!=3 ||
       strcmp(xfrm,"abc"))ok=0;
    wchar_t wbuf[16];
    if(wcscoll(L"a",L"b")>=0 || wcsxfrm(wbuf,L"xyz",16)!=3 ||
       wcscmp(wbuf,L"xyz"))ok=0;
    char err[64]={0};
    if(compat_strerror_r(ENOENT,err,sizeof(err)) || !err[0])ok=0;
    if(compat_strerror_r(ENOENT,err,1)!=ERANGE)ok=0;
    char *dyn=NULL;
    if(fixture_vasprintf(&dyn,"%s-%d","phase",17)!=8 || !dyn ||
       strcmp(dyn,"phase-17"))ok=0;
    free(dyn);
    compat_openlog("phase17",0,0);
    compat_syslog(6,"symbol fixture %d",17);
    compat_closelog();
    errno=0;
    if(compat_getpriority(PRIO_PROCESS,123456789u)!=-1 || errno!=EPERM)ok=0;
    uint64_t addr=0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++)
        if(!lenneth_phase17_lookup(entries[i].name,&addr)||!addr)ok=0;
    if(lenneth_phase17_lookup("__sF",&addr) ||
       lenneth_phase17_lookup("stat",&addr) ||
       lenneth_phase17_lookup("fstat",&addr) ||
       lenneth_phase17_lookup("readdir",&addr) ||
       lenneth_phase17_lookup("newlocale",&addr) ||
       lenneth_phase17_lookup("setpriority",&addr) ||
       lenneth_phase17_lookup("dl_iterate_phdr",&addr))ok=0;
    lenneth_fd_provider_close();
    unlink(link);rmdir(sub);rmdir(assets);rmdir(saves);rmdir(root);
    printf("[lenneth phase17] SELFTEST=%s abi_safe_symbols=%zu "
           "directory_sandbox=PASS locale_t_mbstate_t=UNMAPPED "
           "bionic_stat_FILE_setjmp=UNMAPPED engine_executed=NO\n",
           ok?"PASS":"FAIL",sizeof(entries)/sizeof(entries[0]));
    return ok?0:17;
}
