#include "framebuffer.h"

#include "display/x11_display.h"
#include "pixConfig.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

FbDevice g_fb;
// Logging is extremely expensive on P2-class machines; opt in with PIXSOFTGL_DEBUG=1.
bool printInfo = false;
bool g_fbInitialized = false;
bool g_fbAvailable = false;
bool g_loggedScale = false;

PixelValue* g_presentScratch = nullptr;
int g_presentScratchW = 0;
int g_presentScratchH = 0;

bool envFlag(const char* name) {
    return pix::envFlag(name);
}

int envPositiveInt(const char* name) {
    const int n = pix::envInt(name, 0);
    return n > 0 ? n : 0;
}

struct DebugInit {
    DebugInit() {
        if (envFlag("PIXSOFTGL_DEBUG")) printInfo = true;
    }
} g_debugInit;

const char* presentModeOverride() {
    return std::getenv("PIXSOFTGL_PRESENT");
}

bool wantFbdevPresent() {
    const char* mode = presentModeOverride();
    if (mode && std::strcmp(mode, "x11") == 0) return false;
    if (mode && std::strcmp(mode, "fbdev") == 0) return true;
    return X11DisplayGetContext() == nullptr;
}

bool parseFbMode(const char* text, int& w, int& h, int& bpp) {
    if (!text || !*text) return false;
    int rw = 0, rh = 0, rb = 16;
    if (std::sscanf(text, "%dx%dx%d", &rw, &rh, &rb) >= 2 && rw > 0 && rh > 0) {
        w = rw;
        h = rh;
        bpp = (rb == 15 || rb == 16 || rb == 24 || rb == 32) ? rb : 16;
        return true;
    }
    return false;
}

bool ensureFramebuffer() {
    if (g_fbInitialized) return g_fbAvailable;

    g_fbInitialized = true;
    const char* device = std::getenv("PIXSOFTGL_FBDEV");
    g_fbAvailable = g_fb.open(device && *device ? device : "/dev/fb0");
    if (!g_fbAvailable) {
        std::cerr << "PixSoftGL: Linux framebuffer is unavailable"
                  << " (X11 present still usable)\n";
        return false;
    }

    int mw = 0, mh = 0, mb = 16;
    if (parseFbMode(std::getenv("PIXSOFTGL_FB_MODE"), mw, mh, mb)) {
        VideoMode mode{mw, mh, mb, {}};
        if (!g_fb.setMode(mode)) {
            std::cerr << "PixSoftGL: PIXSOFTGL_FB_MODE failed, keeping current mode\n";
        }
    } else if (envFlag("PIXSOFTGL_PREFER_16") && g_fb.bpp() != 16) {
        VideoMode mode{g_fb.width(), g_fb.height(), 16, {}};
        g_fb.setMode(mode);
    }

    std::cout << "PixSoftGL: using " << g_fb.devicePath()
              << " (" << g_fb.width() << 'x' << g_fb.height()
              << " @ " << g_fb.bpp() << "bpp"
              << (g_fb.isS3() ? ", S3" : "") << ")\n";
    return true;
}

void fillDepthFloatOnes(float* depth, int total) {
    auto* words = reinterpret_cast<uint32_t*>(depth);
    const uint32_t one = 0x3f800000u;
    int i = 0;
    for (; i + 7 < total; i += 8) {
        words[i] = one; words[i + 1] = one; words[i + 2] = one; words[i + 3] = one;
        words[i + 4] = one; words[i + 5] = one; words[i + 6] = one; words[i + 7] = one;
    }
    for (; i < total; ++i) words[i] = one;
}

void fillDepth16Far(uint16_t* depth, int total) {
    // Map float clear 1.0 → far. NDC z≈1 is far under our LESS convention.
    std::memset(depth, 0xff, static_cast<size_t>(total) * sizeof(uint16_t));
}

PixelValue* ensurePresentScratch(int w, int h) {
    if (g_presentScratch && g_presentScratchW == w && g_presentScratchH == h) {
        return g_presentScratch;
    }
    auto* p = static_cast<PixelValue*>(std::realloc(
        g_presentScratch, static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(PixelValue)));
    if (!p) return nullptr;
    g_presentScratch = p;
    g_presentScratchW = w;
    g_presentScratchH = h;
    return g_presentScratch;
}

