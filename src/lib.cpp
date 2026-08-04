#include "lib.h"
#include <algorithm>
#include <iostream>
#include <dlfcn.h>
#include "global.h"
#include <GL/gl.h>

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT 0x0CF5
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT 0x0D05
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif

namespace {

int formatComponents(GLenum format) {
    switch (format) {
    case GL_ALPHA:
    case GL_LUMINANCE: return 1;
    case GL_LUMINANCE_ALPHA: return 2;
    case GL_RGB: return 3;
    case GL_RGBA:
    case GL_BGRA: return 4;
    default: return 0;
    }
}

int unpackRowStride(int width, int comps, GLint alignment) {
    const int rowBytes = width * comps;
    const int align = alignment > 0 ? alignment : 4;
    return ((rowBytes + align - 1) / align) * align;
}

void decodeTexel(const unsigned char* src, GLenum format, Col4& out) {
    switch (format) {
    case GL_ALPHA:
        // Fonts / masks: RGB white, A = coverage.
        out = Col4{1, 1, 1, src[0] / 255.0f};
        break;
    case GL_LUMINANCE: {
        const float l = src[0] / 255.0f;
        out = Col4{l, l, l, 1};
        break;
    }
    case GL_LUMINANCE_ALPHA: {
        const float l = src[0] / 255.0f;
        out = Col4{l, l, l, src[1] / 255.0f};
        break;
    }
    case GL_BGRA:
        out = Col4{
            src[2] / 255.0f,
            src[1] / 255.0f,
            src[0] / 255.0f,
            src[3] / 255.0f
        };
        break;
    case GL_RGB:
        out = Col4{
            src[0] / 255.0f,
            src[1] / 255.0f,
            src[2] / 255.0f,
            1.0f
        };
        break;
    case GL_RGBA:
    default:
        out = Col4{
            src[0] / 255.0f,
            src[1] / 255.0f,
            src[2] / 255.0f,
            src[3] / 255.0f
        };
        break;
    }
}

bool uploadTextureLevel0(Texture2D& tex, GLint xoffset, GLint yoffset,
                         GLsizei width, GLsizei height, GLenum format,
                         GLenum type, const GLvoid* pixels, bool allocateFull) {
    if (type != GL_UNSIGNED_BYTE) {
        errorState = GL_INVALID_ENUM;
        return false;
    }
    const int comps = formatComponents(format);
    if (comps <= 0) {
        errorState = GL_INVALID_ENUM;
        return false;
    }
    if (width <= 0 || height <= 0 || width > MAX_TEXTURE_SIZE || height > MAX_TEXTURE_SIZE) {
        errorState = GL_INVALID_VALUE;
        return false;
    }

    if (allocateFull) {
        if (tex.textureData) {
            free(tex.textureData);
            tex.textureData = nullptr;
        }
        tex.width = width;
        tex.height = height;
        tex.textureData = static_cast<unsigned char*>(
            malloc(4u * static_cast<size_t>(width * height)));
        if (!tex.textureData) {
            errorState = GL_OUT_OF_MEMORY;
            return false;
        }
        // Opaque white so incomplete uploads aren't modulate-to-black.
        for (int i = 0; i < width * height; ++i) {
            unsigned char* p = tex.textureData + i * 4;
            p[0] = p[1] = p[2] = p[3] = 255;
        }
        if (!pixels) return true;

        const unsigned char* src = static_cast<const unsigned char*>(pixels);
        const int stride = unpackRowStride(width, comps, unpackAlignment);
        for (int y = 0; y < height; ++y) {
            const unsigned char* row = src + static_cast<size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                Col4 c{};
                decodeTexel(row + x * comps, format, c);
                unsigned char* dst = tex.textureData + (y * width + x) * 4;
                dst[0] = static_cast<unsigned char>(c.r * 255.0f + 0.5f);
                dst[1] = static_cast<unsigned char>(c.g * 255.0f + 0.5f);
                dst[2] = static_cast<unsigned char>(c.b * 255.0f + 0.5f);
                dst[3] = static_cast<unsigned char>(c.a * 255.0f + 0.5f);
            }
        }
        return true;
    }

