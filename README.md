# PixSoftGL
Linux OpenGL 1.1 Software Rendering Library

## Goals
- OpenGL 1.1 (Whatever is necessary to run Minecraft Beta 1.7.3!)
- Potentially get running on external hardware (ESP32 with a screen, perhaps)

## Roadmap
- [x] Get extremely basic cube program running
- [ ] Test Suite (?)
- [ ] Glxgears
- [ ] Classicube -> Needed to force `DEFAULT_GFX_BACKEND CC_GFX_BACKEND_GL11` in `Core.h`
- [ ] Minecraft Beta 1.7.3

## Architecture (linux-framebuffer)

```
GL 1.1 app  --LD_PRELOAD-->  libGL.so (PixSoftGL)
                                |  software rasterizer
                                +--> X11 window present (default under WM)
                                +--> /dev/fb0 present (optional / fallback)

pixsoftwm  --> claims X11 WM, forces clients fullscreen to FB/screen size
           --> publishes _PIXSOFTGL_WM on the root window for auto-detection
```

### Build

```bash
cmake -S . -B build -DRENDERTARGET=HARDWARE
cmake --build build
./compileTests.sh
```

### Run under the WM

```bash
# Start the bare-bones fullscreen WM and launch a client
./build/pixsoftwm --keep-mode -l ./build/libGL.so -c ./tests/testCube

# Or preload manually against an existing PixSoftWM session
LD_PRELOAD=./build/libGL.so ./tests/testCube
```

libGL auto-detects PixSoftWM via the `_PIXSOFTGL_WM` root property (or
`PIXSOFTGL_WM=1`). Override present target with `PIXSOFTGL_PRESENT=x11|fbdev`.

## Resources
- [Coding Adventure: Software Rasterizer by Sebastian Lague](https://www.youtube.com/watch?v=yyJ-hdISgnw)
- [The OpenGL Graphics System: A Specification (Version 1.1)](https://registry.khronos.org/OpenGL/specs/gl/glspec11.pdf)
