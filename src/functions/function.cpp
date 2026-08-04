#include "function.h"
#include "render.h"
#include "framebuffer.h"
#include "global.h"
#include "maths.h"
#include <GL/gl.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>

void Process_glVertex2f(GLfloat x, GLfloat y) {
    if (vertexIndex >= MAX_VERTICES) {
        errorState = GL_OUT_OF_MEMORY;
        return;
    }
    vertices[vertexIndex].pos = Vec3{x,y,0};
    vertices[vertexIndex].col = currentColor;
    vertices[vertexIndex].uv = currentTextureUV;
    vertexIndex++;
}

void Process_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (vertexIndex >= MAX_VERTICES) {
        errorState = GL_OUT_OF_MEMORY;
        return;
    }
    vertices[vertexIndex].pos = Vec3{x,y,z};
    vertices[vertexIndex].col = currentColor;
    vertices[vertexIndex].uv = currentTextureUV;
    vertexIndex++;
}

void Process_glVertex3fv(const GLfloat *v) {
    if (vertexIndex >= MAX_VERTICES) {
        errorState = GL_OUT_OF_MEMORY;
        return;
    }
    vertices[vertexIndex].pos = Vec3{v[0],v[1],v[2]};
    vertices[vertexIndex].col = currentColor;
    vertices[vertexIndex].uv = currentTextureUV;
    vertexIndex++;
}

void Process_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    currentColor = Col4{red,green,blue, alpha};
}

void Process_glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    currentColor = Col4{red,green,blue, 1.0};
}

void Process_glTexCoord2f(GLfloat s, GLfloat t) {
    currentTextureUV = Vec2{s,t};
}

void Process_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    glViewportX = x;
    glViewportY = y;
    glViewportW = width;
    glViewportH = height;

    // OpenGL viewport origin is bottom-left. Our color buffer is top-left, so
    // convert to a top-left offset for projection/rasterization.
    viewportAreaWidth = width;
    viewportAreaHeight = height;
    viewportAreaTotal = viewportAreaWidth * viewportAreaHeight;
    viewportOffsetX = x;
    viewportOffsetY = renderAreaHeight - (y + height);
    if (viewportOffsetY < 0) viewportOffsetY = 0;

    const int neededW = std::max(renderAreaWidth, x + width);
    const int neededH = std::max(renderAreaHeight, y + height);
    if (!frameBufferColor || neededW > renderAreaWidth || neededH > renderAreaHeight) {
        EnsureRenderBuffers(neededW, neededH);
        viewportOffsetY = renderAreaHeight - (y + height);
        if (viewportOffsetY < 0) viewportOffsetY = 0;
    }

    ReCreateWindow();
}

void Process_glBegin(GLenum mode) {
    drawingMode = mode;
    vertexIndex = 0;
    switch(drawingMode) {
        case GL_POINTS:
            //PrintInfo("GL_POINTS ");
            break;
        case GL_LINES:
            //PrintInfo("GL_LINES ");
            break;
        case GL_LINE_LOOP:
            //PrintInfo("GL_LINE_LOOP ");
            break;
        case GL_LINE_STRIP:
            //PrintInfo("GL_LINE_STRIP ");
            break;
        case GL_TRIANGLES:
            //PrintInfo("GL_TRIANGLES ");
            break;
        case GL_TRIANGLE_STRIP:
            PrintInfo("GL_TRIANGLE_STRIP ");
            break;
        case GL_TRIANGLE_FAN:
            PrintInfo("GL_TRIANGLE_FAN ");
            break;
        case GL_QUADS:
            PrintInfo("GL_QUADS ");
            break;
        case GL_QUAD_STRIP:
            PrintInfo("GL_QUAD_STRIP ");
            break;
        case GL_POLYGON:
            PrintInfo("GL_POLYGON ");
            break;
    }
}

