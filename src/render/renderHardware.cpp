#include "renderHardware.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
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
            for (int i = 0; i < renderAreaTotal; i++) {
                frameBufferColor[i] = fill;
            }
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

void applyFog(Col4& color, float eyeDist) {
    if (!fogActive) return;

    float f = 1.0f;
    switch (fogMode) {
    default:
    case GL_LINEAR:
        if (fogEnd == fogStart) {
            f = (eyeDist >= fogEnd) ? 0.0f : 1.0f;
        } else {
            f = (fogEnd - eyeDist) / (fogEnd - fogStart);
        }
        break;
    case GL_EXP:
        f = std::exp(-fogDensity * eyeDist);
        break;
    case GL_EXP2: {
        const float d = fogDensity * eyeDist;
        f = std::exp(-(d * d));
        break;
    }
    }
    f = std::clamp(f, 0.0f, 1.0f);
    color = Col4{
        fogColor.r * (1.0f - f) + color.r * f,
        fogColor.g * (1.0f - f) + color.g * f,
        fogColor.b * (1.0f - f) + color.b * f,
        fogColor.a * (1.0f - f) + color.a * f
    };
}

struct ScreenVert {
    float x, y, z;
    float r, g, b, a;
    float u, v;
    float invW;
    float eyeDist;
};

struct EdgeEq {
    float a, b, c; // a*x + b*y + c >= 0 inside (or mirrored)
};

