#pragma once

#include <X11/Xlib.h>

// Query window size without letting BadWindow escape to LWJGL's X error handler.
// Returns false if the window is missing/invalid or has a non-positive size.
bool X11SafeGetWindowSize(Display* dpy, Window win, int* width, int* height);

// Query size + visual/depth in one trapped round-trip.
bool X11SafeGetWindowInfo(
    Display* dpy,
    Window win,
    int* width,
    int* height,
    Visual** visual,
    int* depth);

// True if the window still exists on this display.
bool X11SafeWindowExists(Display* dpy, Window win);
