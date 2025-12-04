#include <GL/gl.h>
#include <SDL3/SDL_init.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <string>

#include "defines.h"
#include "datatypes.h"

bool printInfo = false;
bool forwardToSystemGl = true;

void PrintInfo(int s) {
    if (!printInfo) return;
    std::cout << s;
}

void PrintInfo(const std::string& s) {
    if (!printInfo) return;
    std::cout << s;
}

// The current vertex index
int vertexIndex = 0;

// If depth should be tested
bool depthTestActive = false;
bool fogActive = false;
bool colorMaterialActive = false;
bool texture2dActive = false;
bool blendActive = false;
bool lightingActive = false;
bool cullFaceActive = false;
bool normalizeActive = false;

// Lights
bool lightActive[MAX_LIGHTS];
Light lights[MAX_LIGHTS];

// Fog variables
int fogMode = 0;
Col4 fogColor = Col4{0,0,0,0};
float fogStart = 0.0;
float fogEnd = 0.0;

// The current object rendering mode
GLenum drawingMode = GL_POINTS;

// The current projection mode
GLenum projectionMode = 0;

// The current matrix mode
GLenum matrixMode = GL_MODELVIEW;

// Currently active color
Col3 currentColor = Col3{1,1,1};
Col3 clearColor = Col3{0,0,0};

// Matricies
Mat4x4* lastAccessedMatrix = nullptr;
int projMatrixPtr = 0;
int modelMatrixPtr = 0;
int texMatrixPtr = 0;
Mat4x4 projMatricies[MAX_PROJECTION_MATRICIES];
Mat4x4 modelMatricies[MAX_MODEL_MATRICIES];
Mat4x4 texMatricies[MAX_TEXTURE_MATRICIES];

// Vertex buffer
Vertex vertices[MAX_VERTICES];

// Viewport size
int renderAreaWidth = DEFAULT_RENDER_AREA_WIDTH;
int renderAreaHeight = DEFAULT_RENDER_AREA_HEIGHT;
int renderAreaTotal = DEFAULT_RENDER_AREA_WIDTH * DEFAULT_RENDER_AREA_HEIGHT;

// Screen framebuffer
PixelValue* frameBufferColor;
float* frameBufferDepth;

// SDL Stuff
SDL_Window *win;
SDL_Surface *surf;
bool running = true;

// Write a mapped pixel value into a (locked) surface at x,y.
// surface must be valid and locked if SDL_MUSTLOCK(surface) is true.
static void put_pixel_locked(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    if (!surface) return;
    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h) return;

    auto details = SDL_GetPixelFormatDetails(surface->format);
    int bpp = details->bytes_per_pixel;
    uint8_t *row = (uint8_t*)surface->pixels + y * surface->pitch;
    uint8_t *p = row + x * bpp;

    switch (bpp) {
        case 1:
            *p = (uint8_t)pixel;
            break;
        case 2:
            *(uint16_t*)p = (uint16_t)pixel;
            break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                p[0] = (pixel >> 16) & 0xFF;
                p[1] = (pixel >> 8) & 0xFF;
                p[2] = pixel & 0xFF;
            } else {
                p[0] = pixel & 0xFF;
                p[1] = (pixel >> 8) & 0xFF;
                p[2] = (pixel >> 16) & 0xFF;
            }
            break;
        case 4:
            *(uint32_t*)p = pixel;
            break;
    }
}

// Safe wrapper: maps RGBA to surface format, locks/unlocks if needed, then writes.
void put_pixel(SDL_Surface *surface, int x, int y,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!surface) return;

    // Map color for this surface format
    Uint32 mapped = SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format), nullptr, r, g, b, a);

    bool locked = false;
    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) != 0) return; // failed to lock
        locked = true;
    }

    put_pixel_locked(surface, x, y, mapped);

    if (locked) SDL_UnlockSurface(surface);
}

// Interpolate two colors linearly
Col3 lerp(Col3 a, Col3 b, float t) {
    return Col3{
        a.r + t * (b.r - a.r),
        a.g + t * (b.g - a.g),
        a.b + t * (b.b - a.b)
    };
}

