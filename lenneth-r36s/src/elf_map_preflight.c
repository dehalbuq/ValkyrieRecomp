/*
 * Android AArch64 ELF mapping preflight for Lenneth R36S Linux.
 *
 * Linux mmap + AArch64 R_AARCH64_RELATIVE relocation validation only:
 * - this tool NEVER executes game instructions or ELF constructors,
 * - it DOES NOT emulate Android's Bionic/JNI / licensing services,
 * - symbols requiring provider resolution remain explicitly unresolved.
 *
 * Original, independently authored MIT source. No commercial library included.
 */
#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define MAX_SO_FILE ((size_t)128 * 1024 * 1024)
#define MAX_SO_MAP  ((size_t)256 * 1024 * 1024)
#define LOAD_PAGE   ((uint64_t)4096)

static int fits(size_t offset,size_t length,size_t total) {
    return offset<=total && length<=total-offset;
}
static int add_ok(uint64_t a,uint64_t b,uint64_t *out) {
    if(a>UINT64_MAX-b)return 0;
    *out=a+b;return 1;
}
static void memory_report(const char *name) {
    FILE *f=fopen("/proc/self/status","r");
    if(!f)return;
    char line[256];
    while(fgets(line,sizeof(line),f)) {
        if(strncmp(line,"VmRSS:",6)==0 ||
           strncmp(line,"VmPeak:",7)==0 ||
           strncmp(line,"VmSize:",7)==0)
            printf("[lenneth mmap] %s %s",name,line);
    }
    fclose(f);
}
static int read_all(const char *path,uint8_t **buf,size_t *size) {
    FILE *f=fopen(path,"rb");
    if(!f){fprintf(stderr,"[lenneth mmap] open %s: %s\n",path,strerror(errno));return 0;}
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}
    long len=ftell(f);
    if(len<0||(size_t)len<sizeof(Elf64_Ehdr)||(size_t)len>MAX_SO_FILE){
        fprintf(stderr,"[lenneth mmap] file too large/short: %ld\n",len);
        fclose(f);return 0;
    }
    rewind(f);
    *buf=(uint8_t*)malloc((size_t)len);
    if(!*buf){fclose(f);return 0;}
    *size=(size_t)len;
    int ok=fread(*buf,1,*size,f)==*size;
    fclose(f);
    if(!ok){free(*buf);*buf=NULL;return 0;}
    return 1;
}
static int validate_elf(const uint8_t *raw,size_t sz,
                        const Elf64_Ehdr **header,
                        const Elf64_Phdr **phdrs) {
    if(!fits(0,sizeof(Elf64_Ehdr),sz))return 0;
    const Elf64_Ehdr *h=(const Elf64_Ehdr*)(const void*)raw;
    if(memcmp(h->e_ident,ELFMAG,SELFMAG)!=0 ||
       h->e_ident[EI_CLASS]!=ELFCLASS64 ||
       h->e_ident[EI_DATA]!=ELFDATA2LSB ||
       h->e_ident[EI_VERSION]!=EV_CURRENT ||
       h->e_machine!=EM_AARCH64 || h->e_type!=ET_DYN) {
        fprintf(stderr,"[lenneth mmap] unsupported ELF type/architecture\n");
        return 0;
    }
    if(h->e_phentsize!=sizeof(Elf64_Phdr) ||
       h->e_phnum==0 || h->e_phnum>128 ||
       !fits((size_t)h->e_phoff,
             (size_t)h->e_phnum*sizeof(Elf64_Phdr),sz)) {
        fprintf(stderr,"[lenneth mmap] invalid ELF program header size/table (got %u, expected %zu)\n",
                (unsigned)h->e_phentsize,sizeof(Elf64_Phdr));
        return 0;
    }
    *header=h;
    *phdrs=(const Elf64_Phdr*)(const void*)(raw+h->e_phoff);
    return 1;
}
static int map_segments(const uint8_t *raw,size_t sz,
                        const Elf64_Ehdr *h,const Elf64_Phdr *ph,
                        uint8_t **mapped,size_t *mapped_sz,
                        uint64_t *base_vaddr) {
    uint64_t min=UINT64_MAX,max=0,computed=0;
    int load_count=0;
    for(unsigned i=0;i<h->e_phnum;i++) {
        if(ph[i].p_type!=PT_LOAD)continue;
        if(ph[i].p_filesz>ph[i].p_memsz ||
           !fits((size_t)ph[i].p_offset,(size_t)ph[i].p_filesz,sz) ||
           !add_ok(ph[i].p_vaddr,ph[i].p_memsz,&computed)) {
            fprintf(stderr,"[lenneth mmap] malformed PT_LOAD %u\n",i);return 0;
        }
        uint64_t begin=ph[i].p_vaddr&~(LOAD_PAGE-1);
        uint64_t end=(computed+LOAD_PAGE-1)&~(LOAD_PAGE-1);
        if(begin<min)min=begin;
        if(end>max)max=end;
        load_count++;
    }
    if(!load_count||max<=min||max-min>MAX_SO_MAP) {
        fprintf(stderr,"[lenneth mmap] invalid mapped image span\n");return 0;
    }
    *mapped_sz=(size_t)(max-min);
    *base_vaddr=min;
    /* Never grant PROT_EXEC: this is a loading/relocation diagnostic only. */
    void *p=mmap(NULL,*mapped_sz,PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED){
        fprintf(stderr,"[lenneth mmap] mmap %zu bytes failed: %s\n",
                *mapped_sz,strerror(errno));return 0;
    }
    *mapped=(uint8_t*)p;
    for(unsigned i=0;i<h->e_phnum;i++) {
        if(ph[i].p_type!=PT_LOAD)continue;
        uint64_t off=ph[i].p_vaddr-min;
        if(off>*mapped_sz || ph[i].p_memsz>*mapped_sz-off) {
            fprintf(stderr,"[lenneth mmap] segment outside allocated image\n");
            munmap(*mapped,*mapped_sz);*mapped=NULL;return 0;
        }
        memcpy(*mapped+(size_t)off,raw+(size_t)ph[i].p_offset,
               (size_t)ph[i].p_filesz);
    }
    printf("[lenneth mmap] ELF mapped pages=%zu bytes=%zu segments=%d; writable, NEVER executable\n",
           *mapped_sz/LOAD_PAGE,*mapped_sz,load_count);
    return 1;
}
static int relocate_rela(const uint8_t *image,size_t span,
                         uint64_t min_vaddr,const Elf64_Rela *rela,
                         size_t count,uintptr_t load_bias,
                         uint64_t *relative,uint64_t *external,
                         uint64_t *absolute,uint64_t *other) {
    for(size_t i=0;i<count;i++) {
        uint64_t type=ELF64_R_TYPE(rela[i].r_info);
        if(type==R_AARCH64_NONE)continue;
        if(rela[i].r_offset<min_vaddr ||
           rela[i].r_offset-min_vaddr>span ||
           sizeof(uint64_t)>span-(size_t)(rela[i].r_offset-min_vaddr)){
            fprintf(stderr,"[lenneth mmap] relocation target out of mapped range: #%zu\n",i);
            return 0;
        }
        if(type==R_AARCH64_RELATIVE) {
            /* Map no code: validate and apply only non-import address fixups. */
            uint64_t value=(uint64_t)load_bias+(uint64_t)rela[i].r_addend;
            uint8_t *target=(uint8_t*)(uintptr_t)(image+
                            (size_t)(rela[i].r_offset-min_vaddr));
            memcpy(target,&value,sizeof(value));
            (*relative)++;
        } else if(type==R_AARCH64_ABS64)(*absolute)++;
        else if(type==R_AARCH64_GLOB_DAT ||
                type==R_AARCH64_JUMP_SLOT)(*external)++;
        else (*other)++;
    }
    return 1;
}
int lenneth_elf_map_preflight(const char *path) {
    uint8_t *raw=NULL,*image=NULL;
    size_t sz=0,span=0;
    uint64_t vaddr=0;
    int rc=3;
    if(!read_all(path,&raw,&sz))return rc;
    const Elf64_Ehdr *eh=NULL;
    const Elf64_Phdr *ph=NULL;
    if(!validate_elf(raw,sz,&eh,&ph)){rc=4;goto done;}
    if(!map_segments(raw,sz,eh,ph,&image,&span,&vaddr)){
        rc=5;goto done;
    }
    const Elf64_Dyn *dyn=NULL;
    size_t dyn_count=0;
    for(unsigned i=0;i<eh->e_phnum;i++) {
        if(ph[i].p_type!=PT_DYNAMIC)continue;
        uint64_t off=ph[i].p_vaddr-vaddr;
        if(ph[i].p_vaddr<vaddr || off>span ||
           ph[i].p_memsz>span-off ||
           ph[i].p_memsz<sizeof(Elf64_Dyn)){
            fprintf(stderr,"[lenneth mmap] bad dynamic segment\n");
            rc=6;goto done;
        }
        dyn=(const Elf64_Dyn*)(const void*)(image+(size_t)off);
        dyn_count=(size_t)ph[i].p_memsz/sizeof(Elf64_Dyn);
    }
    if(!dyn){fprintf(stderr,"[lenneth mmap] missing PT_DYNAMIC\n");rc=6;goto done;}
    uint64_t rela_vaddr=0,rela_len=0,rela_ent=0;
    uint64_t plt_vaddr=0,plt_len=0,needed=0;
    for(size_t i=0;i<dyn_count;i++) {
        if(dyn[i].d_tag==DT_NULL)break;
        switch(dyn[i].d_tag) {
            case DT_RELA: rela_vaddr=dyn[i].d_un.d_ptr;break;
            case DT_RELASZ: rela_len=dyn[i].d_un.d_val;break;
            case DT_RELAENT: rela_ent=dyn[i].d_un.d_val;break;
            case DT_JMPREL: plt_vaddr=dyn[i].d_un.d_ptr;break;
            case DT_PLTRELSZ: plt_len=dyn[i].d_un.d_val;break;
            case DT_NEEDED: needed++;break;
            default:break;
        }
    }
    if(!rela_vaddr || !rela_len || rela_ent!=sizeof(Elf64_Rela) ||
       rela_len%sizeof(Elf64_Rela) ||
       rela_vaddr<vaddr ||
       rela_vaddr-vaddr>span || rela_len>span-(rela_vaddr-vaddr) ||
       plt_len%sizeof(Elf64_Rela) ||
       (plt_len && (plt_vaddr<vaddr ||
                    plt_vaddr-vaddr>span ||
                    plt_len>span-(plt_vaddr-vaddr)))) {
        fprintf(stderr,"[lenneth mmap] dynamic relocation table invalid/unsupported\n");
        rc=6;goto done;
    }
    uint64_t relative=0,external=0,absolute=0,other=0;
    const uintptr_t load_bias=(uintptr_t)image-(uintptr_t)vaddr;
    /* PLT has many external relocations; none are resolved or invoked. */
    const Elf64_Rela *rela=(const Elf64_Rela*)(const void*)(image+(size_t)(rela_vaddr-vaddr));
    size_t count=(size_t)(rela_len/sizeof(Elf64_Rela));
    if(!relocate_rela(image,span,vaddr,rela,count,load_bias,
                      &relative,&external,&absolute,&other)){
        rc=7;goto done;
    }
    if(plt_len) {
        const Elf64_Rela *plt=(const Elf64_Rela*)(const void*)(image+(size_t)(plt_vaddr-vaddr));
        if(!relocate_rela(image,span,vaddr,plt,
                          (size_t)(plt_len/sizeof(Elf64_Rela)),load_bias,
                          &relative,&external,&absolute,&other)){
            rc=7;goto done;
        }
    }
    printf("[lenneth mmap] relative_applied=%" PRIu64
           " external_pending=%" PRIu64 " absolute_pending=%" PRIu64
           " other_pending=%" PRIu64 " needed_libraries=%" PRIu64 "\n",
           relative,external,absolute,other,needed);
    printf("[lenneth mmap] preflight=PASS; no Android code or init_array was run\n");
    memory_report("after map");
    rc=0;
done:
    if(image)munmap(image,span);
    free(raw);
    return rc;
}
