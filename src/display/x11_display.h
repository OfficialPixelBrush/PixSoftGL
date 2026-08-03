#pragma once

#include <X11/Xlib.h>
#include <cstdint>

struct X11DisplayContext {
    Display* dpy = nullptr;
    Window win = 0;
    int width = 0;
    int height = 0;
    bool active = false;
};

void X11DisplayInit(Display* dpy, Window win, int width, int height);
void X11DisplayShutdown();
X11DisplayContext* X11DisplayGetContext();

bool X11DisplayPresent(const uint8_t* rgb, int width, int height);