// Interpolate two floats linearly
float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Draw a Pixel to the terminal
void DrawPixel(PixelValue p, int x, int y) {
    //PrintInfo("\e[38;2;" << int(p.r) << ");" << int(p.g) << ");" << int(p.b) << "m" << "█");
    put_pixel(surf, x, y, p.r,p.g,p.b,255);
}

// Draw the framebuffer colors to the SDL Window
void UpdateScreen() {
    if (!frameBufferColor) return;
    //PrintInfo("\033[H");
    for (int y = 0; y < renderAreaHeight; y++) {
        for (int x = 0; x < renderAreaWidth; x++) {
            DrawPixel(frameBufferColor[x + y * renderAreaWidth], x, y);
        }
        //PrintInfo("\n");;
    }
    // Poll to make not crash
    SDL_Event e;
    while (SDL_PollEvent(&e)) {  // poll all events
        if (e.type == SDL_EVENT_QUIT) { // window X pressed
            SDL_Quit();
            exit(0);                 // exit your program
        }
    }
    SDL_UpdateWindowSurface(win);
}

// Convert a Vec3 to a Mat4x4
Mat4x4 Vec3ToMat4x4(Vec3 pos) {
    return Mat4x4 {
        Vec4 { 1, 0 ,0,pos.x },
        Vec4 { 0, 1 ,0,pos.y },
        Vec4 { 0, 0 ,1,pos.z },
        Vec4 { 0, 0 ,0,1 }
    };
}

// Project world-space position to screen, return eye-space distance in z
Vec3 ProjectPosition(Vec3 pos) {
    switch(projectionMode) {
        case GL_PROJECTION: {
            // transform to eye (modelview) space first
            Vec4 eye = modelMatricies[modelMatrixPtr] * Vec4{pos.x, pos.y, pos.z, 1.0};
            // eye.z is negative in front of the camera in typical OpenGL; use -eye.z as positive distance
            float eyeDist = float(-eye.z);

            // then project
            Vec4 clip = projMatricies[projMatrixPtr] * eye;
            Vec3 ndc = { clip.x / clip.w, clip.y / clip.w, clip.z / clip.w };

            return Vec3{
                (ndc.x + 1.0f) * 0.5f * renderAreaWidth,
                (1.0f - (ndc.y + 1.0f) * 0.5f) * renderAreaHeight,
                eyeDist           // store eye-space distance for fog calculations
            };
        }
    }
    return Vec3{0,0,0};
}

// Project triangle to screen
Triangle ProjectTriangle(Triangle tri) {
    return Triangle{
        Vertex { ProjectPosition(tri.a.pos), tri.a.col },
        Vertex { ProjectPosition(tri.b.pos), tri.b.col },
        Vertex { ProjectPosition(tri.c.pos), tri.c.col },
    };
}

PixelValue Col3ToPixelValue(Col3 color) {
    color.r = std::fmax(0.0f, std::fmin(1.0f, color.r));
    color.g = std::fmax(0.0f, std::fmin(1.0f, color.g));
    color.b = std::fmax(0.0f, std::fmin(1.0f, color.b));
    return PixelValue{
        (unsigned char)(color.r*255),
        (unsigned char)(color.g*255),
        (unsigned char)(color.b*255)
    };
}

// Render Pixel to framebuffer
void RenderPixel(Vec3 screenPos, Col3 color) {
    // NDC is from -1 to 1, which we'll map to 0 - renderAreaWidth
    int index = int(screenPos.x) + (int(screenPos.y) * renderAreaWidth);
    if (index < 0 || index >= renderAreaTotal) return;

    // If the new pixel is behind the old one, skip
    if (depthTestActive && frameBufferDepth && screenPos.z >= frameBufferDepth[index]) {
        return;
    }

    if (fogActive && screenPos.z > fogStart) {
        Col3 fogCol = Col3{fogColor.r, fogColor.g, fogColor.b};
        switch(fogMode) {
            case GL_LINEAR:
                float fogFactor = (screenPos.z - fogStart) / (fogEnd - fogStart);
                fogFactor = std::clamp(fogFactor, 0.0f, 1.0f);

                color = Col3{
                    color.r * (1.0f - fogFactor) + fogColor.r * fogFactor,
                    color.g * (1.0f - fogFactor) + fogColor.g * fogFactor,
                    color.b * (1.0f - fogFactor) + fogColor.b * fogFactor
                };
                break;
        }
    }

    // Write new values
    if (!frameBufferColor) return;
    frameBufferColor[index] = Col3ToPixelValue(color);
    frameBufferDepth[index] = screenPos.z;

    PixelValue p = Col3ToPixelValue(color);
    DrawPixel(p,int(screenPos.x),int(screenPos.y));
}