void Process_glEnd() {
    switch(drawingMode) {
        case GL_POINTS:
            for (int i = 0; i < vertexIndex; i++) {
                Vec4 screenPos = ProjectPosition(vertices[i].pos);
                RenderPixel(Vec3{screenPos.x, screenPos.y, screenPos.z}, vertices[i].col);
            }
            break;
        case GL_LINES:
            for (int i = 0; i + 1 < vertexIndex; i+=2) {
                Vec4 screenPosA = ProjectPosition(vertices[i].pos);
                Vec4 screenPosB = ProjectPosition(vertices[i+1].pos);
                RenderLine(Vec3{screenPosA.x, screenPosA.y, screenPosA.z}, vertices[i].col, Vec3{screenPosB.x, screenPosB.y, screenPosB.z}, vertices[i+1].col);
            }
            break;
        case GL_TRIANGLE_FAN:
            for (int i = 1; i + 1 < vertexIndex; i++) {
                Triangle tri = Triangle{
                    vertices[0], 
                    vertices[i], 
                    vertices[i+1],
                };
                RenderTriangle(tri);
            }
            break;
        case GL_TRIANGLES:
            for (int i = 0; i + 2 < vertexIndex; i+=3) {
                Triangle tri = Triangle{
                    vertices[i], 
                    vertices[i+1], 
                    vertices[i+2],
                };
                RenderTriangle(tri);
            }
            break;
        case GL_QUADS:
            for (int i = 0; i + 3 < vertexIndex; i+=4) {
                Triangle triA = Triangle{
                    vertices[i], 
                    vertices[i+1], 
                    vertices[i+2],
                };
                Triangle triB = Triangle{
                    vertices[i],
                    vertices[i+2], 
                    vertices[i+3],
                };
                RenderTriangle(triA);
                RenderTriangle(triB);
            } 
            break;
        case GL_QUAD_STRIP:
            for (int i = 0; i + 3 < vertexIndex; i+=2) {
                Triangle triA = Triangle{
                    vertices[i],
                    vertices[i+1],
                    vertices[i+2],
                };
                Triangle triB = Triangle{
                    vertices[i+1],
                    vertices[i+3],
                    vertices[i+2],
                };
                RenderTriangle(triA);
                RenderTriangle(triB);
            } 
            break;
    }
    vertexIndex = 0;
}

void Process_glLoadIdentity() {
    *lastAccessedMatrix = Mat4x4 {
        Vec4 { 1, 0 ,0,0 },
        Vec4 { 0, 1 ,0,0 },
        Vec4 { 0, 0 ,1,0 },
        Vec4 { 0, 0 ,0,1 }
    };
}

void Process_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    Mat4x4 T = {
        Vec4{1, 0, 0, 0},
        Vec4{0, 1, 0, 0},
        Vec4{0, 0, 1, 0},
        Vec4{double(x), double(y), double(z), 1}
    };

    *lastAccessedMatrix = (*lastAccessedMatrix) * T;
}

void Process_glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
    // Convert to radians
    double angle = angleDeg * M_PI / 180.0;

    // Normalize axis
    Vec3 u = Normalize(Vec3{x, y, z});
    double c = cos(angle);
    double s = sin(angle);
    double t = 1 - c;

    Mat4x4 R = {
        Vec4{t*u.x*u.x + c,     t*u.x*u.y + s*u.z, t*u.x*u.z - s*u.y, 0},
        Vec4{t*u.x*u.y - s*u.z, t*u.y*u.y + c,     t*u.y*u.z + s*u.x, 0},
        Vec4{t*u.x*u.z + s*u.y, t*u.y*u.z - s*u.x, t*u.z*u.z + c,     0},
        Vec4{0,                  0,                  0,               1}
    };

    *lastAccessedMatrix = (*lastAccessedMatrix) * R;
}
void Process_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    Mat4x4 S = {
        Vec4{double(x), 0, 0, 0},
        Vec4{0, double(y), 0, 0},
        Vec4{0, 0, double(z), 0},
        Vec4{0, 0, 0, 1}
    };

    *lastAccessedMatrix = (*lastAccessedMatrix) * S;
}

void Process_glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    Mat4x4 F = Mat4x4{
        Vec4{ 2*n/(r-l), 0,            0,               0 },
        Vec4{ 0,         2*n/(t-b),    0,               0 },
        Vec4{ (r+l)/(r-l), (t+b)/(t-b), -(f+n)/(f-n),  -1 },
        Vec4{ 0,          0,           -2*f*n/(f-n),    0 }
    };
    *lastAccessedMatrix = (*lastAccessedMatrix) * F;
}

void Process_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    Mat4x4 O = Mat4x4{
        Vec4{ 2.0/(r-l), 0,           0,           0 },
        Vec4{ 0,         2.0/(t-b),   0,           0 },
        Vec4{ 0,         0,         -2.0/(f-n),    0 },
        Vec4{ -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1 }
    };
    *lastAccessedMatrix = (*lastAccessedMatrix) * O;
}

