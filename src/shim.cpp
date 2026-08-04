#include <dlfcn.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <cstdlib>
#include <cstring>

#include "display/x11_display.h"
#include "global.h"
#include "lib.h"

namespace {

struct PixSoftGLXContext {
    Display* dpy = nullptr;
    Window win = 0;
    int width = 0;
    int height = 0;
    bool direct = false;
};

static PixSoftGLXContext currentCtx{};
static bool wmMode = false;

static void refreshWmMode() {
    wmMode = std::getenv("PIXSOFTGL_WM") != nullptr;
    if (wmMode) forwardToSystemGl = false;
}

static void* realLibGL() {
    static void* handle = dlopen("libGL.so.1", RTLD_LAZY | RTLD_DEEPBIND);
    return handle;
}

template<typename Fn>
Fn realFn(const char* name) {
    return reinterpret_cast<Fn>(dlsym(realLibGL(), name));
}

} // namespace

extern "C" {

XVisualInfo* glXChooseVisual(Display* dpy, int screen, int* attribList) {
    refreshWmMode();
    if (!wmMode) {
        return realFn<XVisualInfo*(*)(Display*, int, int*)>("glXChooseVisual")(dpy, screen, attribList);
    }

    static int defaultAttribs[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };

    if (!attribList) attribList = defaultAttribs;
    return XGetVisualInfo(dpy, VisualScreenMask, nullptr, &screen);
}

GLXContext glXCreateContext(Display* dpy, XVisualInfo* vis, GLXContext share, Bool direct) {
    refreshWmMode();
    if (!wmMode) {
        return realFn<GLXContext(*)(Display*, XVisualInfo*, GLXContext, Bool)>("glXCreateContext")(
            dpy, vis, share, direct);
    }

    PixSoftGLXContext* ctx = new PixSoftGLXContext{};
    ctx->dpy = dpy;
    ctx->direct = direct;
    return reinterpret_cast<GLXContext>(ctx);
}

Bool glXMakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx) {
    refreshWmMode();
    if (!wmMode) {
        return realFn<Bool(*)(Display*, GLXDrawable, GLXContext)>("glXMakeCurrent")(dpy, drawable, ctx);
    }

    if (!ctx) {
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

    currentCtx = *pixCtx;
    X11DisplayInit(dpy, pixCtx->win, pixCtx->width, pixCtx->height);
    return True;
}

void glXDestroyContext(Display* dpy, GLXContext ctx) {
    refreshWmMode();
    if (!wmMode) {
        realFn<void(*)(Display*, GLXContext)>("glXDestroyContext")(dpy, ctx);
        return;
    }

    delete reinterpret_cast<PixSoftGLXContext*>(ctx);
}

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    refreshWmMode();
    if (!wmMode) {
        realFn<void(*)(Display*, GLXDrawable)>("glXSwapBuffers")(dpy, drawable);
        return;
    }

    (void)drawable;
    UpdateScreen();
}

const char* glXQueryExtensionsString(Display* dpy, int screen) {
    refreshWmMode();
    if (!wmMode) {
        return realFn<const char*(*)(Display*, int)>("glXQueryExtensionsString")(dpy, screen);
    }
    return "";
}

const char* glXGetClientString(Display* dpy, int name) {
    refreshWmMode();
    if (!wmMode) {
        return realFn<const char*(*)(Display*, int)>("glXGetClientString")(dpy, name);
    }

    switch (name) {
    case GLX_VENDOR: return "PixSoftGL";
    case GLX_VERSION: return "1.4";
    default: return nullptr;
    }
}

typedef void (*GLFuncPtr)(void);

GLFuncPtr glXGetProcAddress(const GLubyte* name) {
    refreshWmMode();

    if (std::strcmp(reinterpret_cast<const char*>(name), "glVertex2f") == 0) {
        return reinterpret_cast<GLFuncPtr>(glVertex2f);
    }
    if (std::strcmp(reinterpret_cast<const char*>(name), "glVertex3f") == 0) {
        return reinterpret_cast<GLFuncPtr>(glVertex3f);
    }
    if (std::strcmp(reinterpret_cast<const char*>(name), "glVertex3fv") == 0) {
        return reinterpret_cast<GLFuncPtr>(glVertex3fv);
    }

    if (wmMode) {
        void* sym = dlsym(RTLD_DEFAULT, reinterpret_cast<const char*>(name));
        if (sym) return reinterpret_cast<GLFuncPtr>(sym);
    }

    auto orig = realFn<GLFuncPtr(*)(const GLubyte*)>("glXGetProcAddress");
    return orig ? orig(name) : nullptr;
}

}