// Render Line to Framebuffer
void RenderLine(Vec3 posA, Col3 colA, Vec3 posB, Col3 colB) {
    float x0 = posA.x, y0 = posA.y;
    float x1 = posB.x, y1 = posB.y;
    Col3 c0 = colA, c1 = colB;

    bool steep = fabs(y1 - y0) > fabs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }

    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(c0, c1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0 ? 0 : dy / dx;

    for (float x = x0; x <= x1; x += 1.0f) {
        float t = dx == 0 ? 0.0f : (x - x0) / dx;
        float y = y0 + gradient * (x - x0);
        Vec3 screenPos = steep ? Vec3{y, x, lerp(posA.z, posB.z, t)}
                               : Vec3{x, y, lerp(posA.z, posB.z, t)};
        Col3 color = lerp(c0, c1, t);
        RenderPixel(screenPos, color);
    }
}

// Normalize vector
Vec3 Normalize(Vec3 v) {
    float mag = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (mag == 0.0f) return Vec3{0,0,0};
    return Vec3{v.x/mag, v.y/mag, v.z/mag};
};

// 2D Dot Product
float Dot2D(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y;
}

// Rotate vector 90°
Vec3 Perpendicular2D(Vec3 vec) {
    return Vec3{-vec.y, vec.x, 0};
}

// Check if point is on right side of vector
bool PointOnRightSideOfLine(Vec3 a, Vec3 b, Vec3 p) {
    Vec3 ap = p - a;
    Vec3 ab = b - a;
    Vec3 abPerp = Perpendicular2D(ab);
    return Dot2D(ap, abPerp) >= 0.0;
}

// Check if point is inside of triangle
bool PointInTriangle(Triangle tri, Vec3 p) {
    bool sideAB = PointOnRightSideOfLine(tri.a.pos, tri.b.pos, p);
    bool sideBC = PointOnRightSideOfLine(tri.b.pos, tri.c.pos, p);
    bool sideCA = PointOnRightSideOfLine(tri.c.pos, tri.a.pos, p);
    return sideAB == sideBC && sideBC == sideCA;
}

// Simple barycentric color interpolation
Col3 BarycentricColor(Triangle tri, Vec3& p) {
    Vec3 a = tri.a.pos;
    Vec3 b = tri.b.pos;
    Vec3 c = tri.c.pos;

    float det = (b.y - c.y)*(a.x - c.x) + (c.x - b.x)*(a.y - c.y);
    float w1 = ((b.y - c.y)*(p.x - c.x) + (c.x - b.x)*(p.y - c.y)) / det;
    float w2 = ((c.y - a.y)*(p.x - c.x) + (a.x - c.x)*(p.y - c.y)) / det;
    float w3 = 1.0f - w1 - w2;
    w1 = std::clamp(w1, 0.0f, 1.0f);
    w2 = std::clamp(w2, 0.0f, 1.0f);
    w3 = std::clamp(w3, 0.0f, 1.0f);

    // Calulate depth too
    p.z = w1*tri.a.pos.z + w2*tri.b.pos.z + w3*tri.c.pos.z;

    return Col3{
        w1*tri.a.col.r + w2*tri.b.col.r + w3*tri.c.col.r,
        w1*tri.a.col.g + w2*tri.b.col.g + w3*tri.c.col.g,
        w1*tri.a.col.b + w2*tri.b.col.b + w3*tri.c.col.b
    };
}

// Render triangle to framebuffer
void RenderTriangle(Triangle tri) {
    for (int y = 0; y < renderAreaHeight; y++) {
        for (int x = 0; x < renderAreaWidth; x++) {
            Vec3 point = Vec3{float(x)+0.5, float(y)+0.5, 0.0f};
            if (PointInTriangle(tri, point)) {
                Col3 color = BarycentricColor(tri, point);
                RenderPixel(point, color);
            }
        }
    }
}