void Process_glPushMatrix() {
    switch(matrixMode) {
        case GL_PROJECTION:
            if (projMatrixPtr >= MAX_PROJECTION_STACK_DEPTH - 1) {
                errorState = GL_STACK_OVERFLOW;
                return;
            }
            projMatrices[projMatrixPtr + 1] = projMatrices[projMatrixPtr];
            projMatrixPtr++;
            lastAccessedMatrix = &projMatrices[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            if (modelMatrixPtr >= MAX_MODELVIEW_STACK_DEPTH - 1) {
                errorState = GL_STACK_OVERFLOW;
                return;
            }
            modelMatrices[modelMatrixPtr + 1] = modelMatrices[modelMatrixPtr];
            modelMatrixPtr++;
            lastAccessedMatrix = &modelMatrices[modelMatrixPtr];
            break;
        case GL_TEXTURE:
            if (texMatrixPtr >= MAX_TEXTURE_STACK_DEPTH - 1) {
                errorState = GL_STACK_OVERFLOW;
                return;
            }
            texMatrices[texMatrixPtr + 1] = texMatrices[texMatrixPtr];
            texMatrixPtr++;
            lastAccessedMatrix = &texMatrices[texMatrixPtr];
            break;
    }
}

void Process_glPopMatrix() {
    switch(matrixMode) {
        case GL_PROJECTION:
            if (projMatrixPtr-1 < 0) {
                errorState = GL_STACK_UNDERFLOW;
                return;
            }
            projMatrixPtr--;
            lastAccessedMatrix = &projMatrices[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            if (modelMatrixPtr-1 < 0) {
                errorState = GL_STACK_UNDERFLOW;
                return;
            }
            modelMatrixPtr--;
            lastAccessedMatrix = &modelMatrices[modelMatrixPtr];
            break;
        case GL_TEXTURE:
            if (texMatrixPtr-1 < 0) {
                errorState = GL_STACK_UNDERFLOW;
                return;
            }
            texMatrixPtr--;
            lastAccessedMatrix = &texMatrices[texMatrixPtr];
            break;
    }
}

void Process_glNewList(GLuint list, GLenum mode) {
    if (list == 0) {
        errorState = GL_INVALID_VALUE;
        return;
    }
    if (activeDisplayListIndex != 0) {
        errorState = GL_INVALID_OPERATION;
        return;
    }
    compileAndExecute = (mode == GL_COMPILE_AND_EXECUTE);
    if (compileAndExecute)
        PrintInfo("COMPILE_AND_EXECUTE");
    else
        PrintInfo("COMPILE");

    activeDisplayListIndex = list;
    displayListBuffer.commands.clear();
}

void Process_glEndList() {
    if (activeDisplayListIndex == 0) {
        errorState = GL_INVALID_OPERATION;
        return;
    }

    const GLuint idx = activeDisplayListIndex - 1;

    if (activeDisplayListIndex > displayLists.size())
        displayLists.resize(activeDisplayListIndex);

    displayLists[idx] = displayListBuffer;

    activeDisplayListIndex = 0;
}

void Process_glCallList(GLuint list) {
    if (list == 0 || list > displayLists.size()) return;
    if (displayLists[list - 1].commands.empty()) return;
    ExecuteDisplayList(displayLists[list - 1]);
}

void Process_glListBase(GLuint base) {
    listBase = base;
}

void Process_glCallLists(GLsizei n, GLenum type, const GLvoid* lists) {
    if (!lists || n <= 0) return;
    for (GLsizei i = 0; i < n; ++i) {
        GLuint offset = 0;
        switch (type) {
        case GL_BYTE:
            offset = static_cast<GLuint>(static_cast<const GLbyte*>(lists)[i]);
            break;
        case GL_UNSIGNED_BYTE:
            offset = static_cast<const GLubyte*>(lists)[i];
            break;
        case GL_SHORT:
            offset = static_cast<GLuint>(static_cast<const GLshort*>(lists)[i]);
            break;
        case GL_UNSIGNED_SHORT:
            offset = static_cast<const GLushort*>(lists)[i];
            break;
        case GL_INT:
            offset = static_cast<GLuint>(static_cast<const GLint*>(lists)[i]);
            break;
        case GL_UNSIGNED_INT:
            offset = static_cast<const GLuint*>(lists)[i];
            break;
        case GL_2_BYTES:
            offset = (static_cast<const GLubyte*>(lists)[i * 2] << 8) |
                     static_cast<const GLubyte*>(lists)[i * 2 + 1];
            break;
        case GL_3_BYTES:
            offset = (static_cast<const GLubyte*>(lists)[i * 3] << 16) |
                     (static_cast<const GLubyte*>(lists)[i * 3 + 1] << 8) |
                     static_cast<const GLubyte*>(lists)[i * 3 + 2];
            break;
        case GL_4_BYTES:
            offset = (static_cast<const GLubyte*>(lists)[i * 4] << 24) |
                     (static_cast<const GLubyte*>(lists)[i * 4 + 1] << 16) |
                     (static_cast<const GLubyte*>(lists)[i * 4 + 2] << 8) |
                     static_cast<const GLubyte*>(lists)[i * 4 + 3];
            break;
        default:
            return;
        }
        Process_glCallList(listBase + offset);
    }
}

GLuint Process_glGenLists(GLsizei range) {
    if (range <= 0) return 0;
    GLuint firstID = displayLists.size();
    displayLists.resize(displayLists.size() + range);
    return firstID + 1;
}

GLboolean Process_IsList(GLuint list) {
    if (list == 0 || list > displayLists.size()) return GL_FALSE;
    return displayLists[list - 1].commands.empty() ? GL_FALSE : GL_TRUE;
}

void Process_glDeleteLists(GLuint list, GLsizei range) {
    if (list == 0 || list > displayLists.size()) return;

    if (list + range - 1 > displayLists.size())
        range = displayLists.size() - list + 1;

    for (GLuint i = 0; i < range; ++i)
        displayLists[list - 1 + i].commands.clear();
}

void Process_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!vertexArrayActive || !vertexArrayPointer || count <= 0) return;

    auto typeBytes = [](GLenum type) -> int {
        switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE: return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT: return 2;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT: return 4;
        case GL_DOUBLE: return 8;
        default: return 4;
        }
    };

    auto readFloat = [](const uint8_t* p, GLenum type) -> float {
        switch (type) {
        case GL_BYTE: return *reinterpret_cast<const GLbyte*>(p) / 127.0f;
        case GL_UNSIGNED_BYTE: return *p / 255.0f;
        case GL_SHORT: return *reinterpret_cast<const GLshort*>(p) / 32767.0f;
        case GL_UNSIGNED_SHORT: return *reinterpret_cast<const GLushort*>(p) / 65535.0f;
        case GL_INT: return static_cast<float>(*reinterpret_cast<const GLint*>(p));
        case GL_UNSIGNED_INT: return static_cast<float>(*reinterpret_cast<const GLuint*>(p));
        case GL_FLOAT: return *reinterpret_cast<const GLfloat*>(p);
        case GL_DOUBLE: return static_cast<float>(*reinterpret_cast<const GLdouble*>(p));
        default: return *reinterpret_cast<const GLfloat*>(p);
        }
    };

    auto elemPtr = [&](const void* base, GLsizei stride, GLint size, GLenum type, int index) -> const uint8_t* {
        const int natural = size * typeBytes(type);
        const int step = stride > 0 ? stride : natural;
        return static_cast<const uint8_t*>(base) + index * step;
    };

    auto fetchPos = [&](int i) -> Vec3 {
        const uint8_t* p = elemPtr(vertexArrayPointer, vertexArrayStride,
                                   vertexArrayTypeSize, vertexArrayType, i);
        const float x = readFloat(p, vertexArrayType);
        const float y = vertexArrayTypeSize > 1 ? readFloat(p + typeBytes(vertexArrayType), vertexArrayType) : 0.0f;
        const float z = vertexArrayTypeSize > 2 ? readFloat(p + 2 * typeBytes(vertexArrayType), vertexArrayType) : 0.0f;
        // Positions are not normalized for FLOAT/INT — re-read raw floats for FLOAT.
        if (vertexArrayType == GL_FLOAT) {
            const float* f = reinterpret_cast<const float*>(p);
            return Vec3{f[0], vertexArrayTypeSize > 1 ? f[1] : 0.0, vertexArrayTypeSize > 2 ? f[2] : 0.0};
        }
        return Vec3{x, y, z};
    };

    auto fetchCol = [&](int i) -> Col4 {
        if (!colorArrayActive || !colorArrayPointer) return currentColor;
        const uint8_t* p = elemPtr(colorArrayPointer, colorArrayStride,
                                   colorArrayTypeSize, colorArrayType, i);
        if (colorArrayType == GL_UNSIGNED_BYTE || colorArrayType == GL_BYTE) {
            const float r = p[0] / 255.0f;
            const float g = colorArrayTypeSize > 1 ? p[1] / 255.0f : r;
            const float b = colorArrayTypeSize > 2 ? p[2] / 255.0f : r;
            float a = colorArrayTypeSize > 3 ? p[3] / 255.0f : 1.0f;
            if (a <= 0.0f) a = 1.0f;
            return Col4{r, g, b, a};
        }
        if (colorArrayType == GL_FLOAT) {
            const float* f = reinterpret_cast<const float*>(p);
            return Col4{
                f[0],
                colorArrayTypeSize > 1 ? f[1] : f[0],
                colorArrayTypeSize > 2 ? f[2] : f[0],
                colorArrayTypeSize > 3 ? f[3] : 1.0f
            };
        }
        return Col4{
            readFloat(p, colorArrayType),
            colorArrayTypeSize > 1 ? readFloat(p + typeBytes(colorArrayType), colorArrayType) : 1.0f,
            colorArrayTypeSize > 2 ? readFloat(p + 2 * typeBytes(colorArrayType), colorArrayType) : 1.0f,
            colorArrayTypeSize > 3 ? readFloat(p + 3 * typeBytes(colorArrayType), colorArrayType) : 1.0f
        };
    };

    auto fetchUV = [&](int i) -> Vec2 {
        if (!textureArrayActive || !textureArrayPointer) return currentTextureUV;
        const uint8_t* p = elemPtr(textureArrayPointer, textureArrayStride,
                                   textureArrayTypeSize, textureArrayType, i);
        if (textureArrayType == GL_FLOAT) {
            const float* f = reinterpret_cast<const float*>(p);
            return Vec2{f[0], textureArrayTypeSize > 1 ? f[1] : 0.0};
        }
        return Vec2{
            readFloat(p, textureArrayType),
            textureArrayTypeSize > 1 ? readFloat(p + typeBytes(textureArrayType), textureArrayType) : 0.0f
        };
    };

    auto makeVert = [&](int i) -> Vertex {
        Vertex v{};
        v.pos = fetchPos(i);
        v.col = fetchCol(i);
        v.uv = fetchUV(i);
        return v;
    };

    auto drawTri = [&](int a, int b, int c) {
        RenderTriangle(Triangle{makeVert(a), makeVert(b), makeVert(c)});
    };

    switch (mode) {
    case GL_TRIANGLES:
        for (int i = 0; i + 2 < count; i += 3) {
            drawTri(first + i, first + i + 1, first + i + 2);
        }
        break;
    case GL_TRIANGLE_STRIP:
        for (int i = 0; i + 2 < count; ++i) {
            if (i & 1) drawTri(first + i + 1, first + i, first + i + 2);
            else       drawTri(first + i, first + i + 1, first + i + 2);
        }
        break;
    case GL_TRIANGLE_FAN:
        for (int i = 1; i + 1 < count; ++i) {
            drawTri(first, first + i, first + i + 1);
        }
        break;
    case GL_QUADS:
        for (int i = 0; i + 3 < count; i += 4) {
            drawTri(first + i, first + i + 1, first + i + 2);
            drawTri(first + i, first + i + 2, first + i + 3);
        }
        break;
    case GL_QUAD_STRIP:
        for (int i = 0; i + 3 < count; i += 2) {
            drawTri(first + i, first + i + 1, first + i + 2);
            drawTri(first + i + 1, first + i + 3, first + i + 2);
        }
        break;
    default:
        break;
    }
}

