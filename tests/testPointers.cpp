#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <unistd.h>

Display* dpy;
Window win;
GLXContext ctx;

#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 240

float angle = 0.0f;

GLfloat vertices[] = {
    -1,-1, 1,
     1,-1, 1,
     1, 1, 1,
    -1, 1, 1
};

GLubyte colors[] = {
    255,0,0,255,
    0,255,0,255,
    0,0,255,255,
    255,255,0,255
};

GLfloat texcoords[] = {
    0,0,
    1,0,
    1,1,
    0,1
};

// Indices for glDrawElements
GLushort indices[] = {
    0, 1, 2, 3
};
GLushort indices2[] = {
    1, 2, 3, 0
};

bool ind = false;

GLuint texture;

void initTexture() {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    const int texSize = 2;
    GLubyte texData[texSize * texSize * 4] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 0, 255
    };

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void init() {
    glViewport(0,0,WINDOW_WIDTH,WINDOW_HEIGHT);
    glEnable(GL_DEPTH_TEST);
}

void drawQuad() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    glLoadIdentity();
    glTranslatef(0,0,-5);
    glRotatef(angle,0,0,1);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

    glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices);
    /*
    if (ind) {
        
    } else {
        glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices2);
    }
    ind = !ind;
    */

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) return -1;

    static int visAttribs[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None
    };

    XVisualInfo* vi = glXChooseVisual(dpy, 0, visAttribs);
    if (!vi) return -1;

    Colormap cmap = XCreateColormap(dpy,
                                    RootWindow(dpy, vi->screen),
                                    vi->visual,
                                    AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask;

    win = XCreateWindow(
        dpy, RootWindow(dpy, vi->screen),
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );

    XMapWindow(dpy, win);
    XStoreName(dpy, win, "OpenGL DrawElements Test");

    ctx = glXCreateContext(dpy, vi, NULL, True);
    glXMakeCurrent(dpy, win, ctx);

    init();
    initTexture();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.75, 0.75, -0.5625, 0.5625, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);

    while (true) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == KeyPress)
                return 0;
        }

        angle += 1.0f;

        drawQuad();
        glXSwapBuffers(dpy, win);
        usleep(16000);
    }
}
