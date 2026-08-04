#include "renderHardware.h"
#include <cstring>
#include <iostream>

void ClearFramebuffers(GLenum mask) {
    if ((mask & GL_COLOR_BUFFER_BIT) && frameBufferColor) {
        PrintInfo("GL_COLOR_BUFFER_BIT ");
        const PixelValue fill = Col3ToPixelValue(
            Col3{clearColor.r, clearColor.g, clearColor.b});
        // Fast path for common black/solid clears.
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
        // 1.0f bit pattern isn't memset-friendly on all platforms; fill explicitly.
        for (int i = 0; i < renderAreaTotal; i++) {
            frameBufferDepth[i] = 1.0f;
        }
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
            // OpenGL: f = (end - c) / (end - start)
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

void rasterizeWindowTriangle(const Triangle& tri, const Triangle* rawForLighting) {
    int xMin = renderAreaWidth;
    int yMin = renderAreaHeight;
    int xMax = 0;
    int yMax = 0;
    Triangle mutableTri = tri;
    if (!DetermineBounding(mutableTri, xMin, yMin, xMax, yMax)) return;

    for (int y = yMin; y <= yMax; y++) {
        for (int x = xMin; x <= xMax; x++) {
            Vec3 point = Vec3{float(x) + 0.5f, float(y) + 0.5f, 0.0f};
            if (!PointInTriangle(mutableTri, point)) continue;

            FragmentAttrs frag = ShadeFragment(mutableTri, point, true);

            if (lightingActive && rawForLighting) {
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
                float brightness = std::max(0.0f, Dot3D(normal, lightDir));
                frag.color.r *= brightness;
                frag.color.g *= brightness;
                frag.color.b *= brightness;
            }

            applyFog(frag.color, frag.eyeDist);
            point.z = frag.ndcZ;
            RenderPixel(point, frag.color);
        }
    }
}

} // namespace

void RenderPixel(Vec3 screenPos, Col4 color) {
    // screenPos is already in window coordinates (viewport applied in projection).
    const int sx = static_cast<int>(screenPos.x);
    const int sy = static_cast<int>(screenPos.y);

    if (sx < 0 || sy < 0 || sx >= renderAreaWidth || sy >= renderAreaHeight) return;

    // Always clip to the viewport; additionally honor GL_SCISSOR_TEST box.
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
        const float dstA = 1.0f; // we don't store destination alpha
        const float sr = blendFactor(blendSrcFactor, srcA, dstA, color.r, fb.r);
        const float sg = blendFactor(blendSrcFactor, srcA, dstA, color.g, fb.g);
        const float sb = blendFactor(blendSrcFactor, srcA, dstA, color.b, fb.b);
        const float dr = blendFactor(blendDstFactor, srcA, dstA, color.r, fb.r);
        const float dg = blendFactor(blendDstFactor, srcA, dstA, color.g, fb.g);
        const float db = blendFactor(blendDstFactor, srcA, dstA, color.b, fb.b);
        Col3 newColor{
            color.r * sr + fb.r * dr,
            color.g * sg + fb.g * dg,
            color.b * sb + fb.b * db
        };
        frameBufferColor[index] = Col3ToPixelValue(newColor);
    } else {
        frameBufferColor[index] = Col3ToPixelValue(Col3{color.r, color.g, color.b});
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

    bool steep = fabs(y1 - y0) > fabs(x1 - x0);
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
