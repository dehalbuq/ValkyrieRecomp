/* SPDX-License-Identifier: MIT
 * Read-only and non-executing Linux ARM64 preflight for two Android ELF modules.
 * Running this does not call any .init_array, JNI_OnLoad, nativeInit or game code.
 */
#include "elf_linux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    if (argc!=3) {
        fprintf(stderr,"usage: %s libMyGame.so libc++_shared.so\n",argv[0]);
        return 2;
    }
    char error[160]={0};
    VplElf game={0},cxx={0};
    if (vpl_elf_open(&cxx,argv[2],"libc++_shared",error,sizeof(error))!=0) {
        fprintf(stderr,"VPLNX_ELF_PREFLIGHT=ERROR library=cxx reason=%s\n",error);
        return 2;
    }
    if (vpl_elf_open(&game,argv[1],"libMyGame",error,sizeof(error))!=0) {
        fprintf(stderr,"VPLNX_ELF_PREFLIGHT=ERROR library=game reason=%s\n",error);
        vpl_elf_close(&cxx);
        return 2;
    }
    printf("ELF_INPUT=VALID arch=AArch64 game_bytes=%zu cxx_bytes=%zu\n",
           game.file_size,cxx.file_size);
    /* Cross-module relocations can resolve from the partner even before a
       host Bionic imports table is implemented. Never run guest code here. */
    int a=vpl_elf_link(&cxx,&game,stdout,error,sizeof(error));
    int b=vpl_elf_link(&game,&cxx,stdout,error,sizeof(error));
    int rc=0;
    if(a||b) {
        fprintf(stderr,"VPLNX_ELF_PREFLIGHT=ERROR reason=%s\n",error);
        rc=2;
    } else {
        const char *jni_names[]={
            "JNI_OnLoad",
            "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit",
            "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender",
            "Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext"
        };
        int found=0;
        for(size_t i=0;i<sizeof(jni_names)/sizeof(jni_names[0]);i++) {
            int yes=vpl_elf_find_export(&game,jni_names[i])!=0;
            printf("JNI_EXPORT %s %s\n",jni_names[i],yes?"PRESENT":"MISSING");
            found+=yes;
        }
        int clean=!(game.reloc_unresolved||cxx.reloc_unresolved||
                    game.reloc_unsupported||cxx.reloc_unsupported||
                    game.relr_unhandled||cxx.relr_unhandled);
        if(clean) {
            if(vpl_elf_protect(&cxx,error,sizeof(error))!=0 ||
               vpl_elf_protect(&game,error,sizeof(error))!=0) {
                printf("MEMORY_PROTECTION=BLOCKED reason=%s\n",error);
                clean=0;
            } else {
                puts("MEMORY_PROTECTION=PASS W_X=ENFORCED");
            }
        }
        printf("JNI_EXPORTS=%d/4\n",found);
        printf("NEXT_GATE=%s\n",clean&&found==4?"READY_FOR_SEPARATE_JNI_ABI_TEST":
               "BLOCKED_MISSING_HOST_OR_JNI_SERVICES");
        puts("COMMERCIAL_ENGINE=NOT_EXECUTED");
        puts("VPLNX_ELF_PREFLIGHT=PASS");
    }
    vpl_elf_close(&game);
    vpl_elf_close(&cxx);
    return rc;
}
