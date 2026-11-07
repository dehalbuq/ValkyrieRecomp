# R36S: moving PS1 rasterization from CPU to Mali-G31

Status (7 Nov 2026): **not implemented in gameplay**; capability and
GPU-only primitive throughput probe added in `r36s/gles-gpu-probe.c`.
The shipped game still uses PSXRecomp's CPU reference rasterizer through
`r36s/gpu_gl_stub.c`. SDL2 GLES2 accelerated presentation is not equivalent
to accelerating GP0 rasterization. Do not claim this is a GLES PS1 GPU port.

## Reproducible evidence

The user's dArkOSRE log showed:
- `SDL driver=KMSDRM renderer=opengles2 accelerated=1`
- `psxrecomp: renderer backend requested: software`
- A single mapped `GO-Super Gamepad`, ALSA initialized at 44.1kHz,
  and Start+Select exit working.

`r36s/gles-gpu-probe.c` queries actual OpenGL ES/GLSL/driver on the
console. It tries ES 3.1, 3.0, 2.0 and runs a basic 1024×512 FBO
textured triangle benchmark in each available context. It does NOT
exercise in-game PS1 GP0 commands. The 59.0Hz display refresh from
the existing game log does not measure simulated guest FPS. The
actual game now emits `[R36S perf] effective_fps=...` every ~5 s.

## Why not copy current DuckStation?

Current upstream DuckStation (`stenzek/duckstation`) uses CC BY-NC-ND 4.0;
its modern hardware renderer targets OpenGL ES 3.1+. A modified,
redistributed renderer is not authorized by its no-derivatives terms,
and its API requirements cannot be dropped to GLES2 simply by renaming
shaders. No DuckStation source was copied into this R36S branch.
Implement PSX-GPU semantics in original code or adapt the already
licensed-for-noncommercial-modification PSXRecomp backend, respecting
each project's terms and attribution.

## What full GPU rasterization actually requires

The PSXRecomp render facade in `gpu_render.h` exposes the complete
backend API. A replacement must implement **all** draw/transfer paths
rather than pushing just the SDL-present blit onto GLES:

1. Context and backend: SDL2/KMSDRM/EGL GLES3.1 if exposed; GLES2
   alternative or safe software fallback. Keep a valid context for
   GPU draws and present, no SDL_Renderer and GL context contention.
2. Authoritative 1024×512 PSX VRAM on GPU. RGB555+mask bit, exact
   CPU->VRAM and VRAM->CPU ordering, partial dirty uploads, coherent
   framebuffer snapshots before readback.
3. Textured polygons: 4/8-bit indexed CLUT and 15-bit direct texels;
   texture window, page origin, transparent texel and per-texel STP,
   affine interpolation. Avoid sampling from an attached render target.
4. Semi transparency: 4 native PSX blending modes, including
   subtractive blend and mixed opaque/semi textured pixels. Validate
   stencil/bit15 mask test/write and 24-bit MDEC display.
5. Rects, Gouraud/flat triangles, sprites, VRAM copy/fill,
   line drawing, clipping/scissor, draw offsets, optional geometry
   precision and in-game Start+Select overlay. Do not sacrifice
   save-state correctness or black-screen recovery.
6. Benchmark against a recorded frame corpus, compare GPU VRAM dumps
   with the reference software backend, and measure guest
   VBlanks/sec plus CPU/GPU drawing times separately.

A GPU renderer can reduce raster time but cannot fix CPU time spent
executing dynamic MIPS overlays or LLE BIOS calls. The optional
`PSX_IDLE_SKIP=1` loop optimization is an independent CPU-side A/B
test; it doesn't replace graphics acceleration. Counter-store skip
remains OFF until its per-title loop sites are audited.

Hardware capability references:
- https://docs.mesa3d.org/drivers/panfrost.html
- https://github.com/stenzek/duckstation/blob/master/LICENSE
- https://github.com/RetroPortingToolKit/psxrecomp/blob/master/LICENSE

Next technical gate: collect `gles-gpu-probe.log` and actual
`effective_fps` from `valkyrie-r36s.log`, then choose GLES3.1 porting
of PSXRecomp's existing GPU-authoritative backend (if driver provides
required features) versus a new GLES2 CLUT/mask/VRAM backend.
