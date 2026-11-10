/* SPDX-License-Identifier: MIT
 * Linux ELF64/AArch64 relocator for vpl_nx. All guest code remains inert.
 * Intended as a portability foundation, not an Android/Bionic replacement.
 */
#define _GNU_SOURCE
#include "elf_linux.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DT_RELR
#define DT_RELR 36
#endif
#ifndef DT_ANDROID_RELR
#define DT_ANDROID_RELR 0x6fffe000
#endif
#define VPL_PAGE 4096U
#define VPL_FILE_MAX (128U * 1024U * 1024U)
#define VPL_MEMORY_MAX (128U * 1024U * 1024U)

static void error_set(char *out, size_t size, const char *what) {
    if (out && size)
        snprintf(out, size, "%s", what);
}
static int in_range(size_t off, size_t length, size_t total) {
    return off <= total && length <= total - off;
}
static uint64_t align_down(uint64_t n) { return n & ~(uint64_t)(VPL_PAGE-1); }
static uint64_t align_up(uint64_t n) {
    return (n + VPL_PAGE - 1) & ~(uint64_t)(VPL_PAGE - 1);
}
static int in_memory(const VplElf *m, uint64_t vaddr, size_t bytes) {
    return vaddr >= m->min_vaddr &&
        in_range((size_t)(vaddr - m->min_vaddr), bytes, m->memory_size);
}
static void *guest_ptr(const VplElf *m, uint64_t vaddr) {
    return m->memory + (size_t)(vaddr - m->min_vaddr);
}
void vpl_elf_close(VplElf *m) {
    if (!m) return;
    if (m->memory && m->memory_size)
        munmap(m->memory, m->memory_size);
    if (m->file && m->file_size)
        munmap(m->file, m->file_size);
    memset(m, 0, sizeof(*m));
}

int vpl_elf_open(VplElf *m, const char *filename, const char *label,
                 char *err, size_t errsize) {
    int fd = -1;
    struct stat st;
    uint64_t minv = UINT64_MAX, maxv = 0;
    const Elf64_Ehdr *eh = NULL;
    const Elf64_Shdr *sections = NULL;
    int result = -1;
    if (!m || !filename || !label) {
        error_set(err, errsize, "bad arguments");
        return -1;
    }
    memset(m, 0, sizeof(*m));
    snprintf(m->label, sizeof(m->label), "%s", label);
    fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd < 0 || fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) ||
        st.st_size < (off_t)sizeof(Elf64_Ehdr) ||
        st.st_size > (off_t)VPL_FILE_MAX) {
        error_set(err, errsize, "missing, oversized or invalid input file");
        goto done;
    }
    m->file_size = (size_t)st.st_size;
    m->file = mmap(NULL, m->file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m->file == MAP_FAILED) {
        m->file = NULL;
        error_set(err, errsize, "read-only mmap failed");
        goto done;
    }
    eh = (const Elf64_Ehdr *)m->file;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) ||
        eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        eh->e_machine != EM_AARCH64 || eh->e_type != ET_DYN ||
        eh->e_phentsize != sizeof(Elf64_Phdr) ||
        eh->e_shentsize != sizeof(Elf64_Shdr) ||
        !eh->e_phnum || eh->e_phnum > VPL_MAX_PHDR ||
        !eh->e_shnum || eh->e_shnum > 8192 ||
        !in_range(eh->e_phoff, (size_t)eh->e_phnum*sizeof(Elf64_Phdr), m->file_size) ||
        !in_range(eh->e_shoff, (size_t)eh->e_shnum*sizeof(Elf64_Shdr), m->file_size)) {
        error_set(err, errsize, "invalid AArch64 shared-object ELF header/tables");
        goto done;
    }
    const Elf64_Phdr *ph = (const Elf64_Phdr *)(m->file + eh->e_phoff);
    sections = (const Elf64_Shdr *)(m->file + eh->e_shoff);
    m->phnum = eh->e_phnum;
    memcpy(m->phdr, ph, m->phnum*sizeof(Elf64_Phdr));

    for (size_t i = 0; i < m->phnum; i++) {
        const Elf64_Phdr *p = &ph[i];
        if (p->p_type != PT_LOAD) continue;
        if (p->p_filesz > p->p_memsz ||
            !in_range(p->p_offset, p->p_filesz, m->file_size) ||
            p->p_vaddr > UINT64_MAX - p->p_memsz ||
            ((p->p_flags & (PF_W | PF_X)) == (PF_W | PF_X))) {
            error_set(err, errsize, "invalid or W+X PT_LOAD segment");
            goto done;
        }
        uint64_t low = align_down(p->p_vaddr);
        uint64_t high = align_up(p->p_vaddr + p->p_memsz);
        if (high < p->p_vaddr) {
            error_set(err, errsize, "PT_LOAD overflow");
            goto done;
        }
        if (low < minv) minv = low;
        if (high > maxv) maxv = high;
    }
    if (minv == UINT64_MAX || maxv <= minv || maxv-minv > VPL_MEMORY_MAX ||
        sysconf(_SC_PAGESIZE) != VPL_PAGE) {
        error_set(err, errsize, "unsupported memory layout or page size");
        goto done;
    }
    m->min_vaddr = minv;
    m->memory_size = (size_t)(maxv-minv);
    m->memory = mmap(NULL, m->memory_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m->memory == MAP_FAILED) {
        m->memory = NULL;
        error_set(err, errsize, "guest memory mmap failed");
        goto done;
    }
    m->load_bias = (uintptr_t)m->memory - (uintptr_t)minv;
    for (size_t i = 0; i < m->phnum; i++) {
        const Elf64_Phdr *p = &ph[i];
        if (p->p_type == PT_LOAD && p->p_filesz) {
            if (!in_memory(m, p->p_vaddr, p->p_memsz)) {
                error_set(err, errsize, "PT_LOAD outside reserved region");
                goto done;
            }
            memcpy(guest_ptr(m,p->p_vaddr), m->file + p->p_offset, p->p_filesz);
        }
    }
    for (size_t i = 0; i < eh->e_shnum; i++) {
        const Elf64_Shdr *s = &sections[i];
        if (s->sh_type == SHT_DYNSYM) {
            if (s->sh_link >= eh->e_shnum ||
                s->sh_entsize != sizeof(Elf64_Sym) || s->sh_size % sizeof(Elf64_Sym) ||
                !in_range(s->sh_offset,s->sh_size,m->file_size)) {
                error_set(err,errsize,"malformed dynamic symbol table");goto done;
            }
            const Elf64_Shdr *names = &sections[s->sh_link];
            if (names->sh_type != SHT_STRTAB ||
                !in_range(names->sh_offset,names->sh_size,m->file_size)) {
                error_set(err,errsize,"malformed dynamic string table");goto done;
            }
            m->dynsym = (const Elf64_Sym *)(m->file + s->sh_offset);
            m->sym_count = s->sh_size / sizeof(Elf64_Sym);
            m->dynstr = (const char *)(m->file + names->sh_offset);
            m->str_size = names->sh_size;
            break;
        }
    }
    if (!m->dynsym || !m->dynstr) {
        error_set(err,errsize,"ELF has no usable .dynsym/.dynstr");goto done;
    }
    result = 0;