// Actual OpenGL 1.1 Library functions!
extern "C" {
    // Add float vertex (2)
    void glVertex2f(GLfloat x, GLfloat y) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glVertex2f");
            }
            real_gl(x,y);
        }
        vertices[vertexIndex].pos = Vec3{x,y,0};
        vertices[vertexIndex].col = currentColor;
        vertexIndex++;
    }

    // Add float vertex (3)
    void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glVertex3f");
            }
            real_gl(x,y,z);
        }
        vertices[vertexIndex].pos = Vec3{x,y,z};
        vertices[vertexIndex].col = currentColor;
        vertexIndex++;
    }

    // Adjust OpenGL Viewport
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLint,GLsizei,GLsizei) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLint,GLsizei,GLsizei)) dlsym(RTLD_NEXT, "glViewport");
            }
            real_gl(x,y,width,height);
        }
        renderAreaWidth = width;
        renderAreaHeight = height;
        renderAreaTotal = renderAreaWidth * renderAreaHeight;
        free(frameBufferColor);
        free(frameBufferDepth);
        // Resize buffers
        frameBufferColor = (PixelValue*)malloc( renderAreaTotal * sizeof(PixelValue));
        frameBufferDepth = (float*)malloc(renderAreaTotal * sizeof(float));

        if (!win || !surf) {
            SDL_Init(SDL_INIT_VIDEO);
            win = SDL_CreateWindow("PixSoftGL", renderAreaWidth, renderAreaHeight, 0);
            surf = SDL_GetWindowSurface(win); // get the window surface
        }
        
        //SDL_DestroyWindow(win);
        //SDL_Quit();
    }

    // Set float color
    void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glClearColor");
            }
            real_gl(red,green,blue,alpha);
        }
        clearColor = Col3{red,green,blue};
    }

    // Set float color
    void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glColor3f");
            }
            real_gl(red,green,blue);
        }
        currentColor = Col3{red,green,blue};
    }

    // Return OpenGL info
    const GLubyte* glGetString(GLenum name) {
        if (forwardToSystemGl) {
            static const GLubyte* (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (const GLubyte* (*)(GLenum)) dlsym(RTLD_NEXT, "glGetString");
            }
            real_gl(name);
        }
        switch(name) {
            case GL_VERSION:
                return (const GLubyte*)PIXSOFTGL_VERSION;
            case GL_VENDOR:
                return (const GLubyte*)PIXSOFTGL_VENDOR;
            case GL_RENDERER:
                return (const GLubyte*)PIXSOFTGL_RENDERER;
            case GL_EXTENSIONS:
                return nullptr;
        }
        return nullptr;
    }

    // Light Position
    void glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum,GLenum,const GLfloat*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum,GLenum,const GLfloat*)) dlsym(RTLD_NEXT, "glLightfv");
            }
            real_gl(light,pname,params);
        }
        PrintInfo("glLightfv");
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
        static GLuint (*real_gl)(GLsizei) = NULL;
        if (!real_gl) {
            real_gl = (GLuint (*)(GLsizei)) dlsym(RTLD_NEXT, "glGenLists");
        }
        return real_gl(range);
        //return 0;
    }

    void glNewList(GLuint list, GLenum mode) {
        static void (*real_gl)(GLuint,GLenum) = NULL;
        if (!real_gl) {
            real_gl = (void (*)(GLuint,GLenum)) dlsym(RTLD_NEXT, "glNewList");
        }
        real_gl(list,mode);
        PrintInfo("glNewList");
        PrintInfo("\n");;
    }

    void glEndList() {
        static void (*real_gl)() = NULL;
        if (!real_gl) {
            real_gl = (void (*)()) dlsym(RTLD_NEXT, "glEndList");
        }
        real_gl();
        PrintInfo("glEndList");
        PrintInfo("\n");;
    }

    // Clear framebuffer(s)
    void glClear(GLbitfield mask) {
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
        PrintInfo("glClear ");
        // Clear color
        if ((mask & GL_COLOR_BUFFER_BIT) && frameBufferColor) {
            PrintInfo("GL_COLOR_BUFFER_BIT ");
            for (int i = 0; i < renderAreaTotal; i++) {
                frameBufferColor[i] = Col3ToPixelValue(clearColor);
            }
        }
        // Clear depth
        if ((mask & GL_DEPTH_BUFFER_BIT) && frameBufferDepth) {
            PrintInfo("GL_DEPTH_BUFFER_BIT ");
            for (int i = 0; i < renderAreaTotal; i++) {
                frameBufferDepth[i] = INFINITY;
            }
        }
        PrintInfo("\n");;
    }

    // Set Matrix mode
    void glMatrixMode(GLenum mode) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glMatrixMode");
            }
            real_gl(mode);
        }

        // Prepare next matrix Mode
        PrintInfo("glMatrixMode ");
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
        PrintInfo("\n");;
    }

    // Enable property
    void glEnable(GLenum cap) {
            if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glEnable");
            }
            real_gl(cap);
        }

        PrintInfo("glEnable ");
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
            default:
                std::cout << std::hex;
                PrintInfo(cap);
                std::cout << std::dec;
                break;
        }
        PrintInfo("\n");;
    }

    // Disable property
    void glDisable(GLenum cap) {
            if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glDisable");
            }
            real_gl(cap);
        }
        
        PrintInfo("glDisable ");
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
            default:
                std::cout << std::hex;
                PrintInfo(cap);
                std::cout << std::dec;
                break;
        }
        PrintInfo("\n");;
    }

    // Load new data
    void glBegin(GLenum mode) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLenum) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLenum)) dlsym(RTLD_NEXT, "glBegin");
            }
            real_gl(mode);
        }

        drawingMode = mode;
        vertexIndex = 0;
        PrintInfo("glBegin ");
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
        PrintInfo("\n");;
    }

    // We're done, now draw whatever we sent to the fb
    void glEnd() {
            if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glEnd");
            }
            real_gl();
        }

        PrintInfo("glEnd ");
        switch(drawingMode) {
            case GL_POINTS:
                for (int i = 0; i < vertexIndex; i++) {
                    Vec3 screenPos = ProjectPosition(vertices[i].pos);
                    RenderPixel(screenPos, vertices[i].col);
                }
                break;
            case GL_TRIANGLE_FAN:
                for (int i = 1; i < vertexIndex; i+=2) {
                    Triangle screenTri = ProjectTriangle(
                        Triangle{
                            vertices[0], 
                            vertices[i], 
                            vertices[i+1]
                        }
                    );
                    RenderTriangle(screenTri);
                }
                break;
            case GL_TRIANGLES:
                for (int i = 0; i < vertexIndex; i+=3) {
                    Triangle screenTri = ProjectTriangle(
                        Triangle{
                            vertices[i], 
                            vertices[i+1], 
                            vertices[i+2]
                        }
                    );
                    RenderTriangle(screenTri);
                }
                break;
            case GL_QUADS:
                for (int i = 0; i < vertexIndex; i+=4) {
                    Triangle screenTriA = ProjectTriangle(
                        Triangle{
                            vertices[i], 
                            vertices[i+1], 
                            vertices[i+2]
                        }
                    );
                    Triangle screenTriB = ProjectTriangle(
                        Triangle{
                            vertices[i],
                            vertices[i+2], 
                            vertices[i+3]
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
                            vertices[i-2], 
                            vertices[i-1], 
                            vertices[i]
                        }
                    );
                    Triangle screenTriB = ProjectTriangle(
                        Triangle{
                            vertices[i],
                            vertices[i+1], 
                            vertices[i-2]
                        }
                    );
                    RenderTriangle(screenTriA);
                    RenderTriangle(screenTriB);
                } 
                break;
        }
        PrintInfo("\n");;
    }

    // Load identity matrix
    void glLoadIdentity() {
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glLoadIdentity");
            }
            real_gl();
        }
        PrintInfo("glLoadIdentity ");
        *lastAccessedMatrix = Mat4x4 {
            Vec4 { 1, 0 ,0,0 },
            Vec4 { 0, 1 ,0,0 },
            Vec4 { 0, 0 ,1,0 },
            Vec4 { 0, 0 ,0,1 }
        };
        PrintInfo("\n");;
    }

    // Float translate
    void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glTranslatef");
            }
            real_gl(x,y,z);
        }
        Mat4x4 T = {
            Vec4{1, 0, 0, 0},
            Vec4{0, 1, 0, 0},
            Vec4{0, 0, 1, 0},
            Vec4{double(x), double(y), double(z), 1}  // last column is translation
        };

        *lastAccessedMatrix = (*lastAccessedMatrix) * T; // multiply, not add
    }

    // Float Rotate
    void glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glRotatef");
            }
            real_gl(angleDeg,x,y,z);
        }
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

        *lastAccessedMatrix = (*lastAccessedMatrix) * R; // multiply, not add
    }

    // Float scale
    void glScalef(GLfloat x, GLfloat y, GLfloat z) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLfloat,GLfloat,GLfloat) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLfloat,GLfloat,GLfloat)) dlsym(RTLD_NEXT, "glScalef");
            }
            real_gl(x,y,z);
        }
        Mat4x4 S = {
            Vec4{double(x), 0, 0, 0},
            Vec4{0, double(y), 0, 0},
            Vec4{0, 0, double(z), 0},
            Vec4{0, 0, 0, 1}
        };

        *lastAccessedMatrix = (*lastAccessedMatrix) * S; // multiply, not add
    }

    // Perspective Projection Matrix Creation
    void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble)) dlsym(RTLD_NEXT, "glFrustum");
            }
            real_gl(l,r,b,t,n,f);
        }
        *lastAccessedMatrix = Mat4x4{
            Vec4{ (2*n)/(r-l), 0, 0, 0 },
            Vec4{ 0, (2*n)/(t-b), 0, 0 },
            Vec4{ (r+l)/(r-l), (t+b)/(t-b), -(f+n)/(f-n), -1 },
            Vec4{ 0, 0, -(2*f*n)/(f-n), 0 }
        };
    }

    // Orthographic Projection Matrix Creation
    void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble)) dlsym(RTLD_NEXT, "glOrtho");
            }
            real_gl(l,r,b,t,n,f);
        }
        *lastAccessedMatrix = Mat4x4{
            Vec4{ 2/(r-l),0,0,0},
            Vec4{0,2/(t-b),0,0},
            Vec4{0,0,-(2/(f-n)),0},
            Vec4{-((r+l)/(r-l)), -((t+b)/(t-b)), -((f+n)/(f-n)), 1}
        };
    }

    // Set fog integer
    void glFogi(GLenum pname, GLint param) {
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
    }

    // Set fog float values
    void glFogfv(GLenum pname, const GLfloat *params) {
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
    }

    // Set fog float
    void glFogf(GLenum pname, GLfloat param) {
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
    }

    void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {
        if (forwardToSystemGl) {
            static void (*real_gl)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid*) = NULL;
            if (!real_gl) {
                real_gl = (void (*)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid*)) dlsym(RTLD_NEXT, "glReadPixels");
            }
            real_gl(x,y,width,height,format,type,pixels);
        }
        // Assume format is always RGBA
        // Assume format is always unsigned Byte
        for (int iy = y; iy < y+height; iy++) {
            for (int ix = x; ix < x+width; ix++) {
                PixelValue c = frameBufferColor[ix + iy * renderAreaWidth];
                uint8_t* pix = static_cast<uint8_t*>(pixels);
                pix[0] = c.r;
                pix[1] = c.g;
                pix[2] = c.b;
                pix[3] = 255;
            }
        }
    }

    void glPushMatrix() {
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glPushMatrix");
            }
            real_gl();
        }
        PrintInfo("glPushMatrix ");
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
        PrintInfo("\n");;
    }

    void glPopMatrix() {
        if (forwardToSystemGl) {
            static void (*real_gl)() = NULL;
            if (!real_gl) {
                real_gl = (void (*)()) dlsym(RTLD_NEXT, "glPopMatrix");
            }
            real_gl();
        }
        PrintInfo("glPopMatrix ");
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
        PrintInfo("\n");;
    }
}
