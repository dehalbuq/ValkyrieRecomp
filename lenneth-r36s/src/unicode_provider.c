/*
 * Minimal, independently authored ARM64 UTF-32 / wide-char array ABI
 * provider (MIT). Only pure wchar_t 32-bit in-memory operations.
 * No native/guest locale_t or mbstate_t objects are exchanged.
 */
#include "unicode_provider.h"
#include <wchar.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(wchar_t)==4,"Android ARM64 / Linux ARM64 must use wchar_t=4 bytes");
typedef struct {const char *name;void(*fn)(void);} Entry;
#define X(fn) {#fn,(void(*)(void))(fn)}
static const Entry entries[]={
    X(wcslen),X(wmemcpy),X(wmemmove),X(wmemcmp),X(wmemchr),X(wmemset)
};
int lenneth_unicode_provider_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
        if(strcmp(name,entries[i].name))continue;
        void(*fn)(void)=entries[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));return 1;
    }
    return 0;
}
int lenneth_unicode_provider_selftest(void){
    int ok=1;
    wchar_t src[]={L'L',L'E',L'N',L'N',L'E',L'T',L'H',0};
    wchar_t dst[16]={0};
    if(wcslen(src)!=7)ok=0;
    if(wmemcpy(dst,src,8)!=dst||wmemcmp(dst,src,8))ok=0;
    if(wmemchr(dst,L'T',8)!=dst+5)ok=0;
    if(wmemset(dst+7,L'!',2)!=dst+7)ok=0;
    if(wmemmove(dst+1,dst,7)!=dst+1)ok=0;
    uint64_t p=0;
    if(!lenneth_unicode_provider_lookup("wmemcpy",&p)||!p ||
       lenneth_unicode_provider_lookup("newlocale",&p) ||
       lenneth_unicode_provider_lookup("mbrtowc",&p))ok=0;
    printf("[lenneth unicode] SELFTEST=%s utf32_wchar=4 "
           "raw_wide_operations=%zu locale_and_mbstate=UNMAPPED "
           "commercial_execution=NO\n",ok?"PASS":"FAIL",
           sizeof(entries)/sizeof(entries[0]));
    return ok?0:9;
}
