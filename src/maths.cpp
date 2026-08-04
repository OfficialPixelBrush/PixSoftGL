#include "maths.h"
#include "global.h"
#include <cmath>
#include <cstdlib>

Col4 lerp(Col4 a, Col4 b, float t) {
    return Col4{
        a.r + t * (b.r - a.r),
        a.g + t * (b.g - a.g),
        a.b + t * (b.b - a.b),
        a.a + t * (b.a - a.a),
    };
}

Col3 lerp(Col3 a, Col3 b, float t) {
    return Col3{
        a.r + t * (b.r - a.r),
        a.g + t * (b.g - a.g),
        a.b + t * (b.b - a.b)
    };
}

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

Mat4x4 Vec3ToMat4x4(Vec3 pos) {
    return Mat4x4{
        Vec4{1, 0, 0, 0},
        Vec4{0, 1, 0, 0},
        Vec4{0, 0, 1, 0},
        Vec4{pos.x, pos.y, pos.z, 1}
    };
}

Vec4 TransformToClip(Vec3 pos) {
    Vec4 eye = modelMatrices[modelMatrixPtr] * Vec4{pos.x, pos.y, pos.z, 1.0};
    return projMatrices[projMatrixPtr] * eye;
}

float EyeDistance(Vec3 pos) {
    Vec4 eye = modelMatrices[modelMatrixPtr] * Vec4{pos.x, pos.y, pos.z, 1.0};
    return static_cast<float>(std::sqrt(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z));
}

Vec4 ProjectPosition(Vec3 pos) {
    Vec4 clip = TransformToClip(pos);
    if (clip.w <= CLIP_W_EPSILON && clip.w >= -CLIP_W_EPSILON) {
        return Vec4{0, 0, 1, clip.w};
    }

    const double invW = 1.0 / clip.w;
    Vec3 ndc = {clip.x * invW, clip.y * invW, clip.z * invW};

    return Vec4{
        viewportOffsetX + (ndc.x + 1.0) * 0.5 * viewportAreaWidth,
        viewportOffsetY + (1.0 - (ndc.y + 1.0) * 0.5) * viewportAreaHeight,
        ndc.z,
        clip.w
    };
}

namespace {

struct ClipVert {
    Vec4 clip;
    Col4 col;
    Vec2 uv;
    float eyeDist;
};

Vertex toWindowVertex(const ClipVert& v) {
    Vertex out{};
    if (v.clip.w <= CLIP_W_EPSILON && v.clip.w >= -CLIP_W_EPSILON) {
        out.pos = Vec3{0, 0, 1};
        out.w = v.clip.w;
        out.eyeDist = v.eyeDist;
        out.col = v.col;
        out.uv = v.uv;
        return out;
    }
    const double invW = 1.0 / v.clip.w;
    const double ndcX = v.clip.x * invW;
    const double ndcY = v.clip.y * invW;
    const double ndcZ = v.clip.z * invW;
    out.pos = Vec3{
        viewportOffsetX + (ndcX + 1.0) * 0.5 * viewportAreaWidth,
        viewportOffsetY + (1.0 - (ndcY + 1.0) * 0.5) * viewportAreaHeight,
        ndcZ
    };
    out.w = v.clip.w;
    out.eyeDist = v.eyeDist;
    out.col = v.col;
    out.uv = v.uv;
    return out;
}

ClipVert lerpClip(const ClipVert& a, const ClipVert& b, double t) {
    ClipVert o{};
    o.clip = Vec4{
        a.clip.x + (b.clip.x - a.clip.x) * t,
        a.clip.y + (b.clip.y - a.clip.y) * t,
        a.clip.z + (b.clip.z - a.clip.z) * t,
        a.clip.w + (b.clip.w - a.clip.w) * t
    };
    o.col = lerp(a.col, b.col, static_cast<float>(t));
    o.uv = Vec2{
        a.uv.x + (b.uv.x - a.uv.x) * t,
        a.uv.y + (b.uv.y - a.uv.y) * t
    };
    o.eyeDist = lerp(a.eyeDist, b.eyeDist, static_cast<float>(t));
    return o;
}

// Keep vertices with clip.z + clip.w >= 0 (OpenGL near plane).
int clipAgainstNear(const ClipVert* in, int nIn, ClipVert* out) {
    int nOut = 0;
    for (int i = 0; i < nIn; ++i) {
        const ClipVert& cur = in[i];
        const ClipVert& nxt = in[(i + 1) % nIn];
        const double curD = cur.clip.z + cur.clip.w;
        const double nxtD = nxt.clip.z + nxt.clip.w;
        const bool curIn = curD >= 0.0;
        const bool nxtIn = nxtD >= 0.0;

        if (curIn) {
            out[nOut++] = cur;
        }
        if (curIn != nxtIn) {
            const double t = curD / (curD - nxtD);
            out[nOut++] = lerpClip(cur, nxt, t);
        }
    }
    return nOut;
}

} // namespace

