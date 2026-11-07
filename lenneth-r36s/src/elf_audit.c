#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read-only inspection of external ARM64 ELF metadata. No dlopen/execute. */
static int inside(size_t off,size_t bytes,size_t size) {
    return off<=size && bytes<=size-off;
}
static int addr_to_off(Elf64_Addr addr,const Elf64_Phdr *ph,
                       size_t nph,size_t fsize,size_t *out) {
    for(size_t i=0;i<nph;i++) {
        if(ph[i].p_type!=PT_LOAD||addr<ph[i].p_vaddr)continue;
        uint64_t offset=addr-ph[i].p_vaddr;
        if(offset>=ph[i].p_filesz)continue;
        uint64_t real=ph[i].p_offset+offset;
        if(real>=fsize)continue;
        *out=(size_t)real;
        return 1;
    }
    return 0;
}
static void report_sym(const char *name,int defined,
                       unsigned *exported,unsigned *unresolved) {
    if(defined)(*exported)++;else (*unresolved)++;
    if(defined && (strcmp(name,"JNI_OnLoad")==0 ||
        strcmp(name,"cocos_android_app_init")==0 ||
        strstr(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit")!=NULL ||
        strstr(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender")!=NULL ||
        strstr(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesBegin")!=NULL))
        printf("[lenneth elf] exported bridge: %s\n",name);
    if(!defined && (strstr(name,"gl") == name ||
       strstr(name,"egl") == name ||
       strstr(name,"slCreateEngine") == name ||
       strstr(name,"AAssetManager_") == name))
        printf("[lenneth elf] imported native API: %s\n",name);
}
int lenneth_elf_audit(const char *filename) {
    FILE *f=fopen(filename,"rb");
    if(!f){perror(filename);return 3;}
    if(fseek(f,0,SEEK_END)){perror("fseek");fclose(f);return 3;}
    long length=ftell(f);
    if(length<(long)sizeof(Elf64_Ehdr)||length>128L*1024*1024){
        fprintf(stderr,"[lenneth elf] ELF size invalid or over 128 MiB\n");
        fclose(f);return 4;
    }
    rewind(f);
    size_t sz=(size_t)length;
    uint8_t *data=malloc(sz);
    if(!data){fclose(f);return 5;}
    if(fread(data,1,sz,f)!=sz){perror("fread");free(data);fclose(f);return 3;}
    fclose(f);
    Elf64_Ehdr *eh=(Elf64_Ehdr*)data;
    if(memcmp(eh->e_ident,ELFMAG,SELFMAG)!=0 ||
       eh->e_ident[EI_CLASS]!=ELFCLASS64 ||
       eh->e_ident[EI_DATA]!=ELFDATA2LSB ||
       eh->e_machine!=EM_AARCH64) {
        fprintf(stderr,"[lenneth elf] not a little-endian AArch64 ELF64\n");
        free(data);return 6;
    }
    printf("[lenneth elf] file=%s bytes=%zu type=%u machine=AArch64\n",
           filename,sz,(unsigned)eh->e_type);
    if(eh->e_phentsize!=sizeof(Elf64_Phdr) || !eh->e_phnum ||
       eh->e_phnum>128 || !inside((size_t)eh->e_phoff,
                         (size_t)eh->e_phnum*sizeof(Elf64_Phdr),sz)){
        fprintf(stderr,"[lenneth elf] malformed program header table\n");
        free(data);return 7;
    }
    Elf64_Phdr *ph=(Elf64_Phdr*)(data+eh->e_phoff);
    Elf64_Dyn *dynamic=NULL;
    size_t ndyn=0;
    uint64_t dynstr_addr=0,dynstr_len=0;
    for(size_t i=0;i<eh->e_phnum;i++)
        if(ph[i].p_type==PT_DYNAMIC){
            if(!inside((size_t)ph[i].p_offset,(size_t)ph[i].p_filesz,sz)){
                fprintf(stderr,"[lenneth elf] bad PT_DYNAMIC\n");
                free(data);return 8;
            }
            dynamic=(Elf64_Dyn*)(data+ph[i].p_offset);
            ndyn=(size_t)ph[i].p_filesz/sizeof(Elf64_Dyn);
        }
    if(dynamic)for(size_t i=0;i<ndyn;i++){
        if(dynamic[i].d_tag==DT_NULL)break;
        if(dynamic[i].d_tag==DT_STRTAB)
            dynstr_addr=dynamic[i].d_un.d_ptr;
        if(dynamic[i].d_tag==DT_STRSZ)
            dynstr_len=dynamic[i].d_un.d_val;
    }
    size_t dynstr_off=0;
    int have_str=addr_to_off(dynstr_addr,ph,eh->e_phnum,sz,&dynstr_off);
    if(have_str && !inside(dynstr_off,(size_t)dynstr_len,sz))
        have_str=0;
    printf("[lenneth elf] NEEDED libraries:\n");
    unsigned needed=0;
    if(dynamic)for(size_t i=0;i<ndyn;i++) {
        if(dynamic[i].d_tag==DT_NULL)break;
        if(dynamic[i].d_tag!=DT_NEEDED)continue;
        if(have_str && dynamic[i].d_un.d_val<dynstr_len){
            const char *name=(const char*)data+dynstr_off+dynamic[i].d_un.d_val;
            const size_t max=(size_t)(dynstr_len-dynamic[i].d_un.d_val);
            if(memchr(name,0,max)){
                printf("  %s\n",name);
                needed++;continue;
            }
        }
        printf("  <unresolved string at %llu>\n",
               (unsigned long long)dynamic[i].d_un.d_val);
    }
    printf("[lenneth elf] NEEDED total=%u\n",needed);
    unsigned exports=0,imports=0;
    if(eh->e_shentsize==sizeof(Elf64_Shdr) && eh->e_shnum &&
       eh->e_shnum<2048 &&
       inside((size_t)eh->e_shoff,(size_t)eh->e_shnum*sizeof(Elf64_Shdr),sz)){
        Elf64_Shdr *sec=(Elf64_Shdr*)(data+eh->e_shoff);
        for(size_t i=0;i<eh->e_shnum;i++) {
            if(sec[i].sh_type!=SHT_DYNSYM ||
               !sec[i].sh_entsize || sec[i].sh_entsize!=sizeof(Elf64_Sym)||
               sec[i].sh_link>=eh->e_shnum)continue;
            Elf64_Shdr *str=&sec[sec[i].sh_link];
            if(!inside((size_t)str->sh_offset,(size_t)str->sh_size,sz)||
               !inside((size_t)sec[i].sh_offset,(size_t)sec[i].sh_size,sz))continue;
            const char *strings=(const char*)data+str->sh_offset;
            Elf64_Sym *syms=(Elf64_Sym*)(data+sec[i].sh_offset);
            size_t count=(size_t)sec[i].sh_size/sizeof(Elf64_Sym);
            for(size_t j=0;j<count;j++) {
                if(syms[j].st_name>=str->sh_size)continue;
                const char *name=strings+syms[j].st_name;
                if(!*name || !memchr(name,0,
                   (size_t)(str->sh_size-syms[j].st_name)))continue;
                report_sym(name,syms[j].st_shndx!=SHN_UNDEF,
                           &exports,&imports);
            }
        }
    } else {
        fprintf(stdout,"[lenneth elf] section table stripped/unavailable\n");
    }
    printf("[lenneth elf] dynamic exported=%u imported=%u\n",
           exports,imports);
    printf("[lenneth elf] This is a metadata audit; the Android .so was NOT executed.\n");
    free(data);
    return 0;
}
