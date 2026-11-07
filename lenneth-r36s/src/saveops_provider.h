#ifndef LENNETH_SAVEOPS_PROVIDER_H
#define LENNETH_SAVEOPS_PROVIDER_H
#include <stdint.h>
/* Android POSIX pathname modification limited to local saves/ root.
 * NOT a general-purpose guest filesystem or Bionic FILE* implementation. */
int lenneth_saveops_open(const char *saves_root);
void lenneth_saveops_close(void);
int lenneth_saveops_lookup(const char *name,uint64_t *address);
int lenneth_saveops_selftest(void);
#endif
