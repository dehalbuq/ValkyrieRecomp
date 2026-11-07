# Valkyrie Profile: Lenneth (Android) — R36S Linux ARM64

**Experimental bootstrap — phases 1–15, with one-run multitest and controlled synthetic ELF execution. The commercial game does NOT run yet.**

This is a separate effort from the PS1 ValkyrieRecomp project. Its goal is a
native Linux AArch64 compatibility layer for the user's legitimate Android
**Square Enix version 1.0.7** of *Valkyrie Profile: Lenneth*.

## Why the approach is viable enough to investigate

The Android v1.0.7 APK contains:

- `lib/arm64-v8a/libMyGame.so` (11.11 MiB, AArch64)
- `lib/arm64-v8a/libc++_shared.so`
- `lib/arm64-v8a/libRMS.so`
- Game data in `assets/discData.cpk`, `movieData.cpk`,
  `soundData.cpk`
- Cocos2d-x JNI entry points, GLESv2/EGL and OpenSL ES dependencies.

The R36S's dArkOSRE firmware exposes the Mali-G31 through OpenGL ES 3.2.
The owner has also personally run the Android game with 1 GiB of RAM.
None of that guarantees this Linux port works: Android Bionic, C++ ABI,
JNI, asset/audio hooks and runtime memory allocations differ.

