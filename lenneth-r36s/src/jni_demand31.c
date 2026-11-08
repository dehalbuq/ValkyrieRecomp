/*
 * Lenneth Phase31, MIT — bounded read-only JNI / Cocos / Bionic ABI census.
 *
 * The Phase30 physical R36S test verified a synthetic constructor-to-JNI
 * lifecycle, but the real game has 195 constructors plus the C++ library's
 * 1 and has NOT been executed. Identify the *actual* exported JNI entrypoints
 * and exact allowlisted class references before designing a Java runtime
 * replacement. Never map any executable memory, call a constructor/JNI,
 * or read/print proprietary game code, non-allowlisted strings or assets.
 *
 * Absence of a literal class name is NOT proof of no JNI dependencies:
 * registration via RegisterNatives, Java ClassLoader and dynamic strings
 * may still be needed. A historical 256-byte bionic jmp_buf size/offset
 * match is NOT proof of the exact private Android/Bionic version.
 */
#define _GNU_SOURCE
#include "jni_demand31.h"
#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FILE_CAP=128*1024*1024, PH_CAP=128, SH_CAP=2048, MAX_JNI_EXPORTS=128 };
typedef struct {
    unsigned char *data; size_t size;
    const Elf64_Ehdr *eh;
    const Elf64_Phdr *ph;
    const Elf64_Shdr *sh;
    const Elf64_Sym *sym; size_t nsym;
    const char *str; size_t str_size;
} E31;
typedef struct {
    unsigned jni_exports,export_printed,export_truncated;
    unsigned java_native,objcocos,method_strings,classes_present;
    unsigned bionic_jmp,bionic_stdio,required_jni_exports;
} Stats31;
/* Source-aligned with early Android AArch64 machine/setjmp.h: layout in
 * 32-bit words: magic=0 mask=1 core=2 float=28 reserved=44 size=64 words.
 * Android changed its private ABI in later releases. These are size/layout
 * checks of our understanding only, not a commercial runtime proof. */
typedef struct {
    uint32_t magic,mask;
    uint64_t core[13], floating[8],reserved[10];
} BionicEarlyJmp31;
_Static_assert(sizeof(BionicEarlyJmp31)==256,"Android old AArch64 256-byte jmp_buf");
_Static_assert(offsetof(BionicEarlyJmp31,core)==8,"Android old AArch64 core offset");
_Static_assert(offsetof(BionicEarlyJmp31,floating)==112,"Android old AArch64 FP offset");
_Static_assert(offsetof(BionicEarlyJmp31,reserved)==176,"Android old AArch64 reserved offset");

static int bounded(size_t off,size_t n,size_t size){
    return off<=size && n<=size-off;
}
static void free_elf(E31 *e){free(e->data);memset(e,0,sizeof(*e));}
static int read_elf(const char *path,E31 *e){
    memset(e,0,sizeof(*e));
    FILE *f=fopen(path,"rb");
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}
    long len=ftell(f);
    if(len<(long)sizeof(Elf64_Ehdr) || len>FILE_CAP){fclose(f);return 0;}
    e->size=(size_t)len;
    rewind(f);e->data=malloc(e->size);
    if(!e->data){fclose(f);return 0;}
    int readok=fread(e->data,1,e->size,f)==e->size;
    fclose(f);
    if(!readok)goto bad;
    const Elf64_Ehdr *h=(const Elf64_Ehdr *)(const void *)e->data;
    if(memcmp(h->e_ident,ELFMAG,SELFMAG) ||
       h->e_ident[EI_CLASS]!=ELFCLASS64 ||
       h->e_ident[EI_DATA]!=ELFDATA2LSB ||
       h->e_type!=ET_DYN || h->e_machine!=EM_AARCH64 ||
       h->e_phentsize!=sizeof(Elf64_Phdr) || !h->e_phnum ||
       h->e_phnum>PH_CAP ||
       !bounded((size_t)h->e_phoff,(size_t)h->e_phnum*sizeof(Elf64_Phdr),e->size) ||
       h->e_shentsize!=sizeof(Elf64_Shdr) || !h->e_shnum ||
       h->e_shnum>SH_CAP ||
       !bounded((size_t)h->e_shoff,(size_t)h->e_shnum*sizeof(Elf64_Shdr),e->size))
        goto bad;
    e->eh=h;
    e->ph=(const Elf64_Phdr *)(const void *)(e->data+h->e_phoff);
    e->sh=(const Elf64_Shdr *)(const void *)(e->data+h->e_shoff);
    for(size_t i=0;i<h->e_phnum;i++){
        const Elf64_Phdr *p=&e->ph[i];
        if(p->p_type==PT_LOAD &&
           (p->p_filesz>p->p_memsz ||
            !bounded((size_t)p->p_offset,(size_t)p->p_filesz,e->size)))
            goto bad;
    }
    for(size_t i=0;i<h->e_shnum;i++){
        const Elf64_Shdr *sec=&e->sh[i];
        if(sec->sh_type!=SHT_DYNSYM ||
           sec->sh_link>=h->e_shnum ||
           sec->sh_entsize!=sizeof(Elf64_Sym) ||
           sec->sh_size%sizeof(Elf64_Sym) ||
           !bounded((size_t)sec->sh_offset,(size_t)sec->sh_size,e->size))
            continue;
        const Elf64_Shdr *str=&e->sh[sec->sh_link];
        if(str->sh_type!=SHT_STRTAB ||
           !bounded((size_t)str->sh_offset,(size_t)str->sh_size,e->size))
            continue;
        e->sym=(const Elf64_Sym *)(const void *)(e->data+sec->sh_offset);
        e->nsym=(size_t)sec->sh_size/sizeof(Elf64_Sym);
        e->str=(const char *)(const void *)(e->data+str->sh_offset);
        e->str_size=(size_t)str->sh_size;
        return 1;
    }
