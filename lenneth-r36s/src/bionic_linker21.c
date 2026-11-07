/*
 * Lenneth phase 21: Bionic ARM64 guest-only program header inventory.
 * MIT, independent implementation; no proprietary code or data.
 *
 * Android's legacy dl_phdr_info callback ABI contains 4 fields with a
 * 32-byte LP64 layout. We expose only guest images registered by the
 * validated synthetic/nonexecuting ELF loader. Never forward glibc's
 * host modules, handles or addresses to the guest callback.
 *
 * system() is fail-closed: no shell, no subprocess, no side effects.
 */
#define _GNU_SOURCE
#include "bionic_linker21.h"
#include <errno.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_GUEST_PHDR_MODULES=2, MAX_GUEST_PHDRS=128 };
typedef struct {
    Elf64_Addr dlpi_addr;
    const char *dlpi_name;
    const Elf64_Phdr *dlpi_phdr;
    Elf64_Half dlpi_phnum;
    unsigned char padding[6];
} AndroidDlPhdrInfo;
_Static_assert(sizeof(AndroidDlPhdrInfo)==32,
               "Bionic ARM64 legacy dl_phdr_info layout");
_Static_assert(offsetof(AndroidDlPhdrInfo,dlpi_addr)==0,
               "invalid dlpi_addr offset");
_Static_assert(offsetof(AndroidDlPhdrInfo,dlpi_name)==8,
               "invalid dlpi_name offset");
_Static_assert(offsetof(AndroidDlPhdrInfo,dlpi_phdr)==16,
               "invalid dlpi_phdr offset");
_Static_assert(offsetof(AndroidDlPhdrInfo,dlpi_phnum)==24,
               "invalid dlpi_phnum offset");

typedef struct {
    char soname[64];
    uint64_t load_bias;
    Elf64_Phdr headers[MAX_GUEST_PHDRS];
    unsigned count;
} GuestImage;

static GuestImage images[MAX_GUEST_PHDR_MODULES];
static size_t image_count;
static pthread_mutex_t images_lock=PTHREAD_MUTEX_INITIALIZER;

