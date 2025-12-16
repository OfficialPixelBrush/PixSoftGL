#include <dlfcn.h>
#include <GL/glx.h>
#include <cstring>

#include "lib.h"

typedef void (*GLFuncPtr)(void);

static void *real_libGL = nullptr;

static void ensure_real_lib() {
    if (!real_libGL) {
        real_libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_DEEPBIND);
    }
}

extern "C" {

GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext share, Bool direct) {
    ensure_real_lib();
    using PF = GLXContext(*)(Display*, XVisualInfo*, GLXContext, Bool);
    PF orig = (PF)dlsym(real_libGL, "glXCreateContext");
    return orig(dpy, vis, share, direct);
}

GLFuncPtr glXGetProcAddress(const GLubyte *name) {
    ensure_real_lib();

    if (std::strcmp((const char*)name, "glVertex2f") == 0) {
        return (GLFuncPtr)glVertex2f; // your custom entry point
    }

    auto orig = (GLFuncPtr(*)(const GLubyte*))dlsym(real_libGL, "glXGetProcAddress");
    return orig(name);
}

}
