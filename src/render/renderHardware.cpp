#include "renderHardware.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

void ClearFramebuffers(GLenum mask) {
    if ((mask & GL_COLOR_BUFFER_BIT) && frameBufferColor) {
        PrintInfo("GL_COLOR_BUFFER_BIT ");
        const PixelValue fill = Col3ToPixelValue(
            Col3{clearColor.r, clearColor.g, clearColor.b});
        if (fill.r == fill.g && fill.g == fill.b) {
            std::memset(frameBufferColor, fill.r,
                        static_cast<size_t>(renderAreaTotal) * sizeof(PixelValue));
        } else {
            PixelValue* dst = frameBufferColor;
            const int n = renderAreaTotal;
            int i = 0;
            for (; i + 3 < n; i += 4) {
                dst[i] = fill; dst[i + 1] = fill; dst[i + 2] = fill; dst[i + 3] = fill;
            }
            for (; i < n; ++i) dst[i] = fill;
        }
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) && frameBufferDepth) {
        PrintInfo("GL_DEPTH_BUFFER_BIT ");
        // IEEE-754 1.0f is 0x3f800000 — word-fill (faster than float stores).
        auto* words = reinterpret_cast<uint32_t*>(frameBufferDepth);
        const uint32_t one = 0x3f800000u;
        const int n = renderAreaTotal;
        int i = 0;
        for (; i + 7 < n; i += 8) {
            words[i] = one; words[i + 1] = one; words[i + 2] = one; words[i + 3] = one;
            words[i + 4] = one; words[i + 5] = one; words[i + 6] = one; words[i + 7] = one;
        }
        for (; i < n; ++i) words[i] = one;
    }
}

