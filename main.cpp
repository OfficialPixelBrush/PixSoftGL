#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <iostream>

#define DEFAULT_RENDER_AREA_WIDTH 64
#define DEFAULT_RENDER_AREA_HEIGHT 24

#define MAX_VERTICES 4096

#define PIXSOFTGL_VERSION "1.1"
#define PIXSOFTGL_EXTENSIONS ""
#define PIXSOFTGL_VENDOR "PixelBrushArt"
#define PIXSOFTGL_RENDERER "PixSoftGL"

// Screen RGB Pixels
struct PixelValue {
    unsigned char r,g,b;
};

// 3D Vector
struct Vec3 {
    double x,y,z;

    Vec3 operator+(const Vec3& o) const {
        return Vec3{x + o.x, y + o.y, z + o.z};
    }

    Vec3 operator-(const Vec3& o) const {
        return Vec3{x - o.x, y - o.y, z - o.z};
    }

    Vec3 operator*(const Vec3& o) const {
        return Vec3{x * o.x, y * o.y, z * o.z};
    }
};

// 4D Vector
struct Vec4 {
    double x,y,z,w;

    Vec4 operator+(const Vec4& o) const {
        return Vec4{x + o.x, y + o.y, z + o.z, w + o.w};
    }
    
    Vec4 operator-(const Vec4& o) const {
        return Vec4{x - o.x, y - o.y, z - o.z, w - o.w};
    }

    Vec4 operator*(const Vec4& o) const {
        return Vec4{x * o.x, y * o.y, z * o.z, w * o.w};
    }

    Vec4 operator/(const Vec4& o) const {
        return Vec4{x / o.x, y / o.y, z / o.z, w / o.w};
    }
    
    Vec4 operator*(float f) const {
        return Vec4{x*f, y*f, z*f, w*f};
    }

    friend Vec4 operator*(float f, const Vec4& v) {
        return v * f;
    }

};

// Floating-point r,g,b color
struct Col3 {
    float r,g,b;
};

// Floating-point r,g,b,a color
struct Col4 {
    float r,g,b,a;
};

// Vertex
struct Vertex {
    Vec3 pos;
    Col3 col;
};

// Triangle
struct Triangle {
    Vertex a,b,c;
};

// 4x4 Matrix
struct Mat4x4 {
    Vec4 a,b,c,d;

    Mat4x4 operator+(const Mat4x4& o) const {
        return Mat4x4{a + o.a, b + o.b, c + o.c, d + o.d};
    }
    
    Mat4x4 operator-(const Mat4x4& o) const {
        return Mat4x4{a - o.a, b - o.b, c - o.c, d - o.d};
    }

    Vec4 operator*(const Vec4& v) const {
        return Vec4{
            a.x*v.x + b.x*v.y + c.x*v.z + d.x*v.w,
            a.y*v.x + b.y*v.y + c.y*v.z + d.y*v.w,
            a.z*v.x + b.z*v.y + c.z*v.z + d.z*v.w,
            a.w*v.x + b.w*v.y + c.w*v.z + d.w*v.w
        };
    }

    Mat4x4 operator*(const Mat4x4& m) const {
        Mat4x4 result;
        result.a = (*this) * m.a;
        result.b = (*this) * m.b;
        result.c = (*this) * m.c;
        result.d = (*this) * m.d;
        return result;
    }


    Mat4x4 operator/(const Mat4x4& o) const {
        return Mat4x4{a / o.a, b / o.b, c / o.c, d / o.d};
    }

    Mat4x4 operator*(float f) const {
        return Mat4x4{a*f, b*f, c*f, d*f};
    }

    friend Mat4x4 operator*(float f, const Mat4x4& m) {
        return m * f;
    }

};

// The current vertex index
int vertexIndex = 0;

// If depth should be tested
bool depthTestActive = false;
bool fogActive = false;
bool colorMaterialActive = false;
bool texture2dActive = false;
bool blendActive = false;
bool lightingActive = false;

// Fog variables
int fogMode = 0;
Col4 fogColor = Col4{0,0,0,0};
float fogStart = 0.0;
float fogEnd = 0.0;

// The current object rendering mode
GLenum drawingMode = 0;

// The current projection mode
GLenum projectionMode = 0;

// The current matrix mode
GLenum matrixMode = 0;

// Currently active color
Col3 currentColor = Col3{1,1,1};
Col3 clearColor = Col3{0,0,0};

