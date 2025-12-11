#pragma once

// Screen RGB Pixels
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
    
    Col3 operator*(const Col3& o) const {
        return Col3{r * o.r, g * o.g, b * o.b};
    }
};

// Floating-point r,g,b,a color
struct Col4 {
    float r,g,b,a;
    
    Col4 operator*(const Col4& o) const {
        return Col4{r * o.r, g * o.g, b * o.b, a * o.a};
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
};

// Vertex
struct Vertex {
    Vec3 pos;
    double w;
    Col4 col = Col4{1,0,1,1};
    Vec2 uv;
};

// Triangle
struct Triangle {
    Vertex a,b,c;
};

// Light
struct Light {
    Vec3 pos = Vec3{0,0,1};
};

struct Texture2D {
    int textureWrapS;
    int textureWrapT;
    int textureMinFilter;
    int textureMagFilter;
    Col4 textureBorderColor;
    float texturePriority;
    int width;
    int height;
    Col4* textureData;
};

struct TextureSlot {
    int textureType = 0;
    union {
        Texture2D texture2D;
    };
};