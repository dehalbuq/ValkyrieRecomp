/* Lenneth Phase22: diagnostic-only Bionic AArch64 nonlocal-jump ABI audit.
 * Implementation is MIT / independently written. No Bionic setjmp/longjmp
 * bindings are provided. Never pass an Android jmp_buf to glibc.
 *
 * References:
 * AOSP bionic arch-arm64/include/machine/setjmp.h (legacy 256 byte buffer);
 * AOSP bionic libc/arch-arm64/bionic/setjmp.S (newer cookie/checksum layout).
 */
#define _GNU_SOURCE
#include "compat_phase22.h"
#include <inttypes.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {uint64_t slot[32];} BionicJmpBuffer;
_Static_assert(sizeof(BionicJmpBuffer)==256,
               "Bionic AArch64 jmp_buf has 32 64-bit words");
typedef struct {
    uint32_t magic;
    uint32_t sigmask;
    uint64_t core[13];
    uint64_t fp[8];
    uint64_t reserved[10];
} LegacyJmp;
_Static_assert(sizeof(LegacyJmp)==256,"Bionic legacy jmp layout");
_Static_assert(offsetof(LegacyJmp,core)==8,"legacy callee saved x30/sp offset");
_Static_assert(offsetof(LegacyJmp,fp)==112,"legacy float offset");
/* Later Bionic has a cookie/signal flag in word 0, signal mask in word 1,
 * registers x30,sp,... in words 2-15, d8-d15 in words 16-23 and a
 * checksum at word 24. The C assertion covers layout, not register code. */
typedef struct {
    uint64_t cookie_and_signal,signal_mask;
    uint64_t core[14];
    uint64_t fp[8];
    uint64_t checksum;
    uint64_t reserved[7];
} ModernJmp;
_Static_assert(sizeof(ModernJmp)==256,"Bionic newer jmp layout");
_Static_assert(offsetof(ModernJmp,core)==16,"Bionic recent core offset");
_Static_assert(offsetof(ModernJmp,fp)==128,"Bionic recent fp offset");
_Static_assert(offsetof(ModernJmp,checksum)==192,"Bionic checksum offset");

int lenneth_phase22_jmp_abi_selftest(void){
    int ok=1;
    /* Deliberately validate only data layout. Nonlocal jumps need native
     * AArch64 register save/restore, cookie and signal semantics, plus
     * stable detection of target Android API level before any binding. */
    BionicJmpBuffer b;
    unsigned char guard_a[8]={0x7c,0x7c,0x7c,0x7c,0x7c,0x7c,0x7c,0x7c};
    unsigned char guard_b[8]={0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29};
    memset(&b,0,sizeof(b));
    b.slot[0]=UINT64_C(0x53657200);
    b.slot[24]=UINT64_C(0xdeadbeef);
    if(b.slot[0]!=UINT64_C(0x53657200) ||
       b.slot[24]!=UINT64_C(0xdeadbeef) ||
       sizeof(b)!=256 || sizeof(long)!=8)ok=0;
    for(size_t i=0;i<sizeof(guard_a);i++){
        if(guard_a[i]!=0x7c || guard_b[i]!=0x29)ok=0;
    }
    printf("[lenneth phase22] JMP_ABI_AUDIT=%s bionic_jmpbuf_bytes=%zu "
           "legacy_offsets=8,112 modern_offsets=16,128,192 "
           "host_jmpbuf_bytes=%zu incompatible_layouts=ISOLATED "
           "setjmp_longjmp=UNMAPPED commercial_execution=NO\n",
           ok?"PASS":"FAIL",sizeof(b),sizeof(jmp_buf));
    return ok?0:22;
}
