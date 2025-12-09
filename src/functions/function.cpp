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
                Vec3 screenPos = ProjectPosition(vertices[i].pos);
                RenderPixel(screenPos, vertices[i].col);
            }
            break;
        case GL_LINES:
            for (int i = 0; i + 1 < vertexIndex; i+=2) {
                Vec3 screenPosA = ProjectPosition(vertices[i].pos);
                Vec3 screenPosB = ProjectPosition(vertices[i+1].pos);
                RenderLine(screenPosA, vertices[i].col, screenPosB, vertices[i+1].col);
            }
            break;
        case GL_TRIANGLE_FAN:
            for (int i = 1; i + 1 < vertexIndex; i++) {
                Triangle screenTri = ProjectTriangle(
                    Triangle{
                        vertices[0], 
                        vertices[i], 
                        vertices[i+1],
                    }
                );
                RenderTriangle(screenTri);
            }
            break;
        case GL_TRIANGLES:
            for (int i = 0; i + 2 < vertexIndex; i+=3) {
                Triangle screenTri = ProjectTriangle(
                    Triangle{
                        vertices[i], 
                        vertices[i+1], 
                        vertices[i+2],
                    }
                );
                RenderTriangle(screenTri);
            }
            break;
        case GL_QUADS:
            for (int i = 0; i + 3 < vertexIndex; i+=4) {
                Triangle screenTriA = ProjectTriangle(
                    Triangle{
                        vertices[i], 
                        vertices[i+1], 
                        vertices[i+2],
                    }
                );
                Triangle screenTriB = ProjectTriangle(
                    Triangle{
                        vertices[i],
                        vertices[i+2], 
                        vertices[i+3],
                    }
                );
                RenderTriangle(screenTriA);
                RenderTriangle(screenTriB);
            } 
            break;
        case GL_QUAD_STRIP:
            for (int i = 0; i + 3 < vertexIndex; i+=2) {
                Triangle screenTriA = ProjectTriangle(
                    Triangle{
                        vertices[i],
                        vertices[i+1],
                        vertices[i+2],
                    }
                );
                Triangle screenTriB = ProjectTriangle(
                    Triangle{
                        vertices[i+1],
                        vertices[i+3],
                        vertices[i+2],
                    }
                );
                RenderTriangle(screenTriA);
                RenderTriangle(screenTriB);
            } 
            break;
    }
}

void Process_glLoadIdentity() {
    if (!lastAccessedMatrix) return;
    *lastAccessedMatrix = Mat4x4 {
        Vec4 { 1, 0 ,0,0 },
        Vec4 { 0, 1 ,0,0 },
        Vec4 { 0, 0 ,1,0 },
        Vec4 { 0, 0 ,0,1 }
    };
}

void Process_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    if (!lastAccessedMatrix) return;
    Mat4x4 T = {
        Vec4{1, 0, 0, 0},
        Vec4{0, 1, 0, 0},
        Vec4{0, 0, 1, 0},
        Vec4{double(x), double(y), double(z), 1}
    };

    *lastAccessedMatrix = (*lastAccessedMatrix) * T;
}

void Process_glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
    if (!lastAccessedMatrix) return;
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
    if (!lastAccessedMatrix) return;
    Mat4x4 S = {
        Vec4{double(x), 0, 0, 0},
        Vec4{0, double(y), 0, 0},
        Vec4{0, 0, double(z), 0},
        Vec4{0, 0, 0, 1}
    };

    *lastAccessedMatrix = (*lastAccessedMatrix) * S;
}

void Process_glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    if (!lastAccessedMatrix) return;

    *lastAccessedMatrix = Mat4x4{
        Vec4{ 2*n/(r-l), 0,            0,               0 },
        Vec4{ 0,         2*n/(t-b),    0,               0 },
        Vec4{ (r+l)/(r-l), (t+b)/(t-b), -(f+n)/(f-n),  -1 },
        Vec4{ 0,          0,           -2*f*n/(f-n),    0 }
    };
}

void Process_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    if (!lastAccessedMatrix) return;

    *lastAccessedMatrix = Mat4x4{
        Vec4{ 2.0/(r-l), 0,           0,           0 },
        Vec4{ 0,         2.0/(t-b),   0,           0 },
        Vec4{ 0,         0,         -2.0/(f-n),    0 },
        Vec4{ -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1 }
    };
}