void Process_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (!vertexArrayActive || !vertexArrayPointer) {
        errorState = GL_INVALID_OPERATION;
        return;
    }
    if (count < 0) {
        errorState = GL_INVALID_VALUE;
        return;
    }
    if (!indices || count == 0) return;

    auto readIndex = [&](int i) -> GLuint {
        switch (type) {
        case GL_UNSIGNED_BYTE:  return static_cast<const uint8_t*>(indices)[i];
        case GL_UNSIGNED_SHORT: return static_cast<const uint16_t*>(indices)[i];
        case GL_UNSIGNED_INT:   return static_cast<const uint32_t*>(indices)[i];
        default: return 0;
        }
    };
    if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
        errorState = GL_INVALID_ENUM;
        return;
    }

    // Reuse DrawArrays fetch path by temporarily building an index remap via
    // direct triangle emission (no 4096 immediate-mode cap).
    auto typeBytes = [](GLenum t) -> int {
        switch (t) {
        case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
        case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
        case GL_INT: case GL_UNSIGNED_INT: case GL_FLOAT: return 4;
        case GL_DOUBLE: return 8;
        default: return 4;
        }
    };
    auto elemPtr = [&](const void* base, GLsizei stride, GLint size, GLenum t, int index) -> const uint8_t* {
        const int natural = size * typeBytes(t);
        const int step = stride > 0 ? stride : natural;
        return static_cast<const uint8_t*>(base) + index * step;
    };
    auto fetchPos = [&](GLuint i) -> Vec3 {
        const uint8_t* p = elemPtr(vertexArrayPointer, vertexArrayStride,
                                   vertexArrayTypeSize, vertexArrayType, static_cast<int>(i));
        if (vertexArrayType == GL_FLOAT) {
            const float* f = reinterpret_cast<const float*>(p);
            return Vec3{f[0], vertexArrayTypeSize > 1 ? f[1] : 0.0, vertexArrayTypeSize > 2 ? f[2] : 0.0};
        }
        return Vec3{0, 0, 0};
    };
    auto fetchCol = [&](GLuint i) -> Col4 {
        if (!colorArrayActive || !colorArrayPointer) return currentColor;
        const uint8_t* p = elemPtr(colorArrayPointer, colorArrayStride,
                                   colorArrayTypeSize, colorArrayType, static_cast<int>(i));
        if (colorArrayType == GL_UNSIGNED_BYTE) {
            return Col4{
                p[0] / 255.0f,
                colorArrayTypeSize > 1 ? p[1] / 255.0f : p[0] / 255.0f,
                colorArrayTypeSize > 2 ? p[2] / 255.0f : p[0] / 255.0f,
                colorArrayTypeSize > 3 ? p[3] / 255.0f : 1.0f
            };
        }
        if (colorArrayType == GL_FLOAT) {
            const float* f = reinterpret_cast<const float*>(p);
            return Col4{
                f[0],
                colorArrayTypeSize > 1 ? f[1] : f[0],
                colorArrayTypeSize > 2 ? f[2] : f[0],
                colorArrayTypeSize > 3 ? f[3] : 1.0f
            };
        }
        return currentColor;
    };
    auto fetchUV = [&](GLuint i) -> Vec2 {
        if (!textureArrayActive || !textureArrayPointer) return currentTextureUV;
        const uint8_t* p = elemPtr(textureArrayPointer, textureArrayStride,
                                   textureArrayTypeSize, textureArrayType, static_cast<int>(i));
        if (textureArrayType == GL_FLOAT) {
            const float* f = reinterpret_cast<const float*>(p);
            return Vec2{f[0], textureArrayTypeSize > 1 ? f[1] : 0.0};
        }
        return currentTextureUV;
    };
    auto makeVert = [&](GLuint i) -> Vertex {
        Vertex v{};
        v.pos = fetchPos(i);
        v.col = fetchCol(i);
        v.uv = fetchUV(i);
        return v;
    };
    auto drawTri = [&](GLuint a, GLuint b, GLuint c) {
        RenderTriangle(Triangle{makeVert(a), makeVert(b), makeVert(c)});
    };

    switch (mode) {
    case GL_TRIANGLES:
        for (int i = 0; i + 2 < count; i += 3) {
            drawTri(readIndex(i), readIndex(i + 1), readIndex(i + 2));
        }
        break;
    case GL_TRIANGLE_STRIP:
        for (int i = 0; i + 2 < count; ++i) {
            GLuint a = readIndex(i), b = readIndex(i + 1), c = readIndex(i + 2);
            if (i & 1) drawTri(b, a, c);
            else       drawTri(a, b, c);
        }
        break;
    case GL_TRIANGLE_FAN:
        if (count >= 3) {
            GLuint first = readIndex(0);
            for (int i = 1; i + 1 < count; ++i) {
                drawTri(first, readIndex(i), readIndex(i + 1));
            }
        }
        break;
    case GL_QUADS:
        for (int i = 0; i + 3 < count; i += 4) {
            drawTri(readIndex(i), readIndex(i + 1), readIndex(i + 2));
            drawTri(readIndex(i), readIndex(i + 2), readIndex(i + 3));
        }
        break;
    default:
        break;
    }
}

