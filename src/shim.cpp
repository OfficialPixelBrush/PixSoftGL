#include <dlfcn.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "display/x11_display.h"
#include "display/x11_safe.h"
#include "framebuffer.h"
#include "global.h"
#include "lib.h"

namespace {

struct PixSoftGLXContext {
    Display* dpy = nullptr;
    Window win = 0;
    int width = 0;
    int height = 0;
    bool direct = true;
};

PixSoftGLXContext currentCtx{};
GLXContext currentGlxContext = nullptr;
bool modeResolved = false;
bool softwareGlx = true;

void* realLibGL() {
    static void* handle = nullptr;
    if (handle) return handle;

    // Never dlopen("libGL.so.1") by soname — with LD_LIBRARY_PATH pointing at
    // PixSoftGL that would re-open ourselves. Prefer known system paths.
    static const char* candidates[] = {
        "/usr/lib/x86_64-linux-gnu/libGL.so.1",
        "/usr/lib64/libGL.so.1",
        "/usr/lib/libGL.so.1",
        "/lib/x86_64-linux-gnu/libGL.so.1",
        nullptr
    };
    for (const char** p = candidates; *p; ++p) {
        handle = dlopen(*p, RTLD_LAZY | RTLD_LOCAL);
        if (handle) return handle;
    }

    // Last resort: search without our shim directory on LD_LIBRARY_PATH.
    const char* oldPath = std::getenv("LD_LIBRARY_PATH");
    std::string restored = oldPath ? oldPath : "";
    if (oldPath && *oldPath) {
        setenv("LD_LIBRARY_PATH", "", 1);
    }
    handle = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (oldPath) {
        setenv("LD_LIBRARY_PATH", restored.c_str(), 1);
    } else {
        unsetenv("LD_LIBRARY_PATH");
    }
    return handle;
}

template<typename Fn>
Fn realFn(const char* name) {
    void* lib = realLibGL();
    if (!lib) return nullptr;
    return reinterpret_cast<Fn>(dlsym(lib, name));
}

bool envFlag(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

void resolveMode(Display* dpy) {
    // PIXSOFTGL_FORWARD=1 → hand GLX back to the system driver.
    if (envFlag("PIXSOFTGL_FORWARD")) {
        softwareGlx = false;
        modeResolved = true;
        return;
    }

    // Explicit opt-in still supported.
    if (envFlag("PIXSOFTGL_WM")) {
        softwareGlx = true;
        forwardToSystemGl = false;
        modeResolved = true;
        return;
    }

    // Auto-detect bare-bones PixSoftWM via root-window property.
    if (dpy && X11IsPixSoftWm(dpy)) {
        softwareGlx = true;
        forwardToSystemGl = false;
        modeResolved = true;
        return;
    }

    // This library is a software GL implementation loaded via LD_PRELOAD.
    // Default to owning GLX as well so contexts and SwapBuffers match
    // the software renderer (forwardToSystemGl is false on this branch).
    softwareGlx = true;
    forwardToSystemGl = false;
    modeResolved = true;
}

bool useSoftwareGlx(Display* dpy) {
    if (!modeResolved) resolveMode(dpy);
    return softwareGlx;
}

XVisualInfo* chooseSoftwareVisual(Display* dpy, int screen) {
    XWindowAttributes attrs{};
    Window root = RootWindow(dpy, screen);
    if (XGetWindowAttributes(dpy, root, &attrs) != 0) {
        XVisualInfo templateInfo{};
        templateInfo.visualid = XVisualIDFromVisual(attrs.visual);
        templateInfo.screen = screen;
        int count = 0;
        XVisualInfo* info = XGetVisualInfo(
            dpy, VisualIDMask | VisualScreenMask, &templateInfo, &count);
        if (info && count > 0) return info;
        if (info) XFree(info);
    }

    XVisualInfo templateInfo{};
    templateInfo.screen = screen;
    templateInfo.c_class = TrueColor;
    templateInfo.depth = DefaultDepth(dpy, screen);
    int count = 0;
    XVisualInfo* info = XGetVisualInfo(
        dpy,
        VisualScreenMask | VisualClassMask | VisualDepthMask,
        &templateInfo,
        &count);
    if (info && count > 0) return info;
    if (info) XFree(info);

    templateInfo = {};
    templateInfo.screen = screen;
    return XGetVisualInfo(dpy, VisualScreenMask, &templateInfo, &count);
}

} // namespace

extern "C" {

Bool glXQueryExtension(Display* dpy, int* errorBase, int* eventBase) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<Bool(*)(Display*, int*, int*)>("glXQueryExtension");
        return fn ? fn(dpy, errorBase, eventBase) : False;
    }
    if (errorBase) *errorBase = 0;
    if (eventBase) *eventBase = 0;
    return True;
}

