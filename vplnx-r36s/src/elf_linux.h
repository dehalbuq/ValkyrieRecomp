/* SPDX-License-Identifier: MIT
 * vpl_nx Android ELF64/AArch64 loader - Linux preflight interface.
 * No Switch syscalls, no code invocation, no embedded commercial files.
 */
#ifndef VPLNX_ELF_LINUX_H
#define VPLNX_ELF_LINUX_H
#include <elf.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define VPL_MAX_PHDR 128
/* Zero is a deliberate refusal to resolve an unsupported Android ABI import.
 * No arbitrary RTLD_DEFAULT/dlsym fallback is permitted. */
typedef uintptr_t (*VplHostResolver)(const char *symbol, void *context);
typedef struct {
    char label[32];
    unsigned char *file;
    size_t file_size;
    unsigned char *memory;
    size_t memory_size;
    uint64_t min_vaddr;
    uintptr_t load_bias;
    Elf64_Phdr phdr[VPL_MAX_PHDR];
    size_t phnum;
    const Elf64_Sym *dynsym;
    size_t sym_count;
    const char *dynstr;
    size_t str_size;
    size_t reloc_relative;
    size_t reloc_resolved;
    size_t reloc_host_resolved;
    size_t reloc_unresolved;
    size_t reloc_unsupported;
    size_t relr_unhandled;
    int prepared;
} VplElf;

int vpl_elf_open(VplElf *m, const char *filename, const char *label,
                 char *err, size_t errsize);
uintptr_t vpl_elf_find_export(const VplElf *m, const char *name);
int vpl_elf_link(VplElf *m, const VplElf *partner, FILE *log,
                 char *err, size_t errsize);
int vpl_elf_link_host(VplElf *m, const VplElf *partner,
                      VplHostResolver host, void *host_context, FILE *log,
                      char *err, size_t errsize);
int vpl_elf_protect(VplElf *m, char *err, size_t errsize);
void vpl_elf_close(VplElf *m);
#endif
