#include "render.h"
#include "global.h"
#include "maths.h"
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
            frameBufferDepth[i] = INFINITY;
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
    if (depthTestActive && frameBufferDepth && screenPos.z >= frameBufferDepth[index]) {
        return;
    }

    // Apply fog (if active)
    if (fogActive && screenPos.z > fogStart) {
        Col4 fogCol = Col4{fogColor.r, fogColor.g, fogColor.b, fogColor.a};
        switch(fogMode) {
            case GL_LINEAR:
                float fogFactor = (screenPos.z - fogStart) / (fogEnd - fogStart);
                fogFactor = std::clamp(fogFactor, 0.0f, 1.0f);

                color = Col4{
                    color.r * (1.0f - fogFactor) + fogColor.r * fogFactor,
                    color.g * (1.0f - fogFactor) + fogColor.g * fogFactor,
                    color.b * (1.0f - fogFactor) + fogColor.b * fogFactor,
                    color.a * (1.0f - fogFactor) + fogColor.a * fogFactor
                };
                break;
        }
    }

    // Write new values to buffers
    if (!frameBufferColor) {
        std::cout << "Missing framebuffer" << std::endl;
        return;
    }
    Col3 fb = PixelValueToCol3(frameBufferColor[index]);
    Col3 newColor = lerp(fb,Col3{color.r,color.g,color.b}, color.a);
    frameBufferColor[index] = Col3ToPixelValue(newColor);

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

// Render Line to Framebuffer
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
    float gradient = dx == 0 ? 0 : dy / dx;

    for (float x = x0; x <= x1; x += 1.0f) {
        float t = dx == 0 ? 0.0f : (x - x0) / dx;
        float y = y0 + gradient * (x - x0);
        Vec3 screenPos = steep ? Vec3{y, x, lerp(posA.z, posB.z, t)}
                               : Vec3{x, y, lerp(posA.z, posB.z, t)};
        Col4 color = lerp(c0, c1, t);
        RenderPixel(screenPos, color);
    }
}

// Render triangle to framebuffer
void RenderTriangle(Triangle tri) {
    // Just ignore tris behind our camera
    if (tri.a.pos.z < 0 || tri.b.pos.z < 0 || tri.c.pos.z < 0) return;
    // Determine bounding area
    int xMin = renderAreaWidth;
    int yMin = renderAreaHeight;
    int xMax = 0;
    int yMax = 0;
    if (!DetermineBounding(tri,xMin,yMin,xMax,yMax)) return;
    for (int y = yMin; y <= yMax; y++) {
        for (int x = xMin; x <= xMax; x++) {
            Vec3 point = Vec3{float(x)+0.5, float(y)+0.5, 0.0f};
            if (PointInTriangle(tri, point)) {
                Col4 color = BarycentricColor(tri, point);
                if (lastAccessedTexture) {
                    color = color * BarycentricTexture(tri,point);
                }
                if (lightingActive) {
                    Vec3 normal = CalculateNormal(tri);
                    Vec3 lightDir = Normalize(lights[0].pos - point);
                    float brightness = Dot3D(normal, lightDir);
                    brightness *= 3.0;
                    color.r *= brightness;
                    color.g *= brightness;
                    color.b *= brightness;
                }
                RenderPixel(point, color);
            }
        }
    }
}