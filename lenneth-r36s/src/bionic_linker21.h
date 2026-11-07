#ifndef LENNETH_BIONIC_LINKER21_H
#define LENNETH_BIONIC_LINKER21_H
#include <elf.h>
#include <stdint.h>
int lenneth_phase21_register(const char *soname,uint64_t load_bias,
                            const Elf64_Phdr *phdr,unsigned phnum);
void lenneth_phase21_clear(void);
int lenneth_phase21_lookup(const char *name,uint64_t *address);
int lenneth_phase21_selftest(void);
#endif
