# ValkyrieRecomp experimental R36S (RK3326 / DarkOSRE) bring-up

**Status: scaffolding only; no ARM64 game binary has been built or validated.**
This is a separate `VALKYRIE_R36S` CMake profile; regular desktop builds remain unchanged.

## Hardware and display

- R36S: AArch64-capable RK3326 (Cortex-A35), 1 GiB RAM, 640×480 4:3 panel, Mali-G31.
- `4:3` is the correct full-screen content aspect of the R36S display.
- `16:9` is preserved for other displays. On the 4:3 handheld panel, showing the full
  16:9 frame requires horizontal letterboxing (black bars above/below) rather than
  stretching it to 4:3. Do not crop the picture to claim a native widescreen display.
- The game's `game.toml` already opts into widescreen. This profile never edits it.
- On-device script: `VALKYRIE_ASPECT=4:3` (default), or `VALKYRIE_ASPECT=16:9`.

## Renderer: what is and is not supported

The existing PSXRecomp hardware renderer requests **desktop OpenGL 3.3 core** and
uses features including integer `R16UI` texture mirrors and shader APIs absent
in **OpenGL ES 2.0**. Merely changing the SDL context version to GLES2 would not
make this renderer work. The `LNG_GLES2` launcher integration in recomp-ui is
not a GLES2 game-renderer implementation.

For first bring-up, select `--renderer software`: the PlayStation GPU runs
through its CPU/software rasterizer and SDL2 presents the resulting texture.
SDL2 **may** use its GLES2-capable renderer for presentation if the installed
drivers support it, but that is **not** equivalent to native GLES2 PSX rendering.
The upstream GL renderer source may still require desktop GL development
headers/libraries *at compile time* even with software selected.

An eventual native GLES2 backend would require a separate GPU backend:
convert raw PS1 1555 texture/CLUT handling for GLES2-compatible formats,
replace integer samplers/texelFetch, adapt FBO/mask/stencil/readback,
rewrite GLSL to ES 1.00 and handle shader precision. A GLES3 path may be
less invasive for Mali-G31, but it too needs validation. Neither is implemented.

## Source dependencies and disk data

```sh
git clone --recursive https://github.com/dehalbuq/ValkyrieRecomp.git
cd ValkyrieRecomp
git submodule update --init --recursive
```

Two legally obtained US disc images must be provided by the builder at the
paths documented in the root README. The repository must **not** include game
discs, BIOS dumps, generated game C, or copyrighted translation assets.

**Generate first on an x86_64 host** (not on the 1 GiB R36S device):

```sh
./psxrecomp/tools/ci/build_emitters.sh
python3 psxrecomp/psxrecomp_cli.py generate \
    --config game.toml --project-root . \
    --disc 'disc/Disc1/Valkyrie Profile (USA) (Disc 1).cue'
```

## Cross-compilation experiment

Use a Linux AArch64 GCC/G++ toolchain and a development **sysroot matching
DarkOSRE's glibc and installed SDL2/EGL/GL libraries**. A random modern
AArch64 Ubuntu sysroot is not guaranteed binary-compatible. If SDL2 or OpenGL
headers/pkg-config metadata are missing, CMake will fail: that is an unresolved
framework/dependency porting step, not a successful build.

```sh
export R36S_SYSROOT=/absolute/path/to/r36s-dev-sysroot
cmake --preset r36s-arm64-release
cmake --build --preset r36s-arm64-release --parallel 2

# Diagnostic variant:
cmake --preset r36s-arm64-debug
cmake --build --preset r36s-arm64-debug --parallel 2
```

The CMake profile selects SDL2, disables recomp-ui and Vulkan, and requires
generated game C (prevents accidentally shipping a setup-only host).
It tunes compilation for Cortex-A35 without unsafe `-ffast-math`.

**Not yet tested:** compilation, link against the DarkOSRE sysroot, display,
audio, game boot, save states, disc switching, or controller mappings. These
are validation gates before describing the result as an actual R36S port.

## Launch testing (only after a working ARM64 binary exists)

Copy the resulting `ValkyrieRecomp` binary, `game.toml`,
`game_options.toml`, legally obtained `disc/` content and the file
`r36s/ValkyrieRecomp.sh` into the **same** directory on the SD card.
Make the launcher and executable executable with `chmod +x`.
If necessary, stage *ARM64 binaries compatible with the target system* under
`lib/`; never copy x86_64 libraries. The launcher writes
`game-r36s.local.toml` from `game.toml` and logs to `valkyrie-r36s.log`.

```sh
./ValkyrieRecomp.sh                  # Disc 1, 4:3
VALKYRIE_ASPECT=16:9 ./ValkyrieRecomp.sh
VALKYRIE_DISC=2 ./ValkyrieRecomp.sh
```

The script leaves the SDL video driver as provided by the OS. SDL/KMSDRM,
controller input mappings, exit hotkeys, fullscreen handling and PortMaster
integration must be tested on the actual DarkOSRE firmware; no working driver
combination is asserted here.

For troubleshooting, record the entire `valkyrie-r36s.log`, the OS version,
`file ValkyrieRecomp`, `ldd ValkyrieRecomp`, SDL renderer information and
`glGetString(GL_VERSION)` / `GL_RENDERER` from an available GLES diagnostic.

## Next stages

1. Supply/use a matching DarkOSRE AArch64 development sysroot and validate a
   native software+SDL2 build, then measure peak memory and frametime.
2. Diagnose all build/link/runtime errors, map controls and exit hotkey, then
   package a PortMaster-compatible release with ABI-correct libraries.
3. Prototype a separate GLES3 game renderer; evaluate an additional GLES2
   fallback without regressing the desktop OpenGL path.
4. **Later** investigate language asset formats and a legal external
   Brazilian Portuguese translation patch; out of scope for this stage.

## SD card folder layout (manual Ports menu shortcut, not an official package)

The launcher is experimental and not registered/published in the PortMaster catalog.
Stage the two launcher files into different levels, matching the tree:

```text
EASYROMS/ports/
├── ValkyrieRecomp.sh                   # from r36s/PortMaster/ValkyrieRecomp.sh
└── valkyrierecomp/
    ├── ValkyrieRecomp.sh               # from r36s/ValkyrieRecomp.sh
    ├── ValkyrieRecomp                  # NOT BUILT YET (ARM64 game executable)
    ├── game.toml
    ├── game_options.toml
    ├── disc/
    │   ├── Disc1/
    │   │   ├── Valkyrie Profile (USA) (Disc 1).cue
    │   │   └── Valkyrie Profile (USA) (Disc 1).bin
    │   └── Disc2/
    │       ├── Valkyrie Profile (USA) (Disc 2).cue
    │       └── Valkyrie Profile (USA) (Disc 2).bin
    ├── lib/
    └── saves/
```

The inner launcher creates `game-r36s.local.toml` and `valkyrie-r36s.log`
on launch. Do not distribute the ZIP as a working game: it lacks the compiled
executable and proprietary disc assets. A lone .iso or .chd renamed to .bin
is not a valid conversion. Once an ARM64 build exists, ensure its runtime
loader and linked libs match the R36S operating system. The menu shortcut is
a convenience for manual experiments and does not claim full PortMaster
packaging compliance (no validated controls, metadata, or device runtime).
