#pragma once

#include "datatypes.h"

#include <X11/Xlib.h>
#include <cstdint>

struct X11DisplayContext {
    Display* dpy = nullptr;
    Window win = 0;
    Visual* visual = nullptr;
    int depth = 0;
    int width = 0;
    int height = 0;
    bool active = false;
};

// Root-window property published by PixSoftWM for auto-detection.
inline constexpr const char* PIXSOFTGL_WM_ATOM = "_PIXSOFTGL_WM";

bool X11IsPixSoftWm(Display* dpy);

void X11DisplayInit(Display* dpy, Window win, int width, int height);
void X11DisplayShutdown();
X11DisplayContext* X11DisplayGetContext();

// Present an OpenGL software color buffer (top-left origin) into the X11 window.
// Returns false if X11 is inactive.
bool X11DisplayPresent(const PixelValue* pixels, int width, int height);
