#include "render.h"
#include "global.h"
#include "maths.h"
#include <cassert>
#include <cmath>
#include "sdl.h"

void ClearFramebuffers(GLenum mask) {
    // Clear color
    if ((mask & GL_COLOR_BUFFER_BIT) && frameBufferColor) {
        PrintInfo("GL_COLOR_BUFFER_BIT ");
        for (int i = 0; i < renderAreaTotal; i++) {
            Col3 fb = PixelValueToCol3(frameBufferColor[i]);
            Col3 newColor = lerp(fb,Col3{clearColor.r,clearColor.g,clearColor.b}, clearColor.a);
            frameBufferColor[i] = Col3ToPixelValue(newColor);
        }
    }
    // Clear depth
    if ((mask & GL_DEPTH_BUFFER_BIT) && frameBufferDepth) {
        PrintInfo("GL_DEPTH_BUFFER_BIT ");
        for (int i = 0; i < renderAreaTotal; i++) {
            frameBufferDepth[i] = 1.0f;
        }
    }
}

int drawVisualizer = 0;

// Render Pixel to framebuffer
void RenderPixel(Vec3 screenPos, Col4 color) {
    int vx = int(screenPos.x); // local viewport coords
    int vy = int(screenPos.y);

    int sx = vx + viewportOffsetX; // screen coords
    int sy = vy + viewportOffsetY;

    // Scissor / viewport bounds
    if (scissorTestActive) {
        if (sx < viewportOffsetX || sx >= viewportOffsetX + viewportAreaWidth) return;
        if (sy < viewportOffsetY || sy >= viewportOffsetY + viewportAreaHeight) return;
    }

    // Compute safe index inside framebuffer
    // Only use renderAreaWidth for the GLOBAL full buffer.
    // Offset the pixel correctly:
    int index = sx + sy * renderAreaWidth;
    if (index < 0 || index >= renderAreaTotal) return;


    // If the new pixel is behind the old one, skip
    if (depthTestActive && frameBufferDepth) {
        float oldZ = frameBufferDepth[index];
        float newZ = screenPos.z;

        switch (depthFunction) {
            case GL_LESS:
                if (newZ >= oldZ) return;
                break;
            case GL_LEQUAL:
                if (newZ > oldZ) return;
                break;
            case GL_EQUAL:
                if (newZ != oldZ) return;
                break;
            case GL_GREATER:
                if (newZ <= oldZ) return;
                break;
            case GL_GEQUAL:
                if (newZ < oldZ) return;
                break;
        }
    }


    // Apply fog (if active)
    if (fogActive && screenPos.z > fogStart) {
        float f;
        switch (fogMode) {
            default:
            case GL_LINEAR:
                f = (screenPos.z - fogStart) / (fogEnd - fogStart);
                break;
            case GL_EXP:
                f = exp(-fogDensity * screenPos.z);
                break;
            case GL_EXP2:
                f = exp(-(fogDensity * screenPos.z) * (fogDensity * screenPos.z));
                break;
        }

        f = std::clamp(f, 0.0f, 1.0f);

        color = Col4{
            fogColor.r * (1.0f - f) + color.r * f,
            fogColor.g * (1.0f - f) + color.g * f,
            fogColor.b * (1.0f - f) + color.b * f,
            fogColor.a * (1.0f - f) + color.a * f
        };
    }

    // Write new values to buffers
    if (!frameBufferColor) {
        std::cout << "Missing framebuffer" << std::endl;
        return;
    }
    if (blendActive) {
        // Proper alpha blending
        float srcA = color.a;
        float dstA = 1.0f - color.a;
        
        Col3 fb = PixelValueToCol3(frameBufferColor[index]);
        Col3 newColor = Col3{
            color.r * srcA + fb.r * dstA,
            color.g * srcA + fb.g * dstA,
            color.b * srcA + fb.b * dstA
        };
        frameBufferColor[index] = Col3ToPixelValue(newColor);
    } else {
        // No blending
        frameBufferColor[index] = Col3ToPixelValue(Col3{color.r, color.g, color.b});
    }

    if (!frameBufferDepth || !depthWriteActive) return;
    frameBufferDepth[index] = screenPos.z;

    /*
    if (drawVisualizer % 50 == 0) {
        UpdateScreen();
        drawVisualizer = 0;
    }
    drawVisualizer++;
    */
}

