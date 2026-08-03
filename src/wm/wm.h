#pragma once

#include "fbdev/fbdev.h"

#include <X11/Xlib.h>
#include <string>
#include <vector>

class PixSoftWM {
public:
    bool init(FbDevice* fb, const char* displayName = nullptr);
    void shutdown();

    bool isRunning() const { return running; }
    int screenWidth() const { return screenW; }
    int screenHeight() const { return screenH; }

    void run();
    bool launchClient(const std::string& command, const std::string& libPath);

private:
    void handleEvent(XEvent& ev);
    void mapClientFullscreen(Window win);
    void unmanageClient(Window win);

    FbDevice* fbDevice = nullptr;
    Display* dpy = nullptr;
    Window root = 0;
    int screenW = 0;
    int screenH = 0;
    bool running = false;

    Atom wmProtocols = 0;
    Atom wmDeleteWindow = 0;
    Atom netWmState = 0;
    Atom netWmStateFullscreen = 0;

    std::vector<Window> clients;
};

int selectVideoMode(FbDevice& fb);
