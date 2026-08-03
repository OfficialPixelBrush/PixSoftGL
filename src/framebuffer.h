#pragma once

#include "global.h"
#include "fbdev/fbdev.h"

void PrintInfoAddr(void* s);
void PrintInfoHex(int s);
void PrintInfo(int s);
void PrintInfo(const std::string& s);

// Open /dev/fb0 (or PIXSOFTGL_FBDEV) and use the Linux framebuffer
// as the presentation target.
bool ReCreateWindow();
void UpdateScreen();

// Exposed for cleanup/testing.
FbDevice* GetFramebufferDevice();
