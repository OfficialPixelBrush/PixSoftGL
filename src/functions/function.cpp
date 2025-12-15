#include "function.h"
#include "../render.h"
#include "../sdl.h"
#include "../global.h"
#include "../maths.h"
#include <GL/gl.h>
#include <SDL3/SDL_stdinc.h>
#include <cstdlib>

void Process_glVertex2f(GLfloat x, GLfloat y) {
    vertices[vertexIndex].pos = Vec3{x,y,0};
    vertices[vertexIndex].col = currentColor;
    vertices[vertexIndex].uv = currentTextureUV;
    vertexIndex++;
}

void Process_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    vertices[vertexIndex].pos = Vec3{x,y,z};
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
    viewportOffsetX = x;
    viewportOffsetY = y;
    viewportAreaWidth = width;
    viewportAreaHeight = height;
    viewportAreaTotal = viewportAreaWidth * viewportAreaHeight;
    
    // Create buffers
    if (!frameBufferColor) {
        renderAreaWidth = viewportAreaWidth;
        renderAreaHeight = viewportAreaHeight;
        renderAreaTotal = viewportAreaTotal;
        frameBufferColor = (PixelValue*)malloc( renderAreaTotal * sizeof(PixelValue));
    }
    if (!frameBufferDepth) {
        frameBufferDepth = (float*)malloc(renderAreaTotal * sizeof(float));
    }

    ReCreateWindow();
    ClearFramebuffers(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Process_glBegin(GLenum mode) {
    drawingMode = mode;
    vertexIndex = 0;
    switch(drawingMode) {
        case GL_POINTS:
            PrintInfo("GL_POINTS ");
            break;
        case GL_LINES:
            PrintInfo("GL_LINES ");
            break;
        case GL_LINE_LOOP:
            PrintInfo("GL_LINE_LOOP ");
            break;
        case GL_LINE_STRIP:
            PrintInfo("GL_LINE_STRIP ");
            break;
        case GL_TRIANGLES:
            PrintInfo("GL_TRIANGLES ");
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
            if (projMatrixPtr-1 < 0) 
                errorState = GL_STACK_UNDERFLOW; return;
            projMatrixPtr--;
            lastAccessedMatrix = &projMatrices[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            if (modelMatrixPtr-1 < 0) 
                errorState = GL_STACK_UNDERFLOW; return;
            modelMatrixPtr--;
            lastAccessedMatrix = &modelMatrices[modelMatrixPtr];
            break;
        case GL_TEXTURE:
            if (texMatrixPtr-1 < 0) 
                errorState = GL_STACK_UNDERFLOW; return;
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
    if (list == 0 || list > displayLists.size() || displayLists[list - 1].commands.empty()) {
        errorState = GL_INVALID_VALUE;
        return;
    }
    ExecuteDisplayList(displayLists[list - 1]);
}

GLuint Process_glGenLists(GLsizei range) {
    if (range <= 0) return 0;
    GLuint firstID = displayLists.size();
    displayLists.resize(displayLists.size() + range);
    return firstID + 1;
}

GLboolean Process_IsList(GLuint list) {
    if (list == 0 || list > displayLists.size()) return false;
    return displayLists[list - 1].commands.empty();
}

void Process_glDeleteLists(GLuint list, GLsizei range) {
    if (list == 0 || list > displayLists.size()) return;

    if (list + range - 1 > displayLists.size())
        range = displayLists.size() - list + 1;

    for (GLuint i = 0; i < range; ++i)
        displayLists[list - 1 + i].commands.clear();
}

void Process_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if ((mode != GL_TRIANGLES && mode != GL_QUADS))
        return;

    // Each array needs its OWN stride!
    auto fetchPos = [&](int i) -> Vec3 {
        if (!vertexArrayPointer) return Vec3{0,0,0};
        const uint8_t* base;
        if (vertexArrayStride > 0) 
            base = (const uint8_t*)vertexArrayPointer + (i*vertexArrayStride);
        else
            base = (const uint8_t*)vertexArrayPointer + (i * (sizeof(GLfloat)*vertexArrayTypeSize));
        const float* p = (const float*)base;
        return Vec3{p[0], p[1], p[2]};
    };

    auto fetchCol = [&](int i) -> Col4 {
        if (!colorArrayPointer) return Col4{1,1,1,1};
        const uint8_t* base;
        if (colorArrayStride > 0) 
            base = (const uint8_t*)colorArrayPointer + (i*colorArrayStride);
        else
            base = (const uint8_t*)colorArrayPointer + (i * (sizeof(GLubyte)*colorArrayTypeSize));
        const uint8_t* c = base;
        return Col4{c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f};
    };

    auto fetchUV = [&](int i) -> Vec2 {
        if (!textureArrayPointer) return Vec2{0,0};
        const uint8_t* base;
        if (textureArrayStride > 0) 
            base = (const uint8_t*)textureArrayPointer + (i*textureArrayStride);
        else
            base = (const uint8_t*)textureArrayPointer + (i * (sizeof(GLfloat)*textureArrayTypeSize));
        const float* t = (const float*)base;
        return Vec2{t[0], t[1]};
    };

    auto drawTri = [&](int a, int b, int c){
        Triangle tri;
        tri.a.pos = fetchPos(a);
        tri.b.pos = fetchPos(b);
        tri.c.pos = fetchPos(c);

        tri.a.col = fetchCol(a);
        tri.b.col = fetchCol(b);
        tri.c.col = fetchCol(c);

        tri.a.uv = fetchUV(a);
        tri.b.uv = fetchUV(b);
        tri.c.uv = fetchUV(c);

        RenderTriangle(tri);
    };

    if (mode == GL_QUADS) {
        for (int i = 0; i < count; i += 4) {
            drawTri(first + i, first + i + 1, first + i + 2);
            drawTri(first + i, first + i + 2, first + i + 3);
        }
    } else { // GL_TRIANGLES
        for (int i = 0; i < count; i += 3) {
            drawTri(first + i, first + i + 1, first + i + 2);
        }
    }
}

void Process_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (!vertexArrayPointer) {
        errorState = GL_INVALID_OPERATION;
        return;
    }

    if (count < 0) {
        errorState = GL_INVALID_VALUE;
        return;
    }

    if (!indices)
        return;

    const uint8_t* indicesUByte = nullptr;
    const uint16_t* indicesUShort = nullptr;
    const uint32_t* indicesUInt = nullptr;

    switch (type) {
        case GL_UNSIGNED_BYTE:    indicesUByte = (const uint8_t*)indices; break;
        case GL_UNSIGNED_SHORT:   indicesUShort = (const uint16_t*)indices; break;
        case GL_UNSIGNED_INT:     indicesUInt = (const uint32_t*)indices; break;
        default:
            errorState = GL_INVALID_ENUM;
            return;
    }

    auto fetchPos = [&](int i) -> Vec3 {
        if (!vertexArrayPointer) return Vec3{0,0,0};
        const uint8_t* base = (const uint8_t*)vertexArrayPointer + i * (vertexArrayStride > 0 ? vertexArrayStride : sizeof(GLfloat)*3);
        const GLfloat* p = (const GLfloat*)base;
        return Vec3{p[0], p[1], p[2]};
    };

    auto fetchCol = [&](int i) -> Col4 {
        if (!colorArrayPointer) return Col4{1,1,1,1};
        const uint8_t* base = (const uint8_t*)colorArrayPointer + i * (colorArrayStride > 0 ? colorArrayStride : sizeof(GLubyte)*4);
        const GLubyte* c = (const GLubyte*)base;
        return Col4{ c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f };
    };

    auto fetchUV = [&](int i) -> Vec2 {
        if (!textureArrayPointer) return Vec2{0,0};
        const uint8_t* base = (const uint8_t*)textureArrayPointer + i * (textureArrayStride > 0 ? textureArrayStride : sizeof(GLfloat)*2);
        const GLfloat* t = (const GLfloat*)base;
        return Vec2{t[0], t[1]};
    };

    Process_glBegin(mode);
    for (int i = 0; i < count; i++) {
        int index = indicesUByte ? indicesUByte[i] :
            (indicesUShort ? indicesUShort[i] : indicesUInt[i]);
        if (colorArrayPointer) {
            Col4 col = fetchCol(index);
            Process_glColor4f(col.r, col.g, col.b, col.a);
        }
        if (textureArrayPointer) {
            Vec2 uv = fetchUV(index);
            Process_glTexCoord2f(uv.x, uv.y);
        }
        if (vertexArrayPointer) {
            Vec3 pos = fetchPos(index);
            Process_glVertex3f(pos.x, pos.y, pos.z);
        }
    }
    Process_glEnd();
}

void Process_glGenTextures(GLsizei n, GLuint *textures) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        textureArray.push_back(TextureSlot{});
        textures[i] = textureArray.size() - 1;
    }
}

void Process_glBindTexture(GLenum target, GLuint texture) {
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
