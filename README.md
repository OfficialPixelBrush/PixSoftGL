# PixSoftGL
Linux OpenGL 1.1 Software Rendering Library

## Goals
- OpenGL 1.1 (whatever is necessary to run Minecraft Beta 1.7.3 / ClassiCube)
- Optionally present via X11 or a Linux framebuffer (`/dev/fb0`)
- Potentially target external hardware later (e.g. ESP32)

## Roadmap
- [x] Basic cube program
- [ ] Broader GL 1.1 conformance / test suite
- [ ] Glxgears polish
- [ ] ClassiCube (force `DEFAULT_GFX_BACKEND CC_GFX_BACKEND_GL11` in `Core.h`)
- [ ] Minecraft Beta 1.7.3

## Architecture (`linux-framebuffer`)

```
GL 1.1 app  --LD_PRELOAD-->  libGL.so (PixSoftGL)
                                |  software rasterizer
                                +--> X11 window present (default under WM)
                                +--> /dev/fb0 present (optional / fallback)

pixsoftwm  --> bare-bones X11 WM, forces clients fullscreen to FB/screen size
           --> publishes _PIXSOFTGL_WM on the root window for auto-detection
```

## Build

```bash
cmake -S . -B build -DRENDERTARGET=HARDWARE
cmake --build build
./compileTests.sh
```

Artifacts:
- `build/libGL.so` — software OpenGL 1.1 + GLX implementation
- `build/pixsoftwm` — fullscreen X11 window manager helper
- `tests/testCube`, `tests/testSuite`, `tests/testPointers`

`RENDERTARGET` selects `HARDWARE` or `SOFTWARE`; both are currently the same CPU rasterizer (the name is historical).

## Using the library (LD_PRELOAD / LD_LIBRARY_PATH)

Native GLX apps (e.g. `testCube`) usually work with preload alone:

```bash
LD_PRELOAD=/path/to/PixSoftGL/build/libGL.so ./your_app
```

**LWJGL apps (Minecraft, etc.)** call `dlopen("libGL.so.1")`, which **does not
use `LD_PRELOAD`**. Put the build directory on `LD_LIBRARY_PATH` and provide the
`libGL.so.1` SONAME (CMake emits it; `pixsoftwm -l` also sets this up):

```bash
export LD_LIBRARY_PATH=/path/to/PixSoftGL/build:$LD_LIBRARY_PATH
export LD_PRELOAD=/path/to/PixSoftGL/build/libGL.so
./testMinecraft.sh
```

Examples:

```bash
LD_PRELOAD=./build/libGL.so ./tests/testCube
LD_PRELOAD=./build/libGL.so glxgears
mkdir -p cc && cd cc
LD_PRELOAD=../build/libGL.so LD_LIBRARY_PATH=../build:$LD_LIBRARY_PATH ~/ClassiCube/ClassiCube
```
On this branch the software GLX path is the default (so contexts / `SwapBuffers` match the software rasterizer). Optional env vars:

| Variable | Meaning |
|---|---|
| `PIXSOFTGL_WM=1` | Force WM / software-GLX mode (also set automatically by `pixsoftwm` when launching clients) |
| `PIXSOFTGL_PRESENT=x11` | Present into the current GLX window via X11 (`XPutImage` / MIT-SHM) |
| `PIXSOFTGL_PRESENT=fbdev` | Present directly to the Linux framebuffer |
| `PIXSOFTGL_FBDEV=/dev/fb0` | Framebuffer device path |
| `PIXSOFTGL_FORWARD=1` | Hand GLX back to the system `libGL` (escape hatch) |

Without `PIXSOFTGL_PRESENT`, present prefers the bound X11 window; fbdev is used as a fallback when no X11 drawable is active.

libGL also auto-detects PixSoftWM by reading the `_PIXSOFTGL_WM` property on the X root window.

## Using the window manager

`pixsoftwm` claims the X11 WM selection, forces every mapped client to fullscreen at the framebuffer / screen size, and publishes `_PIXSOFTGL_WM` so preloaded clients opt into PixSoftGL automatically.

```bash
# Keep the current fbdev mode (or X size if fbdev is unavailable),
# preload libGL into a client, and run:
./build/pixsoftwm --keep-mode -l ./build/libGL.so -c ./tests/testCube

# Interactive video-mode picker (when /dev/fb0 is usable):
./build/pixsoftwm -l ./build/libGL.so -c ./tests/testCube

# Non-interactive mode index:
./build/pixsoftwm -m 0 -l ./build/libGL.so -c ./tests/testCube

# ClassiCube under the WM:
./build/pixsoftwm --keep-mode -l ./build/libGL.so -c ~/ClassiCube/ClassiCube
```

Useful flags:

| Flag | Meaning |
|---|---|
| `-d / --display` | X display (default `$DISPLAY`) |
| `-f / --fbdev` | Framebuffer device (default `/dev/fb0`) |
| `-m / --mode` | Select enumerated fbdev mode by index |
| `--keep-mode` | Do not change the current fbdev mode |
| `-l / --lib` | `libGL.so` path to `LD_PRELOAD` into clients |
| `-c / --client` | Shell command to launch after the WM starts |
| `Ctrl+Esc` | Quit the WM |

You can also start the WM alone, then launch clients yourself with `LD_PRELOAD` against the same display.

## ClassiCube notes

1. Build ClassiCube with the OpenGL 1.1 backend (`DEFAULT_GFX_BACKEND CC_GFX_BACKEND_GL11` in `Core.h`), or otherwise force GL 1.1 / GLX.
2. Run under `LD_PRELOAD` (and ideally `pixsoftwm` for fullscreen FB-sized windows).
3. Working directory is often expected to contain ClassiCube game data — the `cc/` folder pattern in `runClassicubeTest.sh` is a convenient sandbox.

## Resources
- [Coding Adventure: Software Rasterizer by Sebastian Lague](https://www.youtube.com/watch?v=yyJ-hdISgnw)
- [The OpenGL Graphics System: A Specification (Version 1.1)](https://registry.khronos.org/OpenGL/specs/gl/glspec11.pdf)
