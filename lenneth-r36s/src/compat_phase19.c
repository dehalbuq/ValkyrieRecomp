/* Phase19: Bionic AArch64 stat/sysconf bridge — MIT; no commercial code run.
 * Android stat uses the AOSP arm64 128-byte layout, never the host glibc layout.
 * Android _SC_* numeric values are translated rather than passed to glibc. */
#define _GNU_SOURCE
#include "compat_phase19.h"
#include "posix_fd_provider.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { int64_t sec,nsec; } GuestTime;
typedef struct {
    uint64_t device,inode;
    uint32_t mode,links,user,group;
    uint64_t rdevice,padding1;
    int64_t size;
    int32_t block_size,padding2;
    int64_t blocks;
    GuestTime atim,mtim,ctim;
    uint32_t unused4,unused5;
} GuestStat;
_Static_assert(sizeof(GuestStat)==128,"Bionic AArch64 stat size");
_Static_assert(offsetof(GuestStat,size)==48,"stat size offset");
_Static_assert(offsetof(GuestStat,block_size)==56,"stat blksize offset");
_Static_assert(offsetof(GuestStat,atim)==72,"stat atim offset");
_Static_assert(offsetof(GuestStat,ctim)==104,"stat ctim offset");

static void convert(GuestStat *out,const struct stat *s){
    memset(out,0,sizeof(*out));
    out->device=(uint64_t)s->st_dev;
    out->inode=(uint64_t)s->st_ino;
    out->mode=(uint32_t)s->st_mode;
    out->links=(uint32_t)s->st_nlink;
    out->user=(uint32_t)s->st_uid;
    out->group=(uint32_t)s->st_gid;
    out->rdevice=(uint64_t)s->st_rdev;
    out->size=(int64_t)s->st_size;
    out->block_size=(int32_t)s->st_blksize;
    out->blocks=(int64_t)s->st_blocks;
    out->atim.sec=(int64_t)s->st_atim.tv_sec;
    out->atim.nsec=(int64_t)s->st_atim.tv_nsec;
    out->mtim.sec=(int64_t)s->st_mtim.tv_sec;
    out->mtim.nsec=(int64_t)s->st_mtim.tv_nsec;
    out->ctim.sec=(int64_t)s->st_ctim.tv_sec;
    out->ctim.nsec=(int64_t)s->st_ctim.tv_nsec;
}
static int bridge_fstat(int fd,GuestStat *out){
    if(!out){errno=EFAULT;return -1;}
    struct stat s;
    if(fstat(fd,&s)!=0)return -1;
    convert(out,&s);
    return 0;
}
static int bridge_stat(const char *path,GuestStat *out){
    if(!path||!out){errno=EFAULT;return -1;}
    int fd=lenneth_fd_open_guest_path(path,O_RDONLY,0);
    if(fd<0)return -1;
    int result=bridge_fstat(fd,out);
    int saved=errno;
    close(fd);
    errno=saved;
    return result;
}
/* Android Bionic constants from libc/include/bits/sysconf.h, all other
 * values refused as EINVAL, never interpreted as glibc constants. */
