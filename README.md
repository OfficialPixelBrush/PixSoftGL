# PixSoftGL
Linux OpenGL Software Rendering Library

## Goals
- OpenGL 1.1 (Whatever is necessary to run Minecraft Beta 1.7.3!)
- Potentially get running on external hardware (ESP32 with a screen, perhaps)

## Roadmap
- [x] Get extremely basic cube program running
- [ ] Test Suite (?)
- [ ] Glxgears
- [ ] Classicube -> Needed to force `DEFAULT_GFX_BACKEND CC_GFX_BACKEND_GL11` in `Core.h`
- [ ] Minecraft Beta 1.7.3

## Current architecture
Application -> `libGL` (PixSoftGL) -> SDL3

## Planned architecture
Application -> `libGL` (PixSoftGL) -> ESP32 ("GPU") -> Screen (or SDL3 listening for a reply)