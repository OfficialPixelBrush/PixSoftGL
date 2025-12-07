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
    vertexIndex++;
}

void Process_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    vertices[vertexIndex].pos = Vec3{x,y,z};
    vertices[vertexIndex].col = currentColor;
    
    /*vertices[vertexIndex].col = Col3 {
    SDL_randf(),SDL_randf(),SDL_randf()
    };*/
    
    vertexIndex++;
}

void Process_glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    currentColor = Col3{red,green,blue};
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
            for (int i = 0; i < vertexIndex; i+=2) {
                Vec3 screenPosA = ProjectPosition(vertices[i].pos);
                Vec3 screenPosB = ProjectPosition(vertices[i+2].pos);
                RenderLine(screenPosA, vertices[i].col, screenPosB, vertices[i+2].col);
            }
            break;
        case GL_TRIANGLE_FAN:
            for (int i = 1; i < vertexIndex; i+=2) {
                Triangle screenTri = ProjectTriangle(
                    Triangle{
                        vertices[i+1],
                        vertices[i], 
                        vertices[0], 
                    }
                );
                RenderTriangle(screenTri);
            }
            break;
        case GL_TRIANGLES:
            for (int i = 0; i < vertexIndex; i+=3) {
                Triangle screenTri = ProjectTriangle(
                    Triangle{
                        vertices[i+2],
                        vertices[i+1], 
                        vertices[i], 
                    }
                );
                RenderTriangle(screenTri);
            }
            break;
        case GL_QUADS:
            for (int i = 0; i < vertexIndex; i+=4) {
                Triangle screenTriA = ProjectTriangle(
                    Triangle{
                        vertices[i+2],
                        vertices[i+1], 
                        vertices[i], 
                    }
                );
                Triangle screenTriB = ProjectTriangle(
                    Triangle{
                        vertices[i+3],
                        vertices[i+2], 
                        vertices[i],
                    }
                );
                RenderTriangle(screenTriA);
                RenderTriangle(screenTriB);
            } 
            break;
        case GL_QUAD_STRIP:
            for (int i = 2; i + 1 < vertexIndex; i+=2) {
                Triangle screenTriA = ProjectTriangle(
                    Triangle{
                        vertices[i],
                        vertices[i-1], 
                        vertices[i-2], 
                    }
                );
                Triangle screenTriB = ProjectTriangle(
                    Triangle{
                        vertices[i-2],
                        vertices[i+1], 
                        vertices[i],
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
        Vec4{ (2*n)/(r-l), 0, 0, 0 },
        Vec4{ 0, (2*n)/(t-b), 0, 0 },
        Vec4{ (r+l)/(r-l), (t+b)/(t-b), -(f+n)/(f-n), -1 },
        Vec4{ 0, 0, -(2*f*n)/(f-n), 0 }
    };
}

void Process_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    if (!lastAccessedMatrix) return;
    *lastAccessedMatrix = Mat4x4{
        Vec4{ 2/(r-l),0,0,0},
        Vec4{0,2/(t-b),0,0},
        Vec4{0,0,-(2/(f-n)),0},
        Vec4{-((r+l)/(r-l)), -((t+b)/(t-b)), -((f+n)/(f-n)), 1}
    };
}

void Process_glPushMatrix() {
    switch(drawingMode) {
        case GL_PROJECTION:
            projMatricies[projMatrixPtr + 1] = projMatricies[projMatrixPtr];
            projMatrixPtr++;
            lastAccessedMatrix = &projMatricies[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            modelMatricies[modelMatrixPtr + 1] = modelMatricies[modelMatrixPtr];
            modelMatrixPtr++;
            lastAccessedMatrix = &modelMatricies[modelMatrixPtr];
            break;
    }
}

void Process_glPopMatrix() {
    switch(drawingMode) {
        case GL_PROJECTION:
            projMatrixPtr--;
            lastAccessedMatrix = &projMatricies[projMatrixPtr];
            break;
        case GL_MODELVIEW:
            modelMatrixPtr--;
            lastAccessedMatrix = &modelMatricies[modelMatrixPtr];
            break;
    }
}

void Process_glNewList(GLuint list, GLenum mode) {
    // List 0 is the global scope
    if (list == 0)
        errorState = GL_INVALID_VALUE;
    // We're already making a display list, we can't nest them!
    if (activeDisplayListIndex != 0)
        errorState = GL_INVALID_OPERATION;
    compileAndExecute = (mode == GL_COMPILE_AND_EXECUTE);
    if (compileAndExecute)
        PrintInfo("COMPILE_AND_EXECUTE");
    else
        PrintInfo("COMPILE");
    activeDisplayListIndex = list;
}

void Process_glEndList() {
    // List 0 is the global scope and can't be ended
    if (activeDisplayListIndex == 0)
        errorState = GL_INVALID_OPERATION;
    displayLists[activeDisplayListIndex] = displayLists[0];
    displayLists[activeDisplayListIndex].numberOfCommands = 0;
}

void Process_glCallList(GLuint list) {
    // List 0 is the global scope and can't be called
    if (list == 0)
        errorState = GL_INVALID_VALUE;
    ExecuteDisplayList(displayLists[list]);
}

GLuint Process_glGenLists(GLsizei range) {
    if (range == 0) return 0;
    int numberOfEmptyDisplayLists = 0;
    for (int i = 1; i < MAX_DISPLAY_LIST_ENTRIES; i++) {
        // Look for range # of empty display lists
        if (displayLists[i].numberOfCommands != -1) {
            numberOfEmptyDisplayLists = 0;
        } else {
            numberOfEmptyDisplayLists++;
        }

        // If we find a suitable number of empty Display Lists,
        // return the first empty index
        if (numberOfEmptyDisplayLists == range) {
            return i-numberOfEmptyDisplayLists+1;
        }
    }
    return 0;
}

GLboolean Process_IsList(GLuint list) {
    return (displayLists[list].numberOfCommands > -1);
}

void Process_glDeleteLists(GLuint list, GLsizei range) {
    for (int i = list; i < list+range; i++) {
        displayLists[i].numberOfCommands = -1;
    }
}

void Process_glBindTexture(GLenum target, GLuint texture) {

}