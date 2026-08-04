// glx_gl11_tests.cpp
// Minimal OpenGL 1.1 test-suite using X11 + GLX only.
//
// Allowed includes per user:
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <unistd.h>

// Minimal standard C/C++ includes used only for convenience (print, memory, containers)
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>

#define PAUSE_BETWEEN_TESTS 100000

static const int WIN_W = 256;
static const int WIN_H = 256;

struct TestResult { const char* name; bool pass; std::string msg; };

static void resetGLState() {
    glViewport(0, 0, WIN_W, WIN_H);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);
    glColor4f(1, 1, 1, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, 0, WIN_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void checkGLError(const char* where, std::string &out) {
    GLenum e;
    while ((e = glGetError()) != GL_NO_ERROR) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s: GL error 0x%04X", where, (unsigned)e);
        if (!out.empty()) out += "; ";
        out += buf;
    }
}

static void readPixel(int x, int y, unsigned char dst[4]) {
    glFinish();
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, dst);
}

static bool approxColor(const unsigned char* p, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
}

static TestResult test_version() {
    TestResult t = {"GL_VERSION == 1.1", false, ""};
    const GLubyte* v = glGetString(GL_VERSION);
    if (!v) { t.msg = "glGetString(GL_VERSION) returned NULL"; return t; }
    int major=0, minor=0;
    if (sscanf((const char*)v, "%d.%d", &major, &minor) >= 2) {
        if (major == 1 && minor == 1) { t.pass = true; return t; }
        if (major >= 1 || minor >= 1) {
            t.msg = std::string("version too new: ") + (const char*)v;
            return t;
        }
        t.msg = std::string("version too old: ") + (const char*)v;
        return t;
    }
    t.msg = std::string("couldn't parse GL_VERSION: ") + (const char*)v;
    return t;
}

static TestResult test_clear_and_readback() {
    TestResult t = {"ClearColor & glReadPixels", false, ""};
    resetGLState();
    glViewport(0,0,WIN_W,WIN_H);
    glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    checkGLError("clear", t.msg);
    unsigned char pix[4] = {0,0,0,0};
    readPixel(WIN_W/2, WIN_H/2, pix);
    unsigned char er = (unsigned char)roundf(0.2f * 255.0f);
    unsigned char eg = (unsigned char)roundf(0.4f * 255.0f);
    unsigned char eb = (unsigned char)roundf(0.6f * 255.0f);
    if (approxColor(pix, er, eg, eb, 255)) { t.pass = true; return t; }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "pixel=%u,%u,%u,%u expected~%u,%u,%u,255", pix[0],pix[1],pix[2],pix[3],er,eg,eb);
    t.msg = buf;
    return t;
}

static TestResult test_immediate_triangle() {
    TestResult t = {"Immediate-mode triangle", false, ""};
    resetGLState();
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, WIN_W, 0, WIN_H, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
      glColor3f(1,0,0);
      glVertex2f(WIN_W*0.25f, WIN_H*0.25f);
      glVertex2f(WIN_W*0.75f, WIN_H*0.25f);
      glVertex2f(WIN_W*0.5f,  WIN_H*0.75f);
    glEnd();
    glFlush();
    unsigned char pix[4];
    readPixel(WIN_W/2, WIN_H/2, pix);
    if (pix[0] > 200 && pix[1] < 60 && pix[2] < 60) { t.pass = true; return t; }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "center=%u,%u,%u", pix[0],pix[1],pix[2]);
    t.msg = buf;
    return t;
}

