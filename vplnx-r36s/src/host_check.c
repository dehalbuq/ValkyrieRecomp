/* SPDX-License-Identifier: MIT
 * Native Linux ARM64 whitelist provider + Android ELF preflight.
 * No guest constructors or JNI entry point are ever invoked by this command.
 */
#include "elf_linux.h"
#include "host_imports_linux.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(int argc,char **argv) {
    if (argc == 2 && strcmp(argv[1],"--host-selftest")==0) {
        int rc=vpl_linux_host_selftest();
        printf("BIONIC_HOST_SELFTEST=%s native_abi_subset=1 pthread_layout=BLOCKED file_layout=BLOCKED\n",
               rc?"FAIL":"PASS");
        printf("COMMERCIAL_ENGINE_EXECUTED=NO\n");
        return rc?1:0;
    }
    if (argc != 3) {
        fprintf(stderr,"usage: %s --host-selftest | game.so partner.so\n",argv[0]);
        return 2;
    }
    VplElf game={0},partner={0};
    VplHostStats stats={0};
    char error[160]={0};
    if(vpl_elf_open(&partner,argv[2],"partner",error,sizeof(error))!=0) {
        fprintf(stderr,"BIONIC_HOST=ERROR partner=%s\n",error);
        return 2;
    }
    if(vpl_elf_open(&game,argv[1],"guest",error,sizeof(error))!=0) {
        fprintf(stderr,"BIONIC_HOST=ERROR guest=%s\n",error);
        vpl_elf_close(&partner);
        return 2;
    }
    /* Directly consume host callbacks and partner exports. No dlopen of
     * foreign code and no function calls into the mapped ELF modules. */
    int a=vpl_elf_link_host(&partner,&game,vpl_linux_host_lookup,&stats,stdout,
                            error,sizeof(error));
    int b=vpl_elf_link_host(&game,&partner,vpl_linux_host_lookup,&stats,stdout,
                            error,sizeof(error));
    int complete=!a && !b &&
        !partner.reloc_unresolved && !game.reloc_unresolved &&
        !partner.reloc_unsupported && !game.reloc_unsupported &&
        !partner.relr_unhandled && !game.relr_unhandled;
    if (a || b) fprintf(stderr,"BIONIC_HOST=ERROR detail=%s\n",error);
    if(complete) {
        if(vpl_elf_protect(&partner,error,sizeof(error))!=0 ||
           vpl_elf_protect(&game,error,sizeof(error))!=0) {
            fprintf(stderr,"MEMORY_PROTECTION=BLOCKED detail=%s\n",error);
            complete=0;
        } else {
            puts("MEMORY_PROTECTION=PASS W_X=ENFORCED");
        }
    } else {
        puts("MEMORY_PROTECTION=BLOCKED unresolved_guest_imports=1");
    }
    printf("BIONIC_HOST_IMPORTS accepted=%zu blocked=%zu guest_slots=%zu host_slots=%zu\n",
           stats.found,stats.unknown,
           game.reloc_resolved+partner.reloc_resolved,
           game.reloc_host_resolved+partner.reloc_host_resolved);
    puts("BIONIC_HOST_SECURITY unknown_imports=FAIL_CLOSED pthread_opaque=BLOCKED");
    printf("BIONIC_HOST_LINK=%s\n",complete?"PASS":"BLOCKED");
    puts("COMMERCIAL_ENGINE_EXECUTED=NO");
    vpl_elf_close(&game);
    vpl_elf_close(&partner);
    return (a||b)?2:0;
}