// Matricies
Mat4x4* currentMatrix;
Mat4x4 modelMatrix;
Mat4x4 projMatrix;

// Vertex buffer
Vertex vertices[MAX_VERTICES];

// Viewport size
int renderAreaWidth = DEFAULT_RENDER_AREA_WIDTH;
int renderAreaHeight = DEFAULT_RENDER_AREA_HEIGHT;
int renderAreaTotal = DEFAULT_RENDER_AREA_WIDTH * DEFAULT_RENDER_AREA_HEIGHT;

// Screen framebuffer
PixelValue* frameBufferColor;
float* frameBufferDepth;

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
void DrawPixel(PixelValue p) {
    std::cout << "\e[38;2;" << int(p.r) << ";" << int(p.g) << ";" << int(p.b) << "m" << "█";
}

// Render the framebuffer colors to the terminal
void DrawToScreen() {
    std::cout << "\033[H";
    for (int y = 0; y < renderAreaHeight; y++) {
        for (int x = 0; x < renderAreaWidth; x++) {
            DrawPixel(frameBufferColor[x + y * renderAreaWidth]);
        }
        std::cout << "\n";
    }
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
            Vec4 eye = modelMatrix * Vec4{pos.x, pos.y, pos.z, 1.0};
            // eye.z is negative in front of the camera in typical OpenGL; use -eye.z as positive distance
            float eyeDist = float(-eye.z);

            // then project
            Vec4 clip = projMatrix * eye;
            Vec3 ndc = { clip.x / clip.w, clip.y / clip.w, clip.z / clip.w };

            return Vec3{
                (ndc.x + 1.0f) * 0.5f * renderAreaWidth,
                (1.0f - (ndc.y + 1.0f) * 0.5f) * renderAreaWidth,
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
    if (depthTestActive && screenPos.z >= frameBufferDepth[index]) {
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
    frameBufferColor[index] = Col3ToPixelValue(color);
    frameBufferDepth[index] = screenPos.z;
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
        vertices[vertexIndex].pos = Vec3{x,y,0};
        vertices[vertexIndex].col = currentColor;
        vertexIndex++;
    }

    // Add float vertex (3)
    void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
        vertices[vertexIndex].pos = Vec3{x,y,z};
        vertices[vertexIndex].col = currentColor;
        vertexIndex++;
    }

    // Adjust OpenGL Viewport
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
        renderAreaWidth = width;
        renderAreaHeight = height;
        renderAreaTotal = renderAreaWidth * renderAreaHeight;
        // Resize buffers
        frameBufferColor = (PixelValue*)malloc( renderAreaTotal * sizeof(PixelValue));
        frameBufferDepth = (float*)malloc(renderAreaTotal * sizeof(float));
    }

    // Set float color
    void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
        clearColor = Col3{red,green,blue};
    }

    // Set float color
    void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
        currentColor = Col3{red,green,blue};
    }

    // Return OpenGL info
    const GLubyte* glGetString(GLenum name) {
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

    // Clear framebuffer(s)
    void glClear(GLbitfield mask) {
        // Render last frame
        DrawToScreen();

        // Prepare for next frame
        std::cout << "glClear ";
        // Clear color
        if (mask & GL_COLOR_BUFFER_BIT) {
            std::cout << "GL_COLOR_BUFFER_BIT ";
            for (int i = 0; i < renderAreaTotal; i++) {
                frameBufferColor[i] = Col3ToPixelValue(clearColor);
            }
        }
        // Clear depth
        if (mask & GL_DEPTH_BUFFER_BIT) {
            std::cout << "GL_DEPTH_BUFFER_BIT ";
            for (int i = 0; i < renderAreaTotal; i++) {
                frameBufferDepth[i] = INFINITY;
            }
        }
        std::cout << "\n";
    }

    // Set Matrix mode
    void glMatrixMode(GLenum mode) {
        // Prepare next matrix Mode
        std::cout << "glMatrixMode ";
        matrixMode = mode;
        switch(matrixMode) {
            case GL_PROJECTION:
                std::cout << "GL_PROJECTION";
                projectionMode = GL_PROJECTION;
                currentMatrix = &projMatrix;
                break;
            case GL_MODELVIEW:
                std::cout << "GL_MODELVIEW";
                currentMatrix = &modelMatrix;
                break;
        }
        std::cout << "\n";
    }

    // Enable property
    void glEnable(GLenum cap) {
        std::cout << "glEnable ";
        switch(cap) {
            case GL_COLOR_MATERIAL:
                std::cout << "GL_COLOR_MATERIAL";
                colorMaterialActive = true;
                break;
            case GL_FOG:
                std::cout << "GL_FOG";
                fogActive = true;
                break;
            case GL_DEPTH_TEST:
                std::cout << "GL_DEPTH_TEST";
                depthTestActive = true;
                break;
            case GL_TEXTURE_2D:
                std::cout << "GL_TEXTURE_2D";
                texture2dActive = true;
                break;
            case GL_LIGHTING:
                std::cout << "GL_LIGHTING";
                lightingActive = true;
                break;
            case GL_BLEND:
                std::cout << "GL_BLEND";
                blendActive = true;
                break;
        }
        std::cout << "\n";
    }

    // Disable property
    void glDisable(GLenum cap) {
        std::cout << "glDisable ";
        switch(cap) {
            case GL_COLOR_MATERIAL:
                std::cout << "GL_COLOR_MATERIAL";
                colorMaterialActive = false;
                break;
            case GL_FOG:
                std::cout << "GL_FOG";
                fogActive = false;
                break;
            case GL_DEPTH_TEST:
                std::cout << "GL_DEPTH_TEST";
                depthTestActive = false;
                break;
            case GL_TEXTURE_2D:
                std::cout << "GL_TEXTURE_2D";
                texture2dActive = false;
                break;
            case GL_LIGHTING:
                std::cout << "GL_LIGHTING";
                lightingActive = false;
                break;
            case GL_BLEND:
                std::cout << "GL_BLEND";
                blendActive = false;
                break;
        }
        std::cout << "\n";
    }

    // Load new data
    void glBegin(GLenum mode) {
        drawingMode = mode;
        vertexIndex = 0;
        std::cout << "glBegin ";
        switch(drawingMode) {
            case GL_QUADS:
                std::cout << "GL_QUADS ";
                break;
        }
        std::cout << "\n";
    }

    // We're done, now draw whatever we sent to the fb
    void glEnd() {
        std::cout << "glEnd ";
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
        }
        std::cout << "\n";
    }

    // Load identity matrix
    void glLoadIdentity() {
        std::cout << "glLoadIdentity ";
        *currentMatrix = Mat4x4 {
            Vec4 { 1, 0 ,0,0 },
            Vec4 { 0, 1 ,0,0 },
            Vec4 { 0, 0 ,1,0 },
            Vec4 { 0, 0 ,0,1 }
        };
        std::cout << "\n";
    }

    // Float translate
    void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
        Mat4x4 T = {
            Vec4{1, 0, 0, 0},
            Vec4{0, 1, 0, 0},
            Vec4{0, 0, 1, 0},
            Vec4{double(x), double(y), double(z), 1}  // last column is translation
        };

        *currentMatrix = (*currentMatrix) * T; // multiply, not add
    }

    // Float Rotate
    void glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
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

        *currentMatrix = (*currentMatrix) * R; // multiply, not add
    }

    // Float scale
    void glScalef(GLfloat x, GLfloat y, GLfloat z) {
        Mat4x4 S = {
            Vec4{double(x), 0, 0, 0},
            Vec4{0, double(y), 0, 0},
            Vec4{0, 0, double(z), 0},
            Vec4{0, 0, 0, 1}
        };

        *currentMatrix = (*currentMatrix) * S; // multiply, not add
    }

    // Perspective Projection Matrix Creation
    void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
        *currentMatrix = Mat4x4{
            Vec4{ (2*n)/(r-l), 0, 0, 0 },
            Vec4{ 0, (2*n)/(t-b), 0, 0 },
            Vec4{ (r+l)/(r-l), (t+b)/(t-b), -(f+n)/(f-n), -1 },
            Vec4{ 0, 0, -(2*f*n)/(f-n), 0 }
        };
    }

    // Orthographic Projection Matrix Creation
    void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
            *currentMatrix = Mat4x4{
                Vec4{ 2/(r-l),0,0,0},
                Vec4{0,2/(t-b),0,0},
                Vec4{0,0,-(2/(f-n)),0},
                Vec4{-((r+l)/(r-l)), -((t+b)/(t-b)), -((f+n)/(f-n)), 1}
            };
    }

    // Set fog integer
    void glFogi(GLenum pname, GLint param) {
        switch(pname) {
            case GL_FOG_MODE:
                fogMode = param;
                break;
        }
    }

    // Set fog float values
    void glFogfv(GLenum pname, const GLfloat *params) {
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
}
