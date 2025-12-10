#include "displayLists.h"
#include "global.h"
#include "include/datatypes.h"
#include "render.h"
#include "sdl.h"
#include "maths.h"
#include "functions/function.h"
#include <GL/gl.h>
#include <SDL3/SDL_stdinc.h>
#include <cstdint>
#include <cstdlib>

// Actual OpenGL 1.1 Library functions!
extern "C" {
    // Add float vertex (2)
    void glVertex2f(GLfloat x, GLfloat y) {
        //PrintInfo("glVertex2f");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glVertex2f");
            }
            real_gl(x,y);
        }
        if (activeDisplayListIndex == 0) {
            Process_glVertex2f(x, y);
        } else {
            if (compileAndExecute) {
                Process_glVertex2f(x, y);
            }
            Record_glVector2f(x,y);
        }
        //PrintInfo("\n");
    }

    // Add float vertex (3)
    void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
        //PrintInfo("glVertex3f");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glVertex3f");
            }
            real_gl(x,y,z);
        }
        if (activeDisplayListIndex == 0) {
            Process_glVertex3f(x, y, z);
        } else {
            if (compileAndExecute) {
                Process_glVertex3f(x, y, z);
            }
            Record_glVector3f(x, y, z);
        }
        //PrintInfo("\n");
    }

    // Adjust OpenGL Viewport
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        PrintInfo("glViewport");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLint,GLsizei,GLsizei) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLint,GLsizei,GLsizei)) dlsym(RTLD_NEXT, "glViewport");
            }
            real_gl(x,y,width,height);
        }
        //width /= 3;
        //height /= 3;
        if (activeDisplayListIndex == 0) {
            Process_glViewport(x,y,width,height);
        } else {
            if (compileAndExecute) {
                Process_glViewport(x,y,width,height);
            }
            Record_glViewport(x,y,width,height);
        }
        PrintInfo("\n");
    }

    // Set float color
    void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
        PrintInfo("glClearColor");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glClearColor");
            }
            real_gl(red,green,blue,alpha);
        }
        clearColor = Col4{red,green,blue, alpha};
        PrintInfo("\n");
    }

    // Set float color
    void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
        //PrintInfo("glColor3f");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glColor3f");
            }
            real_gl(red,green,blue);
        }
        if (activeDisplayListIndex == 0) {
            Process_glColor3f(red,green,blue);
        } else {
            if (compileAndExecute) {
                Process_glColor3f(red,green,blue);
            }
            Record_glColor3f(red,green,blue);
        }
        //PrintInfo("\n");
    }

    // Return OpenGL info
    const GLubyte* glGetString(GLenum name) {
        static const GLubyte* (*real_gl)(GLenum) = NULL;
        PrintInfo("glGetString");
        if (forwardToSystemGl) {
            if (!real_gl) {
                real_gl = (const GLubyte* (*)(GLenum)) dlsym(RTLD_NEXT, "glGetString");
            }
        }
        switch(name) {
            case GL_VERSION:
                return (const GLubyte*)PIXSOFTGL_VERSION;
            case GL_VENDOR:
                return (const GLubyte*)PIXSOFTGL_VENDOR;
            case GL_RENDERER:
                return (const GLubyte*)PIXSOFTGL_RENDERER;
            case GL_EXTENSIONS:
                return (const GLubyte*)PIXSOFTGL_EXTENSIONS;
            default:
                std::cout << std::hex << name << std::dec << std::endl;
                return real_gl(name);
        }
        PrintInfo("\n");
        return nullptr;
    }

    // Light Position
    void glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
        PrintInfo("glLightfv");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLenum,const GLfloat*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLenum,const GLfloat*)) dlsym(RTLD_NEXT, "glLightfv");
            }
            real_gl(light,pname,params);
        }
        switch(pname) {
            case GL_POSITION:
                lights[light & 0xFF].pos = Vec3{
                    params[0],
                    params[1],
                    params[2]
                };
                break;
        }
        PrintInfo("\n");
    }

    GLuint glGenLists(GLsizei range) {
        PrintInfo("glGenLists ");
        PrintInfo(range);
        if (forwardToSystemGl) {
            static GLuint (*real_gl)(GLsizei) = NULL;
            if (!real_gl) {
                real_gl = (GLuint (*)(GLsizei)) dlsym(RTLD_NEXT, "glGenLists");
            }
            //return real_gl(range);
        }
        GLuint result = Process_glGenLists(range);
        PrintInfo(": ");
        PrintInfo(result);
        PrintInfo("\n");
        return result;
    }

    void glBindTexture(GLenum target, GLuint texture) {
        PrintInfo("glBindTexture ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLuint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLuint)) dlsym(RTLD_NEXT, "glBindTexture");
            }
            real_gl(target,texture);
        }
        if (activeDisplayListIndex == 0) {
            Process_glBindTexture(target,texture);
        } else {
            if (compileAndExecute) {
                Process_glBindTexture(target,texture);
            }
            Record_glBindTexture(target,texture);
        }

        PrintInfo("\n");
    }

    void glDeleteLists(GLuint list, GLsizei range) {
        PrintInfo("glDeleteLists");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLuint,GLsizei) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLuint,GLsizei)) dlsym(RTLD_NEXT, "glDeleteLists");
            }
            real_gl(list,range);
        }
        Process_glDeleteLists(list, range);
        PrintInfo("\n");
    }

    GLboolean glIsList(GLuint list) {
        PrintInfo("glIsList");
        if (forwardToSystemGl) {
            static GLboolean (*real_gl)(GLuint) = NULL;
            if (!real_gl) {
                real_gl = (GLboolean (*)(GLuint)) dlsym(RTLD_NEXT, "glIsList");
            }
            //return real_gl(list);
        }
        PrintInfo("\n");
        return Process_IsList(list);
    }

    void glNewList(GLuint list, GLenum mode) {
        PrintInfo("glNewList ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLuint,GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLuint,GLenum)) dlsym(RTLD_NEXT, "glNewList");
            }
            real_gl(list,mode);
        }
        Process_glNewList(list, mode);
        PrintInfo("\n");
    }

    void glEndList() {
        PrintInfo("glEndList");
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glEndList");
            }
            real_gl();
        }
        Process_glEndList();
        PrintInfo("\n");
    }

    void glCallList(GLuint list) {
        PrintInfo("glCallList");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLuint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLuint)) dlsym(RTLD_NEXT, "glCallList");
            }
            real_gl(list);
        }
        Process_glCallList(list);
        PrintInfo("\n");
    }

    // Clear framebuffer(s)
    void glClear(GLbitfield mask) {
        PrintInfo("glClear ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLbitfield) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLbitfield)) dlsym(RTLD_NEXT, "glClear");
            }
            real_gl(mask);
        }

        // Render last frame
        UpdateScreen();

        // Prepare for next frame
        ClearFramebuffers(mask);
        PrintInfo("\n");
    }

    // Set Matrix mode
    void glMatrixMode(GLenum mode) {
        PrintInfo("glMatrixMode ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glMatrixMode");
            }
            real_gl(mode);
        }
        if (activeDisplayListIndex == 0) {
            Process_glMatrixMode(mode);
        } else {
            if (compileAndExecute) {
                Process_glMatrixMode(mode);
            }
            Record_glMatrixMode(mode);
        }        
        PrintInfo("\n");
    }

    // Choose face winding order
    void glFrontFace(GLenum mode) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glFrontFace");
            }
            real_gl(mode);
        }
        if (activeDisplayListIndex == 0) {
            Process_glFrontFace(mode);
        } else {
            if (compileAndExecute) {
                Process_glFrontFace(mode);
            }
            Record_glFrontFace(mode);
        }
    }

    void glLoadMatrixf(const GLfloat *m) {
        if (forwardToSystemGl) {
            static void (*real_gl)(const GLfloat *m) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(const GLfloat *m)) dlsym(RTLD_NEXT, "glLoadMatrixf");
            }
            real_gl(m);
        }
        PrintInfo("glLoadMatrixf");
        if (!lastAccessedMatrix) return;
        *lastAccessedMatrix = Mat4x4{
            Vec4{m[0],m[1],m[2],m[3]},
            Vec4{m[4],m[5],m[6],m[7]},
            Vec4{m[8],m[9],m[10],m[11]},
            Vec4{m[12],m[13],m[14],m[15]}
        };
        PrintInfo("\n");

    }

    void glDepthMask(GLboolean flag) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLboolean flag) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLboolean flag)) dlsym(RTLD_NEXT, "glDepthMask");
            }
            real_gl(flag);
        }
        PrintInfo("glDepthMask");
        depthWriteActive = flag;
        PrintInfo("\n");
    }

    // Enable property
    void glEnable(GLenum cap) {
        PrintInfo("glEnable ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glEnable");
            }
            real_gl(cap);
        }
        if (activeDisplayListIndex == 0) {
            Process_glEnable(cap);
        } else {
            if (compileAndExecute) {
                Process_glEnable(cap);
            }
            Record_glEnable(cap);
        }
        PrintInfo("\n");
    }

    // Disable property
    void glDisable(GLenum cap) {
        PrintInfo("glDisable ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glDisable");
            }
            real_gl(cap);
        }
        if (activeDisplayListIndex == 0) {
            Process_glDisable(cap);
        } else {
            if (compileAndExecute) {
                Process_glDisable(cap);
            }
            Record_glDisable(cap);
        }
        PrintInfo("\n");
    }

    // Load new data
    void glBegin(GLenum mode) {
        PrintInfo("glBegin ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glBegin");
            }
            real_gl(mode);
        }
        if (activeDisplayListIndex == 0) {
            Process_glBegin(mode);
        } else {
            if (compileAndExecute) {
                Process_glBegin(mode);
            }
            Record_glBegin(mode);
        }
        PrintInfo("\n");
    }

    // Waits until all changes in GL state, connection state
    // or framebuffer access are finished
    void glFinish() {
        PrintInfo("glFinish ");
        UpdateScreen();
        PrintInfo("\n");
    }

    // Renders whatever is in the framebuffer instantly
    void glFlush() {
        PrintInfo("glFlush ");
        UpdateScreen();
        PrintInfo("\n");
    }

    // We're done, now draw whatever we sent to the fb
    void glEnd() {
        PrintInfo("glEnd ");
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glEnd");
            }
            real_gl();
        }
        if (activeDisplayListIndex == 0) {
            Process_glEnd();
        } else {
            if (compileAndExecute) {
                Process_glEnd();
            }
            Record_glEnd();
        }
        PrintInfo("\n");
    }

    // Load identity matrix
    void glLoadIdentity() {
        PrintInfo("glLoadIdentity ");
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glLoadIdentity");
            }
            real_gl();
        }
        if (activeDisplayListIndex == 0) {
            Process_glLoadIdentity();
        } else {
            if (compileAndExecute) {
                Process_glLoadIdentity();
            }
            Record_glLoadIdentity();
        }
        PrintInfo("\n");
    }

    // Float translate
    void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
        PrintInfo("glTranslatef");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glTranslatef");
            }
            real_gl(x,y,z);
        }
        if (activeDisplayListIndex == 0) {
            Process_glTranslatef(x,y,z);
        } else {
            if (compileAndExecute) {
                Process_glTranslatef(x,y,z);
            }
            Record_glTranslatef(x,y,z);
        }
        PrintInfo("\n");
    }

    // Float Rotate
    void glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
        PrintInfo("glRotatef");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glRotatef");
            }
            real_gl(angleDeg,x,y,z);
        }
        if (activeDisplayListIndex == 0) {
            Process_glRotatef(angleDeg,x,y,z);
        } else {
            if (compileAndExecute) {
                Process_glRotatef(angleDeg,x,y,z);
            }
            Record_glRotatef(angleDeg,x,y,z);
        }
        PrintInfo("\n");
    }

    // Float scale
    void glScalef(GLfloat x, GLfloat y, GLfloat z) {
        PrintInfo("glScalef");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glScalef");
            }
            real_gl(x,y,z);
        }
        if (activeDisplayListIndex == 0) {
            Process_glScalef(x,y,z);
        } else {
            if (compileAndExecute) {
                Process_glScalef(x,y,z);
            }
            Record_glScalef(x,y,z);
        }
        PrintInfo("\n");
    }

    // Perspective Projection Matrix Creation
    void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
        PrintInfo("glFrustum");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble)) dlsym(RTLD_NEXT, "glFrustum");
            }
            real_gl(l,r,b,t,n,f);
        }
        if (activeDisplayListIndex == 0) {
            Process_glFrustum(l,r,b,t,n,f);
        } else {
            if (compileAndExecute) {
                Process_glFrustum(l,r,b,t,n,f);
            }
            Record_glFrustum(l,r,b,t,n,f);
        }
        PrintInfo("\n");
    }

    // Orthographic Projection Matrix Creation
    void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
        PrintInfo("glOrtho");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble)) dlsym(RTLD_NEXT, "glOrtho");
            }
            real_gl(l,r,b,t,n,f);
        }
        if (activeDisplayListIndex == 0) {
            Process_glOrtho(l,r,b,t,n,f);
        } else {
            if (compileAndExecute) {
                Process_glOrtho(l,r,b,t,n,f);
            }
            Record_glOrtho(l,r,b,t,n,f);
        }
        PrintInfo("\n");
    }

    // Set fog integer
    void glFogi(GLenum pname, GLint param) {
        PrintInfo("glFogi");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLint)) dlsym(RTLD_NEXT, "glFogi");
            }
            real_gl(pname,param);
        }
        if (activeDisplayListIndex == 0) {
            Process_glFogi(pname,param);
        } else {
            if (compileAndExecute) {
                Process_glFogi(pname,param);
            }
            Record_glFogi(pname,param);
        }
        PrintInfo("\n");
    }

    // Set fog float values
    void glFogfv(GLenum pname, const GLfloat *params) {
        PrintInfo("glFogfv");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,const GLfloat*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,const GLfloat*)) dlsym(RTLD_NEXT, "glFogfv");
            }
            real_gl(pname,params);
        }
        if (activeDisplayListIndex == 0) {
            Process_glFogfv(pname,params);
        } else {
            if (compileAndExecute) {
                Process_glFogfv(pname,params);
            }
            Record_glFogfv(pname,params);
        }
        PrintInfo("\n");
    }

    // Set fog float
    void glFogf(GLenum pname, GLfloat param) {
        PrintInfo("glFogf");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLfloat)) dlsym(RTLD_NEXT, "glFogf");
            }
            real_gl(pname,param);
        }
        if (activeDisplayListIndex == 0) {
            Process_glFogf(pname,param);
        } else {
            if (compileAndExecute) {
                Process_glFogf(pname,param);
            }
            Record_glFogf(pname,param);
        }
        PrintInfo("\n");
    }

    void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid*)) dlsym(RTLD_NEXT, "glReadPixels");
            }
            real_gl(x,y,width,height,format,type,pixels);
        }
        PrintInfo("glReadPixels ");
        if (!pixels) return;
        // Assume format is always RGBA
        // Assume format is always unsigned Byte
        uint8_t* pix = static_cast<uint8_t*>(pixels);
        for (int iy = y; iy < y + height; iy++) {
            for (int ix = x; ix < x+width; ix++) {
                int srcY = (renderAreaHeight - 1 - iy);
                PixelValue c = frameBufferColor[ix + srcY * renderAreaWidth];
                *pix++ = c.r;
                *pix++ = c.g;
                *pix++ = c.b;
                *pix++ = 255;
            }
        }
        PrintInfo("\n");
    }

    void glPushMatrix() {
        PrintInfo("glPushMatrix ");
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glPushMatrix");
            }
            real_gl();
        }
        if (activeDisplayListIndex == 0) {
            Process_glPushMatrix();
        } else {
            if (compileAndExecute) {
                Process_glPushMatrix();
            }
            Record_glPushMatrix();
        }
        PrintInfo("\n");
    }

    void glPopMatrix() {
        PrintInfo("glPopMatrix ");
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glPopMatrix");
            }
            real_gl();
        }
        if (activeDisplayListIndex == 0) {
            Process_glPopMatrix();
        } else {
            if (compileAndExecute) {
                Process_glPopMatrix();
            }
            Record_glPopMatrix();
        }
        PrintInfo("\n");
    }

    void glGetIntegerv(GLenum pname, GLint *params) {
        static void (*real_gl)(GLenum,GLint*) = NULL;
        if (forwardToSystemGl) {
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLint*)) dlsym(RTLD_NEXT, "glGetIntegerv");
            }
            real_gl(pname,params);
        }
        PrintInfo("glGetIntegerv ");
        switch (pname) {
            case GL_VIEWPORT:
                PrintInfo("GL_VIEWPORT");
                params[0] = 0;
                params[1] = 0;
                params[2] = renderAreaWidth;
                params[3] = renderAreaHeight;
                break;
            case GL_DEPTH_BITS:
                PrintInfo("GL_DEPTH_BITS");
                params[0] = 8;
                break;
            case GL_MAX_TEXTURE_SIZE:
                PrintInfo("GL_MAX_TEXTURE_SIZE");
                params[0] = MAX_TEXTURE_SIZE;
                break;
            default:
                if (forwardToSystemGl) {
                    real_gl(pname, params);
                }
                break;
        }
        PrintInfo("\n");
    }

    void glEnableClientState(GLenum cap) {
        PrintInfo("glEnableClientState ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glEnableClientState");
            }
            real_gl(cap);
        }
        switch(cap) {
            case GL_VERTEX_ARRAY:
                vertexArrayActive = true;
                break;
            case GL_COLOR_ARRAY:
                colorArrayActive = true;
                break;
            case GL_TEXTURE_COORD_ARRAY:
                textureArrayActive = true;
                break;
        }
        PrintInfo("\n");
    }

    void glDisableClientState(GLenum cap) {
        PrintInfo("glEnableClientState ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glDisableClientState");
            }
            real_gl(cap);
        }
        switch(cap) {
            case GL_VERTEX_ARRAY:
                vertexArrayActive = false;
                break;
            case GL_COLOR_ARRAY:
                colorArrayActive = false;
                break;
            case GL_TEXTURE_COORD_ARRAY:
                textureArrayActive = false;
                break;
        }
        PrintInfo("\n");
    }

    void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
        PrintInfo("glVertexPointer ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLenum,GLsizei,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLenum,GLsizei,const GLvoid*)) dlsym(RTLD_NEXT, "glVertexPointer");
            }
            real_gl(size,type,stride,ptr);
        }
        vertexArrayPointer = ptr;
        vertexArrayStride = stride;
        RecomputeBase();
        PrintInfo("\n");
    }
    
    void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
        PrintInfo("glColorPointer ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLenum,GLsizei,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLenum,GLsizei,const GLvoid*)) dlsym(RTLD_NEXT, "glColorPointer");
            }
            real_gl(size,type,stride,ptr);
        }
        colorArrayPointer = ptr;
        colorArrayStride = stride;
        RecomputeBase();
        PrintInfo("\n");
    }

    void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
        PrintInfo("glTexCoordPointer ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLenum,GLsizei,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLenum,GLsizei,const GLvoid*)) dlsym(RTLD_NEXT, "glTexCoordPointer");
            }
            real_gl(size,type,stride,ptr);
        }
        textureArrayPointer = ptr;
        textureArrayStride = stride;
        RecomputeBase();
        PrintInfo("\n");
    }

    void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
        PrintInfo("glDrawArrays ");
        PrintInfo("\n");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLint,GLsizei) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLint,GLsizei)) dlsym(RTLD_NEXT, "glDrawArrays");
            }
            real_gl(mode,first,count);
        }
        if (activeDisplayListIndex == 0) {
            Process_glDrawArrays(mode,first,count);
        } else {
            if (compileAndExecute) {
                Process_glDrawArrays(mode,first,count);
            }
            Record_glDrawArrays(mode,first,count);
        }

    }

    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
        PrintInfo("glDrawElements ");
        PrintInfoHex(mode);
        PrintInfo(" - ");
        PrintInfoHex(count);
        PrintInfo(" - ");
        PrintInfoHex(type);
        PrintInfo("\n");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLsizei,GLenum,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLsizei,GLenum,const GLvoid*)) dlsym(RTLD_NEXT, "glDrawElements");
            }
            real_gl(mode,count,type,indices);
        }
        if (activeDisplayListIndex == 0) {
            Process_glDrawElements(mode,count,type,indices);
        } else {
            if (compileAndExecute) {
                Process_glDrawElements(mode,count,type,indices);
            }
            Record_glDrawElements(mode,count,type,indices);
        }
        //int x;
        //std::cin >> x;
    }

    void glGenTextures(GLsizei n, GLuint *textures) {
        PrintInfo("glGenTextures ");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLsizei, GLuint*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLsizei, GLuint*)) dlsym(RTLD_NEXT, "glGenTextures");
            }
            real_gl(n, textures);
        }
        Process_glGenTextures(n,textures);
        PrintInfo("\n");
    }

    void glTexParameteri(GLenum target, GLenum pname, GLint param) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLenum,GLint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLenum,GLint)) dlsym(RTLD_NEXT, "glTexParameteri");
            }
            real_gl(target,pname,param);
        }
        if (!lastAccessedTexture) return;
        auto& tex = lastAccessedTexture->texture2D;
        switch(pname) {
            case GL_TEXTURE_WRAP_S:
                tex.textureWrapS = param;
                break;
            case GL_TEXTURE_WRAP_T:
                tex.textureWrapT = param;
                break;
            case GL_TEXTURE_MIN_FILTER:
                tex.textureMinFilter = param;
                break;
            case GL_TEXTURE_MAG_FILTER:
                tex.textureMagFilter = param;
                break;
        }
    }

    void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid*)) dlsym(RTLD_NEXT, "glTexImage2D");
            }
            real_gl(target,level,internalFormat,width,height,border,format,type,pixels);
        }
        if (!lastAccessedTexture) return;
        if (width > MAX_TEXTURE_SIZE || height > MAX_TEXTURE_SIZE) return;
        if (!pixels) return;
        lastAccessedTexture->texture2D.width = width;
        lastAccessedTexture->texture2D.height = height;
        lastAccessedTexture->texture2D.textureData = (Col4*)malloc(sizeof(Col4)*width*height);
        const unsigned char* charPix = (const unsigned char*)pixels;
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = (y*width + x)*4;  // RGBA
                int texIdx = y*width + x;   // Col4 array index

                lastAccessedTexture->texture2D.textureData[texIdx] = Col4{
                    charPix[idx] / 255.0f,
                    charPix[idx+1] / 255.0f,
                    charPix[idx+2] / 255.0f,
                    charPix[idx+3] / 255.0f
                };
            }
        }
    }

    void glTexCoord2f(GLfloat s, GLfloat t) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glTexCoord2f");
            }
            real_gl(s,t);
        }
        if (activeDisplayListIndex == 0) {
            Process_glTexCoord2f(s,t);
        } else {
            if (compileAndExecute) {
                Process_glTexCoord2f(s,t);
            }
            Record_glTexCoord2f(s,t);
        }
    }

    void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLenum,const GLfloat *) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLenum,const GLfloat *)) dlsym(RTLD_NEXT, "glMaterialfv");
            }
            real_gl(face,pname,params);
        }
        if (activeDisplayListIndex == 0) {
            Process_glMaterialfv(face,pname,params);
        } else {
            if (compileAndExecute) {
                Process_glMaterialfv(face,pname,params);
            }
            Record_glMaterialfv(face,pname,params);
        }
    }
}