done:
    if (fd >= 0) close(fd);
    if (result != 0) vpl_elf_close(m);
    return result;
}

static const char *sym_name(const VplElf *m, size_t i) {
    if (!m || i >= m->sym_count) return NULL;
    size_t off = m->dynsym[i].st_name;
    if (off >= m->str_size) return NULL;
    const char *name = m->dynstr+off;
    if (!memchr(name,0,m->str_size-off)) return NULL;
    return name;
}
uintptr_t vpl_elf_find_export(const VplElf *m, const char *name) {
    if (!m || !name) return 0;
    for (size_t i=0; i<m->sym_count; i++) {
        const Elf64_Sym *sym = &m->dynsym[i];
        if (sym->st_shndx == SHN_UNDEF ||
            ELF64_ST_BIND(sym->st_info) == STB_LOCAL)
            continue;
        const char *candidate = sym_name(m,i);
        if (candidate && strcmp(candidate,name) == 0) {
            if (sym->st_shndx == SHN_ABS) return (uintptr_t)sym->st_value;
            if (in_memory(m,sym->st_value,1))
                return m->load_bias+(uintptr_t)sym->st_value;
        }
    }
    return 0;
}

int vpl_elf_link(VplElf *m, const VplElf *partner, FILE *log,
                 char *err, size_t errsize) {
    if (!m || !m->file || m->prepared) {
        error_set(err,errsize,"module missing or already linked");return -1;
    }
    const Elf64_Ehdr *eh=(const Elf64_Ehdr *)m->file;
    const Elf64_Shdr *sections=(const Elf64_Shdr *)(m->file+eh->e_shoff);
    for (size_t i=0; i<m->phnum; i++) {
        const Elf64_Phdr *p=&m->phdr[i];
        if (p->p_type != PT_DYNAMIC) continue;
        if (!in_range(p->p_offset,p->p_filesz,m->file_size) ||
            p->p_filesz%sizeof(Elf64_Dyn)) {
            error_set(err,errsize,"invalid PT_DYNAMIC");return -1;
        }
        const Elf64_Dyn *d=(const Elf64_Dyn *)(m->file+p->p_offset);
        for(size_t j=0;j<p->p_filesz/sizeof(Elf64_Dyn);j++) {
            if (d[j].d_tag==DT_NULL) break;
            if (d[j].d_tag==DT_RELR || d[j].d_tag==DT_ANDROID_RELR)
                m->relr_unhandled=1; /* fail closed, don't corrupt guest */
        }
    }
    for (size_t i=0; i<eh->e_shnum; i++) {
        const Elf64_Shdr *s=&sections[i];
        if (s->sh_type != SHT_RELA || !(s->sh_flags & SHF_ALLOC)) continue;
        if (!in_range(s->sh_offset,s->sh_size,m->file_size) ||
            s->sh_entsize != sizeof(Elf64_Rela) ||
            s->sh_size % sizeof(Elf64_Rela)) {
            error_set(err,errsize,"invalid RELA section");return -1;
        }
        const Elf64_Rela *rel=(const Elf64_Rela *)(m->file+s->sh_offset);
        size_t n=s->sh_size/sizeof(Elf64_Rela);
        if (n > 200000) {error_set(err,errsize,"too many relocations");return -1;}
        for (size_t j=0; j<n; j++) {
            if (!in_memory(m,rel[j].r_offset,sizeof(uint64_t))) {
                error_set(err,errsize,"relocation target outside guest memory");
                return -1;
            }
            uintptr_t *ptr=(uintptr_t *)guest_ptr(m,rel[j].r_offset);
            unsigned type=ELF64_R_TYPE(rel[j].r_info);
            size_t symindex=ELF64_R_SYM(rel[j].r_info);
            uintptr_t value=0;
            if (type==R_AARCH64_NONE) continue;
            if (type==R_AARCH64_RELATIVE) {
                *ptr=m->load_bias+(uintptr_t)rel[j].r_addend;
                m->reloc_relative++;
                continue;
            }
            if (type != R_AARCH64_ABS64 && type != R_AARCH64_GLOB_DAT &&
                type != R_AARCH64_JUMP_SLOT) {
                m->reloc_unsupported++;
                continue;
            }
            if (symindex >= m->sym_count) {
                error_set(err,errsize,"relocation symbol out of range");return -1;
            }
            const Elf64_Sym *sym=&m->dynsym[symindex];
            if (sym->st_shndx != SHN_UNDEF) {
                value = sym->st_shndx==SHN_ABS ?
                    (uintptr_t)sym->st_value : m->load_bias+(uintptr_t)sym->st_value;
            } else {
                const char *name=sym_name(m,symindex);
                if (!name) {
                    error_set(err,errsize,"invalid import symbol name");return -1;
                }
                value=vpl_elf_find_export(partner,name);
                if (!value) {
                    m->reloc_unresolved++;
                    if (log && m->reloc_unresolved<=20)
                        fprintf(log,"%s: missing provider for %s\n",m->label,name);
                    continue;
                }
            }
            *ptr=value+(uintptr_t)rel[j].r_addend;
            m->reloc_resolved++;
        }
    }
    m->prepared=1;
    if (log)
        fprintf(log, "%s: mapped=%zu relative=%zu resolved=%zu unresolved=%zu unsupported=%zu relr=%zu\n",
            m->label,m->memory_size,m->reloc_relative,m->reloc_resolved,
            m->reloc_unresolved,m->reloc_unsupported,m->relr_unhandled);
    /* Preflight remains successful even with missing host Bionic providers.
       vpl_elf_protect() refuses to finalize any incomplete linkage. */
    return 0;
}

