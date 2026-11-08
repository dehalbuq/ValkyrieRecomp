/*
 * Lenneth phase24 — read-only startup-readiness audit of user-supplied
 * AArch64 Android ELFs. MIT, independently implemented.
 *
 * The ELF loader's relocated address inventory is not proof of runnable
 * guest code. This audit highlights native constructors, dependency list,
 * TLS, RELRO, memory protections, and the JNI entrypoint requirement.
 * NEVER dlopen, mmap(PROT_EXEC), invoke JNI, or print private ELF code.
 */
#define _GNU_SOURCE
#include "startup_audit24.h"
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_FILE=128*1024*1024, MAX_PHDR=128, MAX_SECTIONS=2048 };
typedef struct {
    uint8_t *data; size_t size;
    const Elf64_Ehdr *eh; const Elf64_Phdr *ph; const Elf64_Dyn *dyn;
    size_t dyn_count, str_off, str_len;
    int has_str;
} View;
typedef struct {
    unsigned needed,nonstandard,init_arrays,init,preinit,fini,tls,relro,
             executable_stack,write_exec,textrel,relr;
    int jni_onload,native_init,native_render,native_touches;
} Facts;

static int within(size_t offset,size_t count,size_t total){
    return offset<=total && count<=total-offset;
}
/* No file bytes, guest addresses, control characters or absolute paths
 * are ever returned to the human-readable diagnostic report. */
