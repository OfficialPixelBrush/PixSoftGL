#include "wm.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

void printUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [options] [-- client-command...]\n"
        << "\n"
        << "Bare-bones fullscreen X11 WM for PixSoftGL.\n"
        << "Publishes " << "_PIXSOFTGL_WM"
        << " on the root window so LD_PRELOAD'd libGL.so auto-detects it.\n"
        << "Opens /dev/fb0 when available (S3 / bare metal) and can force\n"
        << "clients to present via framebuffer instead of X11 PutImage.\n"
        << "\n"
        << "Options:\n"
        << "  -d, --display DISPLAY   X display (default: $DISPLAY)\n"
        << "  -f, --fbdev PATH        framebuffer device (default: /dev/fb0)\n"
        << "  -m, --mode INDEX        select video mode non-interactively\n"
        << "      --keep-mode         do not change the current fbdev mode\n"
        << "      --prefer-16         try current resolution @ 16bpp (S3-friendly)\n"
        << "      --present MODE      x11 | fbdev (default: fbdev if /dev/fb0\n"
        << "                          opened, else x11; does not override env)\n"
        << "  -l, --lib PATH          libGL.so to LD_PRELOAD into clients\n"
        << "  -c, --client CMD        launch a client after the WM starts\n"
        << "  -h, --help              show this help\n"
        << "\n"
        << "S3 / framebuffer example:\n"
        << "  " << argv0
        << " -f /dev/fb0 -m 0 --present fbdev \\\n"
        << "      -l ./build/libGL.so.1.0.0 -c './testMinecraft.sh'\n"
        << "\n"
        << "Desktop / WSL example:\n"
        << "  " << argv0
        << " --keep-mode --present x11 -l ./build/libGL.so -c ./tests/testCube\n";
}

} // namespace

int main(int argc, char** argv) {
    const char* displayName = nullptr;
    const char* fbdevPath = "/dev/fb0";
    const char* libPath = "./build/libGL.so";
    const char* presentMode = nullptr; // null → auto
    std::string clientCmd;
    int modeIndex = -1;
    bool keepMode = false;
    bool prefer16 = false;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        auto need = [&](const char* opt) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << opt << '\n';
                std::exit(2);
            }
            return argv[++i];
        };

        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        if (std::strcmp(arg, "-d") == 0 || std::strcmp(arg, "--display") == 0) {
            displayName = need(arg);
        } else if (std::strcmp(arg, "-f") == 0 || std::strcmp(arg, "--fbdev") == 0) {
            fbdevPath = need(arg);
        } else if (std::strcmp(arg, "-m") == 0 || std::strcmp(arg, "--mode") == 0) {
            modeIndex = std::atoi(need(arg));
        } else if (std::strcmp(arg, "--keep-mode") == 0) {
            keepMode = true;
        } else if (std::strcmp(arg, "--prefer-16") == 0) {
            prefer16 = true;
        } else if (std::strcmp(arg, "--present") == 0) {
            presentMode = need(arg);
            if (std::strcmp(presentMode, "x11") != 0 &&
                std::strcmp(presentMode, "fbdev") != 0) {
                std::cerr << "--present must be x11 or fbdev\n";
                return 2;
            }
        } else if (std::strcmp(arg, "-l") == 0 || std::strcmp(arg, "--lib") == 0) {
            libPath = need(arg);
        } else if (std::strcmp(arg, "-c") == 0 || std::strcmp(arg, "--client") == 0) {
            clientCmd = need(arg);
        } else if (std::strcmp(arg, "--") == 0) {
            for (int j = i + 1; j < argc; ++j) {
                if (!clientCmd.empty()) clientCmd.push_back(' ');
                clientCmd += argv[j];
            }
            break;
        } else {
            std::cerr << "Unknown option: " << arg << '\n';
            printUsage(argv[0]);
            return 2;
        }
    }

    FbDevice fb;
    FbDevice* fbPtr = nullptr;
    if (fb.open(fbdevPath)) {
        fbPtr = &fb;
        if (!keepMode) {
            if (modeIndex >= 0) {
                const auto modes = fb.enumerateModes();
                if (modeIndex >= static_cast<int>(modes.size())) {
                    std::cerr << "Mode index out of range\n";
                    return 1;
                }
                if (!fb.setMode(modes[static_cast<size_t>(modeIndex)])) {
                    return 1;
                }
                std::cout << "Applied " << modes[static_cast<size_t>(modeIndex)].label << '\n';
            } else if (isatty(STDIN_FILENO)) {
                if (selectVideoMode(fb) < 0) {
                    std::cerr << "Continuing with current framebuffer mode\n";
                }
            } else {
                std::cout << "Non-interactive session; keeping current fbdev mode "
                          << fb.width() << 'x' << fb.height() << '\n';
            }
        }
        if (prefer16 && fb.bpp() != 16) {
            VideoMode mode{fb.width(), fb.height(), 16, {}};
            if (fb.setMode(mode)) {
                std::cout << "Switched to " << fb.width() << 'x' << fb.height()
                          << " @ 16bpp\n";
            } else {
                std::cerr << "PixSoftGL WM: --prefer-16 failed; keeping "
                          << fb.bpp() << "bpp\n";
            }
        }
        if (fb.isS3()) {
            std::cout << "PixSoftGL WM: S3 adapter detected — prefer 640x480@16 "
                         "and PIXSOFTGL_PRESENT=fbdev for bare metal\n";
        }
    } else {
        std::cerr << "PixSoftGL WM: continuing without fbdev "
                  << "(X11-only fullscreen mode)\n";
    }

    // Auto present: fbdev when we own a real framebuffer (S3 / KMS), else x11.
    std::string present;
    if (presentMode) {
        present = presentMode;
    } else if (const char* existing = std::getenv("PIXSOFTGL_PRESENT")) {
        present = existing;
    } else if (fbPtr) {
        present = "fbdev";
    } else {
        present = "x11";
    }
    setenv("PIXSOFTGL_PRESENT", present.c_str(), 1);
    setenv("PIXSOFTGL_FBDEV", fbdevPath, 0);
    std::cout << "PixSoftGL WM: default client present mode = " << present << '\n';

    PixSoftWM wm;
    if (!wm.init(fbPtr, displayName)) {
        return 1;
    }

    if (!clientCmd.empty()) {
        if (!wm.launchClient(clientCmd, libPath)) {
            std::cerr << "Failed to launch client\n";
            return 1;
        }
        std::cout << "Launched client: " << clientCmd << '\n';
    }

    wm.run();
    wm.shutdown();
    return 0;
}
