/*
 * Phase 3: two-module AArch64 dynamic linker preflight for Android Lenneth.
 *
 * Intentionally NON-EXECUTABLE: no PROT_EXEC, constructor, JNI or game call.
 * Validates actual relocation records and resolves local and cross-module
 * ELF64 references from the Android v1.0.7 game and its own libc++ runtime.
 * References to Android/host APIs remain explicitly unresolved.
 *
 * Independently authored, MIT; no commercial code or assets included.
 */
#define _GNU_SOURCE
#include <elf.h>
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "portable_provider.h"
#include "asset_provider.h"
#include "android_log_provider.h"
#include "bionic_threads.h"
#include "bionic_fortify.h"
#include "cxx_runtime.h"

enum { MAX_ELF_BYTES = 128*1024*1024, MAX_ELF_IMAGE = 256*1024*1024 };
static int g_asset_binding_enabled=0;
static int g_android_log_binding_enabled=0;
static int g_bionic_thread_binding_enabled=0;
static int g_bionic_fortify_binding_enabled=0;
static int g_cxx_lifecycle_binding_enabled=0;
typedef struct {
    const char *label;
    uint8_t *raw, *image;
    size_t size, span, count;
    uint64_t lo;
    const Elf64_Sym *syms;
    const char *strings;
    size_t string_size;
    const Elf64_Rela *rela, *jmp;
    size_t rela_count, jmp_count;
} Module;

typedef struct {
    uint64_t relative, internal, partner, host_gles, host_portable, host_assets, host_logging, host_threads, host_fortify, host_cxx_lifecycle, unresolved, unsupported;
    uint64_t relocations;
} LinkStats;

