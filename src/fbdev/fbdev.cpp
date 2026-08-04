#include "fbdev.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/fb.h>

namespace {

constexpr uint16_t S3_VENDOR_ID = 0x5333;

std::string readFileLine(const std::string& path) {
    std::ifstream in(path);
    std::string line;
    if (in >> line) return line;
    return {};
}

bool parseHexId(const std::string& text, uint16_t& out) {
    if (text.empty()) return false;
    try {
        out = static_cast<uint16_t>(std::stoul(text, nullptr, 16));
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

std::vector<VideoMode> s3KnownModes() {
    return {
        {640, 480, 16, "640x480 @ 16bpp"},
        {640, 480, 24, "640x480 @ 24bpp"},
        {800, 600, 16, "800x600 @ 16bpp"},
        {800, 600, 24, "800x600 @ 24bpp"},
        {1024, 768, 16, "1024x768 @ 16bpp"},
        {1024, 768, 24, "1024x768 @ 24bpp"},
        {1280, 1024, 16, "1280x1024 @ 16bpp"},
    };
}

FbDevice::FbDevice() = default;

FbDevice::~FbDevice() {
    close();
}

bool FbDevice::open(const char* device) {
    close();

    path = device;
    fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0) {
        std::cerr << "PixSoftGL: failed to open " << path << ": " << std::strerror(errno) << '\n';
        return false;
    }

    detectS3();
    return queryScreenInfo();
}

void FbDevice::close() {
    unmapFramebuffer();
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

bool FbDevice::detectS3() {
    s3Detected = false;
    cardName_.clear();

    const std::string vendor = readFileLine("/sys/class/graphics/fb0/device/vendor");
    uint16_t vendorId = 0;
    if (parseHexId(vendor, vendorId) && vendorId == S3_VENDOR_ID) {
        s3Detected = true;
    }

    cardName_ = readFileLine("/sys/class/graphics/fb0/device/name");
    if (cardName_.empty()) {
        cardName_ = readFileLine("/sys/class/graphics/fb0/name");
    }
    if (cardName_.empty()) {
        cardName_ = s3Detected ? "S3 Graphics" : "Linux Framebuffer";
    }

    return s3Detected;
}

bool FbDevice::queryScreenInfo() {
    if (fd < 0) return false;

    fb_var_screeninfo vinfo{};
    fb_fix_screeninfo finfo{};
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        std::cerr << "PixSoftGL: FBIOGET_VSCREENINFO failed: " << std::strerror(errno) << '\n';
        return false;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        std::cerr << "PixSoftGL: FBIOGET_FSCREENINFO failed: " << std::strerror(errno) << '\n';
        return false;
    }

    screenWidth = static_cast<int>(vinfo.xres);
    screenHeight = static_cast<int>(vinfo.yres);
    screenBpp = static_cast<int>(vinfo.bits_per_pixel);
    lineLength_ = static_cast<int>(finfo.line_length);
    fbSize = finfo.smem_len;

    red_ = {vinfo.red.offset, vinfo.red.length};
    green_ = {vinfo.green.offset, vinfo.green.length};
    blue_ = {vinfo.blue.offset, vinfo.blue.length};
    transp_ = {vinfo.transp.offset, vinfo.transp.length};
    return true;
}

bool FbDevice::getCurrentMode(VideoMode& out) {
    if (!queryScreenInfo()) return false;
    out.width = screenWidth;
    out.height = screenHeight;
    out.bpp = screenBpp;
    std::ostringstream label;
    label << screenWidth << 'x' << screenHeight << " @ " << screenBpp << "bpp (current)";
    out.label = label.str();
    return true;
}

std::vector<VideoMode> FbDevice::enumerateModes() {
    std::vector<VideoMode> modes;

    VideoMode current{};
    if (getCurrentMode(current)) {
        modes.push_back(current);
    }

    const auto& candidates = s3Detected ? s3KnownModes() : s3KnownModes();
    for (const auto& mode : candidates) {
        bool duplicate = false;
        for (const auto& existing : modes) {
            if (existing.width == mode.width &&
                existing.height == mode.height &&
                existing.bpp == mode.bpp) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) modes.push_back(mode);
    }

    return modes;
}

bool FbDevice::trySetVarMode(int width, int height, int bpp) {
    fb_var_screeninfo vinfo{};
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) return false;

    vinfo.xres = static_cast<__u32>(width);
    vinfo.yres = static_cast<__u32>(height);
    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres;
    vinfo.bits_per_pixel = static_cast<__u32>(bpp);
    vinfo.xoffset = 0;
    vinfo.yoffset = 0;
    vinfo.activate = FB_ACTIVATE_NOW;

    if (bpp == 16) {
        vinfo.red.offset = 11; vinfo.red.length = 5;
        vinfo.green.offset = 5; vinfo.green.length = 6;
        vinfo.blue.offset = 0; vinfo.blue.length = 5;
        vinfo.transp.offset = 0; vinfo.transp.length = 0;
    } else if (bpp == 24 || bpp == 32) {
        vinfo.red.offset = 16; vinfo.red.length = 8;
        vinfo.green.offset = 8; vinfo.green.length = 8;
        vinfo.blue.offset = 0; vinfo.blue.length = 8;
        vinfo.transp.offset = 0; vinfo.transp.length = 0;
    }

    if (ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo) < 0) {
        return false;
    }