namespace {

bool envFlagCached(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

bool forceAffineTexturing() {
    static const bool on = envFlagCached("PIXSOFTGL_AFFINE");
    return on;
}

bool alphaTestPasses(float alpha) {
    if (!alphaTestActive) return true;
    switch (alphaFunc) {
    case GL_NEVER:    return false;
    case GL_LESS:     return alpha < alphaRef;
    case GL_EQUAL:    return alpha == alphaRef;
    case GL_LEQUAL:   return alpha <= alphaRef;
    case GL_GREATER:  return alpha > alphaRef;
    case GL_NOTEQUAL: return alpha != alphaRef;
    case GL_GEQUAL:   return alpha >= alphaRef;
    case GL_ALWAYS:
    default:          return true;
    }
}

float blendFactor(GLenum factor, float srcA, float dstA, float channelSrc, float channelDst) {
    switch (factor) {
    case GL_ZERO:                return 0.0f;
    case GL_ONE:                 return 1.0f;
    case GL_SRC_ALPHA:           return srcA;
    case GL_ONE_MINUS_SRC_ALPHA: return 1.0f - srcA;
    case GL_DST_ALPHA:           return dstA;
    case GL_ONE_MINUS_DST_ALPHA: return 1.0f - dstA;
    case GL_SRC_COLOR:           return channelSrc;
    case GL_ONE_MINUS_SRC_COLOR: return 1.0f - channelSrc;
    case GL_DST_COLOR:           return channelDst;
    case GL_ONE_MINUS_DST_COLOR: return 1.0f - channelDst;
    default:                     return srcA;
    }
}

void applyFogLinear(Col4& color, float eyeDist) {
    float f;
    if (fogEnd == fogStart) {
        f = (eyeDist >= fogEnd) ? 0.0f : 1.0f;
    } else {
        f = (fogEnd - eyeDist) / (fogEnd - fogStart);
    }
    if (f <= 0.0f) {
        color.r = fogColor.r;
        color.g = fogColor.g;
        color.b = fogColor.b;
        return;
    }
    if (f >= 1.0f) return;
    const float inv = 1.0f - f;
    color.r = fogColor.r * inv + color.r * f;
    color.g = fogColor.g * inv + color.g * f;
    color.b = fogColor.b * inv + color.b * f;
}

void applyFog(Col4& color, float eyeDist) {
    if (!fogActive) return;
    if (fogMode == GL_LINEAR) {
        applyFogLinear(color, eyeDist);
        return;
    }

    float f = 1.0f;
    if (fogMode == GL_EXP) {
        f = std::exp(-fogDensity * eyeDist);
    } else if (fogMode == GL_EXP2) {
        const float d = fogDensity * eyeDist;
        f = std::exp(-(d * d));
    }
    f = std::clamp(f, 0.0f, 1.0f);
    color = Col4{
        fogColor.r * (1.0f - f) + color.r * f,
        fogColor.g * (1.0f - f) + color.g * f,
        fogColor.b * (1.0f - f) + color.b * f,
        color.a // GL 1.1: fog must not change fragment alpha
    };
}

unsigned char packU8(float c) {
    const int v = static_cast<int>(c * 255.0f + 0.5f);
    if (v <= 0) return 0;
    if (v >= 255) return 255;
    return static_cast<unsigned char>(v);
}

PixelValue packPixel(float r, float g, float b) {
    return PixelValue{packU8(r), packU8(g), packU8(b)};
}

bool depthPasses(float z, float oldZ) {
    switch (depthFunction) {
    case GL_NEVER:    return false;
    case GL_LESS:     return z < oldZ;
    case GL_EQUAL:    return z == oldZ;
    case GL_LEQUAL:   return z <= oldZ;
    case GL_GREATER:  return z > oldZ;
    case GL_NOTEQUAL: return z != oldZ;
    case GL_GEQUAL:   return z >= oldZ;
    case GL_ALWAYS:   return true;
    default:          return false;
    }
}

} // namespace

namespace {

struct ScreenVert {
    float x, y, z;
    float r, g, b, a;
    float u, v;
    float invW;
    float eyeDist;
};

inline float orient2d(float ax, float ay, float bx, float by, float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

void rasterizeWindowTriangle(const Triangle& tri, const Triangle* rawForLighting) {
    (void)rawForLighting;

    ScreenVert sv[3] = {
        {
            tri.a.pos.x, tri.a.pos.y, tri.a.pos.z,
            tri.a.col.r, tri.a.col.g, tri.a.col.b, tri.a.col.a,
            tri.a.uv.x, tri.a.uv.y,
            (tri.a.w == 0.0f) ? 0.0f : (1.0f / tri.a.w),
            tri.a.eyeDist
        },
        {
            tri.b.pos.x, tri.b.pos.y, tri.b.pos.z,
            tri.b.col.r, tri.b.col.g, tri.b.col.b, tri.b.col.a,
            tri.b.uv.x, tri.b.uv.y,
            (tri.b.w == 0.0f) ? 0.0f : (1.0f / tri.b.w),
            tri.b.eyeDist
        },
        {
            tri.c.pos.x, tri.c.pos.y, tri.c.pos.z,
            tri.c.col.r, tri.c.col.g, tri.c.col.b, tri.c.col.a,
            tri.c.uv.x, tri.c.uv.y,
            (tri.c.w == 0.0f) ? 0.0f : (1.0f / tri.c.w),
            tri.c.eyeDist
        },
    };

    // Area in our top-left / Y-down buffer. ProjectPosition already flipped Y,
    // so GL-CCW (front) lands as negative orient2d here.
    float area = orient2d(sv[0].x, sv[0].y, sv[1].x, sv[1].y, sv[2].x, sv[2].y);
    if (area == 0.0f) return;

    bool isFront = counterClockWiseWindingActive ? (area < 0.0f) : (area > 0.0f);
    if (cullFaceActive && !isFront) return;

    // Ensure consistent positive area for barycentrics by swapping if needed.
    if (area < 0.0f) {
        std::swap(sv[1], sv[2]);
        area = -area;
    }
    const float invArea = 1.0f / area;

    int xMin = static_cast<int>(std::floor(std::min({sv[0].x, sv[1].x, sv[2].x})));
    int yMin = static_cast<int>(std::floor(std::min({sv[0].y, sv[1].y, sv[2].y})));
    int xMax = static_cast<int>(std::ceil (std::max({sv[0].x, sv[1].x, sv[2].x})));
    int yMax = static_cast<int>(std::ceil (std::max({sv[0].y, sv[1].y, sv[2].y})));

    xMin = std::max(xMin, 0);
    yMin = std::max(yMin, 0);
    xMax = std::min(xMax, renderAreaWidth - 1);
    yMax = std::min(yMax, renderAreaHeight - 1);
    if (xMin > xMax || yMin > yMax) return;

    xMin = std::max(xMin, viewportOffsetX);
    yMin = std::max(yMin, viewportOffsetY);
    xMax = std::min(xMax, viewportOffsetX + viewportAreaWidth - 1);
    yMax = std::min(yMax, viewportOffsetY + viewportAreaHeight - 1);
    if (scissorTestActive) {
        xMin = std::max(xMin, scissorX);
        yMin = std::max(yMin, scissorY);
        xMax = std::min(xMax, scissorX + scissorWidth - 1);
        yMax = std::min(yMax, scissorY + scissorHeight - 1);
    }
    if (xMin > xMax || yMin > yMax) return;

    // Affine when forced, or when 1/w is nearly constant (ortho / UI / distant tris).
    const float maxInv = std::max({std::fabs(sv[0].invW), std::fabs(sv[1].invW),
                                   std::fabs(sv[2].invW), 1e-6f});
    const bool useAffine = forceAffineTexturing() ||
        (std::fabs(sv[0].invW - sv[1].invW) <= 1e-4f * maxInv &&
         std::fabs(sv[0].invW - sv[2].invW) <= 1e-4f * maxInv);

    float r0, g0, b0, a0, r1, g1, b1, a1, r2, g2, b2, a2;
    float u0, v0, u1, v1, u2, v2;
    float e0, e1, e2;
    if (useAffine) {
        r0 = sv[0].r; g0 = sv[0].g; b0 = sv[0].b; a0 = sv[0].a;
        r1 = sv[1].r; g1 = sv[1].g; b1 = sv[1].b; a1 = sv[1].a;
        r2 = sv[2].r; g2 = sv[2].g; b2 = sv[2].b; a2 = sv[2].a;
        u0 = sv[0].u; v0 = sv[0].v;
        u1 = sv[1].u; v1 = sv[1].v;
        u2 = sv[2].u; v2 = sv[2].v;
        e0 = sv[0].eyeDist; e1 = sv[1].eyeDist; e2 = sv[2].eyeDist;
    } else {
        r0 = sv[0].r * sv[0].invW; g0 = sv[0].g * sv[0].invW;
        b0 = sv[0].b * sv[0].invW; a0 = sv[0].a * sv[0].invW;
        r1 = sv[1].r * sv[1].invW; g1 = sv[1].g * sv[1].invW;
        b1 = sv[1].b * sv[1].invW; a1 = sv[1].a * sv[1].invW;
        r2 = sv[2].r * sv[2].invW; g2 = sv[2].g * sv[2].invW;
        b2 = sv[2].b * sv[2].invW; a2 = sv[2].a * sv[2].invW;
        u0 = sv[0].u * sv[0].invW; v0 = sv[0].v * sv[0].invW;
        u1 = sv[1].u * sv[1].invW; v1 = sv[1].v * sv[1].invW;
        u2 = sv[2].u * sv[2].invW; v2 = sv[2].v * sv[2].invW;
        e0 = sv[0].eyeDist * sv[0].invW;
        e1 = sv[1].eyeDist * sv[1].invW;
        e2 = sv[2].eyeDist * sv[2].invW;
    }

    const bool doTex = texture2dActive && resolveBoundTexture() &&
                       lastAccessedTexture->texture2D.textureData &&
                       lastAccessedTexture->texture2D.width > 0;
    const bool doFog = fogActive;
    const bool doBlend = blendActive;
    const bool doDepth = depthTestActive && frameBufferDepth;
    const bool doAlpha = alphaTestActive;
    const bool depthLess = doDepth && depthFunction == GL_LESS;
    const bool depthLequal = doDepth && depthFunction == GL_LEQUAL;
    const bool blendSrcAlpha = doBlend &&
        blendSrcFactor == GL_SRC_ALPHA &&
        blendDstFactor == GL_ONE_MINUS_SRC_ALPHA;
    const bool fogLinear = doFog && fogMode == GL_LINEAR;

    float lightScale = 1.0f;
    if (lightingActive && rawForLighting && lightActive[0]) {
        Vec4 eyeA4 = modelMatrices[modelMatrixPtr] *
            Vec4{rawForLighting->a.pos.x, rawForLighting->a.pos.y, rawForLighting->a.pos.z, 1};
        Vec4 eyeB4 = modelMatrices[modelMatrixPtr] *
            Vec4{rawForLighting->b.pos.x, rawForLighting->b.pos.y, rawForLighting->b.pos.z, 1};
        Vec4 eyeC4 = modelMatrices[modelMatrixPtr] *
            Vec4{rawForLighting->c.pos.x, rawForLighting->c.pos.y, rawForLighting->c.pos.z, 1};
        Vec3 eyeA{eyeA4.x, eyeA4.y, eyeA4.z};
        Vec3 eyeB{eyeB4.x, eyeB4.y, eyeB4.z};
        Vec3 eyeC{eyeC4.x, eyeC4.y, eyeC4.z};
        Vec3 normal = Normalize(Cross3D(eyeB - eyeA, eyeC - eyeA));
        Vec3 lightDir;
        if (std::fabs(lights[0].w) < 1e-6f) {
            lightDir = Normalize(lights[0].pos);
        } else {
            lightDir = Normalize(lights[0].pos - eyeA);
        }
        const float diff = std::max(0.0f, Dot3D(normal, lightDir));
        lightScale = 0.4f + 0.6f * diff;
    }

    const float a01 = sv[1].y - sv[2].y;
    const float b01 = sv[2].x - sv[1].x;
    const float a12 = sv[2].y - sv[0].y;
    const float b12 = sv[0].x - sv[2].x;
    const float a20 = sv[0].y - sv[1].y;
    const float b20 = sv[1].x - sv[0].x;

    const float px0 = static_cast<float>(xMin) + 0.5f;
    const float py0 = static_cast<float>(yMin) + 0.5f;
    float rowW0 = orient2d(sv[1].x, sv[1].y, sv[2].x, sv[2].y, px0, py0);
    float rowW1 = orient2d(sv[2].x, sv[2].y, sv[0].x, sv[0].y, px0, py0);
    float rowW2 = orient2d(sv[0].x, sv[0].y, sv[1].x, sv[1].y, px0, py0);

    for (int y = yMin; y <= yMax; ++y) {
        float w0 = rowW0;
        float w1 = rowW1;
        float w2 = rowW2;
        const int rowBase = y * renderAreaWidth;

        for (int x = xMin; x <= xMax; ++x) {
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                const float bw0 = w0 * invArea;
                const float bw1 = w1 * invArea;
                const float bw2 = w2 * invArea;

                float W = 1.0f;
                if (!useAffine) {
                    float invW = bw0 * sv[0].invW + bw1 * sv[1].invW + bw2 * sv[2].invW;
                    if (invW == 0.0f) invW = 1e-9f;
                    W = 1.0f / invW;
                }

                Col4 color{
                    (bw0 * r0 + bw1 * r1 + bw2 * r2) * W,
                    (bw0 * g0 + bw1 * g1 + bw2 * g2) * W,
                    (bw0 * b0 + bw1 * b1 + bw2 * b2) * W,
                    (bw0 * a0 + bw1 * a1 + bw2 * a2) * W
                };

                const float z = bw0 * sv[0].z + bw1 * sv[1].z + bw2 * sv[2].z;

                if (doTex) {
                    const float u = (bw0 * u0 + bw1 * u1 + bw2 * u2) * W;
                    const float v = (bw0 * v0 + bw1 * v1 + bw2 * v2) * W;
                    Col4 tex = sampleTexture(u, v);
                    if (tex.a <= 0.0f && (tex.r + tex.g + tex.b) > 0.02f) {
                        tex.a = 1.0f;
                    }
                    if (textureEnvMode == GL_REPLACE) {
                        color = tex;
                    } else {
                        color.r *= tex.r;
                        color.g *= tex.g;
                        color.b *= tex.b;
                        color.a *= tex.a;
                    }
                }

                if (lightScale != 1.0f) {
                    color.r *= lightScale;
                    color.g *= lightScale;
                    color.b *= lightScale;
                }

                if (doFog) {
                    const float eye = (bw0 * e0 + bw1 * e1 + bw2 * e2) * W;
                    if (fogLinear) applyFogLinear(color, eye);
                    else applyFog(color, eye);
                }

                if (!(doAlpha && !alphaTestPasses(color.a))) {
                    const int index = x + rowBase;
                    bool depthOk = true;
                    if (depthLess) {
                        depthOk = z < frameBufferDepth[index];
                    } else if (depthLequal) {
                        depthOk = z <= frameBufferDepth[index];
                    } else if (doDepth) {
                        depthOk = depthPasses(z, frameBufferDepth[index]);
                    }

                    if (depthOk) {
                        if (blendSrcAlpha) {
                            Col3 fb = PixelValueToCol3(frameBufferColor[index]);
                            const float sa = color.a;
                            const float da = 1.0f - sa;
                            frameBufferColor[index] = packPixel(
                                color.r * sa + fb.r * da,
                                color.g * sa + fb.g * da,
                                color.b * sa + fb.b * da);
                        } else if (doBlend) {
                            Col3 fb = PixelValueToCol3(frameBufferColor[index]);
                            const float srcA = color.a;
                            const float dstA = 1.0f;
                            const float sr = blendFactor(blendSrcFactor, srcA, dstA, color.r, fb.r);
                            const float sg = blendFactor(blendSrcFactor, srcA, dstA, color.g, fb.g);
                            const float sb = blendFactor(blendSrcFactor, srcA, dstA, color.b, fb.b);
                            const float dr = blendFactor(blendDstFactor, srcA, dstA, color.r, fb.r);
                            const float dg = blendFactor(blendDstFactor, srcA, dstA, color.g, fb.g);
                            const float db = blendFactor(blendDstFactor, srcA, dstA, color.b, fb.b);
                            frameBufferColor[index] = packPixel(
                                color.r * sr + fb.r * dr,
                                color.g * sg + fb.g * dg,
                                color.b * sb + fb.b * db);
                        } else {
                            frameBufferColor[index] = packPixel(color.r, color.g, color.b);
                        }

                        if (frameBufferDepth && depthWriteActive) {
                            frameBufferDepth[index] = z;
                        }
                    }
                }
            }

            w0 += a01;
            w1 += a12;
            w2 += a20;
        }

        rowW0 += b01;
        rowW1 += b12;
        rowW2 += b20;
    }
}

} // namespace

void RenderPixel(Vec3 screenPos, Col4 color) {
    const int sx = static_cast<int>(screenPos.x);
    const int sy = static_cast<int>(screenPos.y);

    if (sx < 0 || sy < 0 || sx >= renderAreaWidth || sy >= renderAreaHeight) return;
    if (sx < viewportOffsetX || sx >= viewportOffsetX + viewportAreaWidth) return;
    if (sy < viewportOffsetY || sy >= viewportOffsetY + viewportAreaHeight) return;

    if (scissorTestActive) {
        if (sx < scissorX || sx >= scissorX + scissorWidth) return;
        if (sy < scissorY || sy >= scissorY + scissorHeight) return;
    }

    if (!alphaTestPasses(color.a)) return;

    const int index = sx + sy * renderAreaWidth;
    if (index < 0 || index >= renderAreaTotal) return;

    if (depthTestActive && frameBufferDepth) {
        const float oldZ = frameBufferDepth[index];
        const float newZ = screenPos.z;
        if (!depthPasses(newZ, oldZ)) return;
    }

    if (!frameBufferColor) return;

    if (blendActive &&
        blendSrcFactor == GL_SRC_ALPHA &&
        blendDstFactor == GL_ONE_MINUS_SRC_ALPHA) {
        Col3 fb = PixelValueToCol3(frameBufferColor[index]);
        const float sa = color.a;
        const float da = 1.0f - sa;
        frameBufferColor[index] = packPixel(
            color.r * sa + fb.r * da,
            color.g * sa + fb.g * da,
            color.b * sa + fb.b * da);
    } else if (blendActive) {
        Col3 fb = PixelValueToCol3(frameBufferColor[index]);
        const float srcA = color.a;
        const float dstA = 1.0f;
        const float sr = blendFactor(blendSrcFactor, srcA, dstA, color.r, fb.r);
        const float sg = blendFactor(blendSrcFactor, srcA, dstA, color.g, fb.g);
        const float sb = blendFactor(blendSrcFactor, srcA, dstA, color.b, fb.b);
        const float dr = blendFactor(blendDstFactor, srcA, dstA, color.r, fb.r);
        const float dg = blendFactor(blendDstFactor, srcA, dstA, color.g, fb.g);
        const float db = blendFactor(blendDstFactor, srcA, dstA, color.b, fb.b);
        frameBufferColor[index] = packPixel(
            color.r * sr + fb.r * dr,
            color.g * sg + fb.g * dg,
            color.b * sb + fb.b * db);
    } else {
        frameBufferColor[index] = packPixel(color.r, color.g, color.b);
    }

    if (frameBufferDepth && depthWriteActive) {
        frameBufferDepth[index] = screenPos.z;
    }
}

void RenderLine(Vec3 posA, Col4 colA, Vec3 posB, Col4 colB) {
    float x0 = posA.x, y0 = posA.y;
    float x1 = posB.x, y1 = posB.y;
    Col4 c0 = colA, c1 = colB;
    float z0 = posA.z, z1 = posB.z;

    bool steep = std::fabs(y1 - y0) > std::fabs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(c0, c1);
        std::swap(z0, z1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0 ? 0.f : dy / dx;

    for (float x = x0; x <= x1; x += 1.0f) {
        float t = dx == 0 ? 0.0f : (x - x0) / dx;
        float y = y0 + gradient * (x - x0);
        float interpolatedZ = lerp(z0, z1, t);
        Vec3 screenPos = steep ? Vec3{y, x, interpolatedZ}
                               : Vec3{x, y, interpolatedZ};
        RenderPixel(screenPos, lerp(c0, c1, t));
    }
}

void RenderTriangle(Triangle rawTri) {
    Triangle clipped[2];
    const int n = ProjectAndClipTriangle(rawTri, clipped);
    for (int i = 0; i < n; ++i) {
        rasterizeWindowTriangle(clipped[i], &rawTri);
    }
}
