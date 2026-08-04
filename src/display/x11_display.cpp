#include "x11_display.h"
#include "x11_safe.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
int g_presentError = Success;
bool g_visualFromWindow = false;
bool g_shmFailed = false;

bool debugPresent() {
    const char* v = std::getenv("PIXSOFTGL_DEBUG");
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

int trapPresentError(Display*, XErrorEvent* ev) {
    g_presentError = ev->error_code;
    return 0;
}

bool shmExtensionAvailable(Display* dpy) {
    int major = 0, minor = 0, pixmaps = 0;
    return XShmQueryVersion(dpy, &major, &minor, &pixmaps) != 0;
}

bool wantShm() {
    // Default OFF — MIT-SHM is unreliable under WSL/XWayland and often yields
    // a permanent black window. Opt in with PIXSOFTGL_SHM=1.
    if (g_shmFailed) return false;
    const char* noShm = std::getenv("PIXSOFTGL_NOSHM");
    if (noShm && noShm[0] != '\0' && std::strcmp(noShm, "0") != 0) return false;
    const char* yesShm = std::getenv("PIXSOFTGL_SHM");
    return yesShm && yesShm[0] != '\0' && std::strcmp(yesShm, "0") != 0;
}

void destroyImage() {
    if (ximage) {
        if (useShm) {
            if (ctx.dpy) XShmDetach(ctx.dpy, &shmInfo);
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

void destroyGc() {
    if (gc && ctx.dpy) {
        XFreeGC(ctx.dpy, gc);
        gc = 0;
    }
}

bool createImage(int width, int height) {
    destroyImage();
    if (!ctx.dpy || !ctx.visual || width <= 0 || height <= 0) return false;

    const size_t bytes =
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;

    if (wantShm() && shmExtensionAvailable(ctx.dpy)) {
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
        g_shmFailed = true;
        if (debugPresent()) {
            std::cerr << "PixSoftGL: MIT-SHM present setup failed; using XPutImage\n";
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
    if (gc || !ctx.dpy || !ctx.win) return;
    if (!X11SafeWindowExists(ctx.dpy, ctx.win)) return;

    g_presentError = Success;
    XErrorHandler prev = XSetErrorHandler(trapPresentError);
    gc = XCreateGC(ctx.dpy, ctx.win, 0, nullptr);
    XSync(ctx.dpy, False);
    XSetErrorHandler(prev);
    if (g_presentError != Success) {
        gc = 0;
    }
}

void packTopLeft(
    const PixelValue* pixels,
    int fbWidth,
    int fbHeight,
    int putW,
    int putH) {
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

    (void)fbHeight;
    // Software color buffer is already top-left (viewport converts from GL
    // bottom-left). Do not flip again or a smaller window on a larger FB
    // presents undrawn rows and looks permanently black.
    for (int y = 0; y < putH; ++y) {
        const PixelValue* srcRow = pixels + y * fbWidth;
        uint8_t* dstRow = dstBase + static_cast<size_t>(y) * static_cast<size_t>(dstStride);
        for (int x = 0; x < putW; ++x) {
            const PixelValue& p = srcRow[x];
            const unsigned long pixel =
                (scale(p.r, rBits) << rShift) |
                (scale(p.g, gBits) << gShift) |
                (scale(p.b, bBits) << bShift);
            if (bpp == 32) {
                *reinterpret_cast<uint32_t*>(dstRow + x * 4) = static_cast<uint32_t>(pixel);
            } else if (bpp == 16) {
                *reinterpret_cast<uint16_t*>(dstRow + x * 2) = static_cast<uint16_t>(pixel);
            } else {
                XPutPixel(ximage, x, y, pixel);
            }
        }
    }
}

bool applyWindowVisual(Display* dpy, Window win) {
    int probedW = 0;
    int probedH = 0;
    Visual* visual = nullptr;
    int depth = 0;
    if (!X11SafeGetWindowInfo(dpy, win, &probedW, &probedH, &visual, &depth)) {
        return false;
    }

    const bool visualChanged =
        !g_visualFromWindow || ctx.visual != visual || ctx.depth != depth;
    ctx.visual = visual;
    ctx.depth = depth;
    g_visualFromWindow = true;
    if (ctx.width <= 0) ctx.width = probedW;
    if (ctx.height <= 0) ctx.height = probedH;

    if (visualChanged) {
        destroyImage();
        destroyGc();
    }
    return true;
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
    if (ctx.active && ctx.dpy == dpy && ctx.win == win) {
        if (width > 0) ctx.width = width;
        if (height > 0) ctx.height = height;
        // Re-probe visual once the window is queryable — first bind often
        // happens before map and fell back to DefaultVisual (BadMatch → black).
        if (!g_visualFromWindow) {
            applyWindowVisual(dpy, win);
            ensureGc();
        }
        return;
    }

    X11DisplayShutdown();

    ctx.dpy = dpy;
    ctx.win = win;
    ctx.width = width;
    ctx.height = height;
    ctx.active = (dpy != nullptr && win != 0);
    g_visualFromWindow = false;

    if (!ctx.active) return;

    if (!applyWindowVisual(dpy, win)) {
        const int screen = DefaultScreen(dpy);
        ctx.visual = DefaultVisual(dpy, screen);
        ctx.depth = DefaultDepth(dpy, screen);
        g_visualFromWindow = false;
        if (debugPresent()) {
            std::cerr << "PixSoftGL: window not queryable yet; using DefaultVisual\n";
        }
    }

    ensureGc();
}

void X11DisplayShutdown() {
    destroyImage();
    destroyGc();
    ctx = {};
    g_visualFromWindow = false;
}

X11DisplayContext* X11DisplayGetContext() {
    return ctx.active ? &ctx : nullptr;
}

bool X11DisplayPresent(const PixelValue* pixels, int width, int height) {
    if (!ctx.active || !pixels || width <= 0 || height <= 0 || !ctx.visual) {
        if (debugPresent()) {
            std::cerr << "PixSoftGL: present skip (inactive/no buffer)\n";
        }
        return false;
    }

    // Upgrade DefaultVisual fallback as soon as the window is alive.
    if (!g_visualFromWindow) {
        applyWindowVisual(ctx.dpy, ctx.win);
    }

    if (!X11SafeWindowExists(ctx.dpy, ctx.win)) {
        if (debugPresent()) {
            std::cerr << "PixSoftGL: present skip (BadWindow/missing)\n";
        }
        return false;
    }

    int putW = width;
    int putH = height;
    int winW = 0;
    int winH = 0;
    if (X11SafeGetWindowSize(ctx.dpy, ctx.win, &winW, &winH)) {
        if (winW < putW) putW = winW;
        if (winH < putH) putH = winH;
    }
    if (putW <= 0 || putH <= 0) return false;

    if (!ximage || ximage->width != putW || ximage->height != putH) {
        if (!createImage(putW, putH)) {
            if (debugPresent()) std::cerr << "PixSoftGL: createImage failed\n";
            return false;
        }
    }

    ensureGc();
    if (!gc) {
        if (debugPresent()) std::cerr << "PixSoftGL: present skip (no GC)\n";
        return false;
    }

    packTopLeft(pixels, width, height, putW, putH);

    // Count a few non-black samples for diagnostics.
    unsigned nonBlack = 0;
    if (debugPresent()) {
        const unsigned step = std::max(1, (putW * putH) / 64);
        for (unsigned i = 0; i < static_cast<unsigned>(putW * putH); i += step) {
            const PixelValue& p = pixels[(i / putW) * width + (i % putW)];
            if (p.r | p.g | p.b) ++nonBlack;
        }
    }

    g_presentError = Success;
    XErrorHandler prev = XSetErrorHandler(trapPresentError);

    bool ok = false;
    if (useShm) {
        ok = XShmPutImage(
            ctx.dpy, ctx.win, gc, ximage,
            0, 0, 0, 0,
            static_cast<unsigned>(putW), static_cast<unsigned>(putH),
            False) != 0;
    } else {
        XPutImage(
            ctx.dpy, ctx.win, gc, ximage,
            0, 0, 0, 0,
            static_cast<unsigned>(putW), static_cast<unsigned>(putH));
        ok = true;
    }

    XFlush(ctx.dpy);
    // Avoid XSync every frame — it serializes the CPU with the X server and
    // is brutal on slow hosts. Errors are still trapped via the handler on
    // the next round-trip; enable PIXSOFTGL_DEBUG for occasional sync.
    if (debugPresent()) {
        XSync(ctx.dpy, False);
    }
    XSetErrorHandler(prev);

    if (g_presentError != Success) {
        if (debugPresent()) {
            std::cerr << "PixSoftGL: present X error " << g_presentError
                      << " (shm=" << useShm << ")\n";
        }
        if (useShm) {
            g_shmFailed = true;
            destroyImage();
        }
        return false;
    }

    if (debugPresent()) {
        static int frames = 0;
        if (frames < 5 || (frames % 60) == 0) {
            std::cerr << "PixSoftGL: present ok fb=" << width << 'x' << height
                      << " put=" << putW << 'x' << putH
                      << " win=" << winW << 'x' << winH
                      << " nonBlack~" << nonBlack
                      << " shm=" << useShm
                      << " depth=" << ctx.depth
                      << " frame=" << frames << '\n';
        }
        ++frames;
    }

    return ok;
}
