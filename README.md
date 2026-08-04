# PixSoftGL

Linux OpenGL 1.1 software renderer (CPU rasterizer) with GLX, aimed at weak
hardware — especially **Pentium II + S3** machines presenting through the
**Linux framebuffer** (`/dev/fb0`). Also usable under X11 / WSL for development.

Primary target app: **Minecraft Beta 1.7.3** (LWJGL). Secondary: ClassiCube,
`glxgears`, small GL 1.1 demos.

## Architecture

```
GL 1.1 app  --LD_PRELOAD / LD_LIBRARY_PATH-->  build/libGL.so.1*
                     |  software rasterizer (float or fixed-point)
                     +--> X11 window present   (PIXSOFTGL_PRESENT=x11)
                     +--> /dev/fb0 present     (PIXSOFTGL_PRESENT=fbdev)  ← S3 target

pixsoftwm  --> tiny fullscreen X11 WM (input + GLX drawable)
           --> optional fbdev mode set (640x480@16, …)
           --> publishes _PIXSOFTGL_WM so libGL auto-detects it
```

On bare metal, **X still runs** (for LWJGL / GLX / input), but **pixels go to
`/dev/fb0`** when `PIXSOFTGL_PRESENT=fbdev`. That is the intended S3 path.

## Quick start (compile + run)

### 1. Build

```bash
cd /home/torben/PixSoftGL
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRENDERTARGET=HARDWARE
cmake --build build -j$(nproc)
```

Artifacts:

| Path | What |
|------|------|
| `build/libGL.so.1.0.0` | Software OpenGL 1.1 + GLX (also `libGL.so` / `libGL.so.1`) |
| `build/pixsoftwm` | Fullscreen WM helper for fbdev / S3 |

Optional tests (if present):

```bash
./compileTests.sh
```

### 2. Run Minecraft (desktop / WSL — X11 present)

LWJGL `dlopen`s `libGL.so.1`, so you need **both** `LD_PRELOAD` and
`LD_LIBRARY_PATH`:

```bash
cd /home/torben/PixSoftGL

LD_PRELOAD=/home/torben/PixSoftGL/build/libGL.so.1.0.0 \
LD_LIBRARY_PATH=/home/torben/PixSoftGL/build \
PIXSOFTGL_PRESENT=x11 \
PIXSOFTGL_DEBUG=0 \
PIXSOFTGL_FAST=1 \
./testMinecraft.sh
```

