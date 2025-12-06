#include "displayLists.h"
#include "global.h"
#include "render.h"
#include "sdl.h"
#include "maths.h"
#include "functions/function.h"
#include <GL/gl.h>

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
        clearColor = Col3{red,green,blue};
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
        currentColor = Col3{red,green,blue};
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
        PrintInfo("glGenLists");
        static GLuint (*real_gl)(GLsizei) = NULL;
        if (!real_gl) {
            real_gl = (GLuint (*)(GLsizei)) dlsym(RTLD_NEXT, "glGenLists");
        }
        return real_gl(range);
        //return 0;
    }

    void glNewList(GLuint list, GLenum mode) {
        PrintInfo("glNewList");
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

        // Prepare next matrix Mode
        matrixMode = mode;
        switch(matrixMode) {
            case GL_PROJECTION:
                PrintInfo("GL_PROJECTION");
                projectionMode = GL_PROJECTION;
                lastAccessedMatrix = &projMatricies[projMatrixPtr];
                break;
            case GL_MODELVIEW:
                PrintInfo("GL_MODELVIEW");
                lastAccessedMatrix = &modelMatricies[modelMatrixPtr];
                break;
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
        switch (mode) {
            case GL_CCW:
                counterClockWiseWindingActive = true;
                break;
            case GL_CW:
                counterClockWiseWindingActive = false;
                break;
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

        switch(cap) {
            case GL_COLOR_MATERIAL:
                PrintInfo("GL_COLOR_MATERIAL");
                colorMaterialActive = true;
                break;
            case GL_FOG:
                PrintInfo("GL_FOG");
                fogActive = true;
                break;
            case GL_DEPTH_TEST:
                PrintInfo("GL_DEPTH_TEST");
                depthTestActive = true;
                break;
            case GL_TEXTURE_2D:
                PrintInfo("GL_TEXTURE_2D");
                texture2dActive = true;
                break;
            case GL_LIGHTING:
                PrintInfo("GL_LIGHTING");
                lightingActive = true;
                break;
            case GL_BLEND:
                PrintInfo("GL_BLEND");
                blendActive = true;
                break;
            case GL_CULL_FACE:
                PrintInfo("GL_CULL_FACE");
                cullFaceActive = true;
                break;
            case GL_LIGHT0:
                PrintInfo("GL_LIGHT0");
                lightActive[0] = true;
                break;
            case GL_NORMALIZE:
                PrintInfo("GL_NORMALIZE");
                normalizeActive = true;
                break;
            case GL_SCISSOR_TEST:
                PrintInfo("GL_SCISSOR_TEST");
                scissorTestActive = true;
                break;
            case GL_ALPHA_TEST:
                PrintInfo("GL_ALPHA_TEST");
                alphaTestActive = true;
                break;
            default:
                std::cout << std::hex;
                PrintInfo(cap);
                std::cout << std::dec;
                break;
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
        
        switch(cap) {
            case GL_COLOR_MATERIAL:
                PrintInfo("GL_COLOR_MATERIAL");
                colorMaterialActive = false;
                break;
            case GL_FOG:
                PrintInfo("GL_FOG");
                fogActive = false;
                break;
            case GL_DEPTH_TEST:
                PrintInfo("GL_DEPTH_TEST");
                depthTestActive = false;
                break;
            case GL_TEXTURE_2D:
                PrintInfo("GL_TEXTURE_2D");
                texture2dActive = false;
                break;
            case GL_LIGHTING:
                PrintInfo("GL_LIGHTING");
                lightingActive = false;
                break;
            case GL_BLEND:
                PrintInfo("GL_BLEND");
                blendActive = false;
                break;
            case GL_CULL_FACE:
                PrintInfo("GL_CULL_FACE");
                cullFaceActive = false;
                break;
            case GL_LIGHT0:
                PrintInfo("GL_LIGHT0");
                lightActive[0] = false;
                break;
            case GL_NORMALIZE:
                PrintInfo("GL_NORMALIZE");
                normalizeActive = false;
                break;
            case GL_SCISSOR_TEST:
                PrintInfo("GL_SCISSOR_TEST");
                scissorTestActive = false;
                break;
            case GL_ALPHA_TEST:
                PrintInfo("GL_ALPHA_TEST");
                alphaTestActive = false;
                break;
            default:
                std::cout << std::hex;
                PrintInfo(cap);
                std::cout << std::dec;
                break;
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
            Process_glOrtho(l,r,t,b,n,f);
        } else {
            if (compileAndExecute) {
                Process_glOrtho(l,r,t,b,n,f);
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
        switch(pname) {
            case GL_FOG_MODE:
                fogMode = param;
                break;
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
        switch(pname) {
            case GL_FOG_COLOR:
                fogColor = Col4{
                    params[0], params[1], params[2], params[3]
                };
                break;
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
        switch (pname) {
            case GL_FOG_START:
                fogStart = param;
                break;
            case GL_FOG_END:
                fogEnd = param;
                break;        
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
        for (int iy = y; iy < y + height; iy++) {
            for (int ix = x; ix < x+width; ix++) {
                int srcY = (renderAreaHeight - 1 - iy);
                PixelValue c = frameBufferColor[ix + srcY * renderAreaWidth];
                uint8_t* pix = static_cast<uint8_t*>(pixels);
                pix[0] = c.r;
                pix[1] = c.g;
                pix[2] = c.b;
                pix[3] = 255;
                pix += 4;
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
        if (!real_gl) {
            real_gl = (void (*)(GLenum,GLint*)) dlsym(RTLD_NEXT, "glGetIntegerv");
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
}