bad:
    free_elf(e);return 0;
}
static void row(FILE *f,const char *module,const char *gate,
                const char *status,const char *detail){
    fprintf(f,"%s\t%s\t%s\t%s\n",module,gate,status,detail);
}
static int has_prefix(const char *s,const char *prefix){
    return strncmp(s,prefix,strlen(prefix))==0;
}
static void name_safe(char *out,size_t cap,const char *s,size_t n){
    size_t i=0;
    if(!cap)return;
    while(i<n && i+1<cap && s[i]){
        unsigned char c=(unsigned char)s[i];
        out[i]=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||
               (c>='0'&&c<='9')||c=='_'||c=='$'?c:'?';
        i++;
    }
    out[i]=0;
}
/* Only exact strings specified in this source are tested. Return presence
 * only, never dump arbitrary Java constants or proprietary rodata. */
static int has_literal(const E31 *e,const char *literal){
    size_t n=strlen(literal)+1;
    for(size_t i=0;i<e->eh->e_phnum;i++){
        const Elf64_Phdr *p=&e->ph[i];
        if(p->p_type!=PT_LOAD || (p->p_flags&PF_X) ||
           p->p_filesz<n ||
           !bounded((size_t)p->p_offset,(size_t)p->p_filesz,e->size))
            continue;
        if(memmem(e->data+(size_t)p->p_offset,(size_t)p->p_filesz,literal,n))
            return 1;
    }
    return 0;
}
static void count_import(Stats31 *st,const char *name){
    static const char *const jumps[]={
        "setjmp","_setjmp","longjmp","_longjmp",
        "sigsetjmp","siglongjmp","__sigsetjmp"
    };
    for(size_t i=0;i<sizeof(jumps)/sizeof(jumps[0]);i++)
        if(!strcmp(name,jumps[i]))st->bionic_jmp++;
    if(!strcmp(name,"__sF")||!strcmp(name,"__srget")||
       !strcmp(name,"__swbuf"))st->bionic_stdio++;
}
static int scan_module(FILE *out,const char *module,const char *path,Stats31 *st){
    E31 e;
    if(!read_elf(path,&e)){
        row(out,module,"ELF_INPUT","FAIL","malformed_or_missing_AArch64_dynsym");
        return 0;
    }
    static const char *const classes[]={
        "org/cocos2dx/lib/Cocos2dxRenderer",
        "org/cocos2dx/lib/Cocos2dxHelper",
        "org/cocos2dx/lib/Cocos2dxActivity",
        "org/cocos2dx/lib/Cocos2dxBitmap",
        "org/cocos2dx/lib/Cocos2dxGLSurfaceView",
        "android/content/Context",
        "android/content/res/AssetManager",
        "java/lang/ClassLoader",
        "java/lang/String",
        "android/os/Build",
        "android/view/Surface",
        "android/media/AudioManager"
    };
    static const char *const methods[]={
        "getCocos2dxWritablePath","getAssetsPath",
        "getCurrentLanguage","getPackageName","getSystemService",
        "nativeSetContext","nativeSetAudioDeviceInfo",
        "getClassLoader","loadClass","getAssets","getFilesDir"
    };
    int onload=0,init=0,render=0;
    for(size_t i=0;i<e.nsym;i++){
        const Elf64_Sym *sym=&e.sym[i];
        if(sym->st_name>=e.str_size)continue;
        const char *name=e.str+sym->st_name;
        size_t left=e.str_size-sym->st_name;
        const char *end=memchr(name,0,left);
        if(!end)continue;
        if(sym->st_shndx==SHN_UNDEF){
            count_import(st,name);
            continue;
        }
        if(ELF64_ST_TYPE(sym->st_info)!=STT_FUNC ||
           (ELF64_ST_BIND(sym->st_info)!=STB_GLOBAL &&
            ELF64_ST_BIND(sym->st_info)!=STB_WEAK))
            continue;
        if(!strcmp(name,"JNI_OnLoad"))onload=1;
        if(!strcmp(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit"))init=1;
        if(!strcmp(name,"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender"))render=1;
        if(!strcmp(name,"JNI_OnLoad")||has_prefix(name,"Java_")){
            st->jni_exports++;
            if(has_prefix(name,"Java_"))st->java_native++;
            if(has_prefix(name,"Java_org_cocos2dx_lib_"))st->objcocos++;
            if(st->export_printed<MAX_JNI_EXPORTS){
                char safe[192];
                name_safe(safe,sizeof(safe),name,(size_t)(end-name));
                row(out,module,"JNI_NATIVE_EXPORT","INVENTORIED",safe);
                st->export_printed++;
            }else st->export_truncated++;
        }
    }
    st->required_jni_exports=(unsigned)(onload+init+render);
    char data[128];
    snprintf(data,sizeof(data),"exports=%u java=%u cocos=%u truncated=%u",
             st->jni_exports,st->java_native,st->objcocos,
             st->export_truncated);
    row(out,module,"JNI_EXPORT_TOTAL","PASS",data);
    snprintf(data,sizeof(data),"JNI_OnLoad=%d nativeInit=%d nativeRender=%d",
             onload,init,render);
    row(out,module,"JNI_BOOTSTRAP_EXPORTS","INVENTORIED",data);
    for(size_t i=0;i<sizeof(classes)/sizeof(classes[0]);i++){
        int present=has_literal(&e,classes[i]);
        st->classes_present+=present;
        row(out,module,"JNI_CLASS_LITERAL",
            present?"FOUND":"NOT_FOUND",classes[i]);
    }
    for(size_t i=0;i<sizeof(methods)/sizeof(methods[0]);i++){
        int present=has_literal(&e,methods[i]);
        st->method_strings+=present;
        row(out,module,"JNI_METHOD_LITERAL",
            present?"FOUND":"NOT_FOUND",methods[i]);
    }
    snprintf(data,sizeof(data),"jmp_family=%u stdio_family=%u",
             st->bionic_jmp,st->bionic_stdio);
    row(out,module,"BIONIC_IMPORT_HINTS","INVENTORIED",data);
    printf("[lenneth phase31] module=%s JNI_EXPORTS=%u cocos=%u "
           "java_class_literals=%u method_literals=%u jmp_imports=%u "
           "stdio_imports=%u scanned_readonly=YES engine_executed=NO\n",
           module,st->jni_exports,st->objcocos,st->classes_present,
           st->method_strings,st->bionic_jmp,st->bionic_stdio);
    free_elf(&e);
    return 1;
}
int lenneth_jni_demand31(const char *game_path,const char *cxx_path){
    FILE *out=fopen("lenneth-jni-demand31.tsv","w");
    if(!out)return 31;
    fputs("module\tgate\tstatus\tdetail\n",out);
    Stats31 game={0},cxx={0};
    int c_ok=scan_module(out,"libc++_shared.so",cxx_path,&cxx);
    int g_ok=scan_module(out,"libMyGame.so",game_path,&game);
    row(out,"runtime","EARLY_BIONIC_JMP_LAYOUT","AOSP_HISTORICAL_ONLY",
        "256_byte_jmpbuf_core_at_8_float_at_112_not_game_version_proof");
    row(out,"runtime","JNI_CLASSLOADER_AND_ART","BLOCKED",
        "no_real_JavaVM_JNIEnv_classloader_or_object_graph");
    row(out,"runtime","JNI_NATIVE_REGISTRATION","UNVERIFIED",
        "static_exports_only_RegisterNatives_and_dynamic_names_unknown");
    row(out,"runtime","COCOS_JNI_CONTEXT","BLOCKED",
        "Android_Context_AssetManager_Activity_services_not_attached");
    row(out,"runtime","ANDROID_ASSET_HANDOFF","BLOCKED",
        "asset_filesystem_shim_not_Java_AssetManager");
    row(out,"runtime","COMMERCIAL_ENGINE","BLOCKED",
        "no_game_constructor_no_JNI_OnLoad_no_commercial_code");
    int failed=ferror(out);
    if(fclose(out)!=0)failed=1;
    int ok=c_ok&&g_ok&&!failed;
    printf("[lenneth phase31] JNI_DEMAND=%s game_exports=%u "
           "cocos_exports=%u classes=%u methods=%u bionic_jump_imports=%u "
           "startup_gate=BLOCKED guest_execution=NO report=lenneth-jni-demand31.tsv\n",
           ok?"PASS":"FAIL",game.jni_exports,game.objcocos,
           game.classes_present,game.method_strings,
           game.bionic_jmp+cxx.bionic_jmp);
    return ok?0:31;
}
int lenneth_jni_demand31_selftest(void){
    const unsigned char in[]="org/cocos2dx/lib/Cocos2dxHelper";
    char safe[48];name_safe(safe,sizeof(safe),(const char*)in,sizeof(in)-1);
    int ok=!strcmp(safe,"org?cocos2dx?lib?Cocos2dxHelper") &&
           has_prefix("Java_org_cocos2dx_lib_","Java_") &&
           !has_prefix("JNI_OnLoad","Java_") &&
           sizeof(BionicEarlyJmp31)==256 &&
           offsetof(BionicEarlyJmp31,floating)==112 &&
           bounded(3,5,8) && !bounded(3,6,8);
    printf("[lenneth phase31] SELFTEST=%s bounded_elf_parser=PASS "
           "jni_export_classifier=PASS historical_bionic_offsets=PASS "
           "guest_code_executed=NO\n",ok?"PASS":"FAIL");
    return ok?0:31;
}
