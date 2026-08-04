#include "wm.h"

#include "display/x11_display.h"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>

namespace {

PixSoftWM* g_wm = nullptr;

void onSignal(int) {
    if (g_wm) g_wm->shutdown();
}

} // namespace

int selectVideoMode(FbDevice& fb) {
    const auto modes = fb.enumerateModes();
    if (modes.empty()) {
        std::cerr << "PixSoftGL: no framebuffer modes available\n";
        return -1;
    }

    std::cout << "\nPixSoftGL video mode selection\n";
    std::cout << "Framebuffer: " << fb.devicePath() << '\n';
    std::cout << "Adapter: " << fb.cardName();
    if (fb.isS3()) std::cout << " (S3)";
    std::cout << "\n\n";

    for (size_t i = 0; i < modes.size(); ++i) {
        std::cout << "  [" << i << "] " << modes[i].label << '\n';
    }
    std::cout << "\nSelect mode: ";

    int choice = 0;
    if (!(std::cin >> choice) || choice < 0 || static_cast<size_t>(choice) >= modes.size()) {
        std::cerr << "Invalid selection.\n";
        return -1;
    }

    if (!fb.setMode(modes[static_cast<size_t>(choice)])) {
        std::cerr << "Failed to apply selected mode.\n";
        return -1;
    }

    std::cout << "Applied " << modes[static_cast<size_t>(choice)].label << '\n';
    return choice;
}

void PixSoftWM::publishWmProperty() {
    pixsoftWmAtom = XInternAtom(dpy, PIXSOFTGL_WM_ATOM, False);
    long value = 1;
    XChangeProperty(
        dpy,
        root,
        pixsoftWmAtom,
        XA_CARDINAL,
        32,
        PropModeReplace,
        reinterpret_cast<unsigned char*>(&value),
        1);
    XFlush(dpy);
}

bool PixSoftWM::init(FbDevice* fb, const char* displayName) {
    fbDevice = fb;
    dpy = XOpenDisplay(displayName);
    if (!dpy) {
        std::cerr << "PixSoftGL WM: failed to open X display\n";
        return false;
    }

    const int screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    screenW = fb && fb->isOpen() ? fb->width() : DisplayWidth(dpy, screen);
    screenH = fb && fb->isOpen() ? fb->height() : DisplayHeight(dpy, screen);

    wmProtocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    netWmState = XInternAtom(dpy, "_NET_WM_STATE", False);
    netWmStateFullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

    // Claim the WM selection so clients can recognize us as the window manager.
    // SubstructureRedirect will fail if another WM is already running.
    XSetErrorHandler([](Display*, XErrorEvent* ev) -> int {
        if (ev->error_code == BadAccess) {
            std::cerr << "PixSoftGL WM: another window manager is already running\n";
        }
        return 0;
    });

    XSelectInput(
        dpy,
        root,
        SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    XSync(dpy, False);
    XSetErrorHandler(nullptr);

    publishWmProperty();

    running = true;
    g_wm = this;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    std::cout << "PixSoftGL WM running on " << DisplayString(dpy)
              << " (" << screenW << 'x' << screenH << ")\n";
    std::cout << "Published " << PIXSOFTGL_WM_ATOM
              << " for automatic client detection\n";
    return true;
}

void PixSoftWM::shutdown() {
    running = false;
    if (dpy) {
        if (pixsoftWmAtom != None) {
            XDeleteProperty(dpy, root, pixsoftWmAtom);
        }
        XCloseDisplay(dpy);
        dpy = nullptr;
    }
    if (g_wm == this) g_wm = nullptr;
}

void PixSoftWM::mapClientFullscreen(Window win) {
    XUnmapWindow(dpy, win);
    XMoveResizeWindow(dpy, win, 0, 0, screenW, screenH);

    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.border_pixel = 0;
    XChangeWindowAttributes(dpy, win, CWOverrideRedirect | CWBorderPixel, &attrs);

    XMapRaised(dpy, win);
    XMoveResizeWindow(dpy, win, 0, 0, screenW, screenH);

    // Tell the client its new size (ConfigureNotify).
    XEvent cfg{};
    cfg.type = ConfigureNotify;
    cfg.xconfigure.display = dpy;
    cfg.xconfigure.event = win;
    cfg.xconfigure.window = win;
    cfg.xconfigure.x = 0;
    cfg.xconfigure.y = 0;
    cfg.xconfigure.width = screenW;
    cfg.xconfigure.height = screenH;
    cfg.xconfigure.border_width = 0;
    cfg.xconfigure.above = None;
    cfg.xconfigure.override_redirect = True;
    XSendEvent(dpy, win, False, StructureNotifyMask, &cfg);

    if (netWmState != None && netWmStateFullscreen != None) {
        XEvent ev{};
        ev.type = ClientMessage;
        ev.xclient.window = win;
        ev.xclient.message_type = netWmState;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1;
        ev.xclient.data.l[1] = static_cast<long>(netWmStateFullscreen);
        ev.xclient.data.l[2] = 0;
        XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    }

    XSetInputFocus(dpy, win, RevertToPointerRoot, CurrentTime);
    XFlush(dpy);

    if (std::find(clients.begin(), clients.end(), win) == clients.end()) {
        clients.push_back(win);
    }
}

void PixSoftWM::unmanageClient(Window win) {
    auto it = std::find(clients.begin(), clients.end(), win);
    if (it != clients.end()) clients.erase(it);
    XKillClient(dpy, win);
}

void PixSoftWM::handleEvent(XEvent& ev) {
    switch (ev.type) {
    case MapRequest:
        mapClientFullscreen(ev.xmaprequest.window);
        break;

    case ConfigureRequest: {
        XWindowChanges changes{};
        changes.x = 0;
        changes.y = 0;
        changes.width = screenW;
        changes.height = screenH;
        changes.border_width = 0;
        changes.sibling = ev.xconfigurerequest.above;
        changes.stack_mode = ev.xconfigurerequest.detail;
        XConfigureWindow(
            dpy,
            ev.xconfigurerequest.window,
            CWX | CWY | CWWidth | CWHeight | CWBorderWidth,
            &changes);
        break;
    }

    case DestroyNotify: {
        auto it = std::find(clients.begin(), clients.end(), ev.xdestroywindow.window);
        if (it != clients.end()) clients.erase(it);
        break;
    }

    case UnmapNotify:
        // Leave unmanaged unmapped windows alone; MapRequest remaps them.
        break;

    case ClientMessage:
        if (static_cast<Atom>(ev.xclient.data.l[0]) == wmDeleteWindow) {
            unmanageClient(ev.xclient.window);
        }
        break;

    case KeyPress:
        if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_Escape) &&
            (ev.xkey.state & ControlMask)) {
            running = false;
        }
        break;

    default:
        break;
    }
}

void PixSoftWM::run() {
    while (running) {
        XEvent ev{};
        XNextEvent(dpy, &ev);
        handleEvent(ev);
    }
}

bool PixSoftWM::launchClient(const std::string& command, const std::string& libPath) {
    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        setenv("LD_PRELOAD", libPath.c_str(), 1);
        setenv("PIXSOFTGL_WM", "1", 1);
        setenv("DISPLAY", DisplayString(dpy), 1);
        // Prefer X11 present into the fullscreen client window.
        setenv("PIXSOFTGL_PRESENT", "x11", 0);

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        std::cerr << "PixSoftGL: failed to exec client: " << std::strerror(errno) << '\n';
        _exit(127);
    }

    return true;
}
