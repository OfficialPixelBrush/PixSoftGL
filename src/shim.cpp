#include <dlfcn.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>
#include <cstring>

#include "display/x11_display.h"
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
bool modeResolved = false;
bool softwareGlx = true;

void* realLibGL() {
    static void* handle = nullptr;
    if (!handle) {
        handle = dlopen("libGL.so.1", RTLD_LAZY | RTLD_DEEPBIND);
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
    if (major) *major = 1;
    if (minor) *minor = 4;
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
        X11DisplayShutdown();
        return True;
    }

    auto* pixCtx = reinterpret_cast<PixSoftGLXContext*>(ctx);
    pixCtx->dpy = dpy;
    pixCtx->win = static_cast<Window>(drawable);

    XWindowAttributes attrs{};
    if (XGetWindowAttributes(dpy, pixCtx->win, &attrs)) {
        pixCtx->width = attrs.width;
        pixCtx->height = attrs.height;
    }

    // Under PixSoftWM clients are forced fullscreen to the FB/screen size.
    // Fall back to a sensible default if the window is not mapped yet.
    if (pixCtx->width <= 0 || pixCtx->height <= 0) {
        const int screen = DefaultScreen(dpy);
        pixCtx->width = DisplayWidth(dpy, screen);
        pixCtx->height = DisplayHeight(dpy, screen);
    }

    currentCtx = *pixCtx;
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
    if (currentCtx.dpy == pixCtx->dpy && currentCtx.win == pixCtx->win) {
        currentCtx = {};
        X11DisplayShutdown();
    }
    delete pixCtx;
}

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    if (!useSoftwareGlx(dpy)) {
        auto fn = realFn<void(*)(Display*, GLXDrawable)>("glXSwapBuffers");
        if (fn) fn(dpy, drawable);
        return;
    }
    (void)drawable;

    // Refresh size in case the WM resized us to fullscreen after MapRequest.
    if (currentCtx.win) {
        XWindowAttributes attrs{};
        if (XGetWindowAttributes(dpy, currentCtx.win, &attrs)) {
            if (attrs.width != currentCtx.width || attrs.height != currentCtx.height) {
                currentCtx.width = attrs.width;
                currentCtx.height = attrs.height;
                X11DisplayInit(dpy, currentCtx.win, currentCtx.width, currentCtx.height);
                EnsureRenderBuffers(currentCtx.width, currentCtx.height);
            }
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
    case GLX_VERSION: return "1.4 PixSoftGL";
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

static GLFuncPtr lookupLocalGl(const char* name) {
    // Resolve against our own DT_SYMTAB (LD_PRELOAD exports).
    void* sym = dlsym(RTLD_DEFAULT, name);
    return reinterpret_cast<GLFuncPtr>(sym);
}

GLFuncPtr glXGetProcAddress(const GLubyte* name) {
    Display* dpy = currentCtx.dpy;
    const char* str = reinterpret_cast<const char*>(name);

    if (useSoftwareGlx(dpy)) {
        if (GLFuncPtr local = lookupLocalGl(str)) {
            return local;
        }
    }

    auto orig = realFn<GLFuncPtr(*)(const GLubyte*)>("glXGetProcAddress");
    return orig ? orig(name) : lookupLocalGl(str);
}

GLFuncPtr glXGetProcAddressARB(const GLubyte* name) {
    return glXGetProcAddress(name);
}

}