void Process_glGenTextures(GLsizei n, GLuint *textures) {
    if (n < 0) {
        errorState = GL_INVALID_VALUE;
        return;
    }
    // Texture name 0 is reserved (unbind).
    if (textureArray.empty()) {
        textureArray.push_back(TextureSlot{});
    }
    for (GLsizei i = 0; i < n; i++) {
        textureArray.push_back(TextureSlot{});
        auto& slot = textureArray.back();
        slot.texture2D.textureWrapS = GL_REPEAT;
        slot.texture2D.textureWrapT = GL_REPEAT;
        slot.texture2D.textureMinFilter = GL_NEAREST;
        slot.texture2D.textureMagFilter = GL_NEAREST;
        textures[i] = static_cast<GLuint>(textureArray.size() - 1);
    }
}

void Process_glBindTexture(GLenum target, GLuint texture) {
    if (texture == 0) {
        boundTexture2D = 0;
        lastAccessedTexture = nullptr;
        return;
    }
    if (texture >= textureArray.size()) {
        // Auto-create missing names (common with apps that invent IDs).
        textureArray.resize(texture + 1);
    }
    boundTexture2D = texture;
    lastAccessedTexture = &textureArray[texture];
    lastAccessedTexture->textureType = target;
}

void Process_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    currentColor = Col4{
        params[0],
        params[1],
        params[2],
        params[3],
    };
}