// Doom/Quake-style nearest upscale (integer scale factor fast path).
void nearestUpscale(const PixelValue* src, int sw, int sh,
                    PixelValue* dst, int dw, int dh) {
    if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

    if (dw == sw && dh == sh) {
        std::memcpy(dst, src, static_cast<size_t>(sw) * sh * sizeof(PixelValue));
        return;
    }

    // Exact integer scale (SCALE=2 → 2x2 pixel blocks).
    if (dw % sw == 0 && dh % sh == 0) {
        const int sx = dw / sw;
        const int sy = dh / sh;
        if (sx == 2 && sy == 2) {
            for (int y = 0; y < sh; ++y) {
                const PixelValue* srow = src + y * sw;
                PixelValue* d0 = dst + (y * 2) * dw;
                PixelValue* d1 = d0 + dw;
                for (int x = 0; x < sw; ++x) {
                    const PixelValue p = srow[x];
                    d0[x * 2] = p; d0[x * 2 + 1] = p;
                    d1[x * 2] = p; d1[x * 2 + 1] = p;
                }
            }
            return;
        }
        if (sx == 3 && sy == 3) {
            for (int y = 0; y < sh; ++y) {
                const PixelValue* srow = src + y * sw;
                for (int yy = 0; yy < 3; ++yy) {
                    PixelValue* drow = dst + (y * 3 + yy) * dw;
                    for (int x = 0; x < sw; ++x) {
                        const PixelValue p = srow[x];
                        drow[x * 3] = p; drow[x * 3 + 1] = p; drow[x * 3 + 2] = p;
                    }
                }
            }
            return;
        }
        if (sx == 4 && sy == 4) {
            for (int y = 0; y < sh; ++y) {
                const PixelValue* srow = src + y * sw;
                for (int yy = 0; yy < 4; ++yy) {
                    PixelValue* drow = dst + (y * 4 + yy) * dw;
                    for (int x = 0; x < sw; ++x) {
                        const PixelValue p = srow[x];
                        const int xo = x * 4;
                        drow[xo] = p; drow[xo + 1] = p; drow[xo + 2] = p; drow[xo + 3] = p;
                    }
                }
            }
            return;
        }
    }

    // General nearest-neighbor (arbitrary drawable vs internal size).
    for (int y = 0; y < dh; ++y) {
        const int sy = y * sh / dh;
        const PixelValue* srow = src + sy * sw;
        PixelValue* drow = dst + y * dw;
        for (int x = 0; x < dw; ++x) {
            drow[x] = srow[x * sw / dw];
        }
    }
}

} // namespace

bool EnsureRenderBuffers(int width, int height) {
    if (width <= 0 || height <= 0) return false;

    const int maxW = envPositiveInt("PIXSOFTGL_MAX_W");
    const int maxH = envPositiveInt("PIXSOFTGL_MAX_H");
    if (maxW > 0 && width > maxW) width = maxW;
    if (maxH > 0 && height > maxH) height = maxH;

    if (wantFbdevPresent() && ensureFramebuffer()) {
        if (g_fb.width() > 0 && width > g_fb.width()) width = g_fb.width();
        if (g_fb.height() > 0 && height > g_fb.height()) height = g_fb.height();
    }

    // Drawable / present size (what the window or panel actually is).
    presentWidth = width;
    presentHeight = height;

    const int scale = pix::renderScale();
    int rw = width / scale;
    int rh = height / scale;
    if (rw < 1) rw = 1;
    if (rh < 1) rh = 1;

    if (!g_loggedScale && (scale > 1 || pix::fixedRaster() || pix::depth16() || pix::fastPreset())) {
        g_loggedScale = true;
        std::cout << "PixSoftGL: internal " << rw << 'x' << rh
                  << " → present " << width << 'x' << height
                  << " (scale=" << scale
                  << (pix::fixedRaster() ? " fixed" : "")
                  << (pix::depth16() ? " depth16" : "")
                  << (pix::forceAffine() ? " affine" : "")
                  << (pix::fastPreset() ? " FAST" : "")
                  << ")\n";
    }

    const int total = rw * rh;
    const bool wantDepth16 = pix::depth16();

    if (frameBufferColor &&
        renderAreaWidth == rw &&
        renderAreaHeight == rh &&
        (wantDepth16 ? frameBufferDepth16 != nullptr : frameBufferDepth != nullptr)) {
        return true;
    }

    const bool firstAlloc = (frameBufferColor == nullptr);
    auto* color = static_cast<PixelValue*>(std::realloc(
        frameBufferColor, static_cast<size_t>(total) * sizeof(PixelValue)));

    float* depthF = frameBufferDepth;
    uint16_t* depth16 = frameBufferDepth16;

    if (wantDepth16) {
        if (depthF) {
            std::free(depthF);
            depthF = nullptr;
            frameBufferDepth = nullptr;
        }
        depth16 = static_cast<uint16_t*>(std::realloc(
            depth16, static_cast<size_t>(total) * sizeof(uint16_t)));
    } else {
        if (depth16) {
            std::free(depth16);
            depth16 = nullptr;
            frameBufferDepth16 = nullptr;
        }
        depthF = static_cast<float*>(std::realloc(
            depthF, static_cast<size_t>(total) * sizeof(float)));
    }

    if (!color || (wantDepth16 ? !depth16 : !depthF)) {
        std::free(color);
        std::free(depthF);
        std::free(depth16);
        std::cerr << "PixSoftGL: failed to allocate render buffers "
                  << rw << 'x' << rh << '\n';
        return false;
    }

    frameBufferColor = color;
    frameBufferDepth = depthF;
    frameBufferDepth16 = depth16;
    renderAreaWidth = rw;
    renderAreaHeight = rh;
    renderAreaTotal = total;

    // Internal viewport covers the soft buffer unless the app set a tighter one.
    if (viewportAreaWidth <= 0 || viewportAreaHeight <= 0 ||
        viewportOffsetX + viewportAreaWidth > rw ||
        viewportOffsetY + viewportAreaHeight > rh) {
        viewportOffsetX = 0;
        viewportOffsetY = 0;
        viewportAreaWidth = rw;
        viewportAreaHeight = rh;
        viewportAreaTotal = total;
        // glViewport* stays in drawable space for GetIntegerv.
        glViewportX = 0;
        glViewportY = 0;
        glViewportW = width;
        glViewportH = height;
        scissorX = 0;
        scissorY = 0;
        scissorWidth = rw;
        scissorHeight = rh;
    }

    if (firstAlloc) {
        std::memset(frameBufferColor, 0, static_cast<size_t>(total) * sizeof(PixelValue));
        if (wantDepth16) fillDepth16Far(frameBufferDepth16, total);
        else fillDepthFloatOnes(frameBufferDepth, total);
    }
    return true;
}

