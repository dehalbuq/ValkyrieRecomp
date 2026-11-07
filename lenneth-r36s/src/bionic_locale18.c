/*
 * Lenneth R36S phase18: Bionic locale and UTF-8 adaptation (MIT, independent).
 *
 * Designed for Android/Bionic AArch64 ABI on the R36S:
 * - Bionic locale_t is NOT glibc locale_t. Guest pointers are never sent to host.
 * - Bionic mbstate_t is 8 bytes on LP64; its layout is not glibc mbstate_t.
 * - No commercial game code is executed. This is an experimental link fixture.
 * - A strict, private UTF-8 converter avoids mutating the host's locale.
 *
 * Neither __sF/FILE, Bionic stat, sysconf, syscall nor longjmp is mapped here.
 */
#define _GNU_SOURCE
#include "bionic_locale18.h"
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum { MAX_LOCALES=256 };
/* Android uses categories 0..12 except LC_ALL (6) in its bit-mask. */
enum { ANDROID_LOCALE_MASK=((1<<13)-1)&~(1<<6) };
typedef struct { int active, utf8; } GuestLocale;
static GuestLocale locale_slots[MAX_LOCALES];
static unsigned next_locale;
static pthread_mutex_t locale_lock=PTHREAD_MUTEX_INITIALIZER;
static int global_utf8=1;
/* Thread default is Bionic's LC_GLOBAL_LOCALE: pointer value -1. */
static _Thread_local void *tls_locale;
static int locale_supported(const char *s){
    return s && (!strcmp(s,"") || !strcmp(s,"C") || !strcmp(s,"POSIX") ||
                 !strcmp(s,"C.UTF-8") || !strcmp(s,"en_US.UTF-8"));
}
static int locale_is_utf8(const char *s){
    return !strcmp(s,"") || strstr(s,"UTF-8")!=NULL;
}
static void *guest_newlocale(int categories,const char *name,void *base){
    (void)base; /* Bionic intentionally ignores base locale. */
    if(!name || (categories&~ANDROID_LOCALE_MASK)){errno=EINVAL;return NULL;}
    if(!locale_supported(name)){errno=ENOENT;return NULL;}
    pthread_mutex_lock(&locale_lock);
    if(next_locale==MAX_LOCALES){
        pthread_mutex_unlock(&locale_lock);errno=ENOMEM;return NULL;
    }
    GuestLocale *slot=&locale_slots[next_locale++];
    slot->active=1;slot->utf8=locale_is_utf8(name);
    pthread_mutex_unlock(&locale_lock);
    return slot;
}
static void guest_freelocale(void *loc){
    if(!loc || loc==(void*)(intptr_t)-1)return;
    pthread_mutex_lock(&locale_lock);
    for(unsigned i=0;i<next_locale;i++){
        if(loc==&locale_slots[i]){
            locale_slots[i].active=0;break;
        }
    }
    pthread_mutex_unlock(&locale_lock);
}
static void *guest_uselocale(void *newloc){
    void *previous=tls_locale?tls_locale:(void*)(intptr_t)-1;
    if(newloc){
        if(newloc!=(void*)(intptr_t)-1){
            int valid=0;
            pthread_mutex_lock(&locale_lock);
            for(unsigned i=0;i<next_locale;i++)
                if(newloc==&locale_slots[i] && locale_slots[i].active){valid=1;break;}
            pthread_mutex_unlock(&locale_lock);
            if(!valid){errno=EINVAL;return NULL;}
            tls_locale=newloc;
        }else tls_locale=NULL;
    }
    return previous;
}
static char *guest_setlocale(int category,const char *name){
    if(category<0 || category>12){errno=EINVAL;return NULL;}
    pthread_mutex_lock(&locale_lock);
    if(name){
        if(!locale_supported(name)){
            pthread_mutex_unlock(&locale_lock);errno=ENOENT;return NULL;
        }
        global_utf8=locale_is_utf8(name);
    }
    char *result=global_utf8?"C.UTF-8":"C";
    pthread_mutex_unlock(&locale_lock);
    return result;
}
static int current_utf8(void){
    void *loc=tls_locale;
    if(!loc){
        pthread_mutex_lock(&locale_lock);
        int utf8=global_utf8;
        pthread_mutex_unlock(&locale_lock);
        return utf8;
    }
    int utf8=0;
    pthread_mutex_lock(&locale_lock);
    for(unsigned i=0;i<next_locale;i++)
        if(loc==&locale_slots[i] && locale_slots[i].active){utf8=locale_slots[i].utf8;break;}
    pthread_mutex_unlock(&locale_lock);
    return utf8;
}
/* Static Bionic-compatible struct lconv is initialized once. */
static struct lconv g_lconv;
static pthread_once_t g_lconv_once=PTHREAD_ONCE_INIT;
static void init_lconv(void){
    char *empty="";
    g_lconv.decimal_point=".";
    g_lconv.thousands_sep=empty;
    g_lconv.grouping=empty;
    g_lconv.int_curr_symbol=empty;
    g_lconv.currency_symbol=empty;
    g_lconv.mon_decimal_point=empty;
    g_lconv.mon_thousands_sep=empty;
    g_lconv.mon_grouping=empty;
    g_lconv.positive_sign=empty;
    g_lconv.negative_sign=empty;
    g_lconv.int_frac_digits=CHAR_MAX;
    g_lconv.frac_digits=CHAR_MAX;
    g_lconv.p_cs_precedes=CHAR_MAX;
    g_lconv.p_sep_by_space=CHAR_MAX;
    g_lconv.n_cs_precedes=CHAR_MAX;
    g_lconv.n_sep_by_space=CHAR_MAX;
    g_lconv.p_sign_posn=CHAR_MAX;
    g_lconv.n_sign_posn=CHAR_MAX;
    g_lconv.int_p_cs_precedes=CHAR_MAX;
    g_lconv.int_p_sep_by_space=CHAR_MAX;
    g_lconv.int_n_cs_precedes=CHAR_MAX;
    g_lconv.int_n_sep_by_space=CHAR_MAX;
    g_lconv.int_p_sign_posn=CHAR_MAX;
    g_lconv.int_n_sign_posn=CHAR_MAX;
}
static struct lconv *compat_localeconv(void){
    pthread_once(&g_lconv_once,init_lconv);
    return &g_lconv;
}
static long long guest_strtoll_l(const char *s,char **end,int base,void *loc){
    (void)loc;return strtoll(s,end,base);
}
static unsigned long long guest_strtoull_l(const char *s,char **end,int base,void *loc){
    (void)loc;return strtoull(s,end,base);
}
static long double guest_strtold_l(const char *s,char **end,void *loc){
    (void)loc;return strtold(s,end);
}
/* GuestState intentionally matches Bionic LP64 sizeof(mbstate_t)==8, but
 * uses a PRIVATE encoding. It is not passed to or from glibc conversion APIs. */
