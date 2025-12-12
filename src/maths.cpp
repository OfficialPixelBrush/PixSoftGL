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
Vec4 ProjectPosition(Vec3 pos) {
    Vec4 eye = modelMatrices[modelMatrixPtr] * Vec4{pos.x, pos.y, pos.z, 1.0};
    Vec4 clip = projMatrices[projMatrixPtr] * eye;
    
    // Perspective divide
    Vec3 ndc = { clip.x / clip.w, clip.y / clip.w, clip.z / clip.w };
    
    return Vec4{
        (ndc.x + 1.0f) * 0.5f * viewportAreaWidth,
        (1.0f - (ndc.y + 1.0f) * 0.5f) * viewportAreaHeight,
        ndc.z,
        clip.w
    };
}

// Project triangle to screen
Triangle ProjectTriangle(Triangle tri) {
    Vec4 p0 = ProjectPosition(tri.a.pos);
    Vec4 p1 = ProjectPosition(tri.b.pos);
    Vec4 p2 = ProjectPosition(tri.c.pos);

    return Triangle{
        Vertex{ Vec3{p0.x, p0.y, p0.z}, p0.w, tri.a.col, tri.a.uv },
        Vertex{ Vec3{p1.x, p1.y, p1.z}, p1.w, tri.b.col, tri.b.uv },
        Vertex{ Vec3{p2.x, p2.y, p2.z}, p2.w, tri.c.col, tri.c.uv }
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

Col4 BarycentricColor(Triangle tri, Vec3& p) {
    Vec3 a = tri.a.pos;
    Vec3 b = tri.b.pos;
    Vec3 c = tri.c.pos;

    float det = (b.y - c.y)*(a.x - c.x) + (c.x - b.x)*(a.y - c.y);
    if (fabs(det) < 1e-9f) {
        // Degenerate triangle: return first vertex color
        p.z = tri.a.pos.z;
        return tri.a.col;
    }

    float w1 = ((b.y - c.y)*(p.x - c.x) + (c.x - b.x)*(p.y - c.y)) / det;
    float w2 = ((c.y - a.y)*(p.x - c.x) + (a.x - c.x)*(p.y - c.y)) / det;
    float w3 = 1.0f - w1 - w2;

    // Perspective-correct interpolation:
    // each vertex has tri.*.w = original clip.w
    float invWA = (tri.a.w == 0.0f) ? 0.0f : 1.0f / tri.a.w;
    float invWB = (tri.b.w == 0.0f) ? 0.0f : 1.0f / tri.b.w;
    float invWC = (tri.c.w == 0.0f) ? 0.0f : 1.0f / tri.c.w;

    float invW_interp = w1 * invWA + w2 * invWB + w3 * invWC;
    if (invW_interp == 0.0f) invW_interp = 1e-9f;
    float W = 1.0f / invW_interp;

    // Interpolate color components divided by clip.w, then multiply by W
    float r_over_w = w1 * (tri.a.col.r * invWA) + w2 * (tri.b.col.r * invWB) + w3 * (tri.c.col.r * invWC);
    float g_over_w = w1 * (tri.a.col.g * invWA) + w2 * (tri.b.col.g * invWB) + w3 * (tri.c.col.g * invWC);
    float b_over_w = w1 * (tri.a.col.b * invWA) + w2 * (tri.b.col.b * invWB) + w3 * (tri.c.col.b * invWC);
    float a_over_w = w1 * (tri.a.col.a * invWA) + w2 * (tri.b.col.a * invWB) + w3 * (tri.c.col.a * invWC);

    Col4 outCol = Col4{
        r_over_w * W,
        g_over_w * W,
        b_over_w * W,
        a_over_w * W
    };

    // Interpolate depth (tri.*.pos.z is NDC z already)
    float z_over_w = w1 * (tri.a.pos.z * invWA) + w2 * (tri.b.pos.z * invWB) + w3 * (tri.c.pos.z * invWC);
    p.z = z_over_w * W;

    return outCol;
}

// Perspective-correct texture lookup (returns sampled color)
// Uses same barycentric weights; sets p.z as well (keeps consistent with BarycentricColor)
Col4 BarycentricTexture(Triangle tri, Vec3& p) {
    Vec3 a = tri.a.pos;
    Vec3 b = tri.b.pos;
    Vec3 c = tri.c.pos;

    float det = (b.y - c.y)*(a.x - c.x) + (c.x - b.x)*(a.y - c.y);
    if (fabs(det) < 1e-9f) {
        // Degenerate triangle: sample at vertex A
        int tx = 0, ty = 0;
        if (!lastAccessedTexture) return Col4{1,1,1,1};
        tx = std::clamp(int(tri.a.uv.x * (lastAccessedTexture->texture2D.width - 1)), 0, lastAccessedTexture->texture2D.width - 1);
        ty = std::clamp(int(tri.a.uv.y * (lastAccessedTexture->texture2D.height - 1)), 0, lastAccessedTexture->texture2D.height - 1);
        return lastAccessedTexture->texture2D.textureData[ty * lastAccessedTexture->texture2D.width + tx];
    }

    float w1 = ((b.y - c.y)*(p.x - c.x) + (c.x - b.x)*(p.y - c.y)) / det;
    float w2 = ((c.y - a.y)*(p.x - c.x) + (a.x - c.x)*(p.y - c.y)) / det;
    float w3 = 1.0f - w1 - w2;

    float invWA = (tri.a.w == 0.0f) ? 0.0f : 1.0f / tri.a.w;
    float invWB = (tri.b.w == 0.0f) ? 0.0f : 1.0f / tri.b.w;
    float invWC = (tri.c.w == 0.0f) ? 0.0f : 1.0f / tri.c.w;

    float invW_interp = w1 * invWA + w2 * invWB + w3 * invWC;
    if (invW_interp == 0.0f) invW_interp = 1e-9f;
    float W = 1.0f / invW_interp;

    // Interpolate u/v divided by clip.w, then divide by invW_interp
    float u_over_w = w1 * (tri.a.uv.x * invWA) + w2 * (tri.b.uv.x * invWB) + w3 * (tri.c.uv.x * invWC);
    float v_over_w = w1 * (tri.a.uv.y * invWA) + w2 * (tri.b.uv.y * invWB) + w3 * (tri.c.uv.y * invWC);

    float u = u_over_w * W;
    float v = v_over_w * W;

    // Interpolate depth as well to keep p.z consistent with BarycentricColor
    float z_over_w = w1 * (tri.a.pos.z * invWA) + w2 * (tri.b.pos.z * invWB) + w3 * (tri.c.pos.z * invWC);
    p.z = z_over_w * W;

    // Sample texture with wrap/clamp logic
    if (!lastAccessedTexture) return Col4{1,1,1,1};

    int tx, ty;
    int tw = lastAccessedTexture->texture2D.width;
    int th = lastAccessedTexture->texture2D.height;

    // S (u)
    if (lastAccessedTexture->texture2D.textureWrapS == GL_CLAMP || lastAccessedTexture->texture2D.textureWrapS == GL_CLAMP_TO_EDGE) {
        tx = std::clamp(int(u * (tw - 1)), 0, tw - 1);
    } else {
        int i = int(std::floor(u * tw));
        tx = i % tw;
        if (tx < 0) tx += tw;
    }

    // T (v)
    if (lastAccessedTexture->texture2D.textureWrapT == GL_CLAMP || lastAccessedTexture->texture2D.textureWrapT == GL_CLAMP_TO_EDGE) {
        ty = std::clamp(int(v * (th - 1)), 0, th - 1);
    } else {
        int j = int(std::floor(v * th));
        ty = j % th;
        if (ty < 0) ty += th;
    }

    return lastAccessedTexture->texture2D.textureData[ty * tw + tx];
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

void RecomputeBase() {
    const void* minPtr = nullptr;
    if (vertexArrayPointer) minPtr = vertexArrayPointer;
    if (colorArrayPointer) minPtr = !minPtr ? colorArrayPointer : std::min(minPtr, colorArrayPointer);
    if (textureArrayPointer) minPtr = !minPtr ? textureArrayPointer : std::min(minPtr, textureArrayPointer);
    vertexArrayBasePointer = minPtr;
}