static TestResult test_viewport_scissor() {
    TestResult t = {"Viewport & scissor (viewport test)", false, ""};
    resetGLState();
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, WIN_W, 0, WIN_H, -1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    // Restrict drawing to the left half of the window. Ortho still maps the
    // full [0, WIN_W] range into that viewport, so a full-width quad fills it.
    glViewport(0,0,WIN_W/2,WIN_H);
    glColor3f(1,0,0);
    glBegin(GL_QUADS);
      glVertex2f(0,0); glVertex2f(WIN_W,0); glVertex2f(WIN_W,WIN_H); glVertex2f(0,WIN_H);
    glEnd();
    glFlush();
    unsigned char left[4], right[4];
    readPixel(WIN_W/4, WIN_H/2, left);
    readPixel(3*WIN_W/4, WIN_H/2, right);
    if (left[0] > 200 && right[2] > 200) { t.pass = true; return t; }
    char buf[128];
    std::snprintf(buf,sizeof(buf),"left=%u,%u,%u right=%u,%u,%u", left[0],left[1],left[2], right[0],right[1],right[2]);
    t.msg = buf;
    return t;
}

static TestResult test_modelview_translate() {
    TestResult t = {"Modelview transform (translate)", false, ""};
    resetGLState();
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, WIN_W, 0, WIN_H, -1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0,1,0);
    glBegin(GL_QUADS);
      glVertex2f(0,0); glVertex2f(40,0); glVertex2f(40,40); glVertex2f(0,40);
    glEnd();
    glPushMatrix();
    glTranslatef(100,100,0);
    glColor3f(1,0,0);
    glBegin(GL_QUADS);
      glVertex2f(0,0); glVertex2f(40,0); glVertex2f(40,40); glVertex2f(0,40);
    glEnd();
    glPopMatrix();
    glFlush();
    unsigned char a[4], b[4];
    readPixel(20,20,a);
    readPixel(100+20,100+20,b);
    if (a[1] > 200 && b[0] > 200) { t.pass = true; return t; }
    char buf[128];
    std::snprintf(buf,sizeof(buf),"origin=%u,%u translated=%u,%u", a[0],a[1], b[0],b[1]);
    t.msg = buf;
    return t;
}

static TestResult test_display_list() {
    TestResult t = {"Display list (glNewList/glCallList)", false, ""};
    resetGLState();
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, WIN_W, 0, WIN_H, -1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    GLuint list = glGenLists(1);
    if (!list) { t.msg = "glGenLists returned 0"; return t; }
    glNewList(list, GL_COMPILE);
      glColor3f(1,1,0);
      glBegin(GL_QUADS);
        glVertex2f(60,60); glVertex2f(120,60); glVertex2f(120,120); glVertex2f(60,120);
      glEnd();
    glEndList();
    glCallList(list);
    glFlush();
    unsigned char pix[4];
    readPixel(80,80,pix);
    if (pix[0] > 200 && pix[1] > 200) t.pass = true;
    else t.msg = "display list didn't draw expected color";
    glDeleteLists(list,1);
    return t;
}

static TestResult test_texture_2x2() {
    TestResult t = {"2D texture upload & sample (2x2)", false, ""};
    resetGLState();
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, WIN_W, 0, WIN_H, -1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char texdata[4*2*2] = {
      255,0,0,255,   0,255,0,255,
      0,0,255,255,   255,255,0,255
    };
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2,2, 0, GL_RGBA, GL_UNSIGNED_BYTE, texdata);
    glColor3f(1,1,1);
    glBegin(GL_QUADS);
      glTexCoord2f(0,1); glVertex2f(40,40);
      glTexCoord2f(1,1); glVertex2f(120,40);
      glTexCoord2f(1,0); glVertex2f(120,120);
      glTexCoord2f(0,0); glVertex2f(40,120);
    glEnd();
    glFlush();
    unsigned char pix[4];
    readPixel(80,80,pix);
    if ((pix[0] + pix[1] + pix[2]) > 10) t.pass = true;
    else t.msg = "texture sampling produced near-black";
    glDeleteTextures(1, &tex);
    glDisable(GL_TEXTURE_2D);
    return t;
}

static TestResult test_blending() {
    TestResult t = {"Blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)", false, ""};
    resetGLState();
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, WIN_W, 0, WIN_H, -1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1,0,0,0.5f);
    glBegin(GL_QUADS);
      glVertex2f(60,60); glVertex2f(140,60); glVertex2f(140,140); glVertex2f(60,140);
    glEnd();
    glFlush();
    unsigned char pix[4];
    readPixel(100,100,pix);
    // expected purple-ish (red blended with blue)
    if (pix[0] > 100 && pix[2] > 100) { t.pass = true; return t; }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "blended=%u,%u,%u", pix[0],pix[1],pix[2]);
    t.msg = buf;
    return t;
}