void Process_glPushMatrix() {
    switch(matrixMode) {
        case GL_PROJECTION:
            projMatrices[projMatrixPtr + 1] = projMatrices[projMatrixPtr];
            projMatrixPtr++;
            lastAccessedMatrix = &projMatrices[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            modelMatrices[modelMatrixPtr + 1] = modelMatrices[modelMatrixPtr];
            modelMatrixPtr++;
            lastAccessedMatrix = &modelMatrices[modelMatrixPtr];
            break;
    }
}

void Process_glPopMatrix() {
    switch(matrixMode) {
        case GL_PROJECTION:
            projMatrixPtr--;
            lastAccessedMatrix = &projMatrices[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            modelMatrixPtr--;
            lastAccessedMatrix = &modelMatrices[modelMatrixPtr];
            break;
    }
}

void Process_glNewList(GLuint list, GLenum mode) {
    // List 0 is the global scope
    if (list == 0) {
        errorState = GL_INVALID_VALUE;
        return;
    }
    // We're already making a display list, we can't nest them!
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
    displayLists[0].commands.clear();
}

void Process_glEndList() {
    // List 0 is the global scope and can't be ended
    if (activeDisplayListIndex == 0) {
        displayLists[activeDisplayListIndex].commands.clear();
        errorState = GL_INVALID_OPERATION;
        return;
    }
    if (activeDisplayListIndex >= displayLists.size()) {
        displayLists.resize(activeDisplayListIndex + 1);
    }
    displayLists[activeDisplayListIndex] = displayLists[0];
    activeDisplayListIndex = 0;
}

void Process_glCallList(GLuint list) {
    if (list == 0 || list > displayLists.size()) {
        errorState = GL_INVALID_VALUE;
        return;
    }
    ExecuteDisplayList(displayLists[list]);
}

GLuint Process_glGenLists(GLsizei range) {
    if (range <= 0) return 0;
    GLuint firstID = displayLists.size(); // zero-based
    displayLists.resize(displayLists.size() + range);
    return firstID + 1; // return non-zero ID
}

GLboolean Process_IsList(GLuint list) {
    return (displayLists[list].commands.size() > 0);
}

void Process_glDeleteLists(GLuint list, GLsizei range) {
    if (list == 0 || list > displayLists.size()) return;

    if (list + range - 1 > displayLists.size())
        range = displayLists.size() - list + 1;

    for (GLuint i = 0; i < range; ++i) {
        displayLists[list - 1 + i].commands.clear();
    }
}

void Process_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (mode != GL_TRIANGLES) return;

    const uint8_t* BASE = (const uint8_t*)vertexArrayPointer;

    int stride    = vertexArrayStride;
    int posOff = (uint8_t*)vertexArrayPointer - BASE;
    int colOff = colorArrayPointer ? (uint8_t*)colorArrayPointer - BASE : -1;
    int uvOff  = textureArrayPointer ? (uint8_t*)textureArrayPointer - BASE : -1;


    auto fetchPos = [&](int i){
        const float* p = (const float*)(BASE + i * stride + posOff);
        return Vec3{p[0], p[1], p[2]};
    };
    auto fetchCol = [&](int i){
        const uint8_t* c = BASE + i * stride + colOff;
        return Col4{c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f};
    };
    auto fetchUV = [&](int i){
        if (uvOff < 0) return Vec2{0,0};
        const float* t = (const float*)(BASE + i * stride + uvOff);
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

        tri.a.uv  = fetchUV(a);
        tri.b.uv  = fetchUV(b);
        tri.c.uv  = fetchUV(c);

        RenderTriangle(ProjectTriangle(tri));
    };

    if (type == GL_UNSIGNED_SHORT) {
        const uint16_t* idx = (const uint16_t*)indices;
        for (int i = 0; i < count; i += 3)
            drawTri(idx[i], idx[i+1], idx[i+2]);
    } else {
        const uint32_t* idx = (const uint32_t*)indices;
        for (int i = 0; i < count; i += 3)
            drawTri(idx[i], idx[i+1], idx[i+2]);
    }
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
            textureType = GL_TEXTURE_2D;
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
            textureType = 0;
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
            projectionMode = GL_PROJECTION;
            lastAccessedMatrix = &projMatrices[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            PrintInfo("GL_MODELVIEW");
            lastAccessedMatrix = &modelMatrices[modelMatrixPtr];
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