Bool glXQueryVersion(Display* dpy, int* major, int* minor) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<Bool(*)(Display*, int*, int*)>("glXQueryVersion");
        return fn ? fn(dpy, major, minor) : False;
    }
    // Advertise 1.2 so LWJGL uses the legacy path where the GLX drawable is
    // the X11 Window. Reporting 1.3+ makes LWJGL call system glXCreateWindow
    // (via GetProcAddress fallthrough), store a GLXWindow XID, and every
    // XGetWindowAttributes/PutImage then fails with BadWindow → blank screen.
    if (major) *major = 1;
    if (minor) *minor = 2;
    return True;
}

int glXGetConfig(Display* dpy, XVisualInfo* vis, int attrib, int* value) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<int(*)(Display*, XVisualInfo*, int, int*)>("glXGetConfig");
        return fn ? fn(dpy, vis, attrib, value) : GLX_BAD_ATTRIBUTE;
    }
    if (!value) return GLX_BAD_VALUE;

    switch (attrib) {
    case GLX_USE_GL:        *value = 1; break;
    case GLX_BUFFER_SIZE:   *value = 24; break;
    case GLX_LEVEL:         *value = 0; break;
    case GLX_RGBA:          *value = 1; break;
    case GLX_DOUBLEBUFFER:  *value = 1; break;
    case GLX_STEREO:        *value = 0; break;
    case GLX_AUX_BUFFERS:   *value = 0; break;
    case GLX_RED_SIZE:      *value = 8; break;
    case GLX_GREEN_SIZE:    *value = 8; break;
    case GLX_BLUE_SIZE:     *value = 8; break;
    case GLX_ALPHA_SIZE:    *value = 0; break;
    case GLX_DEPTH_SIZE:    *value = 24; break;
    case GLX_STENCIL_SIZE:  *value = 0; break;
    case GLX_ACCUM_RED_SIZE:
    case GLX_ACCUM_GREEN_SIZE:
    case GLX_ACCUM_BLUE_SIZE:
    case GLX_ACCUM_ALPHA_SIZE:
        *value = 0;
        break;
    default:
        *value = 0;
        return GLX_BAD_ATTRIBUTE;
    }
    (void)vis;
    return 0;
}

XVisualInfo* glXChooseVisual(Display* dpy, int screen, int* attribList) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<XVisualInfo*(*)(Display*, int, int*)>("glXChooseVisual");
        return fn ? fn(dpy, screen, attribList) : nullptr;
    }
    (void)attribList;
    return chooseSoftwareVisual(dpy, screen);
}

GLXContext glXCreateContext(Display* dpy, XVisualInfo* vis, GLXContext share, Bool direct) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<GLXContext(*)(Display*, XVisualInfo*, GLXContext, Bool)>("glXCreateContext");
        return fn ? fn(dpy, vis, share, direct) : nullptr;
    }
    (void)vis;
    (void)share;
    auto* ctx = new PixSoftGLXContext{};
    ctx->dpy = dpy;
    ctx->direct = direct != False;
    return reinterpret_cast<GLXContext>(ctx);
}

Bool glXMakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<Bool(*)(Display*, GLXDrawable, GLXContext)>("glXMakeCurrent");
        return fn ? fn(dpy, drawable, ctx) : False;
    }

    if (!ctx || drawable == None) {
        currentCtx = {};
        currentGlxContext = nullptr;
        X11DisplayShutdown();
        return True;
    }

    auto* pixCtx = reinterpret_cast<PixSoftGLXContext*>(ctx);
    pixCtx->dpy = dpy;
    pixCtx->win = static_cast<Window>(drawable);

    int width = 0;
    int height = 0;
    if (X11SafeGetWindowSize(dpy, pixCtx->win, &width, &height)) {
        pixCtx->width = width;
        pixCtx->height = height;
    }

    // Prefer last known size over full-screen guess — a too-large FB + clipped
    // present used to sample empty rows and look permanently black.
    if (pixCtx->width <= 0 || pixCtx->height <= 0) {
        if (currentCtx.width > 0 && currentCtx.height > 0 &&
            currentCtx.win == pixCtx->win) {
            pixCtx->width = currentCtx.width;
            pixCtx->height = currentCtx.height;
        } else {
            pixCtx->width = 854;
            pixCtx->height = 480;
        }
    }

    currentCtx = *pixCtx;
    currentGlxContext = ctx;
    X11DisplayInit(dpy, pixCtx->win, pixCtx->width, pixCtx->height);
    EnsureRenderBuffers(pixCtx->width, pixCtx->height);
    return True;
}

