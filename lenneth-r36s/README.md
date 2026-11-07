# Valkyrie Profile: Lenneth (Android) — R36S Linux ARM64

**Experimental bootstrap — phases 1 and 2. This does NOT run the game yet.**

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
