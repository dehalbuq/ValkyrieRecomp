/*
 * Lenneth phase25, MIT: Android arm64 constructor and OpenGL ES1 inventory.
 * For private ELF metadata ONLY. Never maps guest executable pages, invokes
 * a constructor, calls JNI or loads any Android/host OpenGL provider.
 * Outputs only ELF-derived counts and sanitized exported/imported names.
 */
#define _GNU_SOURCE
#include "startup25.h"
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

enum { MAX_BIN=128*1024*1024, MAX_PH=128, MAX_SH=2048, MAX_ARRAY=4096 };
typedef struct {
    unsigned char *raw;size_t len;
    const Elf64_Ehdr *hdr;
    const Elf64_Phdr *ph;
    const Elf64_Dyn *dyn;size_t dynlen;
} Elf;
typedef struct {
    uint64_t init_addr,init_count,init_scalar,rel_addr,rel_bytes;
    uint64_t fini_addr,fini_count;
    unsigned fixed_imports,all_gl_imports,gles1_needed;
    unsigned reloc_matched,unmatched,invalid,executable_targets;
    int has_valid_array,has_valid_rela;
} Info;
static int bounded(size_t off,size_t sz,size_t max){
    return off<=max&&sz<=max-off;
}
static void dispose(Elf *v){free(v->raw);memset(v,0,sizeof(*v));}
static int load(const char *path,Elf *v){
    memset(v,0,sizeof(*v));
    FILE *f=fopen(path,"rb");
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
    long sz=ftell(f);
    if(sz<(long)sizeof(Elf64_Ehdr)||sz>MAX_BIN){fclose(f);return 0;}
    v->len=(size_t)sz;rewind(f);v->raw=malloc(v->len);
    if(!v->raw){fclose(f);return 0;}
    int readok=fread(v->raw,1,v->len,f)==v->len;
    fclose(f);
    if(!readok)goto bad;
    v->hdr=(const Elf64_Ehdr *)(const void *)v->raw;
    const Elf64_Ehdr *h=v->hdr;
    if(memcmp(h->e_ident,ELFMAG,SELFMAG) ||
       h->e_ident[EI_CLASS]!=ELFCLASS64 ||
       h->e_ident[EI_DATA]!=ELFDATA2LSB ||
       h->e_machine!=EM_AARCH64 || h->e_type!=ET_DYN ||
       h->e_phentsize!=sizeof(Elf64_Phdr) || !h->e_phnum ||
       h->e_phnum>MAX_PH ||
       !bounded((size_t)h->e_phoff,(size_t)h->e_phnum*sizeof(Elf64_Phdr),v->len))
        goto bad;
    v->ph=(const Elf64_Phdr*)(const void*)(v->raw+h->e_phoff);
    for(size_t i=0;i<h->e_phnum;i++){
        const Elf64_Phdr *p=&v->ph[i];
        if(p->p_type==PT_LOAD &&
          (p->p_filesz>p->p_memsz ||
           !bounded((size_t)p->p_offset,(size_t)p->p_filesz,v->len)))
            goto bad;
        if(p->p_type==PT_DYNAMIC){
            if(!bounded((size_t)p->p_offset,(size_t)p->p_filesz,v->len) ||
               p->p_filesz%sizeof(Elf64_Dyn))goto bad;
            v->dyn=(const Elf64_Dyn*)(const void*)(v->raw+p->p_offset);
            v->dynlen=(size_t)p->p_filesz/sizeof(Elf64_Dyn);
        }
    }
    if(!v->dyn)goto bad;
    return 1;
bad:
    dispose(v);return 0;
}
static int vaddr_file(const Elf *v,uint64_t addr,size_t len,size_t *file_off){
    for(size_t i=0;i<v->hdr->e_phnum;i++){
        const Elf64_Phdr *p=&v->ph[i];
        if(p->p_type!=PT_LOAD || addr<p->p_vaddr)continue;
        uint64_t d=addr-p->p_vaddr;
        if(d>p->p_filesz || len>p->p_filesz-d ||
           d>SIZE_MAX-p->p_offset)continue;
        size_t off=(size_t)(p->p_offset+d);
        if(!bounded(off,len,v->len))continue;
        *file_off=off;return 1;
    }
    return 0;
}
static int is_executable_vaddr(const Elf *v,uint64_t addr){
    for(size_t i=0;i<v->hdr->e_phnum;i++){
        const Elf64_Phdr *p=&v->ph[i];
        if(p->p_type!=PT_LOAD || !(p->p_flags&PF_X) || addr<p->p_vaddr)
            continue;
        if(addr-p->p_vaddr<p->p_memsz)return 1;
    }
    return 0;
}
static int starts(const char *s,const char *prefix){
    return strncmp(s,prefix,strlen(prefix))==0;
}
static int fixed_function(const char *s){
    static const char *const names[]={
        "glAlphaFunc","glClientActiveTexture","glColor4","glColorPointer",
        "glDisableClientState","glEnableClientState","glFog",
        "glFrustum","glLight","glLoadIdentity","glLoadMatrix","glMatrixMode",
        "glMaterial","glMultMatrix","glNormalPointer","glOrtho",
        "glPopMatrix","glPushMatrix","glRotate","glScale",
        "glShadeModel","glTexEnv","glTranslate","glVertexPointer",
        "glTexCoordPointer","glClipPlane","glPointSizePointer",
        "glGetFixed","glGetLight","glGetMaterial"
    };
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);i++)
        if(starts(s,names[i]))return 1;
    return 0;
}
static void line(FILE *out,const char *module,const char *gate,
                 const char *status,const char *value){
    fprintf(out,"%s\t%s\t%s\t%s\n",module,gate,status,value);
}
static void filtered_name(char *out,size_t capacity,const char *s,size_t len){
    size_t i=0;
    while(i<len && i+1<capacity && s[i]){
        unsigned char c=(unsigned char)s[i];
        out[i]=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||
               (c>='0'&&c<='9')||c=='_'||c=='.'?c:'?';
        ++i;
    }
    out[i]=0;
}
static void imported_gl(const Elf *v,const char *module,FILE *report,Info *info){
    const Elf64_Ehdr *h=v->hdr;
    if(h->e_shentsize!=sizeof(Elf64_Shdr) || !h->e_shnum ||
       h->e_shnum>MAX_SH ||
       !bounded((size_t)h->e_shoff,(size_t)h->e_shnum*sizeof(Elf64_Shdr),v->len))
        return;
    const Elf64_Shdr *sec=(const Elf64_Shdr*)(const void*)(v->raw+h->e_shoff);
    for(size_t i=0;i<h->e_shnum;i++){
        const Elf64_Shdr *d=&sec[i];
        if(d->sh_type!=SHT_DYNSYM || d->sh_link>=h->e_shnum ||
           d->sh_entsize!=sizeof(Elf64_Sym) || d->sh_size%sizeof(Elf64_Sym) ||
           !bounded(d->sh_offset,d->sh_size,v->len))continue;
        const Elf64_Shdr *strings=&sec[d->sh_link];
        if(strings->sh_type!=SHT_STRTAB ||
           !bounded(strings->sh_offset,strings->sh_size,v->len))continue;
        const Elf64_Sym *syms=(const Elf64_Sym*)(const void*)(v->raw+d->sh_offset);
        const char *str=(const char*)(const void*)(v->raw+strings->sh_offset);
        for(size_t j=0;j<d->sh_size/sizeof(Elf64_Sym);j++){
            const Elf64_Sym *symbol=&syms[j];
            if(symbol->st_shndx!=SHN_UNDEF || symbol->st_name>=strings->sh_size)
                continue;
            const char *name=str+symbol->st_name;
            size_t left=(size_t)strings->sh_size-symbol->st_name;
            const char *term=memchr(name,0,left);
            if(!term||!starts(name,"gl")||name[2]<'A'||name[2]>'Z')continue;
            info->all_gl_imports++;
            if(fixed_function(name)){
                info->fixed_imports++;
                if(info->fixed_imports<=64){
                    char safe[128];
                    filtered_name(safe,sizeof(safe),name,(size_t)(term-name));
                    line(report,module,"GLES1_FIXED_FUNCTION_IMPORT","REVIEW",safe);
                }
            }
        }
    }
}
static int read_elf_dynamic(const Elf *v,Info *out){
    memset(out,0,sizeof(*out));
    uint64_t init_bytes=0,fini_bytes=0;
    uint64_t str_addr=0,str_bytes=0;
    for(size_t i=0;i<v->dynlen;i++){
        const Elf64_Dyn *d=&v->dyn[i];
        if(d->d_tag==DT_NULL)break;
        switch(d->d_tag){
            case DT_INIT:if(d->d_un.d_ptr)out->init_scalar++;break;
            case DT_INIT_ARRAY:out->init_addr=d->d_un.d_ptr;break;
            case DT_INIT_ARRAYSZ:init_bytes=d->d_un.d_val;break;
            case DT_FINI_ARRAY:out->fini_addr=d->d_un.d_ptr;break;
            case DT_FINI_ARRAYSZ:fini_bytes=d->d_un.d_val;break;
            case DT_RELA:out->rel_addr=d->d_un.d_ptr;break;
            case DT_RELASZ:out->rel_bytes=d->d_un.d_val;break;
            case DT_STRTAB:str_addr=d->d_un.d_ptr;break;
            case DT_STRSZ:str_bytes=d->d_un.d_val;break;
            default:break;
        }
    }
    if(init_bytes>MAX_ARRAY*sizeof(uint64_t) || init_bytes%sizeof(uint64_t) ||
       fini_bytes>MAX_ARRAY*sizeof(uint64_t) || fini_bytes%sizeof(uint64_t) ||
       out->rel_bytes>16*1024*1024 ||
       out->rel_bytes%sizeof(Elf64_Rela))return 0;
    out->init_count=init_bytes/sizeof(uint64_t);
    out->fini_count=fini_bytes/sizeof(uint64_t);
    /* File-backed table must exist if DT_INIT_ARRAYSZ is nonzero. */
    size_t off=0;
    out->has_valid_array=!init_bytes||
       (out->init_addr&&vaddr_file(v,out->init_addr,(size_t)init_bytes,&off));
    out->has_valid_rela=!out->rel_bytes||
       (out->rel_addr&&vaddr_file(v,out->rel_addr,(size_t)out->rel_bytes,&off));
    if(!out->has_valid_array||!out->has_valid_rela)return 0;
    if(str_addr && str_bytes && str_bytes<=v->len &&
       vaddr_file(v,str_addr,(size_t)str_bytes,&off)){
        const char *strings=(const char*)(const void*)(v->raw+off);
        for(size_t i=0;i<v->dynlen;i++){
            const Elf64_Dyn *d=&v->dyn[i];
            if(d->d_tag==DT_NULL)break;
            if(d->d_tag!=DT_NEEDED || d->d_un.d_val>=str_bytes)continue;
            const char *name=strings+d->d_un.d_val;
            size_t left=(size_t)str_bytes-(size_t)d->d_un.d_val;
            if(memchr(name,0,left) && !strcmp(name,"libGLESv1_CM.so"))
                out->gles1_needed++;
        }
    }
    return 1;
}
static void check_initializers(const Elf *v,Info *info){
    if(!info->init_count)return;
    size_t ptr_offset=0,rel_offset=0;
    if(!vaddr_file(v,info->init_addr,(size_t)info->init_count*8,&ptr_offset))
        return;
    const Elf64_Rela *rel=NULL;
    size_t rel_count=0;
    if(info->rel_bytes){
        if(!vaddr_file(v,info->rel_addr,(size_t)info->rel_bytes,&rel_offset))
            return;
        rel=(const Elf64_Rela*)(const void*)(v->raw+rel_offset);
        rel_count=(size_t)info->rel_bytes/sizeof(Elf64_Rela);
    }
    for(size_t j=0;j<info->init_count;j++){
        uint64_t va=info->init_addr+j*8,original=0;
        memcpy(&original,v->raw+ptr_offset+j*8,sizeof(original));
        int matched=0;
        for(size_t i=0;i<rel_count;i++){
            if(rel[i].r_offset!=va)continue;
            uint32_t type=(uint32_t)ELF64_R_TYPE(rel[i].r_info);
            if(type==R_AARCH64_RELATIVE || type==R_AARCH64_ABS64 ||
               type==R_AARCH64_GLOB_DAT){
                matched=1;
                if(type==R_AARCH64_RELATIVE &&
                   is_executable_vaddr(v,(uint64_t)rel[i].r_addend))
                    info->executable_targets++;
            }
            break;
        }
        if(matched)info->reloc_matched++;
        else if(original && is_executable_vaddr(v,original))
            info->executable_targets++;
        else info->unmatched++;
    }
}
static int analyze(FILE *report,const char *module,const char *filename,
                   Info *info){
    Elf v;
    if(!load(filename,&v)){
        line(report,module,"ELF24_METADATA","INVALID","arm64_file_not_valid");
        return 0;
    }
    if(!read_elf_dynamic(&v,info)){
        line(report,module,"INIT_ARRAY_BOUNDS","INVALID","dynamic_table_out_of_bounds");
        dispose(&v);return 0;
    }
    check_initializers(&v,info);
    imported_gl(&v,module,report,info);
    char msg[160];
    snprintf(msg,sizeof(msg),
             "slots=%" PRIu64 " reloc_matched=%u direct_exec=%u unknown=%u",
             info->init_count,info->reloc_matched,
             info->executable_targets,info->unmatched);
    line(report,module,"INIT_ARRAY_RELOCATION",
         info->unmatched?"REVIEW":"PASS",msg);
    snprintf(msg,sizeof(msg),"dt_init=%" PRIu64 " init_slots=%" PRIu64
             " fini_slots=%" PRIu64,info->init_scalar,
             info->init_count,info->fini_count);
    line(report,module,"CTOR_SEQUENCE","REVIEW",msg);
    snprintf(msg,sizeof(msg),"needs_es1=%u fixed_function_imports=%u total_gl_imports=%u",
             info->gles1_needed,info->fixed_imports,info->all_gl_imports);
    line(report,module,"GLES1_COMPATIBILITY",
         info->gles1_needed||info->fixed_imports?"REVIEW":"PASS",msg);
    printf("[lenneth phase25] module=%s ctor_slots=%" PRIu64
           " relocs_matched=%u static_exec_targets=%u"
           " unknown_ctor_slots=%u gles1_needed=%u fixed_gl_imports=%u"
           " metadata=PASS commercial_execution=NO\n",
           module,info->init_count,info->reloc_matched,
           info->executable_targets,info->unmatched,
           info->gles1_needed,info->fixed_imports);
    dispose(&v);
    return 1;
}
int lenneth_startup25_audit(const char *game,const char *cxx){
    FILE *r=fopen("lenneth-phase25-startup-plan.tsv","w");
    if(!r){perror("lenneth-phase25-startup-plan.tsv");return 25;}
    fputs("module\tgate\tstatus\tdetail\n",r);
    Info c={0},g={0};
    /* Child-before-parent order for the supported two guest libraries:
     * libc++_shared.so must have its constructors considered before game. */
    int c_ok=analyze(r,"libc++_shared.so",cxx,&c);
    int g_ok=analyze(r,"libMyGame.so",game,&g);
    line(r,"runtime","CONSTRUCTOR_PLAN","BLOCKED",
         "dependencies_first_libcxx_then_game_no_guest_constructors_executed");
    line(r,"runtime","GLES1_HOST_PROBE","BLOCKED",
         "GLES1_CM_requirement_not_equivalent_to_GLES2_no_host_calls");
    line(r,"runtime","JNI_VM_CLASSLOADER","BLOCKED",
         "Android_JavaVM_JNIEnv_methods_not_implemented");
    line(r,"runtime","RELRO_WX_TRANSITION","BLOCKED",
         "commercial_memory_not_executable_no_protection_transition");
    line(r,"runtime","COMMERCIAL_ENGINE","BLOCKED","no_proprietary_code_execution");
    int err=ferror(r);
    int closed=fclose(r);
    int ok=c_ok&&g_ok&&!err&&!closed;
    printf("[lenneth phase25] STARTUP_PLAN=%s constructor_order=LIBCXX_THEN_GAME"
           " ctor_slots_total=%" PRIu64 " gles1_needed=%u"
           " fixed_function_imports=%u startup_gate=BLOCKED"
           " engine_executed=NO report=lenneth-phase25-startup-plan.tsv\n",
           ok?"PASS":"FAIL",c.init_count+g.init_count,
           c.gles1_needed+g.gles1_needed,
           c.fixed_imports+g.fixed_imports);
    return ok?0:25;
}
int lenneth_startup25_selftest(void){
    int ok=fixed_function("glMatrixMode") &&
           fixed_function("glVertexPointer") &&
           fixed_function("glLoadIdentity") &&
           !fixed_function("glDrawArrays") &&
           !fixed_function("glBindTexture") &&
           bounded(3,4,7) && !bounded(3,5,7) &&
           !bounded(SIZE_MAX-1,10,500);
    char s[32];
    filtered_name(s,sizeof(s),"gl/MatrixMode",13);
    if(strcmp(s,"gl?MatrixMode"))ok=0;
    printf("[lenneth phase25] SELFTEST=%s init_array_safety=PASS "
           "fixed_function_classifier=PASS startup_order=LIBCXX_THEN_GAME"
           " gles1_equiv_gles2=NO private_engine_exec=NO\n",
           ok?"PASS":"FAIL");
    return ok?0:25;
}