static long bridge_sysconf(int number){
    switch(number){
        case 0x00:return sysconf(_SC_ARG_MAX);
        case 0x06:return sysconf(_SC_CLK_TCK);
        case 0x0b:return sysconf(_SC_OPEN_MAX);
        case 0x27:case 0x28:return sysconf(_SC_PAGESIZE);
        case 0x4c:return sysconf(_SC_THREAD_STACK_MIN);
        case 0x4f:return sysconf(_SC_THREADS);
        case 0x60:return sysconf(_SC_NPROCESSORS_CONF);
        case 0x61:return sysconf(_SC_NPROCESSORS_ONLN);
        case 0x62:return sysconf(_SC_PHYS_PAGES);
        case 0x63:return sysconf(_SC_AVPHYS_PAGES);
        default:errno=EINVAL;return -1;
    }
}
typedef struct {const char *name;void(*fn)(void);} Entry;
#define X(name,fn) {name,(void(*)(void))(fn)}
static const Entry api[]={
    X("stat",bridge_stat),X("fstat",bridge_fstat),X("sysconf",bridge_sysconf)
};
int lenneth_phase19_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(api)/sizeof(api[0]);i++){
        if(strcmp(name,api[i].name))continue;
        void (*fn)(void)=api[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));
        return 1;
    }
    return 0;
}
int lenneth_phase19_selftest(void){
    int ok=1;
    char base[]="/tmp/lenneth-phase19-XXXXXX";
    char *dir=mkdtemp(base);
    if(!dir)return 19;
    char assets[512],saves[512],file[512],link[512];
    snprintf(assets,sizeof(assets),"%s/assets",dir);
    snprintf(saves,sizeof(saves),"%s/saves",dir);
    snprintf(file,sizeof(file),"%s/assets/fixture.bin",dir);
    snprintf(link,sizeof(link),"%s/assets/link",dir);
    if(mkdir(assets,0700)!=0||mkdir(saves,0700)!=0)ok=0;
    int fd=open(file,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(fd<0)ok=0;
    else{if(write(fd,"SYNTHETIC",9)!=9)ok=0;close(fd);}
    if(symlink("/etc/passwd",link)!=0)ok=0;
    if(!lenneth_fd_provider_open(assets,saves))ok=0;
    struct {GuestStat data;unsigned char guard[16];} buf;
    memset(&buf,0xa5,sizeof(buf));
    if(bridge_stat("assets/fixture.bin",&buf.data)!=0||
       buf.data.size!=9||!S_ISREG(buf.data.mode)||
       buf.data.block_size<=0)ok=0;
    for(unsigned i=0;i<sizeof(buf.guard);i++)
        if(buf.guard[i]!=0xa5)ok=0;
    fd=lenneth_fd_open_guest_path("assets/fixture.bin",O_RDONLY,0);
    if(fd<0)ok=0;
    else{
        GuestStat other={0};
        if(bridge_fstat(fd,&other)!=0||other.size!=9||
           other.inode!=buf.data.inode)ok=0;
        close(fd);
    }
    if(bridge_stat("assets/link",&buf.data)==0||
       bridge_stat("/etc/passwd",&buf.data)==0||
       bridge_stat("assets/../saves",&buf.data)==0)ok=0;
    errno=0;
    if(bridge_fstat(-1,&buf.data)==0||errno!=EBADF)ok=0;
    errno=0;
    if(bridge_stat("assets/fixture.bin",NULL)==0||errno!=EFAULT)ok=0;
    if(bridge_sysconf(0x27)!=getpagesize()||
       bridge_sysconf(0x28)!=getpagesize()||
       bridge_sysconf(0x61)<1||bridge_sysconf(0x62)<1)ok=0;
    errno=0;
    if(bridge_sysconf(0x7fffffff)!=-1||errno!=EINVAL)ok=0;
    uint64_t address=0;
    for(unsigned i=0;i<sizeof(api)/sizeof(api[0]);i++)
        if(!lenneth_phase19_lookup(api[i].name,&address)||!address)ok=0;
    if(lenneth_phase19_lookup("system",&address)||
       lenneth_phase19_lookup("__sF",&address)||
       lenneth_phase19_lookup("setjmp",&address)||
       lenneth_phase19_lookup("syscall",&address)||
       lenneth_phase19_lookup("dl_iterate_phdr",&address))ok=0;
    lenneth_fd_provider_close();
    unlink(link);unlink(file);rmdir(assets);rmdir(saves);rmdir(dir);
    printf("[lenneth phase19] SELFTEST=%s mapped_abis=3 stat_bytes=%zu "
           "bionic_stat_layout=TRANSLATED sandbox_paths=DENIED "
           "sysconf_numbers=TRANSLATED commercial_execution=NO\n",
           ok?"PASS":"FAIL",sizeof(GuestStat));
    return ok?0:19;
}
