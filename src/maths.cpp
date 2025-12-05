#include "maths.h"

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