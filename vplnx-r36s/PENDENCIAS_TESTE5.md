# Teste 5 — pendências de imports do Lenneth no R36S

> Diagnóstico recebido em **10/11/2026**. Fonte privada: arquivo de
> diagnósticos `VPLNX_R36S_DIAGNOSTICOS.tar(3).gz`,
> seção `ELF_REAL_IMPORTS.log`, **7710 bytes**. Somente nomes públicos de
> símbolos e contagens são registrados aqui, sem bibliotecas comerciais,
> APK, assets, ROMs, dumps de memória ou caminhos pessoais.

## Resumo medido no hardware real

| Módulo | Relocações sem provedor | Nomes únicos sem provedor |
|---|---:|---:|
| `libMyGame.so` — guest | **59** | **57** |
| `libc++_shared.so` — partner | **63** | **62** |
| **Total** | **122** | **99 no conjunto** |

Os módulos compartilham 20 símbolos pendentes; portanto, 57 + 62 não é
a quantidade única global. Os códigos JNI originais possuem as quatro
exports esperadas, mas o loader não executa construtores nem `JNI_OnLoad`.
`host_resolved=211` no motor e `host_resolved=70` em libc++.
`unsupported=0`, `relr=0`. `BIONIC_HOST_LINK=BLOCKED` e proteção
executável negada corretamente.

**Não confundir** `ELF_REAL_BASIC.log` (sem resolver imports do host,
registra 270+133 pendências artificiais) com `ELF_REAL_IMPORTS.log`
(com todos os provedores implementados e 59+63 pendências reais).

## Pendentes no motor (57 nomes)

```text
__android_log_assert
__cxa_atexit
__cxa_finalize
__open_2
__read_chk
__sF
__stack_chk_guard
__strcat_chk
__strchr_chk
__strncpy_chk
__vsnprintf_chk
__vsprintf_chk
clearerr
closedir
dl_iterate_phdr
dlclose
dlerror
dlopen
dlsym
exit
fclose
fdopen
ferror
fflush
fileno
fopen
fprintf
fputc
fread
fseek
fstat
ftell
fwrite
gmtime
localtime
longjmp
mkdir
open
opendir
pthread_attr_init
pthread_attr_setschedparam
pthread_attr_setschedpolicy
pthread_create
pthread_detach
pthread_getschedparam
pthread_getspecific
pthread_join
pthread_key_create
pthread_once
pthread_self
pthread_setspecific
remove
rename
setjmp
stat
system
vfprintf
```

## Pendentes na biblioteca C++ (62 nomes)

```text
__cxa_atexit
__cxa_finalize
__cxa_thread_atexit_impl
__open_2
__read_chk
__sF
__vsnprintf_chk
btowc
closelog
dl_iterate_phdr
fflush
fprintf
fputc
freelocale
fwrite
getc
localeconv
mbrlen
mbrtowc
mbsnrtowcs
mbsrtowcs
mbtowc
newlocale
openlog
pthread_create
pthread_detach
pthread_equal
pthread_getspecific
pthread_join
pthread_key_create
pthread_key_delete
pthread_once
pthread_self
pthread_setspecific
setlocale
strcoll
strerror_r
strftime
strtold
strtold_l
strtoll_l
strtoull_l
strxfrm
swprintf
syscall
syslog
ungetc
uselocale
vasprintf
vfprintf
wcrtomb
wcscoll
wcsnrtombs
wcstod
wcstof
wcstol
wcstold
wcstoll
wcstoul
wcstoull
wcsxfrm
wctob
```

## Priorização para a próxima fase

1. **Acessos verificados Bionic FORTIFY:** `__read_chk`,
   `__strcat_chk`, `__strncpy_chk`, `__strchr_chk`,
   `__vsnprintf_chk` e `__vsprintf_chk`. Implementar wrappers
   com testes reais de violação antes de expor no host. Depois validar
   quantas pendências diminuem no R36S.
2. **Conversões simples de texto e wide chars:** testar ABI compatível
   de `wcstol`, `wcstoul`, `wcstod`, `strcoll`, `swprintf`,
   etc. Não passar `locale_t` ou `mbstate_t` Android a glibc sem
   verificar layouts e ownership.
3. **Arquivos/saves:** introduzir `FILE*` Bionic com ponteiros opacos
   para objetos host em mapa seguro, FD/path jail para saves e assets.
   Não expor `open`/`mkdir`/`system` ou objetos glibc diretamente.
4. **Threads/TLS:** `pthread_create`, `pthread_join`,
   `pthread_key_*`, `pthread_once`, `pthread_self` etc exigem
   adaptadores específicos para a ABI Android.
5. **C++/TLS/ART:** `__cxa_atexit`, `__cxa_finalize`,
   `__cxa_thread_atexit_impl`, `__sF`, `__stack_chk_guard`,
   `setjmp/longjmp`, `dlopen/dlsym` e os serviços ainda não
   implementados; todos bloqueados até validação real.

A lista é um **snapshot de um teste real**, não uma garantia de que
qualquer função correspondente tenha ABI segura.
