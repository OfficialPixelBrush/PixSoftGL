#include "framebuffer.h"

#include <cstdlib>
#include <iostream>

namespace {
FbDevice g_fb;
bool printInfo = true;
bool g_fbInitialized = false;

bool ensureFramebuffer() {
    if (g_fbInitialized) return g_fb.isOpen();

    g_fbInitialized = true;
    const char* device = std::getenv("PIXSOFTGL_FBDEV");
    if (!g_fb.open(device && *device ? device : "/dev/fb0")) {
        std::cerr << "PixSoftGL: Linux framebuffer is unavailable\n";
        return false;
    }

    std::cout << "PixSoftGL: using " << g_fb.devicePath()
              << " (" << g_fb.width() << 'x' << g_fb.height()
              << " @ " << g_fb.bpp() << "bpp)\n";
    return true;
}
}

bool ReCreateWindow() {
    return ensureFramebuffer();
}

FbDevice* GetFramebufferDevice() {
    return ensureFramebuffer() ? &g_fb : nullptr;
}

void UpdateScreen() {
    if (!frameBufferColor) return;
    FbDevice* fb = GetFramebufferDevice();
    if (!fb) return;
    fb->present(frameBufferColor, renderAreaWidth, renderAreaHeight);
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
