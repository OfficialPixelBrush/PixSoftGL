#include "x11_display.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <algorithm>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>

namespace {

X11DisplayContext ctx{};
XImage* ximage = nullptr;
GC gc = 0;
bool useShm = false;
XShmSegmentInfo shmInfo{};
std::vector<uint8_t> imageBuffer;

bool shmExtensionAvailable(Display* dpy) {
    int major = 0, minor = 0, pixmaps = 0;
    return XShmQueryVersion(dpy, &major, &minor, &pixmaps) != 0;
}

void destroyImage() {
    if (ximage) {
        if (useShm) {
            XShmDetach(ctx.dpy, &shmInfo);
            ximage->data = nullptr;
            XDestroyImage(ximage);
            if (shmInfo.shmaddr && shmInfo.shmaddr != reinterpret_cast<char*>(-1)) {
                shmdt(shmInfo.shmaddr);
            }
            if (shmInfo.shmid >= 0) {
                shmctl(shmInfo.shmid, IPC_RMID, nullptr);
            }
            shmInfo = {};
            useShm = false;
        } else {
            ximage->data = nullptr;
            XDestroyImage(ximage);
        }
        ximage = nullptr;
    }
    imageBuffer.clear();
}

bool createImage(int width, int height) {
    destroyImage();
    if (!ctx.dpy || !ctx.visual || width <= 0 || height <= 0) return false;

    // Prefer 32bpp ZPixmap padding even for 24-bit visuals — matches common Xorg.
    const int bytesPerPixel = 4;
    const size_t bytes =
        static_cast<size_t>(width) * static_cast<size_t>(height) * bytesPerPixel;

    if (shmExtensionAvailable(ctx.dpy)) {
        ximage = XShmCreateImage(
            ctx.dpy,
            ctx.visual,
            static_cast<unsigned>(ctx.depth),
            ZPixmap,
            nullptr,
            &shmInfo,
            static_cast<unsigned>(width),
            static_cast<unsigned>(height));
        if (ximage) {
            const size_t shmBytes = static_cast<size_t>(ximage->bytes_per_line) *
                                    static_cast<size_t>(height);
            shmInfo.shmid = shmget(IPC_PRIVATE, shmBytes, IPC_CREAT | 0600);
            if (shmInfo.shmid >= 0) {
                shmInfo.shmaddr = static_cast<char*>(shmat(shmInfo.shmid, nullptr, 0));
                shmInfo.readOnly = False;
                if (shmInfo.shmaddr && shmInfo.shmaddr != reinterpret_cast<char*>(-1)) {
                    ximage->data = shmInfo.shmaddr;
                    if (XShmAttach(ctx.dpy, &shmInfo)) {
                        useShm = true;
                        return true;
                    }
                    shmdt(shmInfo.shmaddr);
                }
                shmctl(shmInfo.shmid, IPC_RMID, nullptr);
            }
            ximage->data = nullptr;
            XDestroyImage(ximage);
            ximage = nullptr;
            shmInfo = {};
        }
    }

    imageBuffer.assign(bytes, 0);
    ximage = XCreateImage(
        ctx.dpy,
        ctx.visual,
        static_cast<unsigned>(ctx.depth),
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
    useShm = false;
    return true;
}

void ensureGc() {
    if (!gc && ctx.dpy && ctx.win) {
        gc = XCreateGC(ctx.dpy, ctx.win, 0, nullptr);
    }
}

// Pack RGB888 (OpenGL bottom-left) into native XImage top-left.
void convertAndFlip(const PixelValue* pixels, int width, int height) {
    auto* dstBase = reinterpret_cast<uint8_t*>(ximage->data);
    const int dstStride = ximage->bytes_per_line;
    const int bpp = ximage->bits_per_pixel;

    const unsigned long rMask = ctx.visual->red_mask;
    const unsigned long gMask = ctx.visual->green_mask;
    const unsigned long bMask = ctx.visual->blue_mask;

    auto shiftOf = [](unsigned long mask) {
        int shift = 0;
        if (!mask) return 0;
        while ((mask & 1ul) == 0ul) {
            mask >>= 1;
            ++shift;
        }
        return shift;
    };
    auto bitsOf = [](unsigned long mask) {
        int bits = 0;
        while (mask) {
            bits += static_cast<int>(mask & 1ul);
            mask >>= 1;
        }
        return bits;
    };
    auto scale = [](unsigned char v, int bits) -> unsigned long {
        if (bits <= 0) return 0;
        if (bits >= 8) return static_cast<unsigned long>(v) << (bits - 8);
        const unsigned long maxv = (1ul << bits) - 1ul;
        return (static_cast<unsigned long>(v) * maxv + 127ul) / 255ul;
    };

    const int rShift = shiftOf(rMask);
    const int gShift = shiftOf(gMask);
    const int bShift = shiftOf(bMask);
    const int rBits = bitsOf(rMask);
    const int gBits = bitsOf(gMask);
    const int bBits = bitsOf(bMask);

    for (int y = 0; y < height; ++y) {
        const int srcY = height - 1 - y;
        const PixelValue* srcRow = pixels + srcY * width;
        uint8_t* dstRow = dstBase + static_cast<size_t>(y) * static_cast<size_t>(dstStride);

        for (int x = 0; x < width; ++x) {
            const PixelValue& p = srcRow[x];
            const unsigned long pixel =
                (scale(p.r, rBits) << rShift) |
                (scale(p.g, gBits) << gShift) |
                (scale(p.b, bBits) << bShift);

            if (bpp == 32) {
                auto* dst = reinterpret_cast<uint32_t*>(dstRow + x * 4);
                if (ximage->byte_order == MSBFirst) {
                    *dst = static_cast<uint32_t>(
                        ((pixel & 0xfful) << 24) |
                        ((pixel & 0xff00ul) << 8) |
                        ((pixel & 0xff0000ul) >> 8) |
                        ((pixel & 0xff000000ul) >> 24));
                } else {
                    *dst = static_cast<uint32_t>(pixel);
                }
            } else if (bpp == 16) {
                auto* dst = reinterpret_cast<uint16_t*>(dstRow + x * 2);
                if (ximage->byte_order == MSBFirst) {
                    const auto v = static_cast<uint16_t>(pixel);
                    *dst = static_cast<uint16_t>((v << 8) | (v >> 8));
                } else {
                    *dst = static_cast<uint16_t>(pixel);
                }
            } else if (bpp == 24) {
                uint8_t* px = dstRow + x * 3;
                if (ximage->byte_order == MSBFirst) {
                    px[0] = static_cast<uint8_t>((pixel >> 16) & 0xff);
                    px[1] = static_cast<uint8_t>((pixel >> 8) & 0xff);
                    px[2] = static_cast<uint8_t>(pixel & 0xff);
                } else {
                    px[0] = static_cast<uint8_t>(pixel & 0xff);
                    px[1] = static_cast<uint8_t>((pixel >> 8) & 0xff);
                    px[2] = static_cast<uint8_t>((pixel >> 16) & 0xff);
                }
            } else {
                XPutPixel(ximage, x, y, pixel);
            }
        }
    }
}

} // namespace

bool X11IsPixSoftWm(Display* dpy) {
    if (!dpy) return false;

    Atom atom = XInternAtom(dpy, PIXSOFTGL_WM_ATOM, True);
    if (atom == None) return false;

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long nitems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* prop = nullptr;

    const int status = XGetWindowProperty(
        dpy,
        DefaultRootWindow(dpy),
        atom,
        0,
        1,
        False,
        XA_CARDINAL,
        &actualType,
        &actualFormat,
        &nitems,
        &bytesAfter,
        &prop);

    if (status == Success && prop) {
        XFree(prop);
        return actualType == XA_CARDINAL && nitems >= 1;
    }
    return false;
}

void X11DisplayInit(Display* dpy, Window win, int width, int height) {
    X11DisplayShutdown();

    ctx.dpy = dpy;
    ctx.win = win;
    ctx.width = width;
    ctx.height = height;
    ctx.active = (dpy != nullptr && win != 0);

    if (!ctx.active) return;

    XWindowAttributes attrs{};
    if (XGetWindowAttributes(dpy, win, &attrs)) {
        ctx.visual = attrs.visual;
        ctx.depth = attrs.depth;
        if (width <= 0) ctx.width = attrs.width;
        if (height <= 0) ctx.height = attrs.height;
    } else {
        const int screen = DefaultScreen(dpy);
        ctx.visual = DefaultVisual(dpy, screen);
        ctx.depth = DefaultDepth(dpy, screen);
    }

    ensureGc();
}

void X11DisplayShutdown() {
    destroyImage();
    if (gc && ctx.dpy) {
        XFreeGC(ctx.dpy, gc);
        gc = 0;
    }
    ctx = {};
}

X11DisplayContext* X11DisplayGetContext() {
    return ctx.active ? &ctx : nullptr;
}

bool X11DisplayPresent(const PixelValue* pixels, int width, int height) {
    if (!ctx.active || !pixels || width <= 0 || height <= 0 || !ctx.visual) {
        return false;
    }

    if (!ximage || ximage->width != width || ximage->height != height) {
        if (!createImage(width, height)) return false;
    }

    ensureGc();
    if (!gc) return false;

    convertAndFlip(pixels, width, height);

    if (useShm) {
        XShmPutImage(
            ctx.dpy,
            ctx.win,
            gc,
            ximage,
            0,
            0,
            0,
            0,
            static_cast<unsigned>(width),
            static_cast<unsigned>(height),
            False);
    } else {
        XPutImage(
            ctx.dpy,
            ctx.win,
            gc,
            ximage,
            0,
            0,
            0,
            0,
            static_cast<unsigned>(width),
            static_cast<unsigned>(height));
    }

    XFlush(ctx.dpy);
    return true;
}
