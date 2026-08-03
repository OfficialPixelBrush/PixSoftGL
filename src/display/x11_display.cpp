#include "x11_display.h"

#include <X11/Xutil.h>
#include <vector>

static X11DisplayContext ctx{};
static XImage* ximage = nullptr;
static std::vector<uint8_t> imageBuffer;

static void destroyImage() {
    if (ximage) {
        ximage->data = nullptr;
        XDestroyImage(ximage);
        ximage = nullptr;
    }
    imageBuffer.clear();
}

void X11DisplayInit(Display* dpy, Window win, int width, int height) {
    X11DisplayShutdown();
    ctx.dpy = dpy;
    ctx.win = win;
    ctx.width = width;
    ctx.height = height;
    ctx.active = (dpy != nullptr && win != 0);
}

void X11DisplayShutdown() {
    destroyImage();
    ctx = {};
}

X11DisplayContext* X11DisplayGetContext() {
    return ctx.active ? &ctx : nullptr;
}

bool X11DisplayPresent(const uint8_t* rgb, int width, int height) {
    if (!ctx.active || !rgb || width <= 0 || height <= 0) return false;

    const size_t needed = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (imageBuffer.size() != needed) {
        destroyImage();
        imageBuffer.resize(needed);
        ximage = XCreateImage(
            ctx.dpy,
            DefaultVisual(ctx.dpy, DefaultScreen(ctx.dpy)),
            DefaultDepth(ctx.dpy, DefaultScreen(ctx.dpy)),
            ZPixmap,
            0,
            reinterpret_cast<char*>(imageBuffer.data()),
            static_cast<unsigned>(width),
            static_cast<unsigned>(height),
            32,
            width * 4);
        if (!ximage) {
            imageBuffer.clear();
            return false;
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int src = (x + y * width) * 3;
            const int dst = (x + y * width) * 4;
            imageBuffer[dst + 0] = rgb[src + 0];
            imageBuffer[dst + 1] = rgb[src + 1];
            imageBuffer[dst + 2] = rgb[src + 2];
            imageBuffer[dst + 3] = 0;
        }
    }

    GC gc = XCreateGC(ctx.dpy, ctx.win, 0, nullptr);
    XPutImage(ctx.dpy, ctx.win, gc, ximage, 0, 0, 0, 0, static_cast<unsigned>(width), static_cast<unsigned>(height));
    XFreeGC(ctx.dpy, gc);
    XFlush(ctx.dpy);
    return true;
}