// Render Line to Framebuffer (linear interpolation in screen-space; depth is NDC z)
void RenderLine(Vec3 posA, Col4 colA, Vec3 posB, Col4 colB) {
    float x0 = posA.x, y0 = posA.y;
    float x1 = posB.x, y1 = posB.y;
    Col4 c0 = colA, c1 = colB;

    bool steep = fabs(y1 - y0) > fabs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }

    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(c0, c1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0 ? 0.f : dy / dx;

    for (float x = x0; x <= x1; x += 1.0f) {
        float t = dx == 0 ? 0.0f : (x - x0) / dx;
        float y = y0 + gradient * (x - x0);
        // posA.z and posB.z are expected to be NDC depths (z = clip.z/clip.w)
        float interpolatedZ = lerp(posA.z, posB.z, t);
        Vec3 screenPos = steep ? Vec3{y, x, interpolatedZ}
                               : Vec3{x, y, interpolatedZ};
        Col4 color = lerp(c0, c1, t);
        RenderPixel(screenPos, color);
    }
}

// Render triangle to framebuffer
void RenderTriangle(Triangle rawTri) {
    Triangle tri = ProjectTriangle(rawTri);
    //std::cout << rawTri << "\n";
    // Determine bounding area
    int xMin = renderAreaWidth;
    int yMin = renderAreaHeight;
    int xMax = 0;
    int yMax = 0;

    //if (tri.a.pos.z < 0 || tri.b.pos.z < 0 || tri.c.pos.z < 0) return;

    if (!DetermineBounding(tri, xMin, yMin, xMax, yMax)) return;

    for (int y = yMin; y <= yMax; y++) {
        for (int x = xMin; x <= xMax; x++) {
            // sample at pixel center
            Vec3 point = Vec3{float(x) + 0.5f, float(y) + 0.5f, 0.0f};
            if (PointInTriangle(tri, point)) {
                // BarycentricColor will compute perspective-correct color and also set point.z to interpolated depth
                Col4 color = BarycentricColor(tri, point);
                if (lastAccessedTexture && texture2dActive) {
                    // BarycentricTexture will perform perspective-correct UV sampling
                    Col4 texcol = BarycentricTexture(tri, point);
                    // combine (multiply modulate)
                    color.r *= texcol.r;
                    color.g *= texcol.g;
                    color.b *= texcol.b;
                    color.a *= texcol.a;
                }
                if (lightingActive) {
                    // This is some of the ugliest code ever conceived by man
                    Vec4 eyeA4 = (modelMatrices[modelMatrixPtr] * Vec4{rawTri.a.pos.x, rawTri.a.pos.y, rawTri.a.pos.z, 1});
                    Vec3 eyeA = Vec3{eyeA4.x,eyeA4.y,eyeA4.z};
                    Vec4 eyeB4 = (modelMatrices[modelMatrixPtr] * Vec4{rawTri.b.pos.x, rawTri.b.pos.y, rawTri.b.pos.z, 1});
                    Vec3 eyeB = Vec3{eyeB4.x,eyeB4.y,eyeB4.z};
                    Vec4 eyeC4 = (modelMatrices[modelMatrixPtr] * Vec4{rawTri.c.pos.x, rawTri.c.pos.y, rawTri.c.pos.z, 1});
                    Vec3 eyeC = Vec3{eyeC4.x,eyeC4.y,eyeC4.z};

                    Vec3 normal = Normalize(Cross3D(eyeB - eyeA, eyeC - eyeA));
                    Vec3 lightDir = Normalize(lights[0].pos - eyeA);
                    float brightness = std::max(0.0f, Dot3D(normal, lightDir));

                    color.r *= brightness;
                    color.g *= brightness;
                    color.b *= brightness;
                }
                RenderPixel(point, color);
            }
        }
    }
}