    // Sub-image path
    if (!tex.textureData || tex.width <= 0 || tex.height <= 0) {
        errorState = GL_INVALID_OPERATION;
        return false;
    }
    if (xoffset < 0 || yoffset < 0 ||
        xoffset + width > tex.width || yoffset + height > tex.height) {
        errorState = GL_INVALID_VALUE;
        return false;
    }
    if (!pixels) {
        errorState = GL_INVALID_VALUE;
        return false;
    }

    const unsigned char* src = static_cast<const unsigned char*>(pixels);
    const int stride = unpackRowStride(width, comps, unpackAlignment);
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = src + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            Col4 c{};
            decodeTexel(row + x * comps, format, c);
            unsigned char* dst =
                tex.textureData + ((yoffset + y) * tex.width + (xoffset + x)) * 4;
            dst[0] = static_cast<unsigned char>(c.r * 255.0f + 0.5f);
            dst[1] = static_cast<unsigned char>(c.g * 255.0f + 0.5f);
            dst[2] = static_cast<unsigned char>(c.b * 255.0f + 0.5f);
            dst[3] = static_cast<unsigned char>(c.a * 255.0f + 0.5f);
        }
    }
    return true;
}

} // namespace

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

    void glVertex3fv(const GLfloat *v) {
        //PrintInfo("glVertex3fv");
        if (forwardToSystemGl) {
            static void (*real_gl)(const GLfloat *) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(const GLfloat *)) dlsym(RTLD_NEXT, "glVertex3fv");
            }
            real_gl(v);
        }
        if (activeDisplayListIndex == 0) {
            Process_glVertex3fv(v);
        } else {
            if (compileAndExecute) {
                Process_glVertex3fv(v);
            }
            //Record_glVector3f(v);
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

    void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glColor4f");
            }
            real_gl(red,green,blue,alpha);
        }
        if (activeDisplayListIndex == 0) {
            Process_glColor4f(red, green, blue, alpha);
        } else {
            if (compileAndExecute) {
                Process_glColor4f(red, green, blue, alpha);
            }
            Record_glColor4f(red, green, blue, alpha);
        }
    }

    void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
        glColor4f(red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f);
    }

    void glDeleteTextures(GLsizei n, const GLuint* textures) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLsizei, const GLuint*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLsizei, const GLuint*)) dlsym(RTLD_NEXT, "glDeleteTextures");
            }
            real_gl(n, textures);
        }
        if (!textures || n <= 0) return;
        for (GLsizei i = 0; i < n; ++i) {
            const GLuint id = textures[i];
            if (id == 0 || id >= textureArray.size()) continue;
            auto& slot = textureArray[id];
            if (slot.texture2D.textureData) {
                free(slot.texture2D.textureData);
                slot.texture2D.textureData = nullptr;
            }
            slot = TextureSlot{};
            if (lastAccessedTexture == &textureArray[id] || boundTexture2D == id) {
                lastAccessedTexture = nullptr;
                boundTexture2D = 0;
            }
        }
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
                return real_gl ? real_gl(name) : nullptr;
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
                lights[light & 0xFF].w = params[3];
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
        //PrintInfo("glBindTexture ");
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
        //PrintInfo("\n");
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
        PrintInfo("glCallList ");
        PrintInfo(list);
        if (forwardToSystemGl) {
            static void (*real_gl)(GLuint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLuint)) dlsym(RTLD_NEXT, "glCallList");
            }
            real_gl(list);
        }

        if (activeDisplayListIndex == 0) {
            Process_glCallList(list);
        } else {
            if (compileAndExecute) {
                Process_glCallList(list);
            }
            Record_glCallList(list);
        }
        PrintInfo("\n");
    }

    void glListBase(GLuint base) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLuint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLuint)) dlsym(RTLD_NEXT, "glListBase");
            }
            real_gl(base);
        }
        Process_glListBase(base);
    }

    void glCallLists(GLsizei n, GLenum type, const GLvoid* lists) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLsizei, GLenum, const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLsizei, GLenum, const GLvoid*)) dlsym(RTLD_NEXT, "glCallLists");
            }
            real_gl(n, type, lists);
        }
        Process_glCallLists(n, type, lists);
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

        // Presentation happens on glXSwapBuffers / glFlush / glFinish.
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
        if (activeDisplayListIndex == 0) {
            Process_glLoadMatrixf(m);
        } else {
            if (compileAndExecute) {
                Process_glLoadMatrixf(m);
            }
            Record_glLoadMatrixf(m);
        }
        PrintInfo("\n");
    }

    void glMultMatrixf(const GLfloat *m) {
        if (forwardToSystemGl) {
            static void (*real_gl)(const GLfloat *m) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(const GLfloat *m)) dlsym(RTLD_NEXT, "glMultMatrixf");
            }
            real_gl(m);
        }
        PrintInfo("glMultMatrixf");
        if (activeDisplayListIndex == 0) {
            Process_glMultMatrixf(m);
        } else {
            if (compileAndExecute) {
                Process_glMultMatrixf(m);
            }
            Record_glMultMatrixf(m);
        }
        PrintInfo("\n");
    }

    void glDepthFunc(GLenum func) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glDepthFunc");
            }
            real_gl(func);
        }
        PrintInfo("glDepthFunc");
        if (activeDisplayListIndex == 0) {
            Process_glDepthFunc(func);
        } else {
            if (compileAndExecute) {
                Process_glDepthFunc(func);
            }
            Record_glDepthFunc(func);
        }
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

    // Set fog integer
    void glFogiv(GLenum pname, const GLint *params) {
        PrintInfo("glFogiv");
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,const GLint *) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,const GLint *)) dlsym(RTLD_NEXT, "glFogiv");
            }
            real_gl(pname,params);
        }
        if (activeDisplayListIndex == 0) {
            Process_glFogiv(pname,params);
        } else {
            if (compileAndExecute) {
                Process_glFogiv(pname,params);
            }
            Record_glFogiv(pname,params);
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

        int xEnd = std::min(x + width, renderAreaWidth);
        int yEnd = std::min(y + height, renderAreaHeight);
        // Assume format is always RGBA
        // Assume format is always unsigned Byte
        uint8_t* pix = static_cast<uint8_t*>(pixels);
        for (int iy = y; iy < yEnd; iy++) {
            int srcY = (renderAreaHeight - 1 - iy);
            for (int ix = x; ix < xEnd; ix++) {
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
                params[0] = glViewportX;
                params[1] = glViewportY;
                params[2] = glViewportW;
                params[3] = glViewportH;
                break;
            case GL_DEPTH_BITS:
                PrintInfo("GL_DEPTH_BITS");
                params[0] = DEPTH_BITS;
                break;
            case GL_SCISSOR_BOX:
                // Return OpenGL bottom-left convention.
                params[0] = scissorX;
                params[1] = renderAreaHeight - (scissorY + scissorHeight);
                params[2] = scissorWidth;
                params[3] = scissorHeight;
                break;
            case GL_MAX_TEXTURE_SIZE:
                PrintInfo("GL_MAX_TEXTURE_SIZE");
                params[0] = MAX_TEXTURE_SIZE;
                break;
            case GL_LIST_BASE:
                params[0] = static_cast<GLint>(listBase);
                break;
            case GL_MAX_MODELVIEW_STACK_DEPTH:
                params[0] = MAX_MODELVIEW_STACK_DEPTH;
                break;
            case GL_MAX_PROJECTION_STACK_DEPTH:
                params[0] = MAX_PROJECTION_STACK_DEPTH;
                break;
            case GL_MAX_TEXTURE_STACK_DEPTH:
                params[0] = MAX_TEXTURE_STACK_DEPTH;
                break;
            case GL_MODELVIEW_STACK_DEPTH:
                params[0] = modelMatrixPtr + 1;
                break;
            case GL_PROJECTION_STACK_DEPTH:
                params[0] = projMatrixPtr + 1;
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
                PrintInfo("GL_VERTEX_ARRAY ");
                vertexArrayActive = true;
                break;
            case GL_COLOR_ARRAY:
                PrintInfo("GL_COLOR_ARRAY ");
                colorArrayActive = true;
                break;
            case GL_TEXTURE_COORD_ARRAY:
                PrintInfo("GL_TEXTURE_COORD_ARRAY ");
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
                PrintInfo("GL_VERTEX_ARRAY ");
                vertexArrayActive = false;
                break;
            case GL_COLOR_ARRAY:
                PrintInfo("GL_COLOR_ARRAY ");
                colorArrayActive = false;
                break;
            case GL_TEXTURE_COORD_ARRAY:
                PrintInfo("GL_TEXTURE_COORD_ARRAY ");
                textureArrayActive = false;
                break;
        }
        PrintInfo("\n");
    }

    void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
        PrintInfo("glVertexPointer ");
        PrintInfo(size);
        PrintInfo(" - ");
        PrintInfoHex(type);
        PrintInfo(" - ");
        PrintInfo(stride);
        PrintInfo(" - ");
        PrintInfoAddr((void*)ptr);
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLenum,GLsizei,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLenum,GLsizei,const GLvoid*)) dlsym(RTLD_NEXT, "glVertexPointer");
            }
            real_gl(size,type,stride,ptr);
        }
        vertexArrayPointer = ptr;
        vertexArrayStride = stride;
        vertexArrayType = type;
        vertexArrayTypeSize = size;
        PrintInfo("\n");
    }
    
    void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
        PrintInfo("glColorPointer ");
        PrintInfo(size);
        PrintInfo(" - ");
        PrintInfoHex(type);
        PrintInfo(" - ");
        PrintInfo(stride);
        PrintInfo(" - ");
        PrintInfoAddr((void*)ptr);
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLenum,GLsizei,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLenum,GLsizei,const GLvoid*)) dlsym(RTLD_NEXT, "glColorPointer");
            }
            real_gl(size,type,stride,ptr);
        }
        colorArrayPointer = ptr;
        colorArrayStride = stride;
        colorArrayType = type;
        colorArrayTypeSize = size;
        PrintInfo("\n");
    }

    void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
        PrintInfo("glTexCoordPointer ");
        PrintInfo(size);
        PrintInfo(" - ");
        PrintInfoHex(type);
        PrintInfo(" - ");
        PrintInfo(stride);
        PrintInfo(" - ");
        PrintInfoAddr((void*)ptr);
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLenum,GLsizei,const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLenum,GLsizei,const GLvoid*)) dlsym(RTLD_NEXT, "glTexCoordPointer");
            }
            real_gl(size,type,stride,ptr);
        }
        textureArrayPointer = ptr;
        textureArrayStride = stride;
        textureArrayType = type;
        textureArrayTypeSize = size;
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
        PrintInfo(count);
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

    void glTexEnvi(GLenum target, GLenum pname, GLint param) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum, GLenum, GLint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum, GLenum, GLint)) dlsym(RTLD_NEXT, "glTexEnvi");
            }
            real_gl(target, pname, param);
        }
        if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE) {
            textureEnvMode = static_cast<GLenum>(param);
        }
    }

    void glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
        glTexEnvi(target, pname, static_cast<GLint>(param));
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
        if (level != 0) return; // mipmaps: accept and ignore for now
        (void)internalFormat;
        (void)border;
        (void)target;
        uploadTextureLevel0(lastAccessedTexture->texture2D, 0, 0, width, height,
                            format, type, pixels, true);
    }

    // Minecraft allocates with null glTexImage2D, then fills via TexSubImage2D.
    void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                         GLsizei width, GLsizei height, GLenum format, GLenum type,
                         const GLvoid* pixels) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei,
                                  GLenum, GLenum, const GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei,
                                    GLenum, GLenum, const GLvoid*))
                    dlsym(RTLD_NEXT, "glTexSubImage2D");
            }
            real_gl(target, level, xoffset, yoffset, width, height, format, type, pixels);
        }
        if (!lastAccessedTexture) return;
        if (level != 0) return;
        (void)target;
        uploadTextureLevel0(lastAccessedTexture->texture2D, xoffset, yoffset, width, height,
                            format, type, pixels, false);
    }

    void glPixelStorei(GLenum pname, GLint param) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum, GLint) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum, GLint)) dlsym(RTLD_NEXT, "glPixelStorei");
            }
            real_gl(pname, param);
        }
        if (pname == GL_UNPACK_ALIGNMENT) {
            if (param == 1 || param == 2 || param == 4 || param == 8) {
                unpackAlignment = param;
            } else {
                errorState = GL_INVALID_VALUE;
            }
        } else if (pname == GL_PACK_ALIGNMENT) {
            // Readback packing ignored for now.
        }
    }

    void glAlphaFunc(GLenum func, GLclampf ref) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum, GLclampf) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum, GLclampf)) dlsym(RTLD_NEXT, "glAlphaFunc");
            }
            real_gl(func, ref);
        }
        alphaFunc = func;
        alphaRef = std::clamp(static_cast<float>(ref), 0.0f, 1.0f);
    }

    void glBlendFunc(GLenum sfactor, GLenum dfactor) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum, GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum, GLenum)) dlsym(RTLD_NEXT, "glBlendFunc");
            }
            real_gl(sfactor, dfactor);
        }
        blendSrcFactor = sfactor;
        blendDstFactor = dfactor;
    }

    void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint, GLint, GLsizei, GLsizei) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint, GLint, GLsizei, GLsizei)) dlsym(RTLD_NEXT, "glScissor");
            }
            real_gl(x, y, width, height);
        }
        // Convert bottom-left scissor to top-left buffer coordinates.
        scissorWidth = width;
        scissorHeight = height;
        scissorX = x;
        scissorY = renderAreaHeight - (y + height);
        if (scissorY < 0) scissorY = 0;
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
    
    GLenum glGetError() {
        if (forwardToSystemGl) {
            static GLenum (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (GLenum (*)()) dlsym(RTLD_NEXT, "glGetError");
            }
            real_gl();
        }
        GLenum err = errorState;
        errorState = GL_NO_ERROR;
        return err;
    };

    void glGetFloatv(GLenum pname, GLfloat* params) {
        if (!params) return;
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum, GLfloat*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum, GLfloat*)) dlsym(RTLD_NEXT, "glGetFloatv");
            }
            real_gl(pname, params);
            return;
        }

        auto writeMat = [](const Mat4x4& m, GLfloat* p) {
            // Column-major OpenGL memory layout.
            p[0]  = static_cast<GLfloat>(m.a.x); p[1]  = static_cast<GLfloat>(m.a.y);
            p[2]  = static_cast<GLfloat>(m.a.z); p[3]  = static_cast<GLfloat>(m.a.w);
            p[4]  = static_cast<GLfloat>(m.b.x); p[5]  = static_cast<GLfloat>(m.b.y);
            p[6]  = static_cast<GLfloat>(m.b.z); p[7]  = static_cast<GLfloat>(m.b.w);
            p[8]  = static_cast<GLfloat>(m.c.x); p[9]  = static_cast<GLfloat>(m.c.y);
            p[10] = static_cast<GLfloat>(m.c.z); p[11] = static_cast<GLfloat>(m.c.w);
            p[12] = static_cast<GLfloat>(m.d.x); p[13] = static_cast<GLfloat>(m.d.y);
            p[14] = static_cast<GLfloat>(m.d.z); p[15] = static_cast<GLfloat>(m.d.w);
        };

        switch (pname) {
        case GL_MODELVIEW_MATRIX:
            writeMat(modelMatrices[modelMatrixPtr], params);
            break;
        case GL_PROJECTION_MATRIX:
            writeMat(projMatrices[projMatrixPtr], params);
            break;
        case GL_TEXTURE_MATRIX:
            writeMat(texMatrices[texMatrixPtr], params);
            break;
        case GL_FOG_COLOR:
            params[0] = fogColor.r;
            params[1] = fogColor.g;
            params[2] = fogColor.b;
            params[3] = fogColor.a;
            break;
        case GL_FOG_DENSITY:
            params[0] = fogDensity;
            break;
        case GL_FOG_START:
            params[0] = fogStart;
            break;
        case GL_FOG_END:
            params[0] = fogEnd;
            break;
        case GL_CURRENT_COLOR:
            params[0] = currentColor.r;
            params[1] = currentColor.g;
            params[2] = currentColor.b;
            params[3] = currentColor.a;
            break;
        case GL_COLOR_CLEAR_VALUE:
            params[0] = clearColor.r;
            params[1] = clearColor.g;
            params[2] = clearColor.b;
            params[3] = clearColor.a;
            break;
        case GL_ALPHA_TEST_REF:
            params[0] = alphaRef;
            break;
        case GL_DEPTH_CLEAR_VALUE:
            params[0] = 1.0f;
            break;
        default:
            // Leave params untouched for unimplemented pnames (LWJGL rarely
            // depends on them). Mark error for strict callers.
            errorState = GL_INVALID_ENUM;
            break;
        }
    }

    // LWJGL 2 binds GL11.glGetFloat → glGetFloatv.
    void glGetFloat(GLenum pname, GLfloat* params) {
        glGetFloatv(pname, params);
    }
}
