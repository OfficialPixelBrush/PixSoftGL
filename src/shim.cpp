#include <dlfcn.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <cstring>

#include "global.h"
#include "lib.h"
#include "framebuffer.h"

namespace {

struct PixSoftGLXContext {
    Display* dpy = nullptr;
    Window win = 0;
    int width = 0;
    int height = 0;
    bool direct = false;
};

static PixSoftGLXContext currentCtx{};

} // namespace

extern "C" {

XVisualInfo* glXChooseVisual(Display* dpy, int screen, int* attribList) {
    (void)attribList;

    // The X visual is retained only for source compatibility with programs
    // that create an X drawable before calling GLX. Rendering itself goes to
    // the Linux framebuffer.
    XVisualInfo templ{};
    templ.screen = screen;
    templ.depth = 24;
    templ.c_class = TrueColor;
    int count = 0;

    XVisualInfo* result = XGetVisualInfo(
        dpy,
        VisualScreenMask | VisualDepthMask | VisualClassMask,
        &templ,
        &count);
    if (result && count > 0) return result;

    templ = {};
    templ.screen = screen;
    return XGetVisualInfo(dpy, VisualScreenMask, &templ, &count);
}

GLXContext glXCreateContext(Display* dpy, XVisualInfo* vis,
                            GLXContext share, Bool direct) {
    (void)vis;
    (void)share;

    auto* ctx = new PixSoftGLXContext{};
    ctx->dpy = dpy;
    ctx->direct = direct;
    return reinterpret_cast<GLXContext>(ctx);
}

Bool glXMakeCurrent(Display* dpy, GLXDrawable drawable, GLXContext ctx) {
    if (!ctx) {
        currentCtx = {};
        return True;
    }

    auto* pixCtx = reinterpret_cast<PixSoftGLXContext*>(ctx);
    pixCtx->dpy = dpy;
    pixCtx->win = static_cast<Window>(drawable);

    FbDevice* fb = GetFramebufferDevice();
    if (!fb) return False;

    pixCtx->width = fb->width();
    pixCtx->height = fb->height();
    currentCtx = *pixCtx;

    // OpenGL's default viewport follows the active framebuffer.
    Process_glViewport(0, 0, pixCtx->width, pixCtx->height);
    return True;
}

void glXDestroyContext(Display* dpy, GLXContext ctx) {
    (void)dpy;
    delete reinterpret_cast<PixSoftGLXContext*>(ctx);
}

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    (void)dpy;
    (void)drawable;
    UpdateScreen();
}

const char* glXQueryExtensionsString(Display* dpy, int screen) {
    (void)dpy;
    (void)screen;
    return "";
}

const char* glXGetClientString(Display* dpy, int name) {
    (void)dpy;

    switch (name) {
    case GLX_VENDOR: return "PixSoftGL";
    case GLX_VERSION: return "1.4";
    default: return nullptr;
    }
}

typedef void (*GLFuncPtr)(void);

GLFuncPtr glXGetProcAddress(const GLubyte* name) {
    if (!name) return nullptr;

    const char* symbol = reinterpret_cast<const char*>(name);
    if (std::strcmp(symbol, "glVertex2f") == 0)
        return reinterpret_cast<GLFuncPtr>(glVertex2f);
    if (std::strcmp(symbol, "glVertex3f") == 0)
        return reinterpret_cast<GLFuncPtr>(glVertex3f);
    if (std::strcmp(symbol, "glVertex3fv") == 0)
        return reinterpret_cast<GLFuncPtr>(glVertex3fv);

    return reinterpret_cast<GLFuncPtr>(dlsym(RTLD_DEFAULT, symbol));
}

}
