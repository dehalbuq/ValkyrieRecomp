# Valkyrie Profile: Lenneth (Android) — R36S Linux ARM64

**Experimental bootstrap — phases 1–6. This does NOT run the game yet.**

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