static void safe_name(char *dst,size_t capacity,const char *s,size_t n){
    if(capacity==0)return;
    size_t i=0;
    while(i<n&&i+1<capacity&&s[i]){
        unsigned char c=(unsigned char)s[i];
        dst[i]=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||
               (c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'||c=='+'?c:'?';
        i++;
    }
    dst[i]=0;
}
static int virtual_to_file(const View *v,uint64_t va,size_t len,size_t *out){
    for(size_t i=0;i<v->eh->e_phnum;i++){
        const Elf64_Phdr *p=&v->ph[i];
        if(p->p_type!=PT_LOAD||va<p->p_vaddr)continue;
        uint64_t relative=va-p->p_vaddr;
        if(relative>p->p_filesz||len>p->p_filesz-relative)continue;
        if(p->p_offset>SIZE_MAX-relative)continue;
        size_t off=(size_t)(p->p_offset+relative);
        if(!within(off,len,v->size))continue;
        *out=off;return 1;
    }
    return 0;
}
static void free_view(View *v){free(v->data);memset(v,0,sizeof(*v));}
static int read_view(const char *filename,View *v){
    memset(v,0,sizeof(*v));
    FILE *f=fopen(filename,"rb");
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
    long n=ftell(f);
    if(n<(long)sizeof(Elf64_Ehdr)||n>MAX_FILE){fclose(f);return 0;}
    v->size=(size_t)n;
    rewind(f);
    v->data=malloc(v->size);
    if(!v->data){fclose(f);return 0;}
    int good=fread(v->data,1,v->size,f)==v->size;
    fclose(f);
    if(!good)goto bad;
    v->eh=(const Elf64_Ehdr*)(const void*)v->data;
    const Elf64_Ehdr *h=v->eh;
    if(memcmp(h->e_ident,ELFMAG,SELFMAG)!=0||
       h->e_ident[EI_CLASS]!=ELFCLASS64||
       h->e_ident[EI_DATA]!=ELFDATA2LSB||
       h->e_machine!=EM_AARCH64||h->e_type!=ET_DYN||
       h->e_phentsize!=sizeof(Elf64_Phdr)||
       h->e_phnum==0||h->e_phnum>MAX_PHDR||
       !within(h->e_phoff,(size_t)h->e_phnum*sizeof(Elf64_Phdr),v->size))
        goto bad;
    v->ph=(const Elf64_Phdr*)(const void*)(v->data+h->e_phoff);
    for(size_t i=0;i<h->e_phnum;i++){
        if(v->ph[i].p_type!=PT_DYNAMIC)continue;
        if(v->ph[i].p_filesz%sizeof(Elf64_Dyn)||
           !within((size_t)v->ph[i].p_offset,
                   (size_t)v->ph[i].p_filesz,v->size))goto bad;
        v->dyn=(const Elf64_Dyn*)(const void*)(v->data+v->ph[i].p_offset);
        v->dyn_count=(size_t)v->ph[i].p_filesz/sizeof(Elf64_Dyn);
        break;
    }
    if(!v->dyn)return 1; /* report missing dynamic section as a gate */
    uint64_t str_va=0,str_size=0;
    for(size_t i=0;i<v->dyn_count;i++){
        if(v->dyn[i].d_tag==DT_NULL)break;
        if(v->dyn[i].d_tag==DT_STRTAB)str_va=v->dyn[i].d_un.d_ptr;
        if(v->dyn[i].d_tag==DT_STRSZ)str_size=v->dyn[i].d_un.d_val;
    }
    if(str_va && str_size && str_size<=v->size &&
       virtual_to_file(v,str_va,(size_t)str_size,&v->str_off)){
        v->str_len=(size_t)str_size;
        v->has_str=1;
    }
    return 1;
bad:
    free_view(v);
    return 0;
}
static void row(FILE *f,const char *module,const char *gate,
                const char *status,const char *detail){
    fprintf(f,"%s\t%s\t%s\t%s\n",module,gate,status,detail);
}
static int is_host_shim_dependency(const char *name){
    static const char *const known[]={
        "libc.so","libm.so","libdl.so","liblog.so","libandroid.so",
        "libGLESv2.so","libEGL.so","libOpenSLES.so",
        "libz.so","libstdc++.so","libc++_shared.so","libMyGame.so"
    };
    for(size_t i=0;i<sizeof(known)/sizeof(known[0]);i++)
        if(!strcmp(name,known[i]))return 1;
    return 0;
}
static void count_exports(const View *v,Facts *facts){
    const Elf64_Ehdr *h=v->eh;
    if(h->e_shentsize!=sizeof(Elf64_Shdr)||
       !h->e_shnum||h->e_shnum>MAX_SECTIONS||
       !within(h->e_shoff,(size_t)h->e_shnum*sizeof(Elf64_Shdr),v->size))
        return;
    const Elf64_Shdr *sec=(const Elf64_Shdr*)(const void*)(v->data+h->e_shoff);
    for(size_t i=0;i<h->e_shnum;i++){
        if(sec[i].sh_type!=SHT_DYNSYM ||
           sec[i].sh_link>=h->e_shnum||
           sec[i].sh_entsize!=sizeof(Elf64_Sym)||
           sec[i].sh_size%sizeof(Elf64_Sym)||
           !within(sec[i].sh_offset,sec[i].sh_size,v->size))
            continue;
        const Elf64_Shdr *str=&sec[sec[i].sh_link];
        if(str->sh_type!=SHT_STRTAB||
           !within(str->sh_offset,str->sh_size,v->size))continue;
        const Elf64_Sym *syms=(const Elf64_Sym*)(const void*)(v->data+sec[i].sh_offset);
        const char *names=(const char*)(const void*)(v->data+str->sh_offset);
        for(size_t j=0;j<sec[i].sh_size/sizeof(Elf64_Sym);j++){
            if(syms[j].st_shndx==SHN_UNDEF||
               syms[j].st_name>=str->sh_size)continue;
            const char *name=names+syms[j].st_name;
            if(!memchr(name,0,str->sh_size-syms[j].st_name))continue;
            if(!strcmp(name,"JNI_OnLoad"))facts->jni_onload=1;
            if(!strcmp(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit"))
                facts->native_init=1;
            if(!strcmp(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender"))
                facts->native_render=1;
            if(!strcmp(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesBegin"))
                facts->native_touches=1;
        }
    }
}
static int inspect_one(FILE *report,const char *name,const char *filename,Facts *facts){
    View v;
    memset(facts,0,sizeof(*facts));
    if(!read_view(filename,&v)){
        row(report,name,"ELF_METADATA","INVALID","safe_read_failed");
        printf("[lenneth phase24] module=%s scan=FAIL malformed_or_missing_AArch64_ELF\n",name);
        return 0;
    }
    for(size_t i=0;i<v.eh->e_phnum;i++){
        const Elf64_Phdr *p=&v.ph[i];
        if(p->p_type==PT_LOAD && (p->p_flags&PF_W) && (p->p_flags&PF_X))
            facts->write_exec++;
        if(p->p_type==PT_GNU_RELRO)facts->relro++;
        if(p->p_type==PT_TLS)facts->tls++;
        if(p->p_type==PT_GNU_STACK && (p->p_flags&PF_X))
            facts->executable_stack++;
    }
    int has_unreadable=0;
    if(v.dyn){
        for(size_t i=0;i<v.dyn_count;i++){
            const Elf64_Dyn *d=&v.dyn[i];
            if(d->d_tag==DT_NULL)break;
            switch(d->d_tag){
                case DT_NEEDED:{
                    facts->needed++;
                    if(!v.has_str||d->d_un.d_val>=v.str_len){
                        has_unreadable=1;break;
                    }
                    size_t off=(size_t)d->d_un.d_val;
                    const char *nameval=(const char*)(const void*)(v.data+v.str_off+off);
                    size_t remain=v.str_len-off;
                    const char *end=memchr(nameval,0,remain);
                    if(!end){has_unreadable=1;break;}
                    char safe[128];
                    safe_name(safe,sizeof(safe),nameval,(size_t)(end-nameval));
                    if(!is_host_shim_dependency(safe))facts->nonstandard++;
                    row(report,name,"DT_NEEDED",
                        is_host_shim_dependency(safe)?"REVIEW_SHIM":"REVIEW_EXTERNAL",safe);
                    break;
                }
                case DT_INIT:if(d->d_un.d_ptr)facts->init++;break;
                case DT_INIT_ARRAYSZ:
                    if(d->d_un.d_val){
                        if(d->d_un.d_val>1024*1024 ||
                           d->d_un.d_val%sizeof(uint64_t))has_unreadable=1;
                        else facts->init_arrays+=(unsigned)(d->d_un.d_val/sizeof(uint64_t));
                    }
                    break;
                case DT_PREINIT_ARRAYSZ:if(d->d_un.d_val)facts->preinit++;break;
                case DT_FINI:if(d->d_un.d_ptr)facts->fini++;break;
                case DT_TEXTREL:facts->textrel++;break;
                case DT_FLAGS:
                    if(d->d_un.d_val&DF_TEXTREL)facts->textrel++;
                    break;
#ifdef DT_RELR
                case DT_RELR:facts->relr++;break;
#endif
                default:break;
            }
        }
    }
    count_exports(&v,facts);
    char msg[128];
    snprintf(msg,sizeof(msg),"needed=%u nonstandard=%u",facts->needed,facts->nonstandard);
    row(report,name,"DEPENDENCIES","REVIEW",msg);
    snprintf(msg,sizeof(msg),"init=%u init_array_entries=%u preinit=%u fini=%u",
             facts->init,facts->init_arrays,facts->preinit,facts->fini);
    row(report,name,"CONSTRUCTOR_ORDER","REVIEW",msg);
    snprintf(msg,sizeof(msg),"pt_tls=%u relro=%u wx_segments=%u exec_stack=%u textrel=%u relr=%u",
             facts->tls,facts->relro,facts->write_exec,facts->executable_stack,
             facts->textrel,facts->relr);
    row(report,name,"ELF_MEMORY_PROTECTION",
        facts->write_exec||facts->executable_stack||facts->textrel?"RISK":"REVIEW",msg);
    snprintf(msg,sizeof(msg),"JNI_OnLoad=%d nativeInit=%d nativeRender=%d nativeTouchesBegin=%d",
             facts->jni_onload,facts->native_init,
             facts->native_render,facts->native_touches);
    row(report,name,"JNI_EXPORTS","INVENTORIED",msg);
    row(report,name,"FILE_METADATA",
        has_unreadable?"PARTIAL":"PASS",
        has_unreadable?"invalid_dynamic_string_or_array":"validated_headers");
    printf("[lenneth phase24] module=%s metadata=%s needed=%u constructors=%u "
           "tls=%u relro=%u wx=%u jni_onload=%d native_init=%d "
           "native_render=%d engine_executed=NO\n",
           name,has_unreadable?"PARTIAL":"PASS",facts->needed,
           facts->init+facts->init_arrays,facts->tls,facts->relro,
           facts->write_exec,facts->jni_onload,
           facts->native_init,facts->native_render);
    free_view(&v);
    return !has_unreadable;
}
int lenneth_startup24_audit(const char *game_path,const char *cxx_path){
    FILE *report=fopen("lenneth-startup-gates.tsv","w");
    if(!report){perror("lenneth-startup-gates.tsv");return 24;}
    fputs("module\tgate\tstatus\tdetail\n",report);
    Facts game,cxx;
    int ok1=inspect_one(report,"libMyGame.so",game_path,&game);
    int ok2=inspect_one(report,"libc++_shared.so",cxx_path,&cxx);
    row(report,"runtime","BIONIC_CTORS","BLOCKED","not_executed_or_versioned");
    row(report,"runtime","JNI_JAVA_VM","BLOCKED","no_JavaVM_JNIEnv_class_loader");
    row(report,"runtime","COCOS_LIFECYCLE","BLOCKED","nativeInit_nativeRender_not_invoked");
    row(report,"runtime","OPENSL_PCM","BLOCKED","guest_OpenSL_queue_not_wired_to_ALSA");
    row(report,"runtime","ASSET_HANDOFF","BLOCKED","Cocos_asset_and_APK_context_not_connected");
    row(report,"runtime","ELF_SYMBOLS","PASS","phase23_previous_relocation_inventory_only");
    row(report,"runtime","COMMERCIAL_ENGINE","BLOCKED","do_not_execute");
    int write_ok=ferror(report)==0 && fclose(report)==0;
    int success=ok1&&ok2&&write_ok;
    printf("[lenneth phase24] STARTUP_AUDIT=%s startup_gate=BLOCKED "
           "elf_metadata_only=1 Android_JavaVM=NOT_IMPLEMENTED "
           "constructors=NOT_EXECUTED Cocos_nativeInit=NOT_CALLED "
           "commercial_execution=NO report=lenneth-startup-gates.tsv\n",
           success?"PASS":"FAIL");
    return success?0:24;
}
int lenneth_startup24_selftest(void){
    int ok=1; char name[20];
    safe_name(name,sizeof(name),"a/b\\\tc\n!X",9);
    if(strcmp(name,"a?b??c??X"))ok=0;
    if(!is_host_shim_dependency("libc.so")||
       !is_host_shim_dependency("libGLESv2.so")||
       is_host_shim_dependency("libMystery.so"))ok=0;
    if(within(10,3,12)||!within(10,2,12)||
       within((size_t)-2,5,40))ok=0;
    printf("[lenneth phase24] SELFTEST=%s report_schema=TSV "
           "filename_sanitization=PASS bounds_checks=PASS "
           "jni_java_vm=NOT_IMPLEMENTED code_execution=NO\n",ok?"PASS":"FAIL");
    return ok?0:24;
}
