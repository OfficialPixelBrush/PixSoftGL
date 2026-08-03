#include "fbdev.h"

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

bool FbDevice::present(const uint8_t* rgba, int srcWidth, int srcHeight) {
    if (!rgba) return false;

    void* mem = fbMem ? fbMem : mapFramebuffer();
    if (!mem) return false;

    const int dstW = screenWidth;
    const int dstH = screenHeight;
    const int copyW = srcWidth < dstW ? srcWidth : dstW;
    const int copyH = srcHeight < dstH ? srcHeight : dstH;

    auto* dst = static_cast<uint8_t*>(mem);

    if (screenBpp == 32 || screenBpp == 24) {
        for (int y = 0; y < copyH; ++y) {
            uint8_t* row = dst + y * lineLength_;
            for (int x = 0; x < copyW; ++x) {
                const int srcIdx = (x + y * srcWidth) * 3;
                uint8_t* px = row + x * (screenBpp / 8);
                px[0] = rgba[srcIdx + 2];
                px[1] = rgba[srcIdx + 1];
                px[2] = rgba[srcIdx + 0];
                if (screenBpp == 32) px[3] = 0;
            }
        }
    } else if (screenBpp == 16) {
        for (int y = 0; y < copyH; ++y) {
            uint16_t* row = reinterpret_cast<uint16_t*>(dst + y * lineLength_);
            for (int x = 0; x < copyW; ++x) {
                const int srcIdx = (x + y * srcWidth) * 3;
                const uint8_t r = rgba[srcIdx + 0];
                const uint8_t g = rgba[srcIdx + 1];
                const uint8_t b = rgba[srcIdx + 2];
                row[x] = static_cast<uint16_t>(
                    ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
        }
    }

    return true;
}