bool ReCreateWindow() {
    if (X11DisplayGetContext()) {
        const int w = presentWidth > 0 ? presentWidth : renderAreaWidth * pix::renderScale();
        const int h = presentHeight > 0 ? presentHeight : renderAreaHeight * pix::renderScale();
        return EnsureRenderBuffers(w, h);
    }
    ensureFramebuffer();
    return frameBufferColor != nullptr ||
           EnsureRenderBuffers(presentWidth > 0 ? presentWidth : DEFAULT_RENDER_AREA_WIDTH,
                               presentHeight > 0 ? presentHeight : DEFAULT_RENDER_AREA_HEIGHT);
}

FbDevice* GetFramebufferDevice() {
    return ensureFramebuffer() ? &g_fb : nullptr;
}

void UpdateScreen() {
    if (!frameBufferColor) return;

    const int sw = renderAreaWidth;
    const int sh = renderAreaHeight;
    int dw = presentWidth > 0 ? presentWidth : sw;
    int dh = presentHeight > 0 ? presentHeight : sh;

    const PixelValue* src = frameBufferColor;
    int presentW = sw;
    int presentH = sh;

    if (dw != sw || dh != sh) {
        PixelValue* scratch = ensurePresentScratch(dw, dh);
        if (scratch) {
            nearestUpscale(frameBufferColor, sw, sh, scratch, dw, dh);
            src = scratch;
            presentW = dw;
            presentH = dh;
        }
    }

    X11DisplayContext* x11 = X11DisplayGetContext();
    const char* mode = presentModeOverride();
    const bool forceFbdev = mode && std::strcmp(mode, "fbdev") == 0;
    const bool forceX11 = mode && std::strcmp(mode, "x11") == 0;

    bool presented = false;
    if (x11 && !forceFbdev) {
        presented = X11DisplayPresent(src, presentW, presentH);
    }

    if ((!presented || forceFbdev) && !forceX11 && wantFbdevPresent()) {
        FbDevice* fb = GetFramebufferDevice();
        if (fb) {
            presented = fb->present(src, presentW, presentH) || presented;
        }
    }

    if (!presented && x11 && !forceX11) {
        FbDevice* fb = GetFramebufferDevice();
        if (fb) {
            fb->present(src, presentW, presentH);
        }
    }
}

void PrintInfoAddr(void* s) {
    if (printInfo) std::cout << std::hex << s << std::dec;
}

void PrintInfoHex(int s) {
    if (printInfo) std::cout << std::hex << s << std::dec;
}

void PrintInfo(int s) {
    if (printInfo) std::cout << std::dec << s;
}

void PrintInfo(const std::string& s) {
    if (printInfo) std::cout << s;
}
