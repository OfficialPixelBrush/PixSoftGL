#pragma once

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

// Vertex
struct Vertex {
    Vec3 pos;
    Col3 col;
};

// Triangle
struct Triangle {
    Vertex a,b,c;
};

// Light
struct Light {
    Vec3 pos = Vec3{0,0,1};
};