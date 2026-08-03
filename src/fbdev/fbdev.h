#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct VideoMode {
    int width;
    int height;
    int bpp;
    std::string label;
};

class FbDevice {
public:
    FbDevice();
    ~FbDevice();

    bool open(const char* device = "/dev/fb0");
    void close();

    bool isOpen() const { return fd >= 0; }
    const char* devicePath() const { return path.c_str(); }

    bool detectS3();
    bool isS3() const { return s3Detected; }
    std::string cardName() const { return cardName_; }

    std::vector<VideoMode> enumerateModes();
    bool getCurrentMode(VideoMode& out);
    bool setMode(const VideoMode& mode);

    int width() const { return screenWidth; }
    int height() const { return screenHeight; }
    int bpp() const { return screenBpp; }
    int lineLength() const { return lineLength_; }

    void* mapFramebuffer();
    void unmapFramebuffer();
    bool isMapped() const { return fbMem != nullptr; }

    bool present(const uint8_t* rgba, int srcWidth, int srcHeight);

private:
    bool queryScreenInfo();
    bool trySetVarMode(int width, int height, int bpp);

    int fd = -1;
    std::string path;
    bool s3Detected = false;
    std::string cardName_;

    int screenWidth = 0;
    int screenHeight = 0;
    int screenBpp = 0;
    int lineLength_ = 0;
    size_t fbSize = 0;

    void* fbMem = nullptr;
};

std::vector<VideoMode> s3KnownModes();