void glXDestroyContext(Display* dpy, GLXContext ctx) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<void(*)(Display*, GLXContext)>("glXDestroyContext");
        if (fn) fn(dpy, ctx);
        return;
    }
    if (!ctx) return;
    auto* pixCtx = reinterpret_cast<PixSoftGLXContext*>(ctx);
    if (currentGlxContext == ctx) {
        currentGlxContext = nullptr;
        currentCtx = {};
        X11DisplayShutdown();
    } else if (currentCtx.dpy == pixCtx->dpy && currentCtx.win == pixCtx->win) {
        currentCtx = {};
        X11DisplayShutdown();
    }
    delete pixCtx;
}

void glXCopyContext(Display* dpy, GLXContext src, GLXContext dst, unsigned long mask) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<void(*)(Display*, GLXContext, GLXContext, unsigned long)>("glXCopyContext");
        if (fn) fn(dpy, src, dst, mask);
        return;
    }
    (void)src;
    (void)dst;
    (void)mask;
}

GLXPixmap glXCreateGLXPixmap(Display* dpy, XVisualInfo* vis, Pixmap pixmap) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<GLXPixmap(*)(Display*, XVisualInfo*, Pixmap)>("glXCreateGLXPixmap");
        return fn ? fn(dpy, vis, pixmap) : None;
    }
    (void)vis;
    return static_cast<GLXPixmap>(pixmap);
}

void glXDestroyGLXPixmap(Display* dpy, GLXPixmap pix) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<void(*)(Display*, GLXPixmap)>("glXDestroyGLXPixmap");
        if (fn) fn(dpy, pix);
        return;
    }
    (void)pix;
}

GLXContext glXGetCurrentContext() {
    if (!softwareGlx && modeResolved) {
        auto fn = realFn<GLXContext(*)()>("glXGetCurrentContext");
        return fn ? fn() : nullptr;
    }
    return currentGlxContext;
}

GLXDrawable glXGetCurrentDrawable() {
    if (!softwareGlx && modeResolved) {
        auto fn = realFn<GLXDrawable(*)()>("glXGetCurrentDrawable");
        return fn ? fn() : None;
    }
    return currentCtx.win;
}

void glXWaitGL() {
    if (!softwareGlx && modeResolved) {
        auto fn = realFn<void(*)()>("glXWaitGL");
        if (fn) fn();
    }
}

void glXWaitX() {
    if (!softwareGlx && modeResolved) {
        auto fn = realFn<void(*)()>("glXWaitX");
        if (fn) fn();
    }
}

void glXUseXFont(Font font, int first, int count, int listBase) {
    if (!softwareGlx && modeResolved) {
        auto fn = realFn<void(*)(Font, int, int, int)>("glXUseXFont");
        if (fn) fn(font, first, count, listBase);
        return;
    }
    (void)font;
    (void)first;
    (void)count;
    (void)listBase;
}

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<void(*)(Display*, GLXDrawable)>("glXSwapBuffers");
        if (fn) fn(dpy, drawable);
        return;
    }

    // Prefer the drawable being swapped (LWJGL passes this). Never clear the
    // software color buffer here — that wiped finished frames.
    //
    // Never let BadWindow reach LWJGL's X error handler (Display.create swaps
    // once during initContext).
    Window win = drawable ? static_cast<Window>(drawable) : currentCtx.win;
    if (dpy && win) {
        int width = 0;
        int height = 0;
        if (X11SafeGetWindowSize(dpy, win, &width, &height)) {
            currentCtx.dpy = dpy;
            currentCtx.win = win;
            currentCtx.width = width;
            currentCtx.height = height;
            X11DisplayInit(dpy, win, width, height);
            // Keep the software FB matched to the live window. Mismatched sizes
            // + clipped present previously showed empty (black) regions.
            if (width != renderAreaWidth || height != renderAreaHeight) {
                EnsureRenderBuffers(width, height);
            }
        } else if (currentCtx.win == None || currentCtx.dpy != dpy) {
            currentCtx.dpy = dpy;
            currentCtx.win = win;
            if (currentCtx.width <= 0) currentCtx.width = 854;
            if (currentCtx.height <= 0) currentCtx.height = 480;
            X11DisplayInit(dpy, win, currentCtx.width, currentCtx.height);
            EnsureRenderBuffers(currentCtx.width, currentCtx.height);
        } else {
            // Same window, not queryable this frame — still try present.
            X11DisplayInit(dpy, win, currentCtx.width, currentCtx.height);
        }
    }
    UpdateScreen();
}

