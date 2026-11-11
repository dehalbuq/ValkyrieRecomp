/* SPDX-License-Identifier: MIT
 * cloud ONLY test of executable synthetic AArch64 ELF code, with W^X.
 *
 * SECURITY: This diagnostic executes the provided two .so modules.
 * Do NOT distribute it in PortMaster or run it on commercial libraries.
 * The caller must supply CI-generated, allowlisted synthetic test objects.
 * No real Android engine, native constructor, or game file is executed.
 */
#define _POSIX_C_SOURCE 200809L
#include "elf_linux.h"
#include "host_imports_linux.h"
#include "jni_fake.h"
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define GUEST_MAGIC UINT64_C(0x475553545f43495f)
#define PARTNER_MAGIC UINT64_C(0x5054525f43495f31)

static int child_run(const char *game_path, const char *partner_path) {
    VplElf g={0}, p={0};
    VplHostStats gs={0}, ps={0};
    char err[200]={0};
    int rc=1;
    if(vpl_elf_open(&g,game_path,"CI-synthetic-guest",err,sizeof(err)) ||
       vpl_elf_open(&p,partner_path,"CI-synthetic-partner",err,sizeof(err))) {
        fprintf(stderr,"SYNTHETIC_GUEST=FAIL stage=open reason=%s\n",err);
        goto done;
    }
    /* Confirm exact CI fixture marker before permitting EXEC mappings.
     * This is a project-safety gate, not an adversarial signature scheme. */
    const uint64_t *gm=(const uint64_t *)(uintptr_t)
        vpl_elf_find_export(&g,"vplnx_synthetic_guest_magic");
    const uint64_t *pm=(const uint64_t *)(uintptr_t)
        vpl_elf_find_export(&p,"vplnx_synthetic_partner_magic");
    if(!gm || !pm || *gm!=GUEST_MAGIC || *pm!=PARTNER_MAGIC) {
        fputs("SYNTHETIC_GUEST=BLOCKED reason=fixture-marker-mismatch\n",stderr);
        goto done;
    }
    if(vpl_elf_link_host(&p,&g,vpl_linux_host_lookup,&ps,stderr,
                         err,sizeof(err)) ||
       vpl_elf_link_host(&g,&p,vpl_linux_host_lookup,&gs,stderr,
                         err,sizeof(err)) ||
       g.reloc_unresolved || p.reloc_unresolved ||
       g.reloc_unsupported || p.reloc_unsupported ||
       g.relr_unhandled || p.relr_unhandled) {
        fprintf(stderr,"SYNTHETIC_GUEST=BLOCKED reason=missing-imports"
                       " guest=%zu partner=%zu error=%s\n",
                       g.reloc_unresolved,p.reloc_unresolved,err);
        goto done;
    }
    if(vpl_elf_protect(&p,err,sizeof(err)) ||
       vpl_elf_protect(&g,err,sizeof(err))) {
        fprintf(stderr,"SYNTHETIC_GUEST=BLOCKED reason=W_X %s\n",err);
        goto done;
    }
    uintptr_t entry=vpl_elf_find_export(&g,"JNI_OnLoad");
    if(!entry) {
        fputs("SYNTHETIC_GUEST=BLOCKED reason=missing-JNI-OnLoad\n",stderr);
        goto done;
    }
    /* The JNI fake VM is tested independently. Here we actually cross the
     * relocated native AArch64 -> JNI function-pointer boundary. */
    jni_init();
    if(!fake_vm || !fake_env) goto done;
    int (*jni_onload)(void*,void*)=(int (*)(void*,void*))entry;
    int result=jni_onload(fake_vm,NULL);
    if(result != 0x00010006) {
        fprintf(stderr,"SYNTHETIC_GUEST=FAIL stage=JNI_OnLoad result=%d\n",
                result);
        goto done;
    }
    puts("SYNTHETIC_ARM64_JNI_ONLOAD=PASS AArch64_guest_executed=YES");
    puts("SYNTHETIC_ARM64_LINK=PASS cross_elf=1 pthread_abi=1 libc=1 JNI=1 W_X=1");
    puts("COMMERCIAL_ENGINE_EXECUTED=NO");
    fflush(stdout);
    rc=0;
done:
    vpl_elf_close(&g);
    vpl_elf_close(&p);
    return rc;
}
int main(int argc,char **argv) {
    if(argc!=3) {
        fprintf(stderr,"usage: %s CI-generated-guest.so CI-generated-partner.so\n",
                argv[0]);
        return 2;
    }
    pid_t pid=fork();
    if(pid<0) return 2;
    if(pid==0) {
        alarm(8);
        _exit(child_run(argv[1],argv[2]));
    }
    int st=0;
    if(waitpid(pid,&st,0)!=pid) return 2;
    if(!WIFEXITED(st) || WEXITSTATUS(st)!=0) {
        fprintf(stderr,"SYNTHETIC_ARM64_ISOLATION=FAIL status=%d\n",st);
        return 1;
    }
    puts("SYNTHETIC_ARM64_ISOLATION=PASS forked_child=1 commercial_engine=NO");
    return 0;
}