inline float orient2d(float ax, float ay, float bx, float by, float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

void rasterizeWindowTriangle(const Triangle& tri, const Triangle* rawForLighting) {
    (void)rawForLighting;

    ScreenVert sv[3] = {
        {
            static_cast<float>(tri.a.pos.x), static_cast<float>(tri.a.pos.y),
            static_cast<float>(tri.a.pos.z),
            tri.a.col.r, tri.a.col.g, tri.a.col.b, tri.a.col.a,
            static_cast<float>(tri.a.uv.x), static_cast<float>(tri.a.uv.y),
            (tri.a.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.a.w),
            tri.a.eyeDist
        },
        {
            static_cast<float>(tri.b.pos.x), static_cast<float>(tri.b.pos.y),
            static_cast<float>(tri.b.pos.z),
            tri.b.col.r, tri.b.col.g, tri.b.col.b, tri.b.col.a,
            static_cast<float>(tri.b.uv.x), static_cast<float>(tri.b.uv.y),
            (tri.b.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.b.w),
            tri.b.eyeDist
        },
        {
            static_cast<float>(tri.c.pos.x), static_cast<float>(tri.c.pos.y),
            static_cast<float>(tri.c.pos.z),
            tri.c.col.r, tri.c.col.g, tri.c.col.b, tri.c.col.a,
            static_cast<float>(tri.c.uv.x), static_cast<float>(tri.c.uv.y),
            (tri.c.w == 0.0) ? 0.0f : static_cast<float>(1.0 / tri.c.w),
            tri.c.eyeDist
        },
    };

    // Area in screen space (Y down). Positive => CW on screen ≈ CCW in GL.
    float area = orient2d(sv[0].x, sv[0].y, sv[1].x, sv[1].y, sv[2].x, sv[2].y);
    if (area == 0.0f) return;

    // Prefer: cull against OpenGL front face after accounting for Y flip.
    bool isFront;
    if (counterClockWiseWindingActive) {
        // GL CCW front → after Y flip, screen area is negative (math CW).
        isFront = area < 0.0f;
    } else {
        isFront = area > 0.0f;
    }
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

    // Also clamp to viewport.
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

    // Pre-multiply attributes by invW for perspective-correct interpolation.
    float r0 = sv[0].r * sv[0].invW, g0 = sv[0].g * sv[0].invW, b0 = sv[0].b * sv[0].invW, a0 = sv[0].a * sv[0].invW;
    float r1 = sv[1].r * sv[1].invW, g1 = sv[1].g * sv[1].invW, b1 = sv[1].b * sv[1].invW, a1 = sv[1].a * sv[1].invW;
    float r2 = sv[2].r * sv[2].invW, g2 = sv[2].g * sv[2].invW, b2 = sv[2].b * sv[2].invW, a2 = sv[2].a * sv[2].invW;
    float u0 = sv[0].u * sv[0].invW, v0 = sv[0].v * sv[0].invW;
    float u1 = sv[1].u * sv[1].invW, v1 = sv[1].v * sv[1].invW;
    float u2 = sv[2].u * sv[2].invW, v2 = sv[2].v * sv[2].invW;
    float e0 = sv[0].eyeDist * sv[0].invW;
    float e1 = sv[1].eyeDist * sv[1].invW;
    float e2 = sv[2].eyeDist * sv[2].invW;

    const bool doTex = texture2dActive && lastAccessedTexture &&
                       lastAccessedTexture->texture2D.textureData &&
                       lastAccessedTexture->texture2D.width > 0;
    const bool doFog = fogActive;
    const bool doBlend = blendActive;
    const bool doDepth = depthTestActive && frameBufferDepth;
    const bool doAlpha = alphaTestActive;

    // Lighting: evaluate once per triangle (flat), not per pixel.
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
        Vec3 lightDir = Normalize(lights[0].pos - eyeA);
        lightScale = std::max(0.0f, Dot3D(normal, lightDir));
    }

    // Edge function deltas for walking +1 in x / +1 in y (screen space).
    // w0 = orient(v1,v2,p), etc.
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

                float invW = bw0 * sv[0].invW + bw1 * sv[1].invW + bw2 * sv[2].invW;
                if (invW == 0.0f) invW = 1e-9f;
                const float W = 1.0f / invW;

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
                    applyFog(color, eye);
                }

                if (!(doAlpha && !alphaTestPasses(color.a))) {
                    const int index = x + rowBase;
                    bool depthOk = true;
                    if (doDepth) {
                        const float oldZ = frameBufferDepth[index];
                        switch (depthFunction) {
                        case GL_NEVER:    depthOk = false; break;
                        case GL_LESS:     depthOk = z < oldZ; break;
                        case GL_EQUAL:    depthOk = z == oldZ; break;
                        case GL_LEQUAL:   depthOk = z <= oldZ; break;
                        case GL_GREATER:  depthOk = z > oldZ; break;
                        case GL_NOTEQUAL: depthOk = z != oldZ; break;
                        case GL_GEQUAL:   depthOk = z >= oldZ; break;
                        case GL_ALWAYS:   break;
                        default:          depthOk = false; break;
                        }
                    }

                    if (depthOk) {
                        if (doBlend) {
                            Col3 fb = PixelValueToCol3(frameBufferColor[index]);
                            const float srcA = color.a;
                            const float dstA = 1.0f;
                            const float sr = blendFactor(blendSrcFactor, srcA, dstA, color.r, fb.r);
                            const float sg = blendFactor(blendSrcFactor, srcA, dstA, color.g, fb.g);
                            const float sb = blendFactor(blendSrcFactor, srcA, dstA, color.b, fb.b);
                            const float dr = blendFactor(blendDstFactor, srcA, dstA, color.r, fb.r);
                            const float dg = blendFactor(blendDstFactor, srcA, dstA, color.g, fb.g);
                            const float db = blendFactor(blendDstFactor, srcA, dstA, color.b, fb.b);
                            frameBufferColor[index] = Col3ToPixelValue(Col3{
                                color.r * sr + fb.r * dr,
                                color.g * sg + fb.g * dg,
                                color.b * sb + fb.b * db
                            });
                        } else {
                            frameBufferColor[index] = Col3ToPixelValue(Col3{color.r, color.g, color.b});
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
        const float newZ = static_cast<float>(screenPos.z);
        switch (depthFunction) {
        case GL_NEVER:    return;
        case GL_LESS:     if (newZ >= oldZ) return; break;
        case GL_EQUAL:    if (newZ != oldZ) return; break;
        case GL_LEQUAL:   if (newZ > oldZ) return; break;
        case GL_GREATER:  if (newZ <= oldZ) return; break;
        case GL_NOTEQUAL: if (newZ == oldZ) return; break;
        case GL_GEQUAL:   if (newZ < oldZ) return; break;
        case GL_ALWAYS:   break;
        default:          return;
        }
    }

    if (!frameBufferColor) return;

    if (blendActive) {
        Col3 fb = PixelValueToCol3(frameBufferColor[index]);
        const float srcA = color.a;
        const float dstA = 1.0f;
        const float sr = blendFactor(blendSrcFactor, srcA, dstA, color.r, fb.r);
        const float sg = blendFactor(blendSrcFactor, srcA, dstA, color.g, fb.g);
        const float sb = blendFactor(blendSrcFactor, srcA, dstA, color.b, fb.b);
        const float dr = blendFactor(blendDstFactor, srcA, dstA, color.r, fb.r);
        const float dg = blendFactor(blendDstFactor, srcA, dstA, color.g, fb.g);
        const float db = blendFactor(blendDstFactor, srcA, dstA, color.b, fb.b);
        frameBufferColor[index] = Col3ToPixelValue(Col3{
            color.r * sr + fb.r * dr,
            color.g * sg + fb.g * dg,
            color.b * sb + fb.b * db
        });
    } else {
        frameBufferColor[index] = Col3ToPixelValue(Col3{color.r, color.g, color.b});
    }

    if (frameBufferDepth && depthWriteActive) {
        frameBufferDepth[index] = static_cast<float>(screenPos.z);
    }
}

void RenderLine(Vec3 posA, Col4 colA, Vec3 posB, Col4 colB) {
    float x0 = static_cast<float>(posA.x), y0 = static_cast<float>(posA.y);
    float x1 = static_cast<float>(posB.x), y1 = static_cast<float>(posB.y);
    Col4 c0 = colA, c1 = colB;
    float z0 = static_cast<float>(posA.z), z1 = static_cast<float>(posB.z);

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