int vpl_elf_protect(VplElf *m, char *err, size_t errsize) {
    if (!m || !m->prepared || m->reloc_unresolved ||
        m->reloc_unsupported || m->relr_unhandled) {
        error_set(err,errsize,"link incomplete; executable mapping denied");
        return -1;
    }
    const size_t pages=m->memory_size/VPL_PAGE;
    unsigned char *perms=calloc(pages,1);
    if (!perms) {error_set(err,errsize,"permission array OOM");return -1;}
    for (size_t i=0;i<m->phnum;i++) {
        const Elf64_Phdr *p=&m->phdr[i];
        if (p->p_type != PT_LOAD || !p->p_memsz) continue;
        size_t first=(size_t)((align_down(p->p_vaddr)-m->min_vaddr)/VPL_PAGE);
        size_t last=(size_t)((align_up(p->p_vaddr+p->p_memsz)-m->min_vaddr)/VPL_PAGE);
        if (first>last || last>pages) {
            free(perms);error_set(err,errsize,"PT_LOAD permission outside map");return -1;
        }
        for(size_t j=first;j<last;j++) perms[j] |= (unsigned char)(p->p_flags & 7);
    }
    for(size_t i=0;i<pages;i++) {
        if ((perms[i] & (PF_X|PF_W))==(PF_X|PF_W)) {
            free(perms);error_set(err,errsize,"shared-page W+X collision");return -1;
        }
    }
    for (size_t first=0;first<pages;) {
        size_t end=first+1;
        while (end<pages && perms[end]==perms[first]) end++;
        int prot=0;
        if (perms[first]&PF_R) prot|=PROT_READ;
        if (perms[first]&PF_W) prot|=PROT_WRITE;
        if (perms[first]&PF_X) prot|=PROT_EXEC;
        if (mprotect(m->memory+first*VPL_PAGE,(end-first)*VPL_PAGE,prot)!=0) {
            free(perms);error_set(err,errsize,"mprotect failed");return -1;
        }
        first=end;
    }
    free(perms);
    return 0;
}