    return queryScreenInfo();
}

bool FbDevice::setMode(const VideoMode& mode) {
    if (fd < 0) return false;
    unmapFramebuffer();
    if (!trySetVarMode(mode.width, mode.height, mode.bpp)) {
        std::cerr << "PixSoftGL: failed to set mode "
                  << mode.width << 'x' << mode.height << '@' << mode.bpp << "bpp\n";
        return false;
    }
    return true;
}

void* FbDevice::mapFramebuffer() {
    if (fd < 0 || fbMem) return fbMem;
    if (fbSize == 0 && !queryScreenInfo()) return nullptr;

    fbMem = mmap(nullptr, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fbMem == MAP_FAILED) {
        fbMem = nullptr;
        std::cerr << "PixSoftGL: mmap framebuffer failed: " << std::strerror(errno) << '\n';
    }
    return fbMem;
}

void FbDevice::unmapFramebuffer() {
    if (fbMem) {
        munmap(fbMem, fbSize);
        fbMem = nullptr;
    }
}

namespace {

uint32_t scaleChannel(uint8_t value, uint32_t bits) {
    if (bits == 0) return 0;
    if (bits >= 8) return static_cast<uint32_t>(value) << (bits - 8);
    const uint32_t maxValue = (1u << bits) - 1u;
    return (static_cast<uint32_t>(value) * maxValue + 127u) / 255u;
}

uint32_t mapChannel(uint8_t value, uint32_t offset, uint32_t length) {
    if (length == 0) return 0;
    return scaleChannel(value, length) << offset;
}

uint32_t mapPixel(const PixelValue& p,
                  const FbDevice::Channel& r,
                  const FbDevice::Channel& g,
                  const FbDevice::Channel& b,
                  const FbDevice::Channel& a) {
    return mapChannel(p.r, r.offset, r.length) |
           mapChannel(p.g, g.offset, g.length) |
           mapChannel(p.b, b.offset, b.length) |
           mapChannel(255, a.offset, a.length);
}

} // namespace

bool FbDevice::present(const PixelValue* pixels, int srcWidth, int srcHeight) {
    if (!pixels || srcWidth <= 0 || srcHeight <= 0) return false;

    void* mem = fbMem ? fbMem : mapFramebuffer();
    if (!mem) return false;

    // Software color buffer is top-left (same as X11 present). Do not flip —
    // an older comment assumed bottom-left and inverted fbdev output.
    const int copyW = std::min(srcWidth, screenWidth);
    const int copyH = std::min(srcHeight, screenHeight);
    auto* dst = static_cast<uint8_t*>(mem);

    if (screenBpp != 8 && screenBpp != 16 &&
        screenBpp != 24 && screenBpp != 32) {
        std::cerr << "PixSoftGL: unsupported framebuffer depth "
                  << screenBpp << "bpp\n";
        return false;
    }

    // True/direct-color fbdev modes describe their layout with bitfields.
    // This avoids assuming RGB565/BGR888/ARGB8888 and handles unusual
    // hardware layouts correctly.
    if (red_.length == 0 || green_.length == 0 || blue_.length == 0) {
        std::cerr << "PixSoftGL: framebuffer has no RGB bitfield mapping\n";
        return false;
    }

    const int bytesPerPixel = (screenBpp + 7) / 8;

    // Fast path: packed 32bpp with contiguous writes.
    if (bytesPerPixel == 4 && copyW > 0) {
        for (int y = 0; y < copyH; ++y) {
            const PixelValue* srcRow = pixels + y * srcWidth;
            auto* row = reinterpret_cast<uint32_t*>(
                dst + static_cast<size_t>(y) * lineLength_);
            for (int x = 0; x < copyW; ++x) {
                row[x] = mapPixel(srcRow[x], red_, green_, blue_, transp_);
            }
        }
        return true;
    }

    if (bytesPerPixel == 2 && copyW > 0) {
        for (int y = 0; y < copyH; ++y) {
            const PixelValue* srcRow = pixels + y * srcWidth;
            auto* row = reinterpret_cast<uint16_t*>(
                dst + static_cast<size_t>(y) * lineLength_);
            for (int x = 0; x < copyW; ++x) {
                row[x] = static_cast<uint16_t>(
                    mapPixel(srcRow[x], red_, green_, blue_, transp_));
            }
        }
        return true;
    }

    for (int y = 0; y < copyH; ++y) {
        uint8_t* row = dst + static_cast<size_t>(y) * lineLength_;

        for (int x = 0; x < copyW; ++x) {
            const PixelValue& p = pixels[x + y * srcWidth];
            const uint32_t packed = mapPixel(p, red_, green_, blue_, transp_);
            std::memcpy(row + static_cast<size_t>(x) * bytesPerPixel,
                        &packed, static_cast<size_t>(bytesPerPixel));
        }
    }

    return true;
}
