/*
 * Minimal, independently authored ARM64 UTF-32 / wide-char array ABI
 * provider (MIT). Only pure wchar_t 32-bit in-memory operations.
 * No native/guest locale_t or mbstate_t objects are exchanged.
 */
#include "unicode_provider.h"
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(wchar_t)==4,"Android ARM64 / Linux ARM64 must use wchar_t=4 bytes");
typedef struct {const char *name;void(*fn)(void);} Entry;
#define X(fn) {#fn,(void(*)(void))(fn)}
static const Entry entries[]={
    X(wcslen),X(wmemcpy),X(wmemmove),X(wmemcmp),X(wmemchr),X(wmemset),
    /* Scalar wchar/wint operations: exact 32-bit Android ARM64 ABI.
     * Locale-sensitive *semantics* may differ; no Bionic locale_t,
     * mbstate_t, FILE* or conversion-state pointers cross the boundary. */
    X(iswalpha),X(iswdigit),X(iswspace),X(iswxdigit),
    X(iswupper),X(iswlower),X(iswprint),X(iswcntrl),
    X(iswpunct),X(iswblank),X(towupper),X(towlower),
    X(wcstol),X(wcstoul),X(wcstoll),X(wcstoull),
    X(wcstof),X(wcstod),X(wcstold),X(btowc),X(wctob)
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
    if(!iswalpha(L'A')||!iswdigit(L'8')||!iswspace(L' ')||
       towupper(L'z')!=L'Z'||towlower(L'Q')!=L'q' ||
       wcstol(L"123",NULL,10)!=123 ||
       wcstoul(L"42",NULL,10)!=42 ||
       wcstod(L"3.25",NULL)<3.24 ||
       btowc('A')!=L'A'||wctob(L'A')!='A')ok=0;
    uint64_t p=0;
    if(!lenneth_unicode_provider_lookup("wmemcpy",&p)||!p ||
       lenneth_unicode_provider_lookup("newlocale",&p) ||
       lenneth_unicode_provider_lookup("mbrtowc",&p))ok=0;
    printf("[lenneth unicode] SELFTEST=%s utf32_wchar=4 "
           "raw_and_scalar_wide_operations=%zu locale_and_mbstate=UNMAPPED "
           "commercial_execution=NO\n",ok?"PASS":"FAIL",
           sizeof(entries)/sizeof(entries[0]));
    return ok?0:9;
}