Bool glXIsDirect(Display* dpy, GLXContext ctx) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<Bool(*)(Display*, GLXContext)>("glXIsDirect");
        return fn ? fn(dpy, ctx) : False;
    }
    if (!ctx) return False;
    return reinterpret_cast<PixSoftGLXContext*>(ctx)->direct ? True : False;
}

const char* glXQueryExtensionsString(Display* dpy, int screen) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<const char*(*)(Display*, int)>("glXQueryExtensionsString");
        return fn ? fn(dpy, screen) : "";
    }
    (void)screen;
    return "GLX_ARB_get_proc_address";
}

const char* glXGetClientString(Display* dpy, int name) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<const char*(*)(Display*, int)>("glXGetClientString");
        return fn ? fn(dpy, name) : nullptr;
    }
    switch (name) {
    case GLX_VENDOR: return "PixSoftGL";
    case GLX_VERSION: return "1.2 PixSoftGL";
    case GLX_EXTENSIONS: return "GLX_ARB_get_proc_address";
    default: return nullptr;
    }
}

const char* glXQueryServerString(Display* dpy, int screen, int name) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<const char*(*)(Display*, int, int)>("glXQueryServerString");
        return fn ? fn(dpy, screen, name) : nullptr;
    }
    (void)screen;
    return glXGetClientString(dpy, name);
}

typedef void (*GLFuncPtr)(void);

// LWJGL ContextCapabilities requires a non-null pointer for every GL 1.1 entry.
// Unimplemented commands get this no-op; x86-64 ignores unused args. Prefer
// real exports from our .so whenever present.
static void glUnimplementedStub() {}

static void* selfLibHandle() {
    static void* handle = nullptr;
    static bool attempted = false;
    if (attempted) return handle;
    attempted = true;

    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&glXQueryVersion), &info) && info.dli_fname) {
        handle = dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD);
        if (!handle) {
            handle = dlopen(info.dli_fname, RTLD_NOW | RTLD_LOCAL);
        }
    }
    return handle;
}

static bool isGlxName(const char* name) {
    return name && name[0] == 'g' && name[1] == 'l' && name[2] == 'X';
}

static bool isGlName(const char* name) {
    return name && name[0] == 'g' && name[1] == 'l' && name[2] != '\0';
}

static GLFuncPtr lookupLocalGl(const char* name) {
    if (void* self = selfLibHandle()) {
        if (void* sym = dlsym(self, name)) {
            return reinterpret_cast<GLFuncPtr>(sym);
        }
    }

    // Our lib is LD_PRELOAD'd / opened as libGL.so.1 (global). System libGL is
    // opened RTLD_LOCAL by realLibGL(), so it should not appear here — but never
    // accept glX* from the default namespace (avoids a real GLXWindow drawable).
    if (!isGlxName(name)) {
        if (void* sym = dlsym(RTLD_DEFAULT, name)) {
            return reinterpret_cast<GLFuncPtr>(sym);
        }
    }
    return nullptr;
}

GLFuncPtr glXGetProcAddress(const GLubyte* name) {
    Display* dpy = currentCtx.dpy;
    const char* str = reinterpret_cast<const char*>(name);
    if (!str) return nullptr;

    if (useSoftwareGlx(dpy)) {
        if (GLFuncPtr local = lookupLocalGl(str)) {
            return local;
        }
        // LWJGL refuses to create a context unless every GL11 stub resolves.
        // Return a no-op for unimplemented gl* commands; keep glX* as null so
        // LWJGL cannot pick up system GLX 1.3 entry points.
        if (isGlName(str) && !isGlxName(str)) {
            return reinterpret_cast<GLFuncPtr>(&glUnimplementedStub);
        }
        return nullptr;
    }

    auto orig = realFn<GLFuncPtr(*)(const GLubyte*)>("glXGetProcAddress");
    if (orig) {
        if (GLFuncPtr fn = orig(name)) return fn;
    }
    return lookupLocalGl(str);
}

GLFuncPtr glXGetProcAddressARB(const GLubyte* name) {
    return glXGetProcAddress(name);
}

}