void Process_glEnable(GLenum cap) {
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
}

void Process_glDisable(GLenum cap) {
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
}

void Process_glMatrixMode(GLenum mode) {
    matrixMode = mode;
    switch(matrixMode) {
        case GL_PROJECTION:
            PrintInfo("GL_PROJECTION");
            lastAccessedMatrix = &projMatrices[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            PrintInfo("GL_MODELVIEW");
            lastAccessedMatrix = &modelMatrices[modelMatrixPtr];
            break;
        case GL_TEXTURE:
            PrintInfo("GL_TEXTURE");
            lastAccessedMatrix = &texMatrices[texMatrixPtr];
            break;
    }
}

void Process_glFrontFace(GLenum mode) {
    switch (mode) {
        case GL_CCW:
            counterClockWiseWindingActive = true;
            break;
        case GL_CW:
            counterClockWiseWindingActive = false;
            break;
    }
}

void Process_glFogiv(GLenum pname, const GLint *params) {
    switch(pname) {
    }
}

void Process_glFogi(GLenum pname, GLint param) {
    switch(pname) {
        case GL_FOG_MODE:
            fogMode = param;
            break;
    }
}

void Process_glFogfv(GLenum pname, const GLfloat *params) {
    switch(pname) {
        case GL_FOG_COLOR:
            fogColor = Col4{
                params[0], params[1], params[2], params[3]
            };
            break;
    }
}

void Process_glFogf(GLenum pname, GLfloat param) {
    switch (pname) {
        case GL_FOG_START:
            fogStart = param;
            break;
        case GL_FOG_END:
            fogEnd = param;
            break;        
        case GL_FOG_DENSITY:
            fogDensity = param;
            break;
    }
}

void Process_glLoadMatrixf(const GLfloat *m) {
    *lastAccessedMatrix = Mat4x4{
        Vec4{m[0],m[1],m[2],m[3]},
        Vec4{m[4],m[5],m[6],m[7]},
        Vec4{m[8],m[9],m[10],m[11]},
        Vec4{m[12],m[13],m[14],m[15]}
    };
}

void Process_glMultMatrixf(const GLfloat *m) {
    *lastAccessedMatrix = (*lastAccessedMatrix) * Mat4x4{
        Vec4{m[0],m[1],m[2],m[3]},
        Vec4{m[4],m[5],m[6],m[7]},
        Vec4{m[8],m[9],m[10],m[11]},
        Vec4{m[12],m[13],m[14],m[15]}
    };
}

void Process_glDepthFunc(GLenum func) {
    depthFunction = func;
}