static int bounds(size_t off,size_t len,size_t max) {
    return off <= max && len <= max - off;
}
static void unload(Module *m) {
    if(m->image)munmap(m->image,m->span);
    free(m->raw);
    memset(m,0,sizeof(*m));
}
static int load_file(Module *m,const char *name,const char *path) {
    m->label=name;
    FILE *f=fopen(path,"rb");
    if(!f) {
        fprintf(stderr,"[lenneth link] %s open: %s\n",name,strerror(errno));
        return 0;
    }
    if(fseek(f,0,SEEK_END)) {fclose(f);return 0;}
    long l=ftell(f);
    if(l<(long)sizeof(Elf64_Ehdr)||l>MAX_ELF_BYTES) {
        fprintf(stderr,"[lenneth link] %s size invalid\n",name);
        fclose(f);return 0;
    }
    m->size=(size_t)l;
    rewind(f);
    m->raw=(uint8_t*)malloc(m->size);
    if(!m->raw){fclose(f);return 0;}
    int ok=fread(m->raw,1,m->size,f)==m->size;
    fclose(f);
    if(!ok)return 0;
    Elf64_Ehdr *h=(Elf64_Ehdr*)(void*)m->raw;
    if(memcmp(h->e_ident,ELFMAG,SELFMAG)!=0 ||
       h->e_ident[EI_CLASS]!=ELFCLASS64 ||
       h->e_ident[EI_DATA]!=ELFDATA2LSB ||
       h->e_type!=ET_DYN || h->e_machine!=EM_AARCH64 ||
       h->e_phentsize!=sizeof(Elf64_Phdr) ||
       h->e_phnum==0 || h->e_phnum>128 ||
       !bounds((size_t)h->e_phoff,
               (size_t)h->e_phnum*sizeof(Elf64_Phdr),m->size)) {
        fprintf(stderr,"[lenneth link] %s invalid AArch64 ELF program headers\n",name);
        return 0;
    }
    Elf64_Phdr *ph=(Elf64_Phdr*)(void*)(m->raw+h->e_phoff);
    uint64_t low=UINT64_MAX, high=0;
    unsigned segments=0;
    for(unsigned i=0;i<h->e_phnum;i++) {
        if(ph[i].p_type!=PT_LOAD)continue;
        if(ph[i].p_filesz>ph[i].p_memsz ||
           ph[i].p_offset>m->size ||
           ph[i].p_filesz>m->size-ph[i].p_offset ||
           ph[i].p_vaddr>UINT64_MAX-ph[i].p_memsz ||
           ph[i].p_vaddr+ph[i].p_memsz>UINT64_MAX-4095) {
            fprintf(stderr,"[lenneth link] %s bad PT_LOAD\n",name);return 0;
        }
        uint64_t a=ph[i].p_vaddr&~UINT64_C(4095);
        uint64_t b=(ph[i].p_vaddr+ph[i].p_memsz+4095)&~UINT64_C(4095);
        if(a<low)low=a;
        if(b>high)high=b;
        segments++;
    }
    if(!segments || high<=low || high-low>MAX_ELF_IMAGE) {
        fprintf(stderr,"[lenneth link] %s unreasonable image size\n",name);
        return 0;
    }
    m->lo=low;
    m->span=(size_t)(high-low);
    m->image=(uint8_t*)mmap(NULL,m->span,PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(m->image==MAP_FAILED) {
        m->image=NULL;
        fprintf(stderr,"[lenneth link] %s mmap failed: %s\n",name,strerror(errno));
        return 0;
    }
    for(unsigned i=0;i<h->e_phnum;i++) {
        if(ph[i].p_type!=PT_LOAD)continue;
        if(ph[i].p_vaddr<low || ph[i].p_vaddr-low>m->span ||
           ph[i].p_memsz>m->span-(ph[i].p_vaddr-low)){
            fprintf(stderr,"[lenneth link] %s bad virtual segment\n",name);
            return 0;
        }
        memcpy(m->image+(size_t)(ph[i].p_vaddr-low),
               m->raw+(size_t)ph[i].p_offset,(size_t)ph[i].p_filesz);
    }
    if(h->e_shentsize!=sizeof(Elf64_Shdr) || h->e_shnum==0 ||
       h->e_shnum>2048 || !bounds((size_t)h->e_shoff,
               (size_t)h->e_shnum*sizeof(Elf64_Shdr),m->size)){
        fprintf(stderr,"[lenneth link] %s requires valid dynamic symbol sections\n",name);
        return 0;
    }
    Elf64_Shdr *sh=(Elf64_Shdr*)(void*)(m->raw+h->e_shoff);
    for(unsigned i=0;i<h->e_shnum;i++) {
        if(sh[i].sh_type!=SHT_DYNSYM)continue;
        if(sh[i].sh_link>=h->e_shnum ||
           sh[i].sh_entsize!=sizeof(Elf64_Sym) ||
           sh[i].sh_size%sizeof(Elf64_Sym) ||
           !bounds((size_t)sh[i].sh_offset,(size_t)sh[i].sh_size,m->size))
            continue;
        const Elf64_Shdr *str=&sh[sh[i].sh_link];
        if(str->sh_type!=SHT_STRTAB ||
           !bounds((size_t)str->sh_offset,(size_t)str->sh_size,m->size))
            continue;
        m->syms=(const Elf64_Sym*)(const void*)(m->raw+sh[i].sh_offset);
        m->count=(size_t)sh[i].sh_size/sizeof(Elf64_Sym);
        m->strings=(const char*)m->raw+str->sh_offset;
        m->string_size=(size_t)str->sh_size;
        break;
    }
    if(!m->syms || !m->strings) {
        fprintf(stderr,"[lenneth link] %s missing dynamic symbols\n",name);
        return 0;
    }
    const Elf64_Dyn *dyn=NULL;
    size_t dcount=0;
    for(unsigned i=0;i<h->e_phnum;i++) {
        if(ph[i].p_type!=PT_DYNAMIC)continue;
        if(ph[i].p_vaddr<low || ph[i].p_vaddr-low>m->span ||
           ph[i].p_memsz>m->span-(ph[i].p_vaddr-low))
            return 0;
        dyn=(const Elf64_Dyn*)(const void*)(m->image+
                                            (size_t)(ph[i].p_vaddr-low));
        dcount=(size_t)ph[i].p_memsz/sizeof(Elf64_Dyn);
        break;
    }
    if(!dyn)return 0;
    uint64_t raddr=0,rlen=0,rent=0,jaddr=0,jlen=0,pltrel=0;
    for(size_t i=0;i<dcount;i++){
        if(dyn[i].d_tag==DT_NULL)break;
        switch(dyn[i].d_tag){
            case DT_RELA:raddr=dyn[i].d_un.d_ptr;break;
            case DT_RELASZ:rlen=dyn[i].d_un.d_val;break;
            case DT_RELAENT:rent=dyn[i].d_un.d_val;break;
            case DT_JMPREL:jaddr=dyn[i].d_un.d_ptr;break;
            case DT_PLTRELSZ:jlen=dyn[i].d_un.d_val;break;
            case DT_PLTREL:pltrel=dyn[i].d_un.d_val;break;
            default:break;
        }
    }
    if(!raddr || rent!=sizeof(Elf64_Rela) ||
       !rlen || rlen%sizeof(Elf64_Rela) ||
       raddr<low || raddr-low>m->span ||
       rlen>m->span-(raddr-low) ||
       jlen%sizeof(Elf64_Rela) ||
       (jlen && (jaddr<low || jaddr-low>m->span ||
                 jlen>m->span-(jaddr-low) || pltrel!=DT_RELA))){
        fprintf(stderr,"[lenneth link] %s unsupported RELA metadata\n",name);
        return 0;
    }
    m->rela=(const Elf64_Rela*)(const void*)(m->image+(size_t)(raddr-low));
    m->rela_count=(size_t)(rlen/sizeof(Elf64_Rela));
    if(jlen){
        m->jmp=(const Elf64_Rela*)(const void*)(m->image+(size_t)(jaddr-low));
        m->jmp_count=(size_t)(jlen/sizeof(Elf64_Rela));
    }
    printf("[lenneth link] mapped %s file=%zu image=%zu dynsyms=%zu RELA=%zu PLT=%zu prot=RW, never EXEC\n",
           name,m->size,m->span,m->count,m->rela_count,m->jmp_count);
    return 1;
}
static const char *symname(const Module *m,size_t idx) {
    if(idx>=m->count)return NULL;
    size_t off=(size_t)m->syms[idx].st_name;
    if(off>=m->string_size)return NULL;
    const char *p=m->strings+off;
    return memchr(p,0,m->string_size-off)?p:NULL;
}
static int exported(const Elf64_Sym *s) {
    int type=ELF64_ST_TYPE(s->st_info);
    int bind=ELF64_ST_BIND(s->st_info);
    int visibility=ELF64_ST_VISIBILITY(s->st_other);
    return s->st_shndx!=SHN_UNDEF &&
           type!=STT_TLS &&
           (bind==STB_GLOBAL||bind==STB_WEAK) &&
           (visibility==STV_DEFAULT||visibility==STV_PROTECTED);
}
static int addr_from(const Module *m,const Elf64_Sym *sym,
                     uint64_t *addr) {
    if(sym->st_shndx==SHN_ABS){
        *addr=sym->st_value;
        return 1;
    }
    if(sym->st_value<m->lo ||
       sym->st_value-m->lo>=m->span)return 0;
    *addr=(uint64_t)(uintptr_t)(m->image+(size_t)(sym->st_value-m->lo));
    return 1;
}
static int find_export(const Module *m,const char *name,uint64_t *addr) {
    if(!name||!*name)return 0;
    for(size_t i=0;i<m->count;i++){
        const Elf64_Sym *s=&m->syms[i];
        if(!exported(s))continue;
        const char *candidate=symname(m,i);
        if(candidate && strcmp(candidate,name)==0)
            return addr_from(m,s,addr);
    }
    return 0;
}

/*
 * Phase 4 host GLES/EGL provider. Only native graphics symbols are
 * eligible; Bionic pthread / libc and Android JNI require ABI shims.
 * The resolved pointers are written to RW *data slots* only. They are
 * NEVER invoked by this diagnostic.
 */
typedef void *(*EglGetProc)(const char *);
typedef struct {
    void *gles;
    void *egl;
    EglGetProc getproc;
    int enabled;
} GlesProvider;
static GlesProvider g_provider;

static void *open_graphics(const char *primary,const char *fallback) {
    void *h=dlopen(primary,RTLD_NOW|RTLD_LOCAL);
    if(!h)h=dlopen(fallback,RTLD_NOW|RTLD_LOCAL);
    return h;
}
static int is_graphics_symbol(const char *name) {
    if(!name)return 0;
    /* Exclude unrelated libc names beginning with "gl", such as glob. */
    if(name[0]=='g' && name[1]=='l' &&
       name[2]>='A' && name[2]<='Z')return 1;
    return name[0]=='e' && name[1]=='g' && name[2]=='l' &&
           name[3]>='A' && name[3]<='Z';
}
static void provider_init(void) {
    memset(&g_provider,0,sizeof(g_provider));
    g_provider.enabled=1;
    g_provider.gles=open_graphics("libGLESv2.so.2","libGLESv2.so");
    g_provider.egl=open_graphics("libEGL.so.1","libEGL.so");
    void *p=g_provider.egl?dlsym(g_provider.egl,"eglGetProcAddress"):NULL;
    if(p && sizeof(p)==sizeof(g_provider.getproc))
        memcpy(&g_provider.getproc,&p,sizeof(p));
    printf("[lenneth host] GLESv2_provider=%s EGL_provider=%s eglGetProcAddress=%s\n",
           g_provider.gles?"loaded":"missing",
           g_provider.egl?"loaded":"missing",
           g_provider.getproc?"available":"unavailable");
}
static void provider_close(void) {
    if(g_provider.egl)dlclose(g_provider.egl);
    if(g_provider.gles)dlclose(g_provider.gles);
    memset(&g_provider,0,sizeof(g_provider));
}
static int graphics_lookup(const char *name,uint64_t *value) {
    if(!g_provider.enabled || !is_graphics_symbol(name))return 0;
    void *p=NULL;
    if(strncmp(name,"egl",3)==0) {
        if(g_provider.egl)p=dlsym(g_provider.egl,name);
    } else {
        if(g_provider.gles)p=dlsym(g_provider.gles,name);
    }
    if(!p && g_provider.getproc)p=g_provider.getproc(name);
    if(!p)return 0;
    *value=(uint64_t)(uintptr_t)p;
    return 1;
}

static const char *category(const char *s) {
    if(!s || !*s)return "unnamed";
    if(is_graphics_symbol(s))return "GLES2/EGL";
    if(strncmp(s,"AAsset",6)==0)return "Android AAssetManager";
    if(strncmp(s,"SL_IID_",7)==0 ||
       strcmp(s,"slCreateEngine")==0)return "OpenSL ES";
    if(strncmp(s,"pthread_",8)==0)return "Android Bionic pthread";
    if(strncmp(s,"__android_",10)==0 ||
       strncmp(s,"android_",8)==0)return "Android logging/NDK";
    if(strncmp(s,"__cxa_",6)==0 ||
       (s[0]=='_' && s[1]=='Z'))return "C++ ABI";
    return "Bionic/libc/libm/dl";
}
static int relocate_table(Module *m,Module *partner,
                          const Elf64_Rela *rela,size_t n,LinkStats *st){
    for(size_t i=0;i<n;i++){
        const Elf64_Rela *r=&rela[i];
        unsigned type=ELF64_R_TYPE(r->r_info);
        uint32_t symix=ELF64_R_SYM(r->r_info);
        if(type==R_AARCH64_NONE)continue;
        st->relocations++;
        if(r->r_offset<m->lo || r->r_offset-m->lo>m->span ||
           8>m->span-(size_t)(r->r_offset-m->lo)) {
            fprintf(stderr,"[lenneth link] %s bad relocation address # %zu\n",
                    m->label,i);
            return 0;
        }
        uint64_t value=0;
        if(type==R_AARCH64_RELATIVE) {
            value=(uint64_t)(uintptr_t)m->image-m->lo+
                  (uint64_t)r->r_addend;
            st->relative++;
        } else if(type==R_AARCH64_ABS64 ||
                  type==R_AARCH64_GLOB_DAT ||
                  type==R_AARCH64_JUMP_SLOT) {
            if(symix>=m->count){
                fprintf(stderr,"[lenneth link] %s invalid symbol index %u\n",
                        m->label,(unsigned)symix);
                return 0;
            }
            const Elf64_Sym *s=&m->syms[symix];
            const char *name=symname(m,symix);
            if(!name){
                fprintf(stderr,"[lenneth link] %s malformed symbol string\n",m->label);
                return 0;
            }
            if(s->st_shndx!=SHN_UNDEF) {
                if(!addr_from(m,s,&value)){st->unsupported++;continue;}
                st->internal++;
            } else if(find_export(partner,name,&value)) {
                st->partner++;
            } else if(graphics_lookup(name,&value)) {
                st->host_gles++;
            } else if(ELF64_ST_TYPE(s->st_info)==STT_FUNC &&
                      lenneth_portable_lookup(name,&value)) {
                st->host_portable++;
            } else if(g_asset_binding_enabled &&
                      (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
                       ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
                      lenneth_asset_lookup(name,&value)) {
                st->host_assets++;
            } else if(g_android_log_binding_enabled &&
                      (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
                       ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
                      lenneth_android_log_lookup(name,&value)) {
                st->host_logging++;
            } else if(g_bionic_thread_binding_enabled &&
                      (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
                       ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
                      lenneth_bionic_thread_lookup(name,&value)) {
                st->host_threads++;
            } else if(g_bionic_fortify_binding_enabled &&
                      (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
                       ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
                      lenneth_bionic_fortify_lookup(name,&value)) {
                st->host_fortify++;
            } else if(g_cxx_lifecycle_binding_enabled &&
                      (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
                       ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
                      lenneth_cxx_runtime_lookup(name,&value)) {
                st->host_cxx_lifecycle++;
            } else {
                st->unresolved++;
                continue;
            }
            if(type==R_AARCH64_ABS64)
                value+=(uint64_t)r->r_addend;
        } else {
            st->unsupported++;
            continue;
        }
        memcpy(m->image+(size_t)(r->r_offset-m->lo),&value,sizeof(value));
    }
    return 1;
}
/* One complete, privacy-preserving blocker inventory per batch run.
 * Symbol names only; never dump proprietary code, addresses or game data. */
static size_t dump_blockers(FILE *out,const Module *m,const Module *partner){
    size_t count=0;
    for(size_t i=0;i<m->count;i++){
        const Elf64_Sym *sym=&m->syms[i];
        if(sym->st_shndx!=SHN_UNDEF)continue;
        const char *name=symname(m,i);
        if(!name||!*name)continue;
        uint64_t address=0;
        if(find_export(partner,name,&address) ||
           graphics_lookup(name,&address) ||
           (ELF64_ST_TYPE(sym->st_info)==STT_FUNC &&
            lenneth_portable_lookup(name,&address)) ||
           (g_asset_binding_enabled &&
            (ELF64_ST_TYPE(sym->st_info)==STT_FUNC ||
             ELF64_ST_TYPE(sym->st_info)==STT_NOTYPE) &&
            lenneth_asset_lookup(name,&address)) ||
           (g_android_log_binding_enabled &&
            (ELF64_ST_TYPE(sym->st_info)==STT_FUNC ||
             ELF64_ST_TYPE(sym->st_info)==STT_NOTYPE) &&
            lenneth_android_log_lookup(name,&address)) ||
           (g_bionic_thread_binding_enabled &&
            (ELF64_ST_TYPE(sym->st_info)==STT_FUNC ||
             ELF64_ST_TYPE(sym->st_info)==STT_NOTYPE) &&
            lenneth_bionic_thread_lookup(name,&address)) ||
           (g_bionic_fortify_binding_enabled &&
            (ELF64_ST_TYPE(sym->st_info)==STT_FUNC ||
             ELF64_ST_TYPE(sym->st_info)==STT_NOTYPE) &&
            lenneth_bionic_fortify_lookup(name,&address)) ||
           (g_cxx_lifecycle_binding_enabled &&
            (ELF64_ST_TYPE(sym->st_info)==STT_FUNC ||
             ELF64_ST_TYPE(sym->st_info)==STT_NOTYPE) &&
            lenneth_cxx_runtime_lookup(name,&address)))continue;
        const char *risk=category(name);
        const char *strength=ELF64_ST_BIND(sym->st_info)==STB_WEAK?
                             "weak_optional":"strong_or_other";
        fprintf(out,"%s\t%s\t%s\t",m->label,risk,strength);
        /* Strip unexpected newlines/tabs in binary symbol names. */
        for(const unsigned char *p=(const unsigned char*)name;*p;p++)
            fputc(*p=='\n'||*p=='\r'||*p=='\t'?'_':*p,out);
        fputc('\n',out);
        count++;
    }
    return count;
}

static size_t unresolved_import_names(const Module *m,
                                      const Module *partner) {
    size_t n=0;
    unsigned count[7]={0};
    const char *tags[]={"GLES2/EGL","Android AAssetManager",
        "OpenSL ES","Android Bionic pthread","Android logging/NDK",
        "C++ ABI","Bionic/libc/libm/dl"};
    unsigned sample_count[7]={0};
    for(size_t i=0;i<m->count;i++){
        const Elf64_Sym *s=&m->syms[i];
        if(s->st_shndx!=SHN_UNDEF || !ELF64_ST_BIND(s->st_info))continue;
        const char *name=symname(m,i);
        if(!name||!*name)continue;
        uint64_t addr;
        if(find_export(partner,name,&addr))continue;
        if(graphics_lookup(name,&addr))continue;
        if(ELF64_ST_TYPE(s->st_info)==STT_FUNC &&
           lenneth_portable_lookup(name,&addr))continue;
        if(g_asset_binding_enabled &&
           (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
            ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
           lenneth_asset_lookup(name,&addr))continue;
        if(g_android_log_binding_enabled &&
           (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
            ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
           lenneth_android_log_lookup(name,&addr))continue;
        if(g_bionic_thread_binding_enabled &&
           (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
            ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
           lenneth_bionic_thread_lookup(name,&addr))continue;
        if(g_bionic_fortify_binding_enabled &&
           (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
            ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
           lenneth_bionic_fortify_lookup(name,&addr))continue;
        if(g_cxx_lifecycle_binding_enabled &&
           (ELF64_ST_TYPE(s->st_info)==STT_FUNC ||
            ELF64_ST_TYPE(s->st_info)==STT_NOTYPE) &&
           lenneth_cxx_runtime_lookup(name,&addr))continue;
        const char *cat=category(name);
        size_t c=6;
        for(size_t j=0;j<6;j++)if(strcmp(cat,tags[j])==0){c=j;break;}
        count[c]++;n++;
        if(sample_count[c]++<6)
            printf("[lenneth link] %s pending %s: %s\n",m->label,cat,name);
    }
    for(size_t i=0;i<7;i++)
        if(count[i])printf("[lenneth link] %s pending category=%s unique=%u\n",
                           m->label,tags[i],count[i]);
    return n;
}
int lenneth_link_preflight_mode(const char *game_path,const char *cxx_path,
                              int provider_mode) {
    Module game={0},cxx={0};
    LinkStats gs={0},cs={0};
    int rc=3;
    g_asset_binding_enabled=(provider_mode>=3);
    g_android_log_binding_enabled=(provider_mode>=4);
    g_bionic_thread_binding_enabled=(provider_mode>=6);
    g_bionic_fortify_binding_enabled=(provider_mode>=7);
    g_cxx_lifecycle_binding_enabled=(provider_mode>=8);
    if(provider_mode>=1)provider_init();
    if(provider_mode>=2)lenneth_portable_open();
    if(provider_mode>=3){
        if(!lenneth_asset_set_root("assets"))
            printf("[lenneth asset] root=missing; filesystem API mapped but cannot open files\n");
    }
    if(!load_file(&cxx,"libc++_shared",cxx_path) ||
       !load_file(&game,"libMyGame",game_path)) {
        fprintf(stderr,"[lenneth link] load failed; stopping without calling anything\n");
        goto cleanup;
    }
    if(!relocate_table(&cxx,&game,cxx.rela,cxx.rela_count,&cs)||
       !relocate_table(&cxx,&game,cxx.jmp,cxx.jmp_count,&cs)||
       !relocate_table(&game,&cxx,game.rela,game.rela_count,&gs)||
       !relocate_table(&game,&cxx,game.jmp,game.jmp_count,&gs)) {
        fprintf(stderr,"[lenneth link] invalid relocation; stopping\n");
        goto cleanup;
    }
    printf("[lenneth link] libMyGame: relocs=%" PRIu64
           " RELATIVE=%" PRIu64 " internal=%" PRIu64
           " cross_libcxx=%" PRIu64 " pending_host=%" PRIu64
           " host_gles_slots=%" PRIu64 " host_portable_slots=%" PRIu64
           " host_asset_slots=%" PRIu64 " host_log_slots=%" PRIu64
           " host_thread_slots=%" PRIu64 " host_fortify_slots=%" PRIu64
           " host_cxx_slots=%" PRIu64 " unsupported=%" PRIu64 "\n",
           gs.relocations,gs.relative,gs.internal,gs.partner,
           gs.unresolved,gs.host_gles,gs.host_portable,gs.host_assets,gs.host_logging,gs.host_threads,gs.host_fortify,gs.host_cxx_lifecycle,gs.unsupported);
    printf("[lenneth link] libc++_shared: relocs=%" PRIu64
           " RELATIVE=%" PRIu64 " internal=%" PRIu64
           " cross_game=%" PRIu64 " pending_host=%" PRIu64
           " host_gles_slots=%" PRIu64 " host_portable_slots=%" PRIu64
           " host_asset_slots=%" PRIu64 " host_log_slots=%" PRIu64
           " host_thread_slots=%" PRIu64 " host_fortify_slots=%" PRIu64
           " unsupported=%" PRIu64 "\n",
           cs.relocations,cs.relative,cs.internal,cs.partner,
           cs.unresolved,cs.host_gles,cs.host_portable,cs.host_assets,cs.host_logging,cs.host_threads,cs.host_fortify,cs.host_cxx_lifecycle,cs.unsupported);
    size_t missing_game=unresolved_import_names(&game,&cxx);
    size_t missing_cxx=unresolved_import_names(&cxx,&game);
    printf("[lenneth link] unique_pending game=%zu cxx=%zu\n",
           missing_game,missing_cxx);
    if(provider_mode>=4) {
        const char *jni_bridges[]={
            "JNI_OnLoad",
            "cocos_android_app_init",
            "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit",
            "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender",
            "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesBegin"
        };
        unsigned found=0;
        for(size_t i=0;i<sizeof(jni_bridges)/sizeof(jni_bridges[0]);i++){
            uint64_t addr=0;
            int present=find_export(&game,jni_bridges[i],&addr);
            if(present)found++;
            printf("[lenneth JNI] bridge=%s present=%d invoked=0\n",
                   jni_bridges[i],present);
        }
        printf("[lenneth JNI] exported_entrypoints=%u/%zu calls=0\n",
               found,sizeof(jni_bridges)/sizeof(jni_bridges[0]));
        if(provider_mode>=5) {
            FILE *out=fopen("lenneth-blockers.tsv","w");
            if(out){
                fputs("module\tcategory\tbinding\tsymbol\n",out);
                size_t ng=dump_blockers(out,&game,&cxx);
                size_t nc=dump_blockers(out,&cxx,&game);
                int write_error=ferror(out);
                if(fclose(out)!=0)write_error=1;
                printf("[lenneth suite] blocker_inventory=%s game=%zu cxx=%zu file=lenneth-blockers.tsv\n",
                       write_error?"WRITE_ERROR":"WRITTEN",ng,nc);
                if(write_error){rc=8;goto cleanup;}
            }else{
                perror("[lenneth suite] cannot write blocker inventory");
                rc=8;goto cleanup;
            }
            printf("[lenneth suite] BATCH_LINK=PASS single_mapping_pass=1 "
                   "graphics=%" PRIu64 " portable=%" PRIu64
                   " assets=%" PRIu64 " logging=%" PRIu64
                   " pthread_shims=%" PRIu64
                   " fortify_shims=%" PRIu64
                   " cxx_lifecycle_shims=%" PRIu64
                   " unresolved_sites=%" PRIu64
                   " missing_name_entries=%zu unsupported_types=%" PRIu64
                   " guest_execution=NO\n",
                   gs.host_gles+cs.host_gles,
                   gs.host_portable+cs.host_portable,
                   gs.host_assets+cs.host_assets,
                   gs.host_logging+cs.host_logging,
                   gs.host_threads+cs.host_threads,
                   gs.host_fortify+cs.host_fortify,
                   gs.host_cxx_lifecycle+cs.host_cxx_lifecycle,
                   gs.unresolved+cs.unresolved,
                   missing_game+missing_cxx,
                   gs.unsupported+cs.unsupported);
        }
        if(provider_mode>=8)
            printf("[lenneth cxx] PHASE11_LINK=PASS cxx_lifecycle_slots=%" PRIu64
                   " pending_sites=%" PRIu64
                   " CXX_destructors_not_called=1 commercial_execution=NO\n",
                   gs.host_cxx_lifecycle+cs.host_cxx_lifecycle,
                   gs.unresolved+cs.unresolved);
        if(provider_mode>=7)
            printf("[lenneth fortify] PHASE10_LINK=PASS checked_memory_errno_slots=%" PRIu64
                   " unresolved_sites=%" PRIu64
                   " runtime_stdio_dynloader_OpenSLES=NOT_READY "
                   "commercial_code=NOT_EXECUTED\n",
                   gs.host_fortify+cs.host_fortify,
                   gs.unresolved+cs.unresolved);
        if(provider_mode>=6) {
            printf("[lenneth bionic-thread] PHASE9_LINK=PASS "
                   "guest_mutex_cond_tls_pthread_slots=%" PRIu64
                   " unresolved_sites=%" PRIu64
                   " provider_mode=experimental_nonexecuting "
                   "commercial_execution=NO\n",
                   gs.host_threads+cs.host_threads,
                   gs.unresolved+cs.unresolved);
        }
        printf("[lenneth android-log] PHASE7=PASS graphics_slots=%" PRIu64
               " portable_slots=%" PRIu64 " asset_slots=%" PRIu64
               " log_slots=%" PRIu64 " host_pending_sites=%" PRIu64
               " distinct_pending=%zu; Bionic pthread/JNI/audio NOT READY; "
               "engine NOT executed\n",
               gs.host_gles+cs.host_gles,
               gs.host_portable+cs.host_portable,
               gs.host_assets+cs.host_assets,
               gs.host_logging+cs.host_logging,
               gs.unresolved+cs.unresolved,missing_game+missing_cxx);
    } else if(provider_mode==3)
        printf("[lenneth asset] PHASE6=PASS graphics_slots=%" PRIu64
               " portable_slots=%" PRIu64
               " asset_slots=%" PRIu64
               " host_pending_sites=%" PRIu64
               " distinct_pending=%zu; runtime JNI/Bionic/audio NOT ready; "
               "commercial game NOT executed\n",
               gs.host_gles+cs.host_gles,
               gs.host_portable+cs.host_portable,
               gs.host_assets+cs.host_assets,
               gs.unresolved+cs.unresolved,
               missing_game+missing_cxx);
    else if(provider_mode==2)
        printf("[lenneth portable] PHASE5=PASS graphics_slots=%" PRIu64
               " libc_libm_slots=%" PRIu64 " host_pending_sites=%" PRIu64
               " distinct_pending=%zu; Bionic pthread/JNI/asset/audio NOT ready; "
               "engine NOT executed\n",
               gs.host_gles+cs.host_gles,
               gs.host_portable+cs.host_portable,
               gs.unresolved+cs.unresolved,missing_game+missing_cxx);
    else if(provider_mode==1)
        printf("[lenneth host] PHASE4=PASS graphics_resolved_slots=%" PRIu64
               " unresolved_host_sites=%" PRIu64
               " unresolved_host_unique=%zu; Android ABI still pending; "
               "commercial code NOT executed\n",
               gs.host_gles+cs.host_gles,gs.unresolved+cs.unresolved,
               missing_game+missing_cxx);
    else
        printf("[lenneth link] PHASE3=PASS; link is PARTIAL; "
               "Bionic/JNI/asset/audio symbols NOT ready; game NOT executed\n");
    rc=0;
cleanup:
    g_asset_binding_enabled=0;
    g_android_log_binding_enabled=0;
    g_bionic_thread_binding_enabled=0;
    g_bionic_fortify_binding_enabled=0;
    g_cxx_lifecycle_binding_enabled=0;
    unload(&game);
    unload(&cxx);
    if(provider_mode>=3)lenneth_asset_close_root();
    if(provider_mode>=2)lenneth_portable_close();
    if(provider_mode>=1)provider_close();
    return rc;
}

/*
 * Phase11 first EXECUTABLE AArch64 native-library test.
 *
 * This function is deliberately separate from --batch:
 *  - it only allows two original synthetic ELF files with a matching
 *    synthetic marker exported by BOTH modules,
 *  - no DT_NEEDED and no init arrays / constructors,
 *  - all imports must resolve, no RWX program segments,
 *  - it never calls commercial library code.
 *
 * It creates RX mappings for synthetic guest code ONLY after relocations
 * are fully resolved, and calls a known synthetic JNI_OnLoad returning 77.
 */
static int synthetic_marker_ok(const Module *m) {
    static const char token[]="LENNETH_PHASE11_SYNTHETIC_ONLY_2026";
    uint64_t p=0;
    if(!find_export(m,"lenneth_phase11_mock_marker",&p))
        return 0;
    if(p<(uint64_t)(uintptr_t)m->image || (uint64_t)(uintptr_t)m->image>
       UINT64_MAX-m->span || p>(uint64_t)(uintptr_t)m->image+m->span ||
       sizeof(token)>((uint64_t)(uintptr_t)m->image+m->span-p))
        return 0;
    return memcmp((const void*)(uintptr_t)p,token,sizeof(token))==0;
}
static int synthetic_no_external_or_init(const Module *m) {
    const Elf64_Ehdr *eh=(const Elf64_Ehdr*)(const void*)m->raw;
    const Elf64_Phdr *ph=(const Elf64_Phdr*)(const void*)(m->raw+eh->e_phoff);
    for(unsigned i=0;i<eh->e_phnum;i++) {
        if(ph[i].p_type==PT_LOAD && (ph[i].p_flags&PF_W) &&
           (ph[i].p_flags&PF_X))
            return 0;
        if(ph[i].p_type!=PT_DYNAMIC)continue;
        if(ph[i].p_vaddr<m->lo || ph[i].p_vaddr-m->lo>m->span ||
           ph[i].p_filesz>m->span-(ph[i].p_vaddr-m->lo))
            return 0;
        const Elf64_Dyn *d=(const Elf64_Dyn*)(const void*)
                         (m->image+(size_t)(ph[i].p_vaddr-m->lo));
        size_t count=(size_t)ph[i].p_filesz/sizeof(Elf64_Dyn);
        for(size_t j=0;j<count;j++){
            if(d[j].d_tag==DT_NULL)break;
            if(d[j].d_tag==DT_NEEDED ||
               (d[j].d_tag==DT_INIT && d[j].d_un.d_ptr) ||
               (d[j].d_tag==DT_INIT_ARRAYSZ && d[j].d_un.d_val) ||
               (d[j].d_tag==DT_PREINIT_ARRAYSZ && d[j].d_un.d_val))
                return 0;
        }
    }
    return 1;
}
static int synthetic_protect(Module *m){
    const Elf64_Ehdr *eh=(const Elf64_Ehdr*)(const void*)m->raw;
    const Elf64_Phdr *ph=(const Elf64_Phdr*)(const void*)(m->raw+eh->e_phoff);
    for(unsigned i=0;i<eh->e_phnum;i++) {
        if(ph[i].p_type!=PT_LOAD || !ph[i].p_memsz)continue;
        uint64_t low=ph[i].p_vaddr&~UINT64_C(4095);
        if(ph[i].p_vaddr>UINT64_MAX-ph[i].p_memsz-4095)return 0;
        uint64_t high=(ph[i].p_vaddr+ph[i].p_memsz+4095)&~UINT64_C(4095);
        if(high<=low || low<m->lo || high-m->lo>m->span)return 0;
        int perm=0;
        if(ph[i].p_flags&PF_R)perm|=PROT_READ;
        if(ph[i].p_flags&PF_W)perm|=PROT_WRITE;
        if(ph[i].p_flags&PF_X)perm|=PROT_EXEC;
        if((perm&PROT_WRITE)&&(perm&PROT_EXEC))return 0;
        if(mprotect(m->image+(size_t)(low-m->lo),
                    (size_t)(high-low),perm)!=0) {
            perror("[lenneth synthetic] mprotect");return 0;
        }
    }
    return 1;
}
int lenneth_synthetic_execute(const char *game_path,const char *partner_path) {
    Module game={0},partner={0};
    LinkStats a={0},b={0};
    int rc=8;
    printf("[lenneth synthetic] execution is permitted ONLY for "
           "independently authored synthetic test libraries\n");
    if(!load_file(&partner,"mock_partner",partner_path) ||
       !load_file(&game,"mock_game",game_path))goto end;
    if(!synthetic_marker_ok(&game) ||
       !synthetic_marker_ok(&partner) ||
       !synthetic_no_external_or_init(&game) ||
       !synthetic_no_external_or_init(&partner)){
        printf("[lenneth synthetic] REFUSED: absent matching synthetic markers, "
               "external dependencies, constructors or RWX segment\n");
        goto end;
    }
    if(!relocate_table(&partner,&game,partner.rela,partner.rela_count,&b) ||
       !relocate_table(&partner,&game,partner.jmp,partner.jmp_count,&b) ||
       !relocate_table(&game,&partner,game.rela,game.rela_count,&a) ||
       !relocate_table(&game,&partner,game.jmp,game.jmp_count,&a) ||
       a.unresolved || b.unresolved || a.unsupported || b.unsupported) {
        printf("[lenneth synthetic] REFUSED: incomplete or invalid relocations\n");
        goto end;
    }
    uint64_t addr=0;
    if(!find_export(&game,"JNI_OnLoad",&addr) ||
       addr<(uint64_t)(uintptr_t)game.image ||
       addr>=(uint64_t)(uintptr_t)game.image+game.span)
        goto end;
    if(!synthetic_protect(&game) || !synthetic_protect(&partner)){
        printf("[lenneth synthetic] executable mapping protection failed\n");
        goto end;
    }
    int (*fn)(void*,void*)=NULL;
    void *address=(void*)(uintptr_t)addr;
    if(sizeof(fn)!=sizeof(address))goto end;
    memcpy(&fn,&address,sizeof(fn));
    int val=fn(NULL,NULL); /* KNOWN MOCK ONLY, NEVER COMMERCIAL ENGINE */
    printf("[lenneth synthetic] returned=%d expected=77\n",val);
    if(val==77){
        printf("[lenneth synthetic] EXECUTION=PASS AArch64 ELF "
               "cross_module_calls=1 W_X_policy=enforced guest=commercial_NO\n");
        rc=0;
    }
end:
    unload(&game);
    unload(&partner);
    return rc;
}

int lenneth_link_preflight(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,0);
}
int lenneth_host_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,1);
}
int lenneth_portable_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,2);
}
int lenneth_asset_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,3);
}
int lenneth_logging_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,4);
}
int lenneth_batch_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,5);
}
int lenneth_batch_thread_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,6);
}
int lenneth_batch_fortify_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,7);
}
int lenneth_batch_cxx_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,8);
}
