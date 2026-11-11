# Lenneth R36S: Linux ARM64 port based on vpl_nx

> **Quer retomar o trabalho em outro chat?**
> Abra [CONTINUAR AQUI — checkpoint mais recente](CONTINUAR_AQUI.md)
> e [PROMPT para novo chat](PROMPT_RETOMADA_CHATGPT.md).
> Atualizamos esses dois arquivos ao final de cada etapa.
>
> **Último teste no dispositivo (recebido em 10/11/2026):** Teste 5 com
> logs COMPLETOS (snapshots atômicos). GLES2/GLES3 Mali-G31, áudio 44,1 kHz,
> JNI, assets e pthread **PASS**. Imports da engine Android ainda
> **BLOCKED: 59 relocações no jogo e 63 em libc++ (99 nomes únicos)**.
> Veja [inventário de pendências reais](PENDENCIAS_TESTE5.md).
>
> **Novo Teste 6 (FORTIFY + wide chars):** código ARM64 e testes
> sintéticos aprovados no [GitHub Actions 42941739463](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42941739463).
> Ainda NÃO validado com o jogo real no portátil. **Não jogável ainda.**


**Status: porting investigation / not playable yet.**

The original MIT project [delsonazevedo/vpl_nx](https://github.com/delsonazevedo/vpl_nx)
loads the *Android* AArch64 game engine (libMyGame.so) into a minimal Android-compatible
native environment for Nintendo Switch. It is **not** a fully recompiled,
open-source Lenneth engine.

## Verified facts

- Target: R36S dArkOSRE, AArch64 Cortex-A35, Mali-G31, SDL2/KMSDRM/EGL.
- Original vpl_nx: Cocos2d-x 3.x, **OpenGL ES 2.0** shaders, game loop capped
  around **30 FPS** to maintain CRIWARE/audio timing.
- Shared reusable work: JNI fake VM, Bionic/pthread imports, OpenSL ES backed
  by SDL2, AArch64 ELF relocation and dependency resolution, touchscreen/pad input.
- Switch-specific code that **must be ported**: libnx heap/virtual memory/SVC,
  NWindow/EGL surface, Joy-Con HID, libnx threads/time/tick and cache API.
- No commercial game assets or binaries are checked into this public repo.

## IMPORTANT: current user's APK is not the same build

A local private read-only ELF audit on 2026-11-10 detected
**7 definite collisions** between vpl_nx hard-coded function offsets and exported
functions in the user's libMyGame.so; this includes multiple movie-manager hooks
landing inside the sound effect renderer code. All such original offsets must be
considered invalid until independently mapped and validated.

The APK's exported Cocos2d JNI entry points *are* present. Some CRIMovieManager
entry points also have publicly named ELF exports, which permits creating
version-specific symbol-based bindings instead of blindly applying offsets.

Use the companion script to audit your own copy locally:

    python3 vplnx-r36s/tools/audit_vplnx_elf.py /private/libMyGame.so --report my-compat.tsv

Do not upload binaries, assets, DEX files, game patches or private reports to
public GitHub Actions.

## Actual porting order

1. Reuse the verified MIT source of vpl_nx, retaining its license and attribution.
   Keep proprietary libraries separate. Establish a *Linux AArch64* platform layer
   for memory mapping with strict W^X, without Nintendo syscalls.
2. Port ABI bridge: Bionic libc/C++/pthread, fake JNI methods, asset I/O,
   OpenSL ES/SDL2 audio. Resolve imports or fail closed before executing guest code.
3. Use native SDL2/KMSDRM + EGL + **GLES2 initially** on R36S. Test GLES3
   as an optional A/B setting only when the game renders correctly.
4. Dynamically resolve named native symbols and **verify every binary-specific
   code hook** against the exact .so build. Never apply Switch offsets to a different build.
5. Add physical gamepad, 4:3 rendering and save paths. Revalidate frame
   pacing at 30 FPS before considering higher rates.
6. After gates pass, attempt original JNI_OnLoad in a controlled child process
   (not in public CI), then surface initialization, nativeInit/nativeRender.

**Performance note:** this route avoids the PS1 recompilation's CPU software
rasterizer by trying the native Android/Cocos renderer, but no measured gain or
real-game compatibility on Cortex-A35 has yet been established.

MIT project source: https://github.com/delsonazevedo/vpl_nx


## Current Bionic/Linux import bridge: 2026-11-10

CI [ARM64 host integration](https://github.com/dehalbuq/ValkyrieRecomp/actions/runs/42818884922)
compiles the Linux platform, ELF loader, asset bridge, OpenSL ES/SDL2, JNI 1.6
and host import allowlist; tests *synthetic ELF modules only*. The loader's
allowlisted provider resolves suitable C functions, Android NDK assets, audio,
and the GLES2 core + OES map-buffer dispatch. Unsupported imports are recorded
and block executable mapping. Opaque Bionic pthread and FILE layouts are
**intentionally not forwarded** directly into glibc.

A **private read-only static symbol-name comparison** using the supplied Android
engine build reported **353 unique undefined symbols in libMyGame.so**:
- 85 names have corresponding exports in its accompanying libc++_shared.so;
- 177 other names are present in the Linux host allowlist;
- 91 names remain without providers (43 POSIX/stdio/file, 20 pthread,
  20 Bionic/ABI, 7 math/text/format and 1 other, using an approximate
  classification).

**These are NAME matches, not proven compatible runtime ABIs or executed
relocations.** Additional imports from libc++_shared.so itself, symbol
versions, private data symbols, TLS, native constructors, ART/JNI dynamic
registration, and actual GPU device compatibility still require validation.

Next engineering gates: (1) sandboxed Bionic file/stdio objects and saves,
(2) guest pthread mutex/cond/TLS object ABI, (3) Bionic C++ and exception/unwind
lifecycle, (4) test original two library link in a private nonexecuting runner,
and finally (5) isolated JNI_OnLoad when all compatibility gates pass.

**No modified APK libraries, game assets or third-party loader components
are stored in this repository or sent to the public CI.**
