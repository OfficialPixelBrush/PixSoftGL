#pragma once

#include "fbdev/fbdev.h"

#include <X11/Xlib.h>
#include <string>
#include <sys/types.h>
#include <vector>

class PixSoftWM {
public:
    // wantX=false skips X11 entirely (pure fbdev/bare-metal, no X server
    // available or desired). Used when the effective present mode is
    // "fbdev" — the WM then only forks/supervises the client, it doesn't
    // do any window management.
    bool init(FbDevice* fb, const char* displayName = nullptr, bool wantX = true);
    void shutdown();

    bool isRunning() const { return running; }
    bool isHeadless() const { return headless; }
    int screenWidth() const { return screenW; }
    int screenHeight() const { return screenH; }
    Display* display() const { return dpy; }

    void run();
    bool launchClient(const std::string& command, const std::string& libPath);

private:
    void publishWmProperty();
    void handleEvent(XEvent& ev);
    void mapClientFullscreen(Window win);
    void unmanageClient(Window win);
    void runHeadless();

    FbDevice* fbDevice = nullptr;
    Display* dpy = nullptr;
    Window root = 0;
    int screenW = 0;
    int screenH = 0;
    bool running = false;
    bool headless = false;
    pid_t clientPid = -1;

    Atom wmProtocols = 0;
    Atom wmDeleteWindow = 0;
    Atom netWmState = 0;
    Atom netWmStateFullscreen = 0;
    Atom pixsoftWmAtom = 0;

    std::vector<Window> clients;
};

int selectVideoMode(FbDevice& fb);
