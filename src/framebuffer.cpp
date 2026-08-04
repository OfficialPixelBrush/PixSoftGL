#include "framebuffer.h"

#include "display/x11_display.h"

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

bool envFlag(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

int envPositiveInt(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v) return 0;
    const int n = std::atoi(v);
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
    // Default: use fbdev only when no X11 window is bound.
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

    // Optional mode set for bare-metal S3: PIXSOFTGL_FB_MODE=640x480x16
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

void fillDepthOnes(float* depth, int total) {
    auto* words = reinterpret_cast<uint32_t*>(depth);
    const uint32_t one = 0x3f800000u;
    int i = 0;
    for (; i + 7 < total; i += 8) {
        words[i] = one; words[i + 1] = one; words[i + 2] = one; words[i + 3] = one;
        words[i + 4] = one; words[i + 5] = one; words[i + 6] = one; words[i + 7] = one;
    }
    for (; i < total; ++i) words[i] = one;
}

} // namespace

bool EnsureRenderBuffers(int width, int height) {
    if (width <= 0 || height <= 0) return false;

    const int maxW = envPositiveInt("PIXSOFTGL_MAX_W");
    const int maxH = envPositiveInt("PIXSOFTGL_MAX_H");
    if (maxW > 0 && width > maxW) width = maxW;
    if (maxH > 0 && height > maxH) height = maxH;

    // When presenting only to fbdev, never allocate a larger soft buffer than the panel.
    if (wantFbdevPresent() && ensureFramebuffer()) {
        if (g_fb.width() > 0 && width > g_fb.width()) width = g_fb.width();
        if (g_fb.height() > 0 && height > g_fb.height()) height = g_fb.height();
    }

    const int total = width * height;
    if (frameBufferColor &&
        renderAreaWidth == width &&
        renderAreaHeight == height) {
        return true;
    }

    const bool firstAlloc = (frameBufferColor == nullptr);
    auto* color = static_cast<PixelValue*>(std::realloc(
        frameBufferColor, static_cast<size_t>(total) * sizeof(PixelValue)));
    auto* depth = static_cast<float*>(std::realloc(
        frameBufferDepth, static_cast<size_t>(total) * sizeof(float)));

    if (!color || !depth) {
        std::free(color);
        std::free(depth);
        std::cerr << "PixSoftGL: failed to allocate render buffers "
                  << width << 'x' << height << '\n';
        return false;
    }

    frameBufferColor = color;
    frameBufferDepth = depth;
    renderAreaWidth = width;
    renderAreaHeight = height;
    renderAreaTotal = total;

    // Match viewport to the drawable unless the app already set one that fits.
    if (viewportAreaWidth <= 0 || viewportAreaHeight <= 0 ||
        viewportOffsetX + viewportAreaWidth > width ||
        viewportOffsetY + viewportAreaHeight > height) {
        viewportOffsetX = 0;
        viewportOffsetY = 0;
        viewportAreaWidth = width;
        viewportAreaHeight = height;
        viewportAreaTotal = total;
        glViewportX = 0;
        glViewportY = 0;
        glViewportW = width;
        glViewportH = height;
        scissorX = 0;
        scissorY = 0;
        scissorWidth = width;
        scissorHeight = height;
    }

    // Only clear on first allocation. Resizing from SwapBuffers happens after
    // the app has already drawn — wiping here would present black.
    if (firstAlloc) {
        std::memset(frameBufferColor, 0, static_cast<size_t>(total) * sizeof(PixelValue));
        fillDepthOnes(frameBufferDepth, total);
    }
    return true;
}

bool ReCreateWindow() {
    if (X11DisplayGetContext()) {
        return EnsureRenderBuffers(renderAreaWidth, renderAreaHeight);
    }
    // fbdev is optional; soft-fail so X11-only setups still work.
    ensureFramebuffer();
    return frameBufferColor != nullptr || EnsureRenderBuffers(renderAreaWidth, renderAreaHeight);
}

FbDevice* GetFramebufferDevice() {
    return ensureFramebuffer() ? &g_fb : nullptr;
}

void UpdateScreen() {
    if (!frameBufferColor) return;

    X11DisplayContext* x11 = X11DisplayGetContext();
    const char* mode = presentModeOverride();
    const bool forceFbdev = mode && std::strcmp(mode, "fbdev") == 0;
    const bool forceX11 = mode && std::strcmp(mode, "x11") == 0;

    bool presented = false;
    if (x11 && !forceFbdev) {
        presented = X11DisplayPresent(frameBufferColor, renderAreaWidth, renderAreaHeight);
    }

    if ((!presented || forceFbdev) && !forceX11 && wantFbdevPresent()) {
        FbDevice* fb = GetFramebufferDevice();
        if (fb) {
            presented = fb->present(frameBufferColor, renderAreaWidth, renderAreaHeight) || presented;
        }
    }

    // If X11 was preferred but failed, fall back to fbdev once.
    if (!presented && x11 && !forceX11) {
        FbDevice* fb = GetFramebufferDevice();
        if (fb) {
            fb->present(frameBufferColor, renderAreaWidth, renderAreaHeight);
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