int main() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { std::fprintf(stderr, "XOpenDisplay failed\n"); return 2; }

    static int visual_attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,   8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        None
    };

    int screen = DefaultScreen(dpy);
    XVisualInfo* vi = glXChooseVisual(dpy, screen, visual_attribs);
    if (!vi) {
        std::fprintf(stderr, "glXChooseVisual failed\n");
        XCloseDisplay(dpy);
        return 3;
    }

    Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    Window win = XCreateWindow(
        dpy,
        RootWindow(dpy, vi->screen),
        0, 0, WIN_W, WIN_H, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );
    XStoreName(dpy, win, "GLX GL1.1 tests");
    XMapWindow(dpy, win);

    GLXContext ctx = glXCreateContext(dpy, vi, nullptr, GL_TRUE);
    if (!ctx) {
        std::fprintf(stderr, "glXCreateContext failed\n");
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        return 4;
    }
    glXMakeCurrent(dpy, win, ctx);

    // small pause to let window map (not strictly required)
    usleep(100000);

    // run tests
    std::vector<TestResult> results;
    results.push_back(test_version());
    // swap/draw after every test so drivers update backbuffer -> front
    glXSwapBuffers(dpy, win);

    std::printf("\n### test_clear_and_readback ###\n");
    results.push_back(test_clear_and_readback()); glXSwapBuffers(dpy, win);
    usleep(PAUSE_BETWEEN_TESTS);
    std::printf("\n### test_immediate_triangle ###\n");
    results.push_back(test_immediate_triangle());   glXSwapBuffers(dpy, win);
    usleep(PAUSE_BETWEEN_TESTS);
    std::printf("\n### test_viewport_scissor ###\n");
    results.push_back(test_viewport_scissor());     glXSwapBuffers(dpy, win);
    usleep(PAUSE_BETWEEN_TESTS);
    std::printf("\n### test_modelview_translate ###\n");
    results.push_back(test_modelview_translate());  glXSwapBuffers(dpy, win);
    usleep(PAUSE_BETWEEN_TESTS);
    std::printf("\n### test_display_list ###\n");
    results.push_back(test_display_list());         glXSwapBuffers(dpy, win);
    usleep(PAUSE_BETWEEN_TESTS);
    std::printf("\n### test_texture_2x2 ###\n");
    results.push_back(test_texture_2x2());          glXSwapBuffers(dpy, win);
    usleep(PAUSE_BETWEEN_TESTS);
    std::printf("\n### test_blending ###\n");
    results.push_back(test_blending());             glXSwapBuffers(dpy, win);
    usleep(PAUSE_BETWEEN_TESTS);

    
    // Vendor Info
    const GLubyte* vendor = glGetString(GL_VENDOR);
    if (vendor)
        std::printf("Vendor: %s\n", (const char*)vendor);
    else
        std::printf("Vendor: <null>\n");
    // Renderer Info
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (renderer)
        std::printf("Renderer: %s\n", (const char*)renderer);
    else
        std::printf("Renderer: <null>\n");
    
    int fails = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        const TestResult &r = results[i];
        std::printf("[%s] %s", r.pass ? "PASS" : "FAIL", r.name);
        if (!r.pass && !r.msg.empty()) std::printf(" -- %s", r.msg.c_str());
        std::printf("\n");
        if (!r.pass) ++fails;
    }
    std::printf("%i/%i\n", int(results.size()-fails), int(results.size()));

    // hold window visible briefly so user can inspect (1s)
    glXSwapBuffers(dpy, win);
    usleep(1000000);

    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, ctx);
    XDestroyWindow(dpy, win);
    XFreeColormap(dpy, cmap);
    XFree(vi);
    XCloseDisplay(dpy);

    return (fails == 0) ? 0 : (10 + fails);
}
