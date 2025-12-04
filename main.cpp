#include <GL/gl.h>
#include <cmath>
#include <iostream>

#define RENDER_AREA_WIDTH 64
#define RENDER_AREA_HEIGHT 24
#define RENDER_AREA_TOTAL RENDER_AREA_WIDTH * RENDER_AREA_HEIGHT

#define MAX_VERTICES 4096

struct PixelValue {
    unsigned char r,g,b;
};

struct Vec3 {
    double x,y,z;
};

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

struct Col3 {
    float r,g,b;
};

struct Vertex {
    Vec3 pos;
    Col3 col;
};

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

int vertexIndex = 0;
int colorIndex = 0;

GLenum drawingMode = 0;
GLenum projectionMode = 0;
GLenum matrixMode = 0;
Mat4x4* currentMatrix;
Mat4x4 modelMatrix;
Mat4x4 projMatrix;

Vertex vertices[MAX_VERTICES];

PixelValue frameBufferColor [RENDER_AREA_TOTAL];
float frameBufferDepth [RENDER_AREA_TOTAL];

void DrawPixel(PixelValue p) {
    unsigned char c = ((p.r + p.b + p.g) / 3);
    if (c > 128) {
        std::cout << "#";
    } else {
        std::cout << ".";
    }
}

void DrawToScreen() {
    std::cout << "\033[H";
    for (int y = 0; y < RENDER_AREA_HEIGHT; y++) {
        for (int x = 0; x < RENDER_AREA_WIDTH; x++) {
            DrawPixel(frameBufferColor[x + y * RENDER_AREA_WIDTH]);
        }
        std::cout << "\n";
    }
}

Mat4x4 Vec3ToMat4x4(Vec3 pos) {
    return Mat4x4 {
        Vec4 { 1, 0 ,0,pos.x },
        Vec4 { 0, 1 ,0,pos.y },
        Vec4 { 0, 0 ,1,pos.z },
        Vec4 { 0, 0 ,0,1 }
    };
}

Vec3 ProjectPosition(Vec3 pos) {
    switch(projectionMode) {
        case GL_PROJECTION:
            Vec4 clip = projMatrix * modelMatrix * Vec4{pos.x,pos.y,pos.z,1};
            Vec3 ndc = { clip.x / clip.w, clip.y / clip.w, clip.z / clip.w };
            // Screen coordinates
            return Vec3{
                (ndc.x + 1) * 0.5f * RENDER_AREA_WIDTH,
                (1 - (ndc.y + 1) * 0.5f) * RENDER_AREA_HEIGHT,
                (ndc.z + 1) * 0.5f
            };
    }
    return Vec3{0,0,0};
}

void RenderPixel(Vec3 screenPos) {
    // NDC is from -1 to 1, which we'll map to 0 - RENDER_AREA_WIDTH
    int index = int(screenPos.x) + (int(screenPos.y) * RENDER_AREA_WIDTH);
    if (index < 0) return;
    if (index > RENDER_AREA_TOTAL) return;
    frameBufferColor[index] = PixelValue{255,255,255};
    frameBufferDepth[index] = screenPos.z;
}

Vec3 normalize(Vec3 v) {
    float mag = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return Vec3{v.x/mag, v.y/mag, v.z/mag};
};

extern "C" {
    void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
        vertices[vertexIndex++].pos = Vec3{x,y,z};
    }

    void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
        vertices[colorIndex++].col = Col3{red,green,blue};
    }

    void glClear(GLbitfield mask) {
        // Render last frame
        for (int i = 0; i < vertexIndex; i++) {
            Vec3 screenPos = ProjectPosition(vertices[i].pos);
            RenderPixel(screenPos);
        } 
        DrawToScreen();

        // Prepare for next frame
        vertexIndex = 0;
        colorIndex = 0;
        std::cout << "glClear ";
        // Clear color
        if (mask & GL_COLOR_BUFFER_BIT) {
            std::cout << "GL_COLOR_BUFFER_BIT ";
            for (int i = 0; i < RENDER_AREA_TOTAL; i++) {
                frameBufferColor[i] = PixelValue{0,0,0};
            }
        }
        // Clear depth
        if (mask & GL_DEPTH_BUFFER_BIT) {
            std::cout << "GL_DEPTH_BUFFER_BIT ";
            for (int i = 0; i < RENDER_AREA_TOTAL; i++) {
                frameBufferDepth[i] = 0.0f;
            }
        }
        std::cout << "\n";
    }

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

    void glEnable(GLenum cap) {
        std::cout << "glEnable ";
        switch(cap) {
            case GL_COLOR_MATERIAL:
                std::cout << "GL_COLOR_MATERIAL";
                break;
            case GL_FOG:
                std::cout << "GL_FOG";
                break;
            case GL_DEPTH_TEST:
                std::cout << "GL_DEPTH_TEST";
                break;
        }
        std::cout << "\n";
    }

    // Load new data
    void glBegin(GLenum mode) {
        drawingMode = mode;
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
            case GL_QUADS:
                break;
        }
        std::cout << "\n";
    }

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

    void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
        Mat4x4 T = {
            Vec4{1, 0, 0, 0},
            Vec4{0, 1, 0, 0},
            Vec4{0, 0, 1, 0},
            Vec4{double(x), double(y), double(z), 1}  // last column is translation
        };

        *currentMatrix = (*currentMatrix) * T; // multiply, not add
    }

    void glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
        // Convert to radians
        double angle = angleDeg * M_PI / 180.0;

        // Normalize axis
        Vec3 u = normalize(Vec3{x, y, z});
        double c = cos(angle);
        double s = sin(angle);
        double t = 1 - c;

        // Rotation matrix (column-major)
        Mat4x4 R = {
            Vec4{t*u.x*u.x + c,     t*u.x*u.y - s*u.z, t*u.x*u.z + s*u.y, 0},
            Vec4{t*u.x*u.y + s*u.z, t*u.y*u.y + c,     t*u.y*u.z - s*u.x, 0},
            Vec4{t*u.x*u.z - s*u.y, t*u.y*u.z + s*u.x, t*u.z*u.z + c,     0},
            Vec4{0,                  0,                  0,               1}
        };

        *currentMatrix = (*currentMatrix) * R; // multiply, not add
    }

    void glScalef(GLfloat x, GLfloat y, GLfloat z) {
        Mat4x4 S = {
            Vec4{double(x), 0, 0, 0},
            Vec4{0, double(y), 0, 0},
            Vec4{0, 0, double(z), 0},
            Vec4{0, 0, 0, 1}
        };

        *currentMatrix = (*currentMatrix) * S; // multiply, not add
    }

    void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
        *currentMatrix = Mat4x4{
            Vec4{ (2*n)/(r-l), 0, 0, 0 },
            Vec4{ 0, (2*n)/(t-b), 0, 0 },
            Vec4{ (r+l)/(r-l), (t+b)/(t-b), -(f+n)/(f-n), -1 },
            Vec4{ 0, 0, -(2*f*n)/(f-n), 0 }
        };
    }
}