We studied the architecture of the open-source Nintendo Switch wrapper
[delsonazevedo/vpl_nx](https://github.com/delsonazevedo/vpl_nx)
(MIT license). It supports **a different 1.1.9 Crunchyroll build**
with hard-coded internal game offsets. No source files were copied from
that wrapper into this initial phase-1 bootstrap. Offset values from
that version must never be applied blindly to v1.0.7.

## What phase 1 actually does

`lenneth-probe --inspect path/to/libMyGame.so`:
- Reads only the ELF64 metadata (program headers, DT_NEEDED and exported
  JNI / imported GLES2 symbol names).
- Rejects malformed, oversized, non-AArch64 inputs.
- **Never** loads or executes the Android library.

`lenneth-probe --video --seconds 30`:
- Opens a Linux SDL2 EGL/OpenGL ES 2.0 context directly.
- Draws a simple triangle with GPU shaders.
- Reports the GPU driver, audio interface and controller identity.
- Tests Start+Select held for 2.5 seconds; **A** confirms exit,
  **B** resumes. F10 on a keyboard opens the exit confirmation.
- Stops automatically after 30 seconds so a missing gamepad mapping
  cannot trap the user in the test.

**This is not an Android compatibility runtime yet.** The commercial
`libMyGame.so` is never executed during phase 1.

## Install on dArkOSRE / R36S

1. From the successful [GitHub Actions workflow](../../actions/workflows/lenneth-r36s.yml),
   download the artifact `Lenneth-Android-R36S-ARM64-Phase1`.
2. Unzip the inner `lenneth-r36s-phase1-arm64.zip` to your ROM partition,
   keeping the `ports/` folder structure intact.
3. In EmulationStation, open **Ports > Lenneth Android R36S Test**.
4. A GPU-rendered triangle should appear, then return to Ports after 30 s.
5. Share `ports/lenneth-r36s/lenneth-r36s.log` if there are errors.

For comparison with the actual commercial ARM64 ELF metadata, the user
may optionally place their own legitimately obtained `libMyGame.so` and
`libRMS.so` in `ports/lenneth-r36s/`. The wrapper logs dependency
names only and does not try to execute or modify these libraries.

No game files are provided, bundled, downloaded or uploaded by this
project. Keep the full APK and original data private.

## Build on native Linux ARM64

```sh
sudo apt-get install cmake ninja-build pkg-config libsdl2-dev libgles2-mesa-dev
cmake -S lenneth-r36s -B build-lenneth -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-lenneth
./build-lenneth/lenneth-probe --inspect /usr/bin/true
./build-lenneth/lenneth-probe --video --seconds 20
```

The public CI uses `ubuntu-24.04-arm` and creates a legally
redistributable PortMaster test ZIP, not an APK or a playable release.

## Next phase: native Android .so loader

Adapt a Bionic/JNI/Android asset/import compatibility layer with proper
licensing and attribution for any borrowed open-source code. Plan to use
SDL2 for input and OpenSL ES compatibility and EGL/GLES for the game.
Find or safely resolve version-specific entry points in the **v1.0.7**
library. Test allocation and memory usage on the R36S's 1 GiB RAM.

Only after the loader initializes the game should we integrate
Cocos2d-x nativeInit/nativeRender and preserve mobile skip-scene
controls and savestates. No FPS, boot, playable compatibility or video
codec success claim until demonstrated by a physical R36S test.

## Legal and license

The phase-1 source code is independently written for this project and
distributed under MIT (see `LICENSE`). Future use of third-party
code must retain its original license and credit. The Valkyrie Profile
name, game, artwork, commercial code, and assets belong to their
respective owners.

## Phase 2: Android AArch64 ELF map preflight

The owner's private Square Enix **v1.0.7** libraries were inspected locally.
They were **not** committed or uploaded to GitHub:

| Library | Architecture | Notable facts |
| --- | --- | --- |
| `libMyGame.so` | ARM64 (NDK r21e) | ~11.1 MiB, valid dynamic ELF, exposes `JNI_OnLoad`, `cocos_android_app_init`, Cocos2d-x native init/render/touch methods |
| `libc++_shared.so` | ARM64 (NDK r21e) | ~0.87 MiB, Android C++ standard library |
| `libRMS.so` | ARM64 | ~0.64 MiB, unusual ELF program-header entry size (17 instead of 56); `readelf` rejects the program-header table; this is **not** proof the library is required or malware |

`libMyGame.so` imports 353 unique external symbols, including 96
`gl*` functions, 11 `AAsset*` functions, OpenSL ES/JNI and Bionic pthread.
It also has a large relocation table: 12,357 RELATIVE, 18,605 ABS64,
3,178 GLOB_DAT and 9,245 JUMP_SLOT entries across its two tables.
These are real native Android dependencies, **not** runnable by glibc alone.

The new `--preflight <libMyGame.so>` subcommand:

1. Parses and validates the ARM64 ELF program headers.
2. Reserves a bounded anonymous Linux `mmap` (never executable) and
   copies `PT_LOAD` segments, including zero-initialized BSS.
3. Applies only `R_AARCH64_RELATIVE` address fixups.
4. Reports `ABS64`, `GLOB_DAT`, `JUMP_SLOT` and other unresolved
   relocations, RSS and missing dependent-provider work.
5. Unmaps everything. **It does not call constructors, JNI_OnLoad or
   game code**, does not bypass license checks and requires no data CPKs.

**How to test on R36S:** download the phase-2 artifact
`Lenneth-Android-R36S-ARM64-Phase2` from the Actions tab and unzip
the nested `lenneth-r36s-phase2-arm64.zip` into `ports/`.
Place only your own three Android `.so` files into
`ports/lenneth-r36s/`, then open **Lenneth Android R36S Test**.
The existing GLES2/gamepad/audio test still appears and exits normally.
Share only `ports/lenneth-r36s/lenneth-r36s.log` (no commercial files).

The preflight intentionally reports `libRMS.so` as structurally
unsupported while its header is malformed. Do **not** patch or
normalize the file header merely to force it to pass the parser.

Next engineering milestone: replace the unresolved symbol counts with a
safe provider-resolution table, port the Bionic ABI mapping and minimal
JNI/AAssetManager scaffolding for v1.0.7 and validate on physical
hardware **before enabling constructors or executable mappings**.

## Phase 3: cross-library AArch64 relocation linking

The physical R36S validated the phase-2 Linux mapping with the genuine
Square Enix v1.0.7 `libMyGame.so` and `libc++_shared.so`:

- Game: `relative_applied=12357`; `external_pending=12423`,
  `absolute_pending=18605`, mapped image 12.57 MiB.
- Android libc++: `relative_applied=1187`,
  `external_pending=543`, `absolute_pending=1778`.
- GLES2 3.2 / Mali-G31, SDL2 gamepad, ALSA and Start+Select still work.
- `libRMS.so` was **not listed** in the game's `DT_NEEDED` set of ten
  libraries, so we defer investigating its atypical program header.

An offline symbol-name comparison of these two original ARM64 libraries
found that **85 of 353 unique imported symbols in `libMyGame.so`**
are provided by `libc++_shared.so`. The other ~268 distinct imported
symbols include **97 GLES2/EGL**, **20 Bionic pthread**, **11 Android
asset-manager**, **5 OpenSL ES**, **3 Android logging**, **2 C++ ABI** and
**130 libc/libm/dl or Bionic helper** names. These are *API categories*,
not claims that an ABI-safe Linux implementation is already available.

The new `--link-plan libMyGame.so libc++_shared.so` command:

- Simultaneously maps **both original Android ELF64 modules** into
  separate, bounded, read/write-only Linux memory ranges.
- Parses their dynamic symbols and relocation tables.
- Applies `R_AARCH64_RELATIVE` and in-module
  `R_AARCH64_ABS64/GLOB_DAT/JUMP_SLOT` references.
- Applies cross-module symbol-address references resolved through
  `libc++_shared.so`; reports unresolved Android-provider references.
- Displays distinct missing symbol categories for actual Bionic/JNI/
  GLES2/OpenSL/AAssetManager implementation planning.
- Exits and frees the mappings. **Neither module is executable**; no
  JNI_OnLoad, init arrays, game code, billing or asset access occurs.

To test, install the latest phase-3 PortMaster artifact from
[GitHub Actions](../../actions/workflows/lenneth-r36s.yml) and place
your own licensed `libMyGame.so` and `libc++_shared.so` in
`ports/lenneth-r36s/`. Launch the existing Ports menu test.
The log should show `[lenneth link] PHASE3=PASS` followed by the
same 30-second GPU test; share **only** `lenneth-r36s.log`.

### Next step

Implement a Linux import-provider registry and Bionic-compatible
memory, thread, file and TLS wrappers. Prefer correctly typed
native GLES2/EGL bindings, and implement JNI/AAssetManager/OpenSL
with the Cocos2d-x interface only as those dependencies are validated.
The full loader must initialize constructors only after all
required relocations, memory-protection and ABI checks pass.
No performance or playable-game claim can be made at this stage.

## Phase 4: resolve Linux-native GLESv2/EGL entry points, safely

From the real phase-3 R36S test:

- `libMyGame.so`: 43,385 relocation sites checked, 12,357 RELATIVE,
  29,841 internal, **917** bound to the Android libc++ library,
  270 relocations referring to unavailable host APIs.
- `libc++_shared.so`: 3,508 relocation sites checked, with 133
  unavailable host relocation sites.
- 268 distinct imported names in the game and 131 in libc++ remain
  unimplemented. The game-side categories include **97 GLES2/EGL**.
- These data are from the *actual dArkOSRE R36S runtime log*.
  "Cross-library relocations" counts relocation sites, not unique names.

The new `lenneth-probe --host-plan libMyGame.so libc++_shared.so`:

1. Maps and links both *privately supplied* original ARM64 ELF files
   in non-executable, read/write-only memory, as in phase 3.
2. Loads **host** `libGLESv2.so[.2]` and `libEGL.so[.1]` from Linux.
   For imports named `gl*` and `egl*`, queries exports with
   `dlsym`; for GLES OES extensions also tries
   `eglGetProcAddress`.
3. Writes obtained native GLES/EGL addresses to guest import
   relocation *data slots only*. Logs `host_gles_slots` and
   `graphics_resolved_slots` separately from unresolved Android APIs.
   This does **not** call OpenGL functions from the game or guarantee
   compatibility with an eventual live GLES context.
4. **Never** resolves `pthread_*`, libc, Bionic TLS, C++ ABI or Android
   NDK symbols directly to Linux glibc merely because names match.
   Bionic's object layouts / calling conventions may differ.
5. Does not run ELF initializers or `JNI_OnLoad`, and never maps game
   code executable. This remains a **non-playable preflight**.

### Test

Download `Lenneth-Android-R36S-ARM64-Phase4` from the
[phase-4 workflow](../../actions/workflows/lenneth-r36s.yml) and extract
its inner `lenneth-r36s-phase4-arm64.zip` to your existing ROM
partition's `ports/` directory. Preserve your own private
`libMyGame.so` and `libc++_shared.so` in
`ports/lenneth-r36s/`; **nothing from the commercial APK is bundled
or uploaded**. Launch **Lenneth Android R36S Test**, then send
`ports/lenneth-r36s/lenneth-r36s.log`.

Look for both `[lenneth link] PHASE3=PASS` and
`[lenneth host] PHASE4=PASS`. The second line contains native GL
resolution counts. A low or zero count indicates a missing graphics
provider or unsupported host extension: do not guess about real
game performance.

Next: Bionic compatible libc/threads/TLS, AAssetManager, minimal
Cocos2d JNI, OpenSL ES, ELF memory protection and safe game launch.
Respect licenses and original commercial data ownership.

## Phase 5: conservative libc/libm symbol provider

Physical R36S phase-4 result: **all 97 OpenGL ES/EGL guest import
sites** in `libMyGame.so` were populated from native host
`libGLESv2` and `libEGL`. The engine still had 171 distinct
unresolved symbols, and Android `libc++_shared.so` had 131. Native
GLES2, ALSA, gamepad and the Start+Select exit menu remained functional.

The phase-5 `--portable-plan libMyGame.so libc++_shared.so` mode adds
an explicit small, audited set of C functions from native Linux
`libc.so.6` and `libm.so.6`. The initial allowlist is restricted to
numeric math, portable string and memory operations, safe allocator
functions and basic string-to-number conversions. It **does not**
automatically resolve C functions merely because Android and glibc
export the same symbol name.

**Never** bridge these without separate ABI-aware wrappers:
`pthread_*` mutex/cond object layouts, `FILE*`/`__sF`,
`setjmp`/`longjmp`, Bionic `__errno`/TLS, locale objects,
Bionic fortify wrappers, `stat`, Java/JNI, OpenSL ES and AAssetManager.
The pending symbols remain visible by category in the phase-5 log.

Each audited symbol address is placed into a relocation data slot
in a read-write **non-executable** Android ELF mapping. This validates
linking, **not actual calls**, and doesn't prove runtime safety
in the full game. Game initialization remains disabled.

### Test on R36S

Download the latest GitHub Actions artifact
`Lenneth-Android-R36S-ARM64-Phase5` from this branch.
Extract the nested `lenneth-r36s-phase5-arm64.zip` into the ROM
partition and keep your own `libMyGame.so` and
`libc++_shared.so` private in `ports/lenneth-r36s/`.
The game APK, commercial libraries and CPK data are never included
in GitHub build artifacts. Run the existing Ports test and send
`ports/lenneth-r36s/lenneth-r36s.log` only.

Expected additional log lines:
```
[lenneth portable] libc_provider=loaded libm_provider=loaded ...
[lenneth portable] PHASE5=PASS graphics_slots=... libc_libm_slots=... ...
```

Next: typed wrappers for Android Bionic threads and TLS, JNI and
asset APIs. Enabling execution of any commercial code will require
full relocation/provider coverage, appropriate memory protections
and explicit runtime controls after the ABI compatibility is assessed.

## Phase 6: Android AAssetManager read-only shim

The actual **R36S phase-5 log** confirmed:
- 97 graphics relocation sites bound to native GLES2/EGL.
- 70 portable libc/libm relocation sites resolved (49 in the game,
  21 in the original Android libc++ runtime).
- The game then has 122 distinct host symbols still unresolved,
  with another 111 in libc++ (233 entries across the two lists,
  before deduplicating names across libraries).
- GPU, joystick, ALSA, menu and native ELF checks remain stable.

**New source:** `src/asset_provider.c` and `.h`. Phase 6 includes
typed Android NDK-style methods for `AAssetManager_open`,
`AAsset_read`, `AAsset_seek`, `AAsset_getLength`,
`AAsset_getBuffer`, directory enumeration, file descriptors,
and other AAsset methods. These access **extracted, local, ordinary
files** in `ports/lenneth-r36s/assets/` — they do not read compressed
APK or ZIP entries directly. Compressed entries must be extracted
locally by the game owner and are not provided or uploaded here.

The implementation opens the local root with `openat`/O_NOFOLLOW,
blocks absolute paths, `../` traversals and symlink traversal, and
never writes game assets. `AAsset_getBuffer` uses private,
read-only `mmap` rather than copying a possibly huge CPK into RAM.
Real game compatibility, compressed assets, archive decoding,
Java-facing JNI and other lifecycle work remain unverified.

New command:
```sh
./lenneth-probe --asset-plan libMyGame.so libc++_shared.so
```

This maps the two commercial libraries **non-executable**, binds
the native GLES/EGL + audited libc/libm providers, and now also
connects the recognised AAsset function import *slots* to the
local shim. It logs `host_asset_slots` and
`[lenneth asset] PHASE6=PASS`.

An independent test that uses only a tiny fixture file (no proprietary
game bytes) can be run as:
```sh
./lenneth-probe --asset-selftest /path/to/test-assets sample.txt
```

**How to test on R36S:** download the workflow artifact named
`Lenneth-Android-R36S-ARM64-Phase6`, extract its inner ZIP
`lenneth-r36s-phase6-arm64.zip` into the ROM partition, and preserve
your own `libMyGame.so` and `libc++_shared.so` privately at
`ports/lenneth-r36s/`. Launch Ports → Lenneth Android R36S Test.
The script creates an *empty* `assets/` folder if absent. You
**do not need** the 900 MB APK or CPK files for this diagnostic.
Send only `ports/lenneth-r36s/lenneth-r36s.log`.

This is a **non-executing import and stand-alone asset shim test**,
not gameplay or a full Android emulation runtime. Future work:
Bionic thread object layouts/TLS, JNI Cocos2d glue, OpenSL ES
runtime and careful ELF memory protection before execution.

## Phase 7: Android NDK logging bridge and JNI entrypoint inventory

The actual dArkOSRE R36S phase-6 test passed:
- All **11/11 AAssetManager import slots** in the private Android
  `libMyGame.so` were resolved to our local read-only adapter.
- Native GLES/EGL: 97 bound slots. Portable libc/libm: 70.
- Unresolved unique guest names: game 111, Android libc++ 111.
- Mali-G31 GLES3.2, SDL2 controls, ALSA and Start+Select remain working.
- No commercial game code has been executed.

Phase 7 adds `src/android_log_provider.c`, a typed implementation of:
`__android_log_write`, `__android_log_print`,
`__android_log_vprint`, `__android_log_assert` and
`android_set_abort_message`, writing bounded, sanitized messages to
the PortMaster test log (stderr). The assert function intentionally
calls `abort()`; it is **never** invoked by the synthetic self-test.

The new `--logging-plan libMyGame.so libc++_shared.so` mode performs
all prior *non-executing* relocations and also binds only these named
Android NDK logging functions. It additionally checks for exported
`JNI_OnLoad`, `cocos_android_app_init`, and three Cocos2d
`Cocos2dxRenderer` JNI entrypoints, reporting
`present=0|1 invoked=0`. **This does not initialize Java/JNI or
execute the Cocos2d-x engine.**

The separate `--logging-selftest` mode invokes the logging wrappers
with synthetic strings only; it does **not** invoke imported code.

### Running phase 7 on your R36S

Download the artifact named `Lenneth-Android-R36S-ARM64-Phase7`
from the Actions workflow in this branch; extract the inner
`lenneth-r36s-phase7-arm64.zip` to the ROM partition as usual.
Keep only your own locally sourced `libMyGame.so` and
`libc++_shared.so` in `ports/lenneth-r36s/`. No asset CPKs are
required for this test. Run Ports → Lenneth Android R36S Test.
Send only `ports/lenneth-r36s/lenneth-r36s.log`.

Look for `[lenneth android-log] PHASE7=PASS`,
`host_log_slots=...`, and `[lenneth JNI] bridge=...`.
Existing video, ALSA, controls and exit tests remain intact.

### Remaining critical runtime work

Bionic pthread mutex/cond/tls object representation, signal and libc
ABI glue, C++ constructors and exception lifecycle, JNI fake runtime,
OpenSL ES/CRIWARE audio and correct executable ELF permissions. The
launch of the original ARM64 binary is **not** enabled yet. No claim
is made about playable performance, licensing behavior, or movie/codec
support.

## Phase 8: fast ALL-IN-ONE testing

After the physical R36S phase-7 test the owner requested fewer manual
build/test iterations. The log confirms all four Android-logging slots
were bound (three in \`libMyGame.so\`, one in \`libc++_shared.so\`), in
addition to 97 GLES2/EGL, 70 audited libc/libm and 11 AAsset references.
At phase 7 there are 108 unresolved imported symbol names in the game
and 110 in Android libc++, counted per library. All real game
instructions have remained **unexecuted**.

Rather than loading and remapping the two original ARM64 libraries in
five separate processes (phases 3, 4, 5, 6 and 7), the new
\`lenneth-probe --batch libMyGame.so libc++_shared.so\`:

- Maps both ARM64 ELF libraries **only once** in non-executable memory.
- Binds cross-library references, native GLES2/EGL and restricted
  libc/libm C functions, plus our local AAsset and Android log shims.
- Lists the original Cocos2d/JNI *exports*, without invoking them.
- Writes **all still-unresolved guest symbol names** to
  \`lenneth-blockers.tsv\`, grouped by module and ABI category with a
  weak/strong binding flag (no binary bytes, pointers or addresses).
- Runs read-only AAsset functionality on a **synthetic temporary
  fixture** and Android logging wrapper self-tests using a synthetic
  message.
- Then tests SDL2 KMSDRM GPU, ALSA audio device and joystick on the R36S
  for 15 seconds (previously 30). The overlay still allows
  held Start+Select then A to exit early.
- Creates \`lenneth-suite-summary.txt\` and one short
  \`lenneth-r36s.log\` (prior log backed up).
- Tries to package **only these three TEXT files** into
  \`lenneth-r36s-diagnostics.zip\` (tar.gz fallback if zip is absent).
  No private Android libraries or commercial CPK assets are included.

**Simple user workflow**

1. Download GitHub Action artifact named
   \`Lenneth-Android-R36S-ARM64-Multitest-Phase8\`.
2. Extract its inner \`lenneth-r36s-multitest-phase8-arm64.zip\` into
   the existing ROM partition, preserving \`ports/\`.
3. Preserve your own private \`libMyGame.so\` and
   \`libc++_shared.so\` in \`ports/lenneth-r36s/\`. For this
   preflight, the 900 MB commercial game assets are not needed.
4. Run **Ports → Lenneth Android R36S Test** **one time**.
5. Send only \`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\` to
   help guide several ABI changes at once. If ZIP tools are missing,
   send the three .txt/.tsv/.log files separately.

The previous detailed tools \`--link-plan\`, \`--host-plan\`,
\`--portable-plan\`, \`--asset-plan\` and \`--logging-plan\`
remain available. To run all those verbose legacy modes, set
\`LENNETH_FULL_DIAG=1\` in the launcher environment. The **default**
is much shorter and performs only one full guest mapping/link pass.

For GitHub CI (no GPU display), \`--batch-headless\` runs the same
non-executing symbol resolution and shim self-tests using **synthetic
ELF fixtures** and skips the physical display/audio/gamepad portion.
An ARM64 CI success does not replace a real dArkOSRE R36S test.

### Important compatibility limits

A fully executable Android compatibility runtime has **not** been
implemented. Resolving symbol addresses is not proof that the
Android Bionic and Linux glibc calling conventions/objects match.
Android pthread mutex/cond and TLS, JNI object models, OpenSL ES
and other system APIs remain unresolved. Future development can
now prioritize and validate multiple independent ABI adapters using
the complete blocker inventory in one iteration, with targeted
unit-tests in CI before a combined R36S physical run.

## Phase 9: accelerate Bionic/thread and portable C ABI coverage in one run

The owner's real R36S phase-8 ZIP passed every synthetic and hardware
diagnostic. Its complete \`lenneth-blockers.tsv\` contained **218 pending
guest-symbol entries across two modules**, or **180 distinct names**
after deduplicating the modules:

| unresolved category | entries across both modules |
| --- | ---: |
| Bionic/libc/libm/libdl | 165 |
| Bionic pthread | 43 |
| OpenSL ES | 5 |
| C++ ABI / destructor lifecycle | 5 |

**Phase 9 does NOT execute Android commercial game code.**

Changes now tested together:

1. Added \`bionic_threads.c/.h\`, an **experimental and bounded**
   Android-pthread compatibility shim. Guest mutex/condition/once/
   mutex-attribute/thread-attribute pointers are used only as opaque
   lookup keys, while the real pthread state is in a native Linux
   registry. A thread/TLS/condition/mutex/once fixture runs using
   independently written, synthetic C code, not the game.
2. The shim can provide named Android \`pthread_*\` function addresses
   during the **non-executable** ELF relocation inventory, while
   preserving the distinction between guest and host data layouts.
3. Expanded the conservative C allowlist with basic math, ctype,
   POSIX time and numeric-formatting functions. Passing the linker
   does not guarantee compatible behavior of the game at runtime.
4. All prior tests (GLES2/EGL, libc/libm, AAsset, Android log, JNI
   export listing) and the new pthread fixture run in the **same**
   R36S launch. The two ARM64 libraries are mapped only once.
5. The report ZIP still contains just \`lenneth-r36s.log\`,
   \`lenneth-suite-summary.txt\` and \`lenneth-blockers.tsv\`.
   It does not contain any game library, asset, DRM key or CPK.

### Cautions

- This is NOT a completed Bionic pthread ABI emulation.
- Android mutex static-recursive/error-check initializer values are
  not interpreted yet; some scheduling/cond-attr/cancellation features
  are unsupported, and the 2,048-object registry is deliberately
  capped. Further target-specific tests are required.
- No dynamic guest constructors, \`JNI_OnLoad\`, graphics or audio engine
  code run. There are still unresolved imported functions and object
  symbols. **Do not interpret the reduced blocker count as playable
  progress until actual controlled game startup is demonstrated.**
- GNU libstdc++ exceptions, \`__cxa_*\` destructor lifecycle, Bionic
  \`FILE*\`, \`__errno\`, \`__sF\`, OpenSL ES/CRIWARE and Java/JNI
  remain future compatibility work.

### Testing on your R36S

Download the \`Lenneth-Android-R36S-ARM64-Multitest-Phase9\` workflow
artifact; extract its inner
\`lenneth-r36s-multitest-phase9-arm64.zip\` to the same ROM partition.
Preserve your private \`libMyGame.so\` and \`libc++_shared.so\`
in \`ports/lenneth-r36s/\`. Launch **Ports > Lenneth Android R36S
Test** once. Send back only
\`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\`.

Expected new lines (counts depend on actual firmware/libraries):

\`\`\`
[lenneth bionic-thread] SELFTEST=PASS ...
[lenneth bionic-thread] PHASE9_LINK=PASS ...
[lenneth suite] BATCH_LINK=PASS ...
[lenneth suite] FINAL=PASS ...
\`\`\`

The release still does NOT contain game content. The owner must keep
commercial libraries on their own hardware. This is a development
diagnostic, not a playable PortMaster release.

## Phase 10: multiple fortified Bionic ABI adapters in the same batch

The physical dArkOSRE phase-9 ZIP supplied by the owner shows:

- **Overall diagnostic: PASS** (asset fixture, logging, pthread/cond/
  TLS, linking/JNI inventory, GLES2, ALSA and gamepad).
- Guest link sites bound: **97 GLES/EGL + 104 audited libc/libm +
  11 AAsset + 4 Android logging + 44 pthread = 260 sites**.
- **141 unresolved import entries**, 63 from the original game and 78
  from Android libc++; 124 unique names after cross-module deduplication.
- Remaining categories: 131 Bionic/libc/libm/dl, 5 OpenSL ES,
  5 C++ ABI. The phase-8 comparison was 218 unresolved entries.
- The JNI inventory reports JNI_OnLoad and Cocos2dxRenderer nativeInit/
  nativeRender/touch entrypoints exported, **not invoked**. The original
  Android game engine has **never** executed under our loader.

Phase 10 adds \`src/bionic_fortify.c\` / \`.h\`: independently written,
typed Linux wrappers for eleven named Android Bionic functions:

\`__strlen_chk\`, \`__memcpy_chk\`, \`__memmove_chk\`, \`__strcat_chk\`,
\`__strncpy_chk\`, \`__strchr_chk\`, \`__read_chk\`, \`__vsnprintf_chk\`,
\`__open_2\`, \`__stack_chk_fail\` and \`__errno\`.

The signatures were compared against Android Open Source Project
[Bionic's fortify implementation](https://android.googlesource.com/platform/bionic/+/5f112cd7564f092c93c877e526d269ec68d47922/libc/bionic/fortify.cpp)
and [errno declarations](https://android.googlesource.com/platform/bionic/+/main/libc/include/errno.h).
The independently written wrappers abort on invalid bounds as the
real fortified API does, rather than ignoring a fortified check.

These are **non-executing import-address bindings**, and our unit-test
calls them only with synthetic inputs. The following complex ABIs are
NOT declared supported: Bionic FILE* and \`__sF\`, glibc FILE stream
conversion, Android dynamic library namespace, \`dl_iterate_phdr\`,
\`setjmp/longjmp\`, complex locale/stdio state, C++ destructor registration,
OpenSL ES engine and full Java/JNI. Bionic \`__stack_chk_guard\` is an
imported **data object**, and is deliberately not mapped as a function.

The single-launch Phase-10 R36S batch performs *all* previous tests
and adds a direct FORTIFY/errno fixture and the eleven optional
import-symbol candidates, using one guest ELF mapping cycle.

**Installation:** download \`Lenneth-Android-R36S-ARM64-Multitest-Phase10\`
from the GitHub Actions workflow and extract its inner
\`lenneth-r36s-multitest-phase10-arm64.zip\` to the ROM partition.
Keep your original \`libMyGame.so\` and \`libc++_shared.so\` private
in \`ports/lenneth-r36s/\`. No game data is included or uploaded.
Launch **Ports → Lenneth Android R36S Test** once, then send only
\`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\`.

Look for:
\`\`\`
[lenneth fortify] SELFTEST=PASS ...
[lenneth fortify] PHASE10_LINK=PASS ...
[lenneth suite] FINAL=PASS ...
\`\`\`

Important: success proves only *independent diagnostic fixtures and
relocation-address resolution*. It does **not** validate execution of
original game code with this combined Android/Linux ABI.

### After Phase 10

Focus on isolated C++ destruction/constructor lifecycle, safe libc
FILE* and file-backed asset access, Bionic-specific dynamic linking and
OpenSL ES before the first controlled executable-map experiment. These
will need real executable/ABI testing on independent synthetic Android
ARM64 libraries, not just symbolic relocation counts.

## Phase 11: verified AArch64 synthetic execution and C++ lifecycle ABI

The physical R36S phase-10 report confirmed \`FINAL=PASS\`:
- Guest library host-import binding sites: **277 total** —
  97 GLES/EGL, 104 audited libc/libm, 11 AAsset, 4 Android logging,
  44 pthread, and 17 FORTIFY.
- **124** still-unresolved guest import entries (53 in \`libMyGame\`,
  71 in \`libc++_shared\`), representing **113 distinct symbol names**.
- Remaining: **114** Bionic/libc/libm/dynamic-linker entries,
  **5** C++ ABI destructor-lifecycle entries, **5** OpenSL ES entries.
- Original private ARM64 game and C++ libraries still mapped RW
  **NON-EXECUTABLE** and remain uncalled. JNI exports were found but
  not invoked.

### The new combined improvements

1. C++ destructor registration: the three exported Itanium ABI
   interfaces \`__cxa_atexit\`, \`__cxa_finalize\` and
   \`__cxa_thread_atexit_impl\` now have independent, privately named
   Linux-backed wrapper functions. The global bounded destructor list
   and per-thread cleanup undergo independent synthetic LIFO tests.
   The source is in \`src/cxx_runtime.c\`.
2. During normal \`--batch\` inspection, the guest ELF relocator maps
   these three C++ ABI symbols alongside our earlier graphics,
   FORTIFY, asset, pthread and logging providers. The original
   proprietary code is **still not executed**.
3. A separate, tightly gated \`--execute-synthetic\` mode implements
   the first **actual execution of ARM64 code by our loader**. It
   accepts ONLY TWO independently authored test libraries with the
   exact synthetic marker, no DT_NEEDED, no constructors, no
   unresolved imports and no writable+executable segments. It then
   applies ELF64 relocations, invalidates the AArch64 instruction
   cache, changes segment page permissions with \`mprotect\` to
   enforce W^X, and calls the test's known \`JNI_OnLoad\` export.
   The export calls the other test library's function and is
   expected to return the integer **77**.
4. GitHub Actions on a **native Ubuntu ARM64 runner** has already
   executed this purely synthetic \`JNI_OnLoad\` successfully with
   \`[lenneth synthetic] EXECUTION=PASS\`. Negative tests confirm
   that libraries without matching markers are refused.
5. The CI release ZIP contains both self-authored synthetic mock
   libraries in \`ports/lenneth-r36s/mock/\`, so the dArkOSRE R36S
   can independently verify the same dynamic ELF code execution
   and record \`[lenneth synthetic] R36S_EXECUTION=PASS\`.
   No commercial binaries, CPK files or APK contents are bundled.

### Phase-11 R36S test instructions

1. Download the GitHub Actions artifact
   \`Lenneth-Android-R36S-ARM64-Multitest-Phase11\` from
   this branch's workflow. Unzip its inner
   \`lenneth-r36s-multitest-phase11-arm64.zip\` into the ROM partition,
   preserving \`ports/\`.
2. Keep your legally obtained \`libMyGame.so\` and
   \`libc++_shared.so\` in \`ports/lenneth-r36s/\`.
3. Launch **Ports → Lenneth Android R36S Test** once. The launcher
   first calls the two synthetic test libraries, then executes
   the full original game-library **non-executable** linker batch,
   asset/logger/threads/FORTIFY/C++ fixture tests, and GLES2/ALSA/
   gamepad hardware checks.
4. Share only \`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\`.
   The ZIP contains three **text reports only**, never the private
   commercial libraries. Its summary distinguishes synthetic
   code execution from private game code.

### Still required for actual game startup

The synthetic execution is a prerequisite, **not** an Android
runtime. Do not pass the original \`libMyGame.so\` to
\`--execute-synthetic\`; its missing marker and dynamic dependencies
cause an explicit refusal. We still need a Bionic FILE* adapter
(including \`__sF\` data objects), dynamic linker namespace and
\`dl_iterate_phdr\`, \`setjmp/longjmp\` ABI, OpenSL ES/CRIWARE audio,
actual JNI emulation and Android data-path mappings. Original game's
constructors, TLS and callback lifecycle have not been validated
under executable mappings.

The C++ ABI matching signatures and destructor order were checked
against upstream Android Bionic
https://android.googlesource.com/platform/bionic/+/master/libc/bionic/atexit.cpp
and
https://android.googlesource.com/platform/bionic/+/main/libc/bionic/__cxa_thread_atexit_impl.cpp .
The adapters are independently written and carry the project's
MIT license.

## Phase 12: sandboxed fd / wchar ABI / stack guard, grouped validation

The owner's dArkOSRE R36S phase-11 ZIP confirms that the independently
authored \`--execute-synthetic\` ARM64 test **actually ran** on the RK3326:
\`R36S_EXECUTION=PASS\`, crossing from one separately loaded synthetic
library to a second and correctly returning **77**. This does **not**
execute the commercial game. GPU, ALSA, gamepad, and all prior fixture
tests remained PASS.

The phase-11 physical report recorded **283 externally bound guest
relocation sites** (GLES/EGL 97, portable C 104, AAsset 11, Android log
4, experimental Bionic pthread 44, FORTIFY 17 and C++ lifecycle 6)
and **119 pending named import entries**, representing **110 unique
names** (51 in the game module, 68 in Android libc++). Of these,
114 were Bionic/libc/libm/dl and five were OpenSL ES. Binding a
relocation slot does not prove the shim is safe when called by the game.

### Phase-12 batch changes

- \`src/posix_fd_provider.c\`: typed Android-like POSIX file-descriptor
  functions \`open\`, \`read\`, \`write\`, \`close\`, \`lseek\`,
  \`gettid\`, \`clock_nanosleep\`, \`memalign\`.
  A relative \`assets/...\` path is read-only. A \`saves/...\` path is
  private to the local port directory; parent traversal, symlinks and
  unsupported Android absolute paths are rejected. The owner can put
  private game CPKs in \`assets/\` later, but the phase-12 test requires
  none. This is not yet a complete Android path or filesystem service.
- \`src/unicode_provider.c\`: only the **six** primitive UTF-32 wchar
  array functions (\`wcslen\`, \`wmemcpy\`, \`wmemmove\`, \`wmemset\`,
  \`wmemchr\`, \`wmemcmp\`). Does NOT map Bionic \`locale_t\`,
  \`mbstate_t\`, encodings, conversions or locale-sensitive \`FILE*\`.
- \`src/bionic_data_provider.c\`: one independently randomized,
  process-local \`__stack_chk_guard\` **data-symbol** import; it
  deliberately does not fake the incompatible Bionic \`__sF\`
  FILE array or OpenSL ES \`SL_IID_*\` objects.
- All three modules include **independent synthetic tests**. Each is
  tested by CI on native ARM64 and by the full R36S \`--batch\` suite
  along with earlier C++, pthread, fortified libc, AAsset, Android log,
  graphics/audio/gamepad probes.
- The guest ELF continues to be mapped non-executable; only our
  explicitly marked independent synthetic test ELF is permitted to
  execute. The \`lenneth-blockers.tsv\` inventory reveals further
  unsupported Android interfaces without storing private binary data.

### How to test the combined Phase 12

1. Download the GitHub Actions artifact
   \`Lenneth-Android-R36S-ARM64-Multitest-Phase12\` from the
   \`.github/workflows/lenneth-r36s.yml\` workflow.
2. Extract the inner \`lenneth-r36s-multitest-phase12-arm64.zip\`
   into the ROM partition, preserving \`ports/\`.
3. Keep your locally sourced private \`libMyGame.so\` and
   \`libc++_shared.so\` in \`ports/lenneth-r36s/\`. No APK or game
   data assets are bundled.
4. Run Ports → **Lenneth Android R36S Test** once and send back only
   \`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\`.

Expected new markers: \`[lenneth fd] SELFTEST=PASS\`,
\`[lenneth unicode] SELFTEST=PASS\`,
\`[lenneth bionic-data] SELFTEST=PASS\`,
\`[lenneth io] PHASE12_LINK=PASS\`, and existing
\`[lenneth synthetic] R36S_EXECUTION=PASS\`.

### Remaining blockers

Not yet implemented or guest-executed: Bionic \`FILE*\` and \`__sF\`,
Android-relative and absolute path translation beyond the two local
roots, Android \`dlopen/dlsym/dl_iterate_phdr\` namespace emulation,
\`setjmp/longjmp\` and platform signal-context ABI, full Java/JNI
object/VM/class tables, locale/multibyte state, and OpenSL ES
interfaces and CRIWARE audio/video lifecycle. Bionic CRT constructors
and game executable mappings stay disabled. This remains a hardware
and ABI **diagnostic**, not a runnable game port.

## Phase 13: allowlisted dynamic linker, sandboxed saves and scalar wchar support

The latest physical R36S Phase-12 diagnostic from dArkOSRE
reported \`FINAL=PASS\` and \`R36S_EXECUTION=PASS\` for the *synthetic*
cross-module ARM64 mock only. The private Android game engine remains
non-executable. The real Android libraries had **299** externally
bound guest relocation sites and **104 unresolved relocation sites**.
The \`lenneth-blockers.tsv\` contains **103 module-specific import
names** (42 game + 61 libc++), or **95 distinct names across both
modules**. The largest group is Bionic libc/stdio/locale/linker (98
import entries); OpenSL ES has five additional unresolved entries.

### Combined phase-13 changes

1. \`src/ndk_dloader.c\` is an independently written, deliberately
   restricted \`dlopen\`/\`dlsym\`/\`dlclose\`/\`dlerror\` shim. Android
   guest handles are **not** glibc handles. Only \`libGLESv2.so\` and
   \`libEGL.so\` may be opened. Symbols are restricted to exported
   \`gl[A-Z]*\` or \`egl[A-Z]*\` methods. Paths and unknown guest
   libraries are rejected with explicit errors. Android linker
   namespaces and \`dl_iterate_phdr\` are **not** implemented. No
   arbitrary filesystem libraries may be opened by the guest.
2. \`src/saveops_provider.c\` adds \`rename\`, \`mkdir\` and \`remove\`
   as narrowly scoped, app-private operations on \`saves/\` only.
   It uses \`openat\` relative to a saved directory FD and rejects
   absolute paths, symlinks in path components, \`..\`, unrelated
   paths and unsupported filesystem operations.
3. \`src/unicode_provider.c\` now explicitly includes extra primitive
   \`isw*\`/\`tow*\` scalar operations and wide-number parsing;
   32-bit \`wchar_t\` matches Android ARM64. The calling convention
   can match for these primitive signatures, but Unicode/locale
   semantics can still vary, so full Android locale and multibyte
   conversion state are NOT supported.
4. CI checks these three groups together using independent synthetic
   fixture code **and also reruns every previous phase 1–12 unit
   test**. On R36S, one run also repeats the real synthetic RX ELF
   test and the GLES/ALSA/joystick hardware tests.

### R36S test

Download \`Lenneth-Android-R36S-ARM64-Multitest-Phase13\` from
[GitHub Actions](../../actions/workflows/lenneth-r36s.yml), extract
the inner \`lenneth-r36s-multitest-phase13-arm64.zip\` into ROMs,
and keep your *own* commercial \`libMyGame.so\` and
\`libc++_shared.so\` in \`ports/lenneth-r36s/\`. The package contains
only public port code and independent synthetic ARM64 mocks,
not the game assets or original APK.

Launch **Ports → Lenneth Android R36S Test** once, and upload
\`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\`.

Expected new markers: \`[lenneth ndk-dl] SELFTEST=PASS\`,
\`[lenneth saveops] SELFTEST=PASS\`,
\`[lenneth ndk-dl] PHASE13_LINK=PASS\`,
plus the existing \`[lenneth synthetic] R36S_EXECUTION=PASS\`
and \`[lenneth suite] FINAL=PASS\`.

**Critical remaining work**: Bionic \`FILE*\`/\`__sF\` object
representation and stdio, \`setjmp/longjmp\` ABI, locale and
\`mbstate_t\` conversions, Android linker ELF module namespaces
and \`dl_iterate_phdr\`, OpenSL ES interfaces, and a working JNI/VM
environment. None of these are safely replaceable with glibc
by matching their symbol names. Original commercial game code is
still NOT executed. This remains an ABI and R36S hardware diagnostic,
not a playable game port.

## Phase 14: typed opaque Bionic FILE proxy (not the legacy __sF ABI)

The owner's real dArkOSRE R36S phase-13 multitest reports:
- \`Overall diagnostic: PASS\` and actual execution of only our
  independent synthetic AArch64 pair: \`R36S_EXECUTION=PASS\` (return 77).
- Graphics GLES2/EGL, ALSA, gamepad, asset, threads, FORTIFY, C++,
  sandboxed fd, Unicode, Bionic stack guard and restricted guest dl
  probes all passed.
- The commercial Android libraries remained mapped **non-executable**.
- **327** externally bound Android-to-Linux relocation sites (not
  320: 97 GLES/EGL +104 portable libc/libm +11 AAsset +4 logging
  +44 pthread +17 FORTIFY +6 C++ +9 fd +27 Unicode +1 stack guard
  +4 restricted libdl +3 save operations).
- **76 unresolved relocation sites**, **75 module-specific import
  entries** across two Android ELF files, corresponding to **67
  unique symbol names**. 70 of those 75 entries are libc/stdio/locale/
  loader; 5 are OpenSL ES.

We verified the original Android Bionic stdio headers against:
https://android.googlesource.com/platform/bionic/+/main/libc/include/stdio.h .
It is **not safe** to pass a Linux glibc FILE* directly to old Android
libraries. Further, the two ELF files import Bionic's legacy \`__sF\`
FILE data array, which depends on precise private structure layout
and ABI-version assumptions.

### New implementation

\`src/bionic_stdio.c\` independently implements **15 API-mediated
stream operations** using private finite token slots whose host-side
\`FILE*\` objects never cross into the imported guest code: \`fopen\`,
\`fclose\`, \`fread\`, \`fwrite\`, \`fseek\`, \`ftell\`, \`ferror\`,
\`clearerr\`, \`fflush\`, \`fputc\`, \`getc\`, \`ungetc\`,
\`fileno\`, \`vfprintf\`, and \`fprintf\`. File opens use the existing
\`openat(O_NOFOLLOW)\` path sandbox: \`assets/\` is read-only and
\`saves/\` is app-private. No arbitrary absolute paths are allowed.
The guest pointer is a distinct token and is never reinterpreted as
the internal host glibc FILE layout.

\`src/portable_provider.c\` adds the primitive standard-C-only
\`printf\`, \`sscanf\`, and \`vsscanf\` import candidates, while
\`src/bionic_fortify.c\` adds a bounds-checked \`__vsprintf_chk\`
wrapper. These changes also have synthetic unit tests, and the
standalone batch continues to run every regression from phases 1–13.

**Critical limitations:**
- \`__sF\` is explicitly **NOT** mapped or fabricated; the real game
  and Android libc++ import this symbol and may dereference it.
- Android-compatible \`fdopen\`, direct Bionic FILE field access,
  platform-specific \`struct stat\`, longjmp/setjmp and locale
  internals are NOT solved.
- \`OpenSL ES\` engine/interface vtables and \`SL_IID_*\` data symbols
  are NOT mapped, so the game audio engine cannot start.
- A synthetic \`FILE\` fixture passing does not prove the full old
  Bionic ABI works during actual game/JNI initialization. No original
  game instructions are executed; live-game launch is disabled.

### One physical R36S test

Download the GitHub Actions artifact
\`Lenneth-Android-R36S-ARM64-Multitest-Phase14\`, unpack its nested
\`lenneth-r36s-multitest-phase14-arm64.zip\` preserving
\`ports/\`, and keep the two original Android private libraries
\`libMyGame.so\` and \`libc++_shared.so\` in
\`ports/lenneth-r36s/\`. No game assets or APK are included.
Launch Ports → **Lenneth Android R36S Test** once and send only
\`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\`.

Expect \`[lenneth stdio] SELFTEST=PASS\` and
\`[lenneth stdio] PHASE14_LINK=PASS\`, plus
\`[lenneth synthetic] R36S_EXECUTION=PASS\` and
\`[lenneth suite] FINAL=PASS\`. The new blocker TSV should still
contain the legacy Bionic \`__sF\` object and five OpenSL ES imports;
do not interpret missing \`FILE\` function names as a runnable
Android application.

## Phase 15: five OpenSL ES imports, typed object/interface ABI

The physical Phase-14 R36S ZIP supplied by the owner confirms:
- **Overall diagnostic PASS**. Mock AArch64 execution across two ELF
  libraries still returns 77 on dArkOSRE. Only independent synthetic
  libraries, **never commercial game code**, were made executable.
- **352** guest relocations bound to native Linux or compatibility
  providers: 97 graphics, 108 libc/libm, 11 assets, four Android
  logging, 44 pthread, 18 FORTIFY, six C++ lifecycle, nine POSIX fd,
  27 Unicode, one stack guard, four restricted libdl, three saveops
  and 20 opaque stdio API references.
- **51 unresolved relocation sites**, **50 module-specific import
  entries** (19 in the game, 31 in Android libc++) representing
  **48 unique names**. Five are the OpenSL ES symbol references
  \`slCreateEngine\`, \`SL_IID_ENGINE\`, \`SL_IID_PLAY\`,
  \`SL_IID_BUFFERQUEUE\` and \`SL_IID_VOLUME\`.

### What phase 15 adds

\`src/opensl_bridge.c\` / \`.h\` independently implement **the first
OpenSL ES 1.0.1 ABI surface**:
- Correctly distinguish the **four SL_IID_* pointer DATA objects**
  (addresses of global pointer variables) from \`slCreateEngine\`,
  which is a function. The non-executing ARM64 ELF relocator checks
  the guest ELF symbol type before providing any of these five
  addresses.
- Create/realize/destroy OpenSL engine, output mix and limited PCM
  audio player objects; retrieve the Engine/Play/BufferQueue/Volume
  interfaces by interface ID, using a vtable layout aligned to the
  standard OpenSL ES 1.0.1 C API.
- Support limited logical play states, volume/mute, bounded PCM
  buffer queue operations, and callback registration. Independent
  synthetic test code consumes one queued buffer and invokes the
  callback to check ordering and interfaces.

**This implementation does NOT yet play sound.** There is no SDL/ALSA
PCM output wired to this bridge in Phase 15. Outside of the synthetic
test, enqueued PCM data will not be consumed by a hardware backend,
so callback-driven game playback is not functional. This phase tests
symbol/data ABI, interface dispatch and queue semantics only and
must not be described as a working OpenSL ES audio engine.
Unsupported formats/interfaces return errors or are not exposed.

**Source/reference:** [Android NDK OpenSL ES documentation](https://developer.android.com/ndk/guides/audio/opensl/opensl-for-android)
and the publicly available Khronos
[OpenSL ES 1.0.1 header](https://android.googlesource.com/platform/prebuilt/+/968372cd8fffab51ff89c6b3cb40bc74ede24cb9/ndk/android-ndk-r8/platforms/android-9/arch-mips/usr/include/SLES/OpenSLES.h).
The bridge itself was independently written for this port.

### Phase-15 R36S single-run diagnostic

Download workflow artifact
\`Lenneth-Android-R36S-ARM64-Multitest-Phase15\`.
Extract inner \`lenneth-r36s-multitest-phase15-arm64.zip\`
to the ROM partition, preserving \`ports/\`. Keep the two
privately supplied \`libMyGame.so\` and \`libc++_shared.so\` in
\`ports/lenneth-r36s/\`. The test requires no commercial CPK assets
and does not include any original game files.

Launch Ports → Lenneth Android R36S Test **once**. Upload only
\`ports/lenneth-r36s/lenneth-r36s-diagnostics.zip\`.

Expected markers:
\`\`\`
[lenneth opensl] SELFTEST=PASS ... real_pcm_output=NOT_IMPLEMENTED
[lenneth opensl] PHASE15_LINK=PASS ...
[lenneth synthetic] R36S_EXECUTION=PASS
[lenneth suite] FINAL=PASS
\`\`\`

### Critical unsolved runtime tasks

The OpenSL engine still needs **actual PCM output** and callback
scheduling against SDL/ALSA, correct audio-device lifecycle and
recovery after sleep/resume. These guest interfaces also remain
unvalidated: Bionic \`__sF\` and direct \`FILE*\` layout, locale_t and
mbstate_t, \`stat/fstat\`, \`setjmp/longjmp\`, POSIX directory
object layout, Android ELF dynamic linker introspection, and full
Java/JNI initialization. Reducing an unresolved-symbol list does
not mean that commercial code is safe to run. Original game code
remains **non-executable** in the production diagnostic loader.