int ProjectAndClipTriangle(const Triangle& tri, Triangle outTris[2]) {
    ClipVert in[3];
    for (int i = 0; i < 3; ++i) {
        const Vertex& v = (i == 0) ? tri.a : (i == 1) ? tri.b : tri.c;
        in[i].clip = TransformToClip(v.pos);
        in[i].col = v.col;
        in[i].uv = v.uv;
        in[i].eyeDist = EyeDistance(v.pos);
    }

    // Fast reject: all behind near plane.
    if ((in[0].clip.z + in[0].clip.w) < 0.0 &&
        (in[1].clip.z + in[1].clip.w) < 0.0 &&
        (in[2].clip.z + in[2].clip.w) < 0.0) {
        return 0;
    }

    // Fast path: fully in front of near plane.
    if ((in[0].clip.z + in[0].clip.w) >= 0.0 &&
        (in[1].clip.z + in[1].clip.w) >= 0.0 &&
        (in[2].clip.z + in[2].clip.w) >= 0.0) {
        // Also reject if any w is non-positive (behind camera / w-flip).
        if (in[0].clip.w <= CLIP_W_EPSILON ||
            in[1].clip.w <= CLIP_W_EPSILON ||
            in[2].clip.w <= CLIP_W_EPSILON) {
            // Fall through to clipper which also guards w.
        } else {
            outTris[0] = Triangle{
                toWindowVertex(in[0]),
                toWindowVertex(in[1]),
                toWindowVertex(in[2])
            };
            return 1;
        }
    }

    ClipVert clipped[8];
    int n = clipAgainstNear(in, 3, clipped);
    if (n < 3) return 0;

    // Guard remaining vertices with non-positive w.
    for (int i = 0; i < n; ++i) {
        if (clipped[i].clip.w <= CLIP_W_EPSILON) return 0;
    }

    Vertex window[8];
    for (int i = 0; i < n; ++i) {
        window[i] = toWindowVertex(clipped[i]);
    }

    int count = 0;
    for (int i = 1; i + 1 < n && count < 2; ++i) {
        outTris[count++] = Triangle{window[0], window[i], window[i + 1]};
    }
    // Fan can produce more than 2 tris if clip yields a quad+; emit up to n-2.
    // Caller only has 2 slots — for near-plane clip of a triangle, max is a quad → 2 tris.
    return count;
}

Triangle ProjectTriangle(Triangle tri) {
    Triangle out[2];
    if (ProjectAndClipTriangle(tri, out) >= 1) {
        return out[0];
    }
    // Degenerate placeholder discarded by bounding box.
    return Triangle{
        Vertex{Vec3{-1, -1, 1}, 1, 0, tri.a.col, tri.a.uv},
        Vertex{Vec3{-1, -1, 1}, 1, 0, tri.b.col, tri.b.uv},
        Vertex{Vec3{-1, -1, 1}, 1, 0, tri.c.col, tri.c.uv}
    };
}

PixelValue Col3ToPixelValue(Col3 color) {
    color.r = std::fmax(0.0f, std::fmin(1.0f, color.r));
    color.g = std::fmax(0.0f, std::fmin(1.0f, color.g));
    color.b = std::fmax(0.0f, std::fmin(1.0f, color.b));
    return PixelValue{
        static_cast<unsigned char>(color.r * 255.0f),
        static_cast<unsigned char>(color.g * 255.0f),
        static_cast<unsigned char>(color.b * 255.0f)
    };
}

Col3 PixelValueToCol3(PixelValue color) {
    return Col3{
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f
    };
}

Vec3 Normalize(Vec3 v) {
    float mag = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (mag == 0.0f) return Vec3{0, 0, 0};
    return Vec3{v.x / mag, v.y / mag, v.z / mag};
}

