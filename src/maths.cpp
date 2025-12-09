#include "maths.h"
#include "global.h"
#include <cstdlib>

// Interpolate two colors linearly
Col4 lerp(Col4 a, Col4 b, float t) {
    return Col4{
        a.r + t * (b.r - a.r),
        a.g + t * (b.g - a.g),
        a.b + t * (b.b - a.b),
        a.a + t * (b.a - a.a),
    };
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
    Vec4 eye = modelMatrices[modelMatrixPtr] * Vec4{pos.x, pos.y, pos.z, 1.0};
    float eyeDist = -eye.z;

    Vec4 clip = projMatrices[projMatrixPtr] * eye;
    Vec3 ndc = { clip.x / clip.w, clip.y / clip.w, clip.z / clip.w };

    return Vec3{
        (ndc.x + 1.0f) * 0.5f * renderAreaWidth,
        (1.0f - (ndc.y + 1.0f) * 0.5f) * renderAreaHeight,
        eyeDist
    };
}

// Project triangle to screen
Triangle ProjectTriangle(Triangle tri) {
    return Triangle{
        Vertex { ProjectPosition(tri.a.pos), tri.a.col, tri.a.uv },
        Vertex { ProjectPosition(tri.b.pos), tri.b.col, tri.b.uv },
        Vertex { ProjectPosition(tri.c.pos), tri.c.col, tri.c.uv },
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

Col3 PixelValueToCol3(PixelValue color) {
    return Col3{
        static_cast<float>(float(color.r)/255.0),
        static_cast<float>(float(color.g)/255.0),
        static_cast<float>(float(color.b)/255.0)
    };
}

// Normalize vector
Vec3 Normalize(Vec3 v) {
    float mag = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (mag == 0.0f) return Vec3{0,0,0};
    return Vec3{v.x/mag, v.y/mag, v.z/mag};
};

// 3D Dot Product
float Dot3D(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// 2D Dot Product
float Dot2D(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y;
}

// 2D Cross Product
float Cross2D(Vec3 a, Vec3 b) {
    return a.x * b.y - a.y * b.x;
}

// Rotate vector 90°
Vec3 Perpendicular2D(Vec3 vec) {
    return Vec3{-vec.y, vec.x, 0};
}

// Check if point is on right side of vector
float SignedTriangleArea(Vec3 a, Vec3 b, Vec3 p) {
    Vec3 ap = p - a;
    Vec3 abPerp = Perpendicular2D(b - a);
    return Dot2D(ap, abPerp) / 2.0;
}

// Check if point is inside of triangle
bool PointInTriangle(Triangle tri, Vec3 p) {
    float areaABP = SignedTriangleArea(tri.a.pos, tri.b.pos, p);
    float areaBCP = SignedTriangleArea(tri.b.pos, tri.c.pos, p);
    float areaCAP = SignedTriangleArea(tri.c.pos, tri.a.pos, p);

    bool frontFace = areaABP >= 0 && areaBCP >= 0 && areaCAP >= 0;
    bool backFace = areaABP <= 0 && areaBCP <= 0 && areaCAP <= 0;

    if (counterClockWiseWindingActive)
        std::swap(frontFace, backFace);
    
    // When culling is OFF, accept both sides
    // When culling is ON, only accept front faces
    bool inTri = cullFaceActive ? frontFace : (frontFace || backFace);
    
    float totalArea = std::abs(areaABP + areaBCP + areaCAP);

    return inTri && totalArea > 0;
}

// Simple barycentric color interpolation
Col4 BarycentricColor(Triangle tri, Vec3& p) {
    Vec3 a = tri.a.pos;
    Vec3 b = tri.b.pos;
    Vec3 c = tri.c.pos;

    float det = (b.y - c.y)*(a.x - c.x) + (c.x - b.x)*(a.y - c.y);
    float w1 = ((b.y - c.y)*(p.x - c.x) + (c.x - b.x)*(p.y - c.y)) / det;
    float w2 = ((c.y - a.y)*(p.x - c.x) + (a.x - c.x)*(p.y - c.y)) / det;
    float w3 = 1.0f - w1 - w2;
    //w1 = std::clamp(w1, 0.0f, 1.0f);
    //w2 = std::clamp(w2, 0.0f, 1.0f);
    //w3 = std::clamp(w3, 0.0f, 1.0f);

    // Calulate depth too
    p.z = w1*tri.a.pos.z + w2*tri.b.pos.z + w3*tri.c.pos.z;

    return Col4{
        w1*tri.a.col.r + w2*tri.b.col.r + w3*tri.c.col.r,
        w1*tri.a.col.g + w2*tri.b.col.g + w3*tri.c.col.g,
        w1*tri.a.col.b + w2*tri.b.col.b + w3*tri.c.col.b,
        w1*tri.a.col.a + w2*tri.b.col.a + w3*tri.c.col.a
    };
}

Col4 BarycentricTexture(Triangle tri, Vec3& p) {
    Vec3 a = tri.a.pos;
    Vec3 b = tri.b.pos;
    Vec3 c = tri.c.pos;

    // Compute barycentric coordinates
    float det = (b.y - c.y)*(a.x - c.x) + (c.x - b.x)*(a.y - c.y);
    float w1 = ((b.y - c.y)*(p.x - c.x) + (c.x - b.x)*(p.y - c.y)) / det;
    float w2 = ((c.y - a.y)*(p.x - c.x) + (a.x - c.x)*(p.y - c.y)) / det;
    float w3 = 1.0f - w1 - w2;

    // Clamp weights to [0,1] to avoid sampling outside
    //w1 = std::clamp(w1, 0.0f, 1.0f);
    //w2 = std::clamp(w2, 0.0f, 1.0f);
    //w3 = std::clamp(w3, 0.0f, 1.0f);

    // Depth (optional)
    //p.z = w1*tri.a.pos.z + w2*tri.b.pos.z + w3*tri.c.pos.z;

    // Interpolate UVs
    float u = w1*tri.a.uv.x + w2*tri.b.uv.x + w3*tri.c.uv.x;
    float v = w1*tri.a.uv.y + w2*tri.b.uv.y + w3*tri.c.uv.y;

    int tx, ty;

    // clamp
    if (lastAccessedTexture->texture2D.textureWrapS == GL_CLAMP || lastAccessedTexture->texture2D.textureWrapS == GL_CLAMP_TO_EDGE) {
        tx = std::clamp(int(u * (lastAccessedTexture->texture2D.width - 1)), 0, lastAccessedTexture->texture2D.width - 1);
    } 
    // repeat
    else {
        int i = int(std::floor(u * lastAccessedTexture->texture2D.width));
        tx = i % lastAccessedTexture->texture2D.width;
        if (tx < 0) tx += lastAccessedTexture->texture2D.width;
    }

    if (lastAccessedTexture->texture2D.textureWrapT == GL_CLAMP || lastAccessedTexture->texture2D.textureWrapT == GL_CLAMP_TO_EDGE) {
        ty = std::clamp(int(v * (lastAccessedTexture->texture2D.height - 1)), 0, lastAccessedTexture->texture2D.height - 1);
    } else {
        int j = int(std::floor(v * lastAccessedTexture->texture2D.height));
        ty = j % lastAccessedTexture->texture2D.height;
        if (ty < 0) ty += lastAccessedTexture->texture2D.height;
    }

    return lastAccessedTexture->texture2D.textureData[ty * lastAccessedTexture->texture2D.width + tx];
}

bool DetermineBounding(Triangle& tri, int& xMin, int& yMin, int& xMax, int& yMax) {
    xMin = std::min({xMin, int(tri.a.pos.x), int(tri.b.pos.x), int(tri.c.pos.x)});
    yMin = std::min({yMin, int(tri.a.pos.y), int(tri.b.pos.y), int(tri.c.pos.y)});
    xMax = std::max({xMax, int(tri.a.pos.x), int(tri.b.pos.x), int(tri.c.pos.x)});
    yMax = std::max({yMax, int(tri.a.pos.y), int(tri.b.pos.y), int(tri.c.pos.y)});

    // Check if completely outside render area
    if (xMax < 0 || yMax < 0 || xMin >= renderAreaWidth || yMin >= renderAreaHeight) {
        return false; // discard
    }

    // Clamp to render area
    xMin = std::max(0, xMin);
    yMin = std::max(0, yMin);
    xMax = std::min(renderAreaWidth, xMax);
    yMax = std::min(renderAreaHeight, yMax);
    return true;
}

Vec3 CalculateNormal(Triangle& tri) {
    Vec3 A = tri.b.pos - tri.a.pos;
    Vec3 B = tri.c.pos - tri.a.pos;
    return Normalize(Vec3 {
        A.y * B.z - A.z * B.y,
        A.z * B.x - A.x * B.z,
        A.x * B.y - A.y * B.x
    });
}