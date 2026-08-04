#include "x11_safe.h"

#include <X11/Xlib.h>

namespace {

int g_trappedError = Success;
unsigned long g_trappedSerial = 0;

int trapXError(Display*, XErrorEvent* ev) {
    g_trappedError = ev->error_code;
    g_trappedSerial = ev->serial;
    return 0;
}

bool getAttrs(Display* dpy, Window win, XWindowAttributes* attrs) {
    if (!dpy || win == None || !attrs) return false;

    XLockDisplay(dpy);
    g_trappedError = Success;
    g_trappedSerial = 0;
    XErrorHandler prev = XSetErrorHandler(trapXError);

    // Only treat errors from THIS request as failure (ignore stale BadWindow).
    const unsigned long serial = NextRequest(dpy);
    const Status ok = XGetWindowAttributes(dpy, win, attrs);
    XSync(dpy, False);

    XSetErrorHandler(prev);
    XUnlockDisplay(dpy);

    const bool ourError =
        g_trappedError != Success && g_trappedSerial == serial;
    return ok != 0 && !ourError;
}

} // namespace

bool X11SafeWindowExists(Display* dpy, Window win) {
    XWindowAttributes attrs{};
    return getAttrs(dpy, win, &attrs);
}

bool X11SafeGetWindowSize(Display* dpy, Window win, int* width, int* height) {
    if (!width || !height) return false;
    XWindowAttributes attrs{};
    if (!getAttrs(dpy, win, &attrs)) return false;
    if (attrs.width <= 0 || attrs.height <= 0) return false;
    *width = attrs.width;
    *height = attrs.height;
    return true;
}

bool X11SafeGetWindowInfo(
    Display* dpy,
    Window win,
    int* width,
    int* height,
    Visual** visual,
    int* depth) {
    XWindowAttributes attrs{};
    if (!getAttrs(dpy, win, &attrs)) return false;
    if (attrs.width <= 0 || attrs.height <= 0) return false;
    if (width) *width = attrs.width;
    if (height) *height = attrs.height;
    if (visual) *visual = attrs.visual;
    if (depth) *depth = attrs.depth;
    return true;
}
