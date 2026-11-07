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

enum { MAX_ELF_BYTES = 128*1024*1024, MAX_ELF_IMAGE = 256*1024*1024 };
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
    uint64_t relative, internal, partner, host_gles, unresolved, unsupported;
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
    return name && (strncmp(name,"gl",2)==0 ||
                    strncmp(name,"egl",3)==0);
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
    if((s[0]=='g' && s[1]=='l') ||
       strncmp(s,"egl",3)==0)return "GLES2/EGL";
    if(strncmp(s,"AAsset",6)==0)return "Android AAssetManager";
    if(strncmp(s,"SL_IID_",7)==0 ||
       strcmp(s,"slCreateEngine")==0)return "OpenSL ES";
    if(strncmp(s,"pthread_",8)==0)return "Android Bionic pthread";
    if(strncmp(s,"__android_",10)==0 ||
       strncmp(s,"android_",8)==0)return "Android logging/NDK";
    if(strncmp(s,"__cxa_",6)==0 ||
       s[0]=='_' && s[1]=='Z')return "C++ ABI";
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
                              int with_host_graphics) {
    Module game={0},cxx={0};
    LinkStats gs={0},cs={0};
    int rc=3;
    if(with_host_graphics)provider_init();
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
           " host_gles_slots=%" PRIu64 " unsupported=%" PRIu64 "\n",
           gs.relocations,gs.relative,gs.internal,gs.partner,
           gs.unresolved,gs.host_gles,gs.unsupported);
    printf("[lenneth link] libc++_shared: relocs=%" PRIu64
           " RELATIVE=%" PRIu64 " internal=%" PRIu64
           " cross_game=%" PRIu64 " pending_host=%" PRIu64
           " host_gles_slots=%" PRIu64 " unsupported=%" PRIu64 "\n",
           cs.relocations,cs.relative,cs.internal,cs.partner,
           cs.unresolved,cs.host_gles,cs.unsupported);
    size_t missing_game=unresolved_import_names(&game,&cxx);
    size_t missing_cxx=unresolved_import_names(&cxx,&game);
    printf("[lenneth link] unique_pending game=%zu cxx=%zu\n",
           missing_game,missing_cxx);
    if(with_host_graphics)
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
    unload(&game);
    unload(&cxx);
    if(with_host_graphics)provider_close();
    return rc;
}
int lenneth_link_preflight(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,0);
}
int lenneth_host_plan(const char *game_path,const char *cxx_path) {
    return lenneth_link_preflight_mode(game_path,cxx_path,1);
}
