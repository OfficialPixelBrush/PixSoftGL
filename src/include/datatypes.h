#pragma once

// Screen RGB Pixels
#include <ostream>
struct PixelValue {
    unsigned char r,g,b;
};

// 2D Vector
struct Vec2 {
    double x,y;

    Vec2 operator+(const Vec2& o) const {
        return Vec2{x + o.x, y + o.y};
    }

    Vec2 operator-(const Vec2& o) const {
        return Vec2{x - o.x, y - o.y};
    }

    Vec2 operator*(const Vec2& o) const {
        return Vec2{x * o.x, y * o.y};
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Vec2& v) {
        os << "(" << (v.x) << ", " << (v.y) << ")";
        return os;
    }
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
    
    friend std::ostream& operator<<(std::ostream& os, const Vec3& v) {
        os << "(" << (v.x) << ", " << (v.y) << ", " << (v.z) << ")";
        return os;
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
    
    friend std::ostream& operator<<(std::ostream& os, const Vec4& v) {
        os << "(" << (v.x) << ", " << (v.y) << ", " << (v.z) << ", " << (v.w) << ")";
        return os;
    }

};

// Floating-point r,g,b color
struct Col3 {
    float r,g,b;
    
    Col3 operator*(const Col3& o) const {
        return Col3{r * o.r, g * o.g, b * o.b};
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Col3& c) {
        os << "("  << (c.r) << ", " << (c.g) << ", " << (c.b) << ")";
        return os;
    }
};

// Floating-point r,g,b,a color
struct Col4 {
    float r,g,b,a;
    
    Col4 operator*(const Col4& o) const {
        return Col4{r * o.r, g * o.g, b * o.b, a * o.a};
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Col4& c) {
        os << "("  << (c.r) << ", " << (c.g) << ", " << (c.b) << ", " << (c.a) << ")";
        return os;
    }
};

// 4x4 Matrix
struct Mat4x4 {
    Vec4 a, b, c, d; // columns

    // matrix + matrix
    Mat4x4 operator+(const Mat4x4& o) const {
        return Mat4x4{a + o.a, b + o.b, c + o.c, d + o.d};
    }

    // matrix - matrix
    Mat4x4 operator-(const Mat4x4& o) const {
        return Mat4x4{a - o.a, b - o.b, c - o.c, d - o.d};
    }

    // matrix * vector (column-major)
    Vec4 operator*(const Vec4& v) const {
        return Vec4{
            a.x*v.x + b.x*v.y + c.x*v.z + d.x*v.w,
            a.y*v.x + b.y*v.y + c.y*v.z + d.y*v.w,
            a.z*v.x + b.z*v.y + c.z*v.z + d.z*v.w,
            a.w*v.x + b.w*v.y + c.w*v.z + d.w*v.w
        };
    }

    // matrix * matrix (column-major)
    Mat4x4 operator*(const Mat4x4& m) const {
        return Mat4x4{
            (*this) * m.a,
            (*this) * m.b,
            (*this) * m.c,
            (*this) * m.d
        };
    }

    // scalar multiply
    Mat4x4 operator*(float f) const {
        return Mat4x4{a*f, b*f, c*f, d*f};
    }

    friend Mat4x4 operator*(float f, const Mat4x4& m) {
        return m * f;
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Mat4x4& m) {
        os << "(" << m.a.x << ", " << m.a.y << ", " << m.a.z << ", " << m.a.w << "),"
           << "(" << m.b.x << ", " << m.b.y << ", " << m.b.z << ", " << m.b.w << "),"
           << "(" << m.c.x << ", " << m.c.y << ", " << m.c.z << ", " << m.c.w << "),"
           << "(" << m.d.x << ", " << m.d.y << ", " << m.d.z << ", " << m.d.w << ")";
        return os;
    }
};

// Vertex
struct Vertex {
    Vec3 pos;
    double w = 1.0;       // clip-space w (after projection)
    float eyeDist = 0.0f; // eye-space distance for fog
    Col4 col = Col4{1,0,1,1};
    Vec2 uv;
};

// Triangle
struct Triangle {
    Vertex a,b,c;
    
    friend std::ostream& operator<<(std::ostream& os, const Triangle& t) {
        os << "(" << t.a.pos << ", " << t.b.pos << ", " << t.c.pos << ")";
        return os;
    }
};

// Light
struct Light {
    Vec3 pos = Vec3{0, 0, 1};
    float w = 0.0f; // 0 = directional, 1 = positional
};

struct Texture2D {
    int textureWrapS = 0;
    int textureWrapT = 0;
    int textureMinFilter = 0;
    int textureMagFilter = 0;
    Col4 textureBorderColor = Col4{0, 0, 0, 0};
    float texturePriority = 0.0f;
    int width = 0;
    int height = 0;
    // Tight RGBA8888 (4 bytes/texel) — much friendlier to P2-era caches than Col4 floats.
    unsigned char* textureData = nullptr;
};

struct TextureSlot {
    int textureType = 0;
    Texture2D texture2D{};
};