int lenneth_phase21_register(const char *soname,uint64_t load_bias,
                            const Elf64_Phdr *phdr,unsigned phnum){
    if(!soname||!phdr||!phnum||phnum>MAX_GUEST_PHDRS ||
       !*soname||strnlen(soname,64)>=64){
        errno=EINVAL;return 0;
    }
    /* Only the two supported guest sonames can be enumerated; never a host
     * library or user-supplied pathname. */
    if(strcmp(soname,"libMyGame.so") && strcmp(soname,"libc++_shared.so")){
        errno=EACCES;return 0;
    }
    pthread_mutex_lock(&images_lock);
    if(image_count==MAX_GUEST_PHDR_MODULES){
        pthread_mutex_unlock(&images_lock);
        errno=ENOSPC;return 0;
    }
    for(size_t i=0;i<image_count;i++){
        if(strcmp(images[i].soname,soname)==0){
            pthread_mutex_unlock(&images_lock);
            errno=EEXIST;return 0;
        }
    }
    GuestImage *item=&images[image_count];
    memset(item,0,sizeof(*item));
    memcpy(item->soname,soname,strlen(soname)+1);
    memcpy(item->headers,phdr,(size_t)phnum*sizeof(Elf64_Phdr));
    item->load_bias=load_bias;
    item->count=phnum;
    image_count++;
    pthread_mutex_unlock(&images_lock);
    return 1;
}
void lenneth_phase21_clear(void){
    pthread_mutex_lock(&images_lock);
    memset(images,0,sizeof(images));
    image_count=0;
    pthread_mutex_unlock(&images_lock);
}
static int guest_dl_iterate_phdr(int (*callback)(AndroidDlPhdrInfo*,size_t,void*),
                                 void *data){
    if(!callback){errno=EINVAL;return -1;}
    /* Snapshot removes any possibility of invoking guest callbacks while a
     * provider lock is held, and keeps ELF headers valid for each callback. */
    GuestImage *copy=calloc(MAX_GUEST_PHDR_MODULES,sizeof(GuestImage));
    if(!copy){errno=ENOMEM;return -1;}
    pthread_mutex_lock(&images_lock);
    size_t n=image_count;
    if(n>MAX_GUEST_PHDR_MODULES)n=MAX_GUEST_PHDR_MODULES;
    memcpy(copy,images,n*sizeof(*copy));
    pthread_mutex_unlock(&images_lock);
    int result=0;
    for(size_t i=0;i<n;i++){
        AndroidDlPhdrInfo info={0};
        info.dlpi_addr=copy[i].load_bias;
        info.dlpi_name=copy[i].soname;
        info.dlpi_phdr=copy[i].headers;
        info.dlpi_phnum=(Elf64_Half)copy[i].count;
        result=callback(&info,sizeof(info),data);
        if(result)break;
    }
    free(copy);
    return result;
}
static int guest_system(const char *command){
    /* Android's system(NULL) asks whether a shell is available. We do not
     * expose a shell, so return false. Otherwise return conventional exit
     * status 127, never execute a command nor spawn a child process. */
    if(!command)return 0;
    return 127<<8;
}
typedef struct { const char *name; void (*fn)(void); } Symbol;
#define S(name,fn) {name,(void(*)(void))(fn)}
static const Symbol symbols[]={
    S("dl_iterate_phdr",guest_dl_iterate_phdr),
    S("system",guest_system)
};
int lenneth_phase21_lookup(const char *name,uint64_t *address){
    if(!name||!address)return 0;
    for(size_t i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++){
        if(strcmp(symbols[i].name,name)!=0)continue;
        void(*fn)(void)=symbols[i].fn;
        if(sizeof(fn)!=sizeof(*address))return 0;
        memcpy(address,&fn,sizeof(fn));
        return 1;
    }
    return 0;
}
typedef struct { int calls; int distinct; int bad; int stop; } Context;
static int inspect_callback(AndroidDlPhdrInfo *info,size_t size,void *data){
    Context *c=(Context*)data;
    if(!info||!c||size!=32||!info->dlpi_name||!info->dlpi_phdr ||
       info->dlpi_phnum!=2)c->bad++;
    else if((!strcmp(info->dlpi_name,"libMyGame.so") &&
             info->dlpi_addr==UINT64_C(0x600000)) ||
            (!strcmp(info->dlpi_name,"libc++_shared.so") &&
             info->dlpi_addr==UINT64_C(0x700000))){
        c->distinct++;
        if(info->dlpi_phdr[0].p_type!=PT_LOAD ||
           info->dlpi_phdr[1].p_type!=PT_DYNAMIC)c->bad++;
    } else c->bad++;
    c->calls++;
    return c->stop?41:0;
}
int lenneth_phase21_selftest(void){
    int ok=1;
    Elf64_Phdr fake[2]={{.p_type=PT_LOAD,.p_vaddr=0x1000},
                        {.p_type=PT_DYNAMIC,.p_vaddr=0x2000}};
    lenneth_phase21_clear();
    Context empty={0};
    if(guest_dl_iterate_phdr(inspect_callback,&empty)!=0 || empty.calls!=0)ok=0;
    if(!lenneth_phase21_register("libMyGame.so",0x600000,fake,2) ||
       !lenneth_phase21_register("libc++_shared.so",0x700000,fake,2))ok=0;
    errno=0;
    if(lenneth_phase21_register("libGLESv2.so",0x800000,fake,2) ||
       errno!=EACCES)ok=0;
    errno=0;
    if(lenneth_phase21_register("libMyGame.so",0x600000,fake,2) ||
       errno!=ENOSPC)ok=0;
    Context ctx={0};
    if(guest_dl_iterate_phdr(inspect_callback,&ctx)!=0 ||
       ctx.calls!=2 || ctx.distinct!=2 || ctx.bad)ok=0;
    Context stop={.stop=1};
    if(guest_dl_iterate_phdr(inspect_callback,&stop)!=41 ||
       stop.calls!=1 || stop.bad)ok=0;
    errno=0;
    if(guest_dl_iterate_phdr(NULL,&ctx)!=-1 || errno!=EINVAL)ok=0;
    if(guest_system(NULL)!=0 ||
       guest_system("touch /tmp/never-run-phase21")!=(127<<8))ok=0;
    uint64_t addr=0;
    if(!lenneth_phase21_lookup("dl_iterate_phdr",&addr)||!addr ||
       !lenneth_phase21_lookup("system",&addr)||!addr ||
       lenneth_phase21_lookup("__sF",&addr) ||
       lenneth_phase21_lookup("setjmp",&addr) ||
       lenneth_phase21_lookup("longjmp",&addr))ok=0;
    lenneth_phase21_clear();
    if(guest_dl_iterate_phdr(inspect_callback,&ctx)!=0 ||
       ctx.calls!=2)ok=0;
    printf("[lenneth phase21] SELFTEST=%s guest_phdr_modules=2 "
           "phdr_legacy_bytes=32 callback_stop=PASS host_phdrs=HIDDEN "
           "system_execution=DENIED unresolved_Bionic_FILE_jmpbuf=1 "
           "commercial_execution=NO\n",ok?"PASS":"FAIL");
    return ok?0:21;
}
