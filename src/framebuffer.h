#pragma once

#include "global.h"
#include "fbdev/fbdev.h"

void PrintInfoAddr(void* s);
void PrintInfoHex(int s);
void PrintInfo(int s);
void PrintInfo(const std::string& s);

// Ensure color/depth buffers match the given size (reallocates on change).
bool EnsureRenderBuffers(int width, int height);

// Open presentation targets (X11 window and/or Linux fbdev).
bool ReCreateWindow();
void UpdateScreen();

FbDevice* GetFramebufferDevice();