float Dot3D(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Dot2D(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y;
}

Vec3 Cross3D(Vec3 a, Vec3 b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float Cross2D(Vec3 a, Vec3 b) {
    return a.x * b.y - a.y * b.x;
}

Vec3 Perpendicular2D(Vec3 vec) {
    return Vec3{-vec.y, vec.x, 0};
}

float SignedTriangleArea(Vec3 a, Vec3 b, Vec3 p) {
    Vec3 ap = p - a;
    Vec3 abPerp = Perpendicular2D(b - a);
    return Dot2D(ap, abPerp) / 2.0;
}

bool PointInTriangle(Triangle tri, Vec3 p) {
    float areaABP = SignedTriangleArea(tri.a.pos, tri.b.pos, p);
    float areaBCP = SignedTriangleArea(tri.b.pos, tri.c.pos, p);
    float areaCAP = SignedTriangleArea(tri.c.pos, tri.a.pos, p);

    // Apply a top-left ownership rule so adjacent triangles do not both shade
    // pixels whose centers lie exactly on their shared edge.
    auto isTopLeft = [](Vec3 a, Vec3 b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        return dy > 0.0f || (dy == 0.0f && dx < 0.0f);
    };
    auto edgeIncluded = [&isTopLeft](float area, Vec3 a, Vec3 b) {
        return area > 0.0f || (area == 0.0f && isTopLeft(a, b));
    };

    bool frontFace = edgeIncluded(areaABP, tri.a.pos, tri.b.pos) &&
                     edgeIncluded(areaBCP, tri.b.pos, tri.c.pos) &&
                     edgeIncluded(areaCAP, tri.c.pos, tri.a.pos);
    bool backFace = edgeIncluded(-areaABP, tri.b.pos, tri.a.pos) &&
                    edgeIncluded(-areaBCP, tri.c.pos, tri.b.pos) &&
                    edgeIncluded(-areaCAP, tri.a.pos, tri.c.pos);

    // When culling is OFF, accept both sides
    // When culling is ON, only accept front faces.
    // Screen Y grows downward, so positive area is opposite of GL CCW.
    if (counterClockWiseWindingActive)
        std::swap(frontFace, backFace);

    bool inTri = cullFaceActive ? frontFace : (frontFace || backFace);
    float totalArea = std::abs(areaABP + areaBCP + areaCAP);
    return inTri && totalArea > 0;
}

namespace {

bool computeBarycentric(const Triangle& tri, const Vec3& p,
                        float& w1, float& w2, float& w3) {
    Vec3 a = tri.a.pos;
    Vec3 b = tri.b.pos;
    Vec3 c = tri.c.pos;
    float det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (std::fabs(det) < 1e-9f) return false;
    w1 = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / det;
    w2 = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / det;
    w3 = 1.0f - w1 - w2;
    return true;
}

Col4 sampleTexture(float u, float v) {
    if (!lastAccessedTexture || !lastAccessedTexture->texture2D.textureData) {
        return Col4{1, 1, 1, 1};
    }
    int tw = lastAccessedTexture->texture2D.width;
    int th = lastAccessedTexture->texture2D.height;
    if (tw <= 0 || th <= 0) return Col4{1, 1, 1, 1};

    int tx, ty;
    if (lastAccessedTexture->texture2D.textureWrapS == GL_CLAMP ||
        lastAccessedTexture->texture2D.textureWrapS == GL_CLAMP_TO_EDGE) {
        tx = std::clamp(static_cast<int>(u * (tw - 1)), 0, tw - 1);
    } else {
        int i = static_cast<int>(std::floor(u * tw));
        tx = i % tw;
        if (tx < 0) tx += tw;
    }
    if (lastAccessedTexture->texture2D.textureWrapT == GL_CLAMP ||
        lastAccessedTexture->texture2D.textureWrapT == GL_CLAMP_TO_EDGE) {
        ty = std::clamp(static_cast<int>(v * (th - 1)), 0, th - 1);
    } else {
        int j = static_cast<int>(std::floor(v * th));
        ty = j % th;
        if (ty < 0) ty += th;
    }
    return lastAccessedTexture->texture2D.textureData[ty * tw + tx];
}

} // namespace

FragmentAttrs ShadeFragment(const Triangle& tri, Vec3& p, bool sampleTex) {
    FragmentAttrs out{};
    float w1, w2, w3;
    if (!computeBarycentric(tri, p, w1, w2, w3)) {
        out.color = tri.a.col;
        out.ndcZ = static_cast<float>(tri.a.pos.z);
        out.eyeDist = tri.a.eyeDist;
        p.z = out.ndcZ;
        return out;
    }

    const float invWA = (tri.a.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.a.w);
    const float invWB = (tri.b.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.b.w);
    const float invWC = (tri.c.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.c.w);

    float invW = w1 * invWA + w2 * invWB + w3 * invWC;
    if (invW == 0.0f) invW = 1e-9f;
    const float W = 1.0f / invW;

    auto persp = [&](float a, float b, float c) {
        return (w1 * a * invWA + w2 * b * invWB + w3 * c * invWC) * W;
    };

    out.color = Col4{
        persp(tri.a.col.r, tri.b.col.r, tri.c.col.r),
        persp(tri.a.col.g, tri.b.col.g, tri.c.col.g),
        persp(tri.a.col.b, tri.b.col.b, tri.c.col.b),
        persp(tri.a.col.a, tri.b.col.a, tri.c.col.a)
    };

    // NDC Z is already post-divide — interpolate affinely in screen space.
    out.ndcZ = w1 * static_cast<float>(tri.a.pos.z) +
               w2 * static_cast<float>(tri.b.pos.z) +
               w3 * static_cast<float>(tri.c.pos.z);
    out.eyeDist = persp(tri.a.eyeDist, tri.b.eyeDist, tri.c.eyeDist);
    p.z = out.ndcZ;

    if (sampleTex && texture2dActive && lastAccessedTexture &&
        lastAccessedTexture->texture2D.textureData &&
        lastAccessedTexture->texture2D.width > 0 &&
        lastAccessedTexture->texture2D.height > 0) {
        const float u = persp(static_cast<float>(tri.a.uv.x),
                              static_cast<float>(tri.b.uv.x),
                              static_cast<float>(tri.c.uv.x));
        const float v = persp(static_cast<float>(tri.a.uv.y),
                              static_cast<float>(tri.b.uv.y),
                              static_cast<float>(tri.c.uv.y));
        Col4 tex = sampleTexture(u, v);
        out.color.r *= tex.r;
        out.color.g *= tex.g;
        out.color.b *= tex.b;
        out.color.a *= tex.a;
    }

    return out;
}

Col4 BarycentricColor(Triangle tri, Vec3& p) {
    return ShadeFragment(tri, p, false).color;
}

Col4 BarycentricTexture(Triangle tri, Vec3& p) {
    float w1, w2, w3;
    if (!computeBarycentric(tri, p, w1, w2, w3)) {
        return sampleTexture(static_cast<float>(tri.a.uv.x), static_cast<float>(tri.a.uv.y));
    }
    const float invWA = (tri.a.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.a.w);
    const float invWB = (tri.b.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.b.w);
    const float invWC = (tri.c.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.c.w);
    float invW = w1 * invWA + w2 * invWB + w3 * invWC;
    if (invW == 0.0f) invW = 1e-9f;
    const float W = 1.0f / invW;
    float u = (w1 * static_cast<float>(tri.a.uv.x) * invWA +
               w2 * static_cast<float>(tri.b.uv.x) * invWB +
               w3 * static_cast<float>(tri.c.uv.x) * invWC) * W;
    float v = (w1 * static_cast<float>(tri.a.uv.y) * invWA +
               w2 * static_cast<float>(tri.b.uv.y) * invWB +
               w3 * static_cast<float>(tri.c.uv.y) * invWC) * W;
    p.z = w1 * static_cast<float>(tri.a.pos.z) +
          w2 * static_cast<float>(tri.b.pos.z) +
          w3 * static_cast<float>(tri.c.pos.z);
    return sampleTexture(u, v);
}

bool DetermineBounding(Triangle& tri, int& xMin, int& yMin, int& xMax, int& yMax) {
    xMin = std::min({xMin, int(tri.a.pos.x), int(tri.b.pos.x), int(tri.c.pos.x)});
    yMin = std::min({yMin, int(tri.a.pos.y), int(tri.b.pos.y), int(tri.c.pos.y)});
    xMax = std::max({xMax, int(tri.a.pos.x), int(tri.b.pos.x), int(tri.c.pos.x)});
    yMax = std::max({yMax, int(tri.a.pos.y), int(tri.b.pos.y), int(tri.c.pos.y)});

    if (xMax < 0 || yMax < 0 || xMin >= renderAreaWidth || yMin >= renderAreaHeight) {
        return false;
    }

    xMin = std::max(0, xMin);
    yMin = std::max(0, yMin);
    xMax = std::min(renderAreaWidth - 1, xMax);
    yMax = std::min(renderAreaHeight - 1, yMax);
    return true;
}

Vec3 CalculateNormal(Triangle& tri) {
    Vec3 A = tri.b.pos - tri.a.pos;
    Vec3 B = tri.c.pos - tri.a.pos;
    return Normalize(Vec3{
        A.y * B.z - A.z * B.y,
        A.z * B.x - A.x * B.z,
        A.x * B.y - A.y * B.x
    });
}

float LinearizeDepth(float ndcZ, float near, float far) {
    return (2.0f * near * far) / (far + near - ndcZ * (far - near));
}