typedef struct {
    uint32_t code;
    uint8_t seen, required, reserved[2];
} GuestState;
_Static_assert(sizeof(GuestState)==8,"Android ARM64 mbstate_t must occupy 8 bytes");
static _Thread_local GuestState implicit_state;
static GuestState *state_ptr(void *ps){
    return ps?(GuestState*)ps:&implicit_state;
}
static void state_clear(GuestState *st){memset(st,0,sizeof(*st));}
static size_t illegal_utf8(GuestState *st){
    state_clear(st);errno=EILSEQ;return (size_t)-1;
}
/* Return consumed bytes of CURRENT call, 0 for NUL, -2 for incomplete,
 * -1/EILSEQ for invalid. Boundary/surrogate/overlong checks enforced. */
static size_t guest_mbrtowc(wchar_t *out,const char *str,size_t n,void *ps){
    GuestState *st=state_ptr(ps);
    if(!str){str="";n=1;}
    if(n==0)return (size_t)-2;
    size_t i=0;
    if(!st->required){
        unsigned char x=(unsigned char)str[i++];
        if(x==0){state_clear(st);if(out)*out=0;return 0;}
        if(x<0x80){if(out)*out=x;return 1;}
        if(!current_utf8())return illegal_utf8(st);
        if(x>=0xC2 && x<=0xDF){st->code=x&31;st->required=2;}
        else if(x>=0xE0 && x<=0xEF){st->code=x&15;st->required=3;}
        else if(x>=0xF0 && x<=0xF4){st->code=x&7;st->required=4;}
        else return illegal_utf8(st);
        st->seen=1;
    }
    while(i<n && st->seen<st->required){
        unsigned char x=(unsigned char)str[i++];
        if((x&0xC0)!=0x80)return illegal_utf8(st);
        st->code=(st->code<<6)|(x&0x3F);
        st->seen++;
    }
    if(st->seen<st->required)return (size_t)-2;
    uint32_t scalar=st->code;
    unsigned len=st->required;
    state_clear(st);
    if((len==2 && scalar<0x80) || (len==3 && scalar<0x800) ||
       (len==4 && scalar<0x10000) || scalar>0x10FFFF ||
       (scalar>=0xD800 && scalar<=0xDFFF))
        return illegal_utf8(st);
    if(out)*out=(wchar_t)scalar;
    return i;
}
static size_t guest_mbrlen(const char *s,size_t n,void *ps){
    return guest_mbrtowc(NULL,s,n,ps);
}
static int guest_mbtowc(wchar_t *out,const char *s,size_t n){
    if(!s)return 0;
    GuestState state={0};
    size_t result=guest_mbrtowc(out,s,n,&state);
    if(result==(size_t)-1)return -1;
    if(result==(size_t)-2){errno=EILSEQ;return -1;}
    return (int)result;
}
static size_t guest_wcrtomb(char *out,wchar_t wc,void *ps){
    GuestState *st=state_ptr(ps);
    state_clear(st);
    if(!out)return 1;
    uint32_t x=(uint32_t)wc;
    if(x<=0x7F){out[0]=(char)x;return 1;}
    if(!current_utf8() || x>0x10FFFF || (x>=0xD800 && x<=0xDFFF)){
        errno=EILSEQ;return (size_t)-1;
    }
    if(x<=0x7FF){
        out[0]=(char)(0xC0|(x>>6));
        out[1]=(char)(0x80|(x&63));return 2;
    }
    if(x<=0xFFFF){
        out[0]=(char)(0xE0|(x>>12));
        out[1]=(char)(0x80|((x>>6)&63));
        out[2]=(char)(0x80|(x&63));return 3;
    }
    out[0]=(char)(0xF0|(x>>18));
    out[1]=(char)(0x80|((x>>12)&63));
    out[2]=(char)(0x80|((x>>6)&63));
    out[3]=(char)(0x80|(x&63));return 4;
}
static size_t guest_mbsnrtowcs(wchar_t *dst,const char **src,
                               size_t nmc,size_t len,void *ps){
    if(!src || !*src){errno=EINVAL;return (size_t)-1;}
    GuestState *original=state_ptr(ps),copy;
    GuestState *st=original;
    if(!dst){copy=*original;st=&copy;len=(size_t)-1;}
    const char *p=*src;
    size_t bytes=0,output=0;
    while(bytes<nmc && output<len){
        wchar_t ch=0;
        size_t r=guest_mbrtowc(&ch,p+bytes,nmc-bytes,st);
        if(r==(size_t)-1){if(dst)*src=p+bytes;return (size_t)-1;}
        if(r==(size_t)-2){bytes=nmc;break;}
        if(r==0){
            if(dst){dst[output]=0;*src=NULL;}
            return output;
        }
        if(dst)dst[output]=ch;
        bytes+=r;output++;
    }
    if(dst)*src=p+bytes;
    return output;
}
static size_t guest_mbsrtowcs(wchar_t *dst,const char **src,size_t len,void *ps){
    if(!src || !*src){errno=EINVAL;return (size_t)-1;}
    return guest_mbsnrtowcs(dst,src,strlen(*src)+1,len,ps);
}
static size_t guest_wcsnrtombs(char *dst,const wchar_t **src,
                              size_t nwc,size_t len,void *ps){
    if(!src || !*src){errno=EINVAL;return (size_t)-1;}
    const wchar_t *p=*src;
    size_t out=0,i=0;
    GuestState *original=state_ptr(ps),copy;
    GuestState *st=original;
    if(!dst){copy=*original;st=&copy;len=(size_t)-1;}
    for(;i<nwc;i++){
        char encoded[4];
        size_t n=guest_wcrtomb(encoded,p[i],st);
        if(n==(size_t)-1){if(dst)*src=p+i;return (size_t)-1;}
        if(p[i]==0){
            if(dst && out<len){dst[out]=0;*src=NULL;}
            return out;
        }
        if(n>len-out)break;
        if(dst)memcpy(dst+out,encoded,n);
        out+=n;
    }
    if(dst)*src=p+i;
    return out;
}
static int guest_swprintf(wchar_t *dst,size_t len,const wchar_t *format,...){
    va_list ap;va_start(ap,format);
    int count=vswprintf(dst,len,format,ap);
    va_end(ap);return count;
}
typedef struct {const char *name;void(*fn)(void);} Entry;
#define E(name,fn) {name,(void(*)(void))(fn)}
static const Entry entries[]={
    E("setlocale",guest_setlocale),
    E("newlocale",guest_newlocale),
    E("freelocale",guest_freelocale),
    E("uselocale",guest_uselocale),
    E("localeconv",compat_localeconv),
    E("strtoll_l",guest_strtoll_l),
    E("strtoull_l",guest_strtoull_l),
    E("strtold_l",guest_strtold_l),
    E("strtold",strtold),
    E("swprintf",guest_swprintf),
    E("mbtowc",guest_mbtowc),
    E("mbrtowc",guest_mbrtowc),
    E("mbrlen",guest_mbrlen),
    E("mbsrtowcs",guest_mbsrtowcs),
    E("mbsnrtowcs",guest_mbsnrtowcs),
    E("wcsnrtombs",guest_wcsnrtombs),
    E("wcrtomb",guest_wcrtomb)
};
int lenneth_locale18_lookup(const char *name,uint64_t *out){
    if(!name||!out)return 0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++){
        if(strcmp(name,entries[i].name))continue;
        void(*fn)(void)=entries[i].fn;
        if(sizeof(fn)!=sizeof(*out))return 0;
        memcpy(out,&fn,sizeof(fn));
        return 1;
    }
    return 0;
}
int lenneth_locale18_selftest(void){
    int ok=1;
    uint64_t addr=0;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++)
        if(!lenneth_locale18_lookup(entries[i].name,&addr)||!addr)ok=0;
    if(lenneth_locale18_lookup("__sF",&addr) ||
       lenneth_locale18_lookup("syscall",&addr) ||
       lenneth_locale18_lookup("dl_iterate_phdr",&addr) ||
       lenneth_locale18_lookup("stat",&addr))ok=0;
    void *a=guest_newlocale(ANDROID_LOCALE_MASK,"C.UTF-8",NULL);
    void *b=guest_newlocale(ANDROID_LOCALE_MASK,"C",NULL);
    if(!a||!b||guest_newlocale(1<<6,"C",NULL)!=NULL ||
       guest_newlocale(1,"unsupported",NULL)!=NULL)ok=0;
    if(guest_uselocale(a)!=(void*)(intptr_t)-1 ||
       guest_uselocale(NULL)!=a)ok=0;
    const char *utf="\xc3\xa9!";
    wchar_t wide[16]={0};
    GuestState state={0};
    size_t result=guest_mbrtowc(&wide[0],utf,1,&state);
    if(result!=(size_t)-2 || state.required!=2 || state.seen!=1)ok=0;
    result=guest_mbrtowc(&wide[0],utf+1,1,&state);
    if(result!=1 || wide[0]!=0xe9 || state.required!=0)ok=0;
    if(guest_mbrlen("A",1,&state)!=1)ok=0;
    if(guest_mbrtowc(&wide[0],"\xc0\x80",2,&state)!=(size_t)-1 ||
       errno!=EILSEQ)ok=0;
    if(guest_mbrtowc(&wide[0],"\xed\xa0\x80",3,&state)!=(size_t)-1 ||
       errno!=EILSEQ)ok=0;
    char output[16]={0};
    if(guest_wcrtomb(output,(wchar_t)0xe9,&state)!=2 ||
       (unsigned char)output[0]!=0xc3 || (unsigned char)output[1]!=0xa9)ok=0;
    const char *source="\xc3\xa9!";
    memset(wide,0,sizeof(wide));
    memset(&state,0,sizeof(state));
    if(guest_mbsrtowcs(wide,&source,16,&state)!=2 ||
       source!=NULL || wide[0]!=0xe9 || wide[1]!='!')ok=0;
    source="abcd";memset(wide,0,sizeof(wide));
    if(guest_mbsnrtowcs(wide,&source,2,16,&state)!=2 ||
       strncmp(source,"cd",2) || wide[0]!='a' || wide[1]!='b')ok=0;
    const wchar_t *ws=L"\u00e9!";
    memset(output,0,sizeof(output));
    if(guest_wcsnrtombs(output,&ws,3,sizeof(output),&state)!=3 ||
       ws!=NULL || strcmp(output,"\xc3\xa9!"))ok=0;
    if(guest_swprintf(wide,16,L"n=%d",42)!=4 ||
       wcscmp(wide,L"n=42"))ok=0;
    if(guest_mbtowc(&wide[0],"\xc3\xa9",2)!=2 || wide[0]!=0xe9)ok=0;
    if(!guest_localeconv() || strcmp(guest_localeconv()->decimal_point,".") ||
       guest_localeconv()->frac_digits!=CHAR_MAX)ok=0;
    char *end=NULL;
    if(guest_strtoll_l("-123x",&end,10,a)!=-123 || *end!='x')ok=0;
    if(guest_strtoull_l("45x",&end,10,a)!=45 || *end!='x')ok=0;
    if(guest_strtold_l("1.5x",&end,a)!=1.5L || *end!='x')ok=0;
    if(guest_uselocale((void*)(intptr_t)-1)!=a)ok=0;
    if(!guest_setlocale(6,"C") || current_utf8() || guest_mbtowc(NULL,"\xc3\xa9",2)!=-1)ok=0;
    if(guest_uselocale(b)!=(void*)(intptr_t)-1 || current_utf8()!=0)ok=0;
    if(guest_uselocale(a)!=b || current_utf8()!=1)ok=0;
    if(!guest_setlocale(6,"C.UTF-8") || guest_uselocale((void*)(intptr_t)-1)!=a ||
       !current_utf8())ok=0;
    guest_freelocale(a);guest_freelocale(b);
    printf("[lenneth locale18] SELFTEST=%s mapped_symbols=%zu "
           "bionic_locale_handles=OPAQUE guest_mbstate_bytes=8 "
           "utf8_incremental_invalid_surrogate=CHECKED locale_host_isolated=1 "
           "commercial_execution=NO\n",
           ok?"PASS":"FAIL",sizeof(entries)/sizeof(entries[0]));
    return ok?0:18;
}
