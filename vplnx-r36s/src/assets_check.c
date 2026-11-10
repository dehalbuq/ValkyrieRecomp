/* SPDX-License-Identifier: MIT
 * Test real 64-bit file I/O and traversal denial, not any game asset.
 */
#include "assets_linux.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void fail(const char *s) {fprintf(stderr,"ASSET_CHECK=FAIL %s\n",s);exit(1);}
int main(int argc,char **argv) {
    if(argc!=2) {fprintf(stderr,"usage: %s test-assets-dir\n",argv[0]);return 2;}
    if(vpl_assets_set_root(argv[1])!=0) fail("root not available");
    AAssetManager *mgr=asset_manager();
    if(!mgr) fail("manager unavailable");
    AAsset *a=AAssetManager_open_fake(mgr,"assets/discData.cpk",2);
    if(!a) fail("sparse archive not opened");
    if(AAsset_getLength64_fake(a)!=350000004LL) fail("incorrect 64-bit size");
    if(AAsset_seek_fake(a,350000000LL,SEEK_SET)!=350000000LL)
        fail("large offset seek");
    char magic[5]={0};
    if(AAsset_read_fake(a,magic,4)!=4 || strcmp(magic,"CPK!")!=0)
        fail("large offset read");
    int64_t first=123,last=0;
    int fd=AAsset_openFileDescriptor64_fake(a,&first,&last);
    if(fd<0 || first!=0 || last!=350000004LL)
        fail("file descriptor range");
    char magic2[5]={0};
    if(lseek(fd,350000000LL,SEEK_SET)!=350000000LL ||
       read(fd,magic2,4)!=4 || strcmp(magic2,"CPK!")!=0)
        fail("file descriptor streaming");
    close(fd);
    AAsset_close_fake(a);
    if(AAssetManager_open_fake(mgr,"../passwd",0)!=NULL ||
       AAssetManager_open_fake(mgr,"/etc/passwd",0)!=NULL ||
       AAssetManager_open_fake(mgr,"link-passwd",0)!=NULL ||
       AAssetManager_open_fake(mgr,"link-dir/passwd",0)!=NULL ||
       AAssetManager_open_fake(mgr,"assets/../etc/passwd",0)!=NULL)
        fail("path escape was accepted");
    a=AAssetManager_open_fake(mgr,"nested/ok.txt",0);
    if(!a) fail("nested file missing");
    char v[3]={0};
    if(AAsset_read_fake(a,v,2)!=2 || strcmp(v,"OK")!=0) fail("nested read");
    AAsset_close_fake(a);
    AAssetDir *d=AAssetManager_openDir_fake(mgr,"nested");
    if(!d) fail("directory list");
    const char *entry=AAssetDir_getNextFileName_fake(d);
    if(!entry || strcmp(entry,"nested/ok.txt")!=0) fail("directory filename");
    AAssetDir_close_fake(d);
    vpl_assets_shutdown();
    puts("VPLNX_ASSET_STREAM=PASS large_cpk_offset=350000000 symlink_denial=PASS traversal_denial=PASS commercial_assets=NONE");
    return 0;
}
