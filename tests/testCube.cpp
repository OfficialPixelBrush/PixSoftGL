#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <unistd.h>

Display* dpy;
Window win;
GLXContext ctx;

float angle = 0.0f;

#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 240

void init() {
    glViewport(0,0,WINDOW_WIDTH,WINDOW_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    //glEnable(GL_CULL_FACE);
    //glFrontFace(GL_CW);

    GLfloat fogColor[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 2.0f);
    glFogf(GL_FOG_END, 6.0f);

    glEnable(GL_COLOR_MATERIAL);
}

void drawCube() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0, 0, -5);
    glRotatef(angle, 1, 1, 0);

    glBegin(GL_QUADS);
    // Front face
    glColor3f(1,0,0); glVertex3f(-1,-1,1);
    glColor3f(0,1,0); glVertex3f(1,-1,1);
    glColor3f(0,0,1); glVertex3f(1,1,1);
    glColor3f(1,1,0); glVertex3f(-1,1,1);

    // Back face
    glColor3f(1,0,1); glVertex3f(-1,-1,-1);
    glColor3f(0,1,1); glVertex3f(1,-1,-1);
    glColor3f(1,1,1); glVertex3f(1,1,-1);
    glColor3f(0,0,0); glVertex3f(-1,1,-1);

    // Left face
    glColor3f(1,0,0); glVertex3f(-1,-1,-1);
    glColor3f(1,0,1); glVertex3f(-1,-1,1);
    glColor3f(1,1,0); glVertex3f(-1,1,1);
    glColor3f(0,0,0); glVertex3f(-1,1,-1);

    // Right face
    glColor3f(0,1,0); glVertex3f(1,-1,-1);
    glColor3f(0,1,1); glVertex3f(1,-1,1);
    glColor3f(0,0,1); glVertex3f(1,1,1);
    glColor3f(1,1,1); glVertex3f(1,1,-1);

    // Top face
    glColor3f(1,1,0); glVertex3f(-1,1,-1);
    glColor3f(0,0,0); glVertex3f(1,1,-1);
    glColor3f(0,0,1); glVertex3f(1,1,1);
    glColor3f(1,1,0); glVertex3f(-1,1,1);

    // Bottom face
    glColor3f(1,0,0); glVertex3f(-1,-1,-1);
    glColor3f(0,1,0); glVertex3f(1,-1,-1);
    glColor3f(0,1,1); glVertex3f(1,-1,1);
    glColor3f(1,0,1); glVertex3f(-1,-1,1);
    glEnd();

    glXSwapBuffers(dpy, win);
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) return -1;

    static int visual_attribs[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None
    };
    XVisualInfo* vi = glXChooseVisual(dpy, 0, visual_attribs);
    if (!vi) return -1;

    Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask;

    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, vi->depth,
                        InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "OpenGL 1.1 Cube with Fog");

    ctx = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, ctx);

    init();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.75, 0.75, -0.5625, 0.5625, 1.0, 100.0); // 45° FOV ~ aspect 800/600
    glMatrixMode(GL_MODELVIEW);

    while (true) {
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (xev.type == KeyPress) return 0;
        }

        angle += 1.0f;
        if (angle > 360) angle -= 360;
        if (angle > 30) break;
        drawCube();
        usleep(16000); // ~60 FPS
    }

    glXMakeCurrent(dpy, 0, 0);
    glXDestroyContext(dpy, ctx);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
