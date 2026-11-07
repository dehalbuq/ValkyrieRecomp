#ifndef LENNETH_POSIX_FD_PROVIDER_H
#define LENNETH_POSIX_FD_PROVIDER_H
#include <stdint.h>
#include <sys/types.h>
/* Experimental Android POSIX file-descriptor ABI adapter.
 * Guest reads are constrained to assets/, guest writes to saves/.
 * This intentionally does NOT translate Bionic FILE* or struct stat.
 */
int lenneth_fd_provider_open(const char *assets_dir,const char *saves_dir);
void lenneth_fd_provider_close(void);
int lenneth_fd_provider_lookup(const char *name,uint64_t *out);
int lenneth_fd_provider_selftest(void);
/* Safe internal call from our own opaque FILE adapter only. */
int lenneth_fd_open_guest_path(const char *path,int flags,mode_t mode);
#endif