`PIXSOFTGL_FAST=1` turns on the P2-oriented preset (half-res scale, fixed-point
raster, 16-bit depth, affine texturing). See [Environment variables](#environment-variables).

### 3. Run on S3 / framebuffer (bare metal)

1. Boot Linux with a usable `/dev/fb0` (S3 fbdev or similar).
2. Start a minimal X server on that machine (needed for GLX + input).
3. Prefer **640×480 @ 16bpp**.
4. Launch via `pixsoftwm` so the client is fullscreen and inherits fbdev present:

```bash
cd /home/torben/PixSoftGL

# Interactive mode picker (pick 640x480 @ 16bpp on S3):
./build/pixsoftwm -f /dev/fb0 --present fbdev --prefer-16 \
  -l ./build/libGL.so.1.0.0 \
  -c 'PIXSOFTGL_FAST=1 PIXSOFTGL_DEBUG=0 ./testMinecraft.sh'

# Or non-interactive (mode index from the picker list, often 0 = 640x480@16):
./build/pixsoftwm -f /dev/fb0 -m 0 --present fbdev \
  -l ./build/libGL.so.1.0.0 \
  -c 'PIXSOFTGL_FAST=1 ./testMinecraft.sh'
```

You can also force mode from the library itself without the WM picker:

```bash
export PIXSOFTGL_PRESENT=fbdev
export PIXSOFTGL_FBDEV=/dev/fb0
export PIXSOFTGL_FB_MODE=640x480x16
export PIXSOFTGL_FAST=1
```

### 4. Tiny demos

```bash
LD_PRELOAD=./build/libGL.so.1.0.0 LD_LIBRARY_PATH=./build \
  PIXSOFTGL_PRESENT=x11 ./tests/testCube

LD_PRELOAD=./build/libGL.so.1.0.0 glxgears
```

## Environment variables

All knobs are read once at first use (process lifetime). Empty / unset = off
unless noted. Any non-empty value except `"0"` counts as true for flags.

### Present / display

| Variable | Default | Meaning |
|----------|---------|---------|
| `PIXSOFTGL_PRESENT` | auto | `x11` = blit into the GLX window; `fbdev` = blit to `/dev/fb0`. Auto: X11 window if bound, else fbdev. **`pixsoftwm` defaults to `fbdev` when `/dev/fb0` opens.** |
| `PIXSOFTGL_FBDEV` | `/dev/fb0` | Framebuffer device node |
| `PIXSOFTGL_FB_MODE` | — | Set mode at open, e.g. `640x480x16` |
| `PIXSOFTGL_PREFER_16` | off | Keep current resolution, switch to 16bpp if possible |
| `PIXSOFTGL_MAX_W` / `PIXSOFTGL_MAX_H` | — | Cap drawable size before internal scale |
| `PIXSOFTGL_SHM` | off | Opt into MIT-SHM for X11 present (often broken under WSL/XWayland) |
| `PIXSOFTGL_NOSHM` | — | Force-disable SHM |

### Quality / speed (P2 / S3)

| Variable | Default | Meaning |
|----------|---------|---------|
| **`PIXSOFTGL_FAST`** | off | Preset: `SCALE=2` + fixed raster + `DEPTH16` + affine (unless you override individuals) |
| `PIXSOFTGL_SCALE` | `1` (`2` if FAST) | Doom/Quake-style internal resolution: render at `1/N`, nearest-upscale on present (`1`…`8`) |
| `PIXSOFTGL_FIXED` | off (on if FAST) | Integer / fixed-point triangle fill |
| `PIXSOFTGL_FIXED_BITS` | `4` | Sub-pixel bits for fixed edges (`0`…`12`; lower = faster, coarser) |
| `PIXSOFTGL_DEPTH16` | off (on if FAST) | 16-bit depth buffer instead of float |
| `PIXSOFTGL_AFFINE` | off (on if FAST/FIXED) | Force affine texturing (skip per-pixel `1/w`) |
| `PIXSOFTGL_NO_FOG` | off | Ignore GL fog (large fill-rate win; look changes) |

### Debug / escape hatches

| Variable | Default | Meaning |
|----------|---------|---------|
| `PIXSOFTGL_DEBUG` | off | Verbose `PrintInfo` tracing (**keep off on P2** — logging dominates runtime) |
| `PIXSOFTGL_WM` | off | Force software-GLX / WM client mode (set by `pixsoftwm`) |
| `PIXSOFTGL_FORWARD` | off | Hand GLX back to the system `libGL` |

### Suggested profiles

**Desktop debug (X11):**

```bash
PIXSOFTGL_PRESENT=x11 PIXSOFTGL_DEBUG=1 PIXSOFTGL_FAST=0
```

**Desktop playable:**

```bash
PIXSOFTGL_PRESENT=x11 PIXSOFTGL_DEBUG=0 PIXSOFTGL_FAST=1
```

**S3 / Pentium II (framebuffer):**

```bash
PIXSOFTGL_PRESENT=fbdev
PIXSOFTGL_FB_MODE=640x480x16
PIXSOFTGL_FAST=1
PIXSOFTGL_DEBUG=0
# optional: PIXSOFTGL_SCALE=3 PIXSOFTGL_NO_FOG=1 PIXSOFTGL_FIXED_BITS=2
```

On startup with scale/fast enabled you should see a line like:

```text
PixSoftGL: internal 320x240 → present 640x480 (scale=2 fixed depth16 affine FAST)
```

## pixsoftwm

`pixsoftwm` is a minimal X11 window manager that:

1. Opens `/dev/fb0` (optional) and can change video mode (interactive, `-m`, or `--prefer-16`).
2. Claims the WM selection and forces every mapped client **fullscreen** to the FB/screen size.
3. Publishes `_PIXSOFTGL_WM` on the root window so preloaded `libGL` auto-enables software GLX.
4. Launches a client with `LD_PRELOAD` + `LD_LIBRARY_PATH` (creates `libGL.so.1` symlink if needed).
5. Sets **`PIXSOFTGL_PRESENT=fbdev`** when a framebuffer was opened (S3 / bare metal), or `x11` otherwise. Override with `--present` or the env var.

X is still required for GLX contexts and typically for keyboard/mouse. Presenting
via **fbdev** writes the soft color buffer straight to the S3 framebuffer — that
is the path that matters on weak cards.

### Flags

| Flag | Meaning |
|------|---------|
| `-d / --display` | X display (default `$DISPLAY`) |
| `-f / --fbdev` | Framebuffer device (default `/dev/fb0`) |
| `-m / --mode` | Select enumerated mode by index (non-interactive) |
| `--keep-mode` | Do not change the current fbdev mode |
| `--prefer-16` | Try current resolution at 16bpp |
| `--present x11\|fbdev` | Force client present mode (also exported into the environment) |
| `-l / --lib` | `libGL.so` / `libGL.so.1.0.0` to preload into clients |
| `-c / --client` | Shell command to launch after the WM starts |
| `Ctrl+Esc` | Quit the WM |

### Examples

```bash
# S3 bare metal — fbdev present, pick a mode, run Minecraft
./build/pixsoftwm -f /dev/fb0 --present fbdev --prefer-16 \
  -l ./build/libGL.so.1.0.0 \
  -c 'PIXSOFTGL_FAST=1 ./testMinecraft.sh'

# Keep current mode, still present to fb0
./build/pixsoftwm --keep-mode --present fbdev \
  -l ./build/libGL.so.1.0.0 -c './tests/testCube'

# Desktop / WSL — X11 present into the fullscreen window
./build/pixsoftwm --keep-mode --present x11 \
  -l ./build/libGL.so -c ./tests/testCube
```

You can also start the WM alone, then launch clients yourself on the same
`$DISPLAY` with the env vars from above.

## Why both LD_PRELOAD and LD_LIBRARY_PATH?

| Mechanism | Who uses it |
|-----------|-------------|
| `LD_PRELOAD=…/libGL.so.1.0.0` | Native apps linking `-lGL` / resolving `libGL.so` at load |
| `LD_LIBRARY_PATH=…/build` | **LWJGL** (`dlopen("libGL.so.1")`), which **ignores** `LD_PRELOAD` |

CMake installs SONAME links (`libGL.so.1` → `libGL.so.1.0.0`). `pixsoftwm -l`
also ensures `libGL.so.1` exists beside your chosen `.so`.

## ClassiCube notes

1. Build with the OpenGL 1.1 backend (`DEFAULT_GFX_BACKEND CC_GFX_BACKEND_GL11` in `Core.h`).
2. Run under `LD_PRELOAD` + `LD_LIBRARY_PATH` (and ideally `pixsoftwm` for fullscreen FB-sized windows).
3. Working directory often needs ClassiCube game data — see `cc/` / `runClassicubeTest.sh` if present.

## Goals / roadmap

- [x] Basic cube program
- [x] X11 + Linux framebuffer present
- [x] pixsoftwm fullscreen helper
- [x] P2-oriented scale / fixed-point / depth16 knobs
- [ ] Broader GL 1.1 conformance / test suite
- [ ] Glxgears polish
- [ ] ClassiCube polish
- [ ] Minecraft Beta 1.7.3 — playable on S3 @ 640×480×16

## Resources

- [Coding Adventure: Software Rasterizer by Sebastian Lague](https://www.youtube.com/watch?v=yyJ-hdISgnw)
- [The OpenGL Graphics System: A Specification (Version 1.1)](https://registry.khronos.org/OpenGL/specs/gl/glspec11.pdf)
