#include "framebuffer.h"

#include "display/x11_display.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

FbDevice g_fb;
bool printInfo = false;
bool g_fbInitialized = false;
bool g_fbAvailable = false;

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

    std::cout << "PixSoftGL: using " << g_fb.devicePath()
              << " (" << g_fb.width() << 'x' << g_fb.height()
              << " @ " << g_fb.bpp() << "bpp)\n";
    return true;
}

} // namespace

bool EnsureRenderBuffers(int width, int height) {
    if (width <= 0 || height <= 0) return false;

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

    // Only clear on first allocation. Resizes must not wipe a rendered frame
    // (glXSwapBuffers used to trigger that and present pure black).
    if (firstAlloc) {
        std::memset(frameBufferColor, 0, static_cast<size_t>(total) * sizeof(PixelValue));
        for (int i = 0; i < total; ++i) {
            frameBufferDepth[i] = 1.0f;
        }
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
