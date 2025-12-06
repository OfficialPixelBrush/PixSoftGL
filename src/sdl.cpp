#include "sdl.h"
#include "maths.h"

// SDL Stuff
SDL_Window *win;
SDL_Surface *surf;
bool running = true;

bool printInfo = true;
bool pauseForEveryRefresh = false;

void PrintInfo(int s) {
    if (!printInfo) return;
    std::cout << s;
}

void PrintInfo(const std::string& s) {
    if (!printInfo) return;
    std::cout << s;
}

void SDL_KeepAliveAndUpdate() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {  // poll all events
        if (e.type == SDL_EVENT_QUIT) { // window X pressed
            SDL_Quit();
            exit(0);                 // exit your program
        }
    }
    SDL_UpdateWindowSurface(win);
}

void ReCreateWindow() {
    if (!win) {
        SDL_Init(SDL_INIT_VIDEO);
        win = SDL_CreateWindow("PixSoftGL", renderAreaWidth, renderAreaHeight, 0);
        surf = SDL_GetWindowSurface(win); // get the window surface
        return;
    }
    //SDL_SetWindowSize(win, renderAreaWidth, renderAreaHeight);
    //surf = SDL_GetWindowSurface(win); // get the window surface
}

// Write a mapped pixel value into a (locked) surface at x,y.
// surface must be valid and locked if SDL_MUSTLOCK(surface) is true.
static void put_pixel_locked(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    if (!surface) return;
    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h) return;

    auto details = SDL_GetPixelFormatDetails(surface->format);
    int bpp = details->bytes_per_pixel;
    uint8_t *row = (uint8_t*)surface->pixels + y * surface->pitch;
    uint8_t *p = row + x * bpp;

    switch (bpp) {
        case 1:
            *p = (uint8_t)pixel;
            break;
        case 2:
            *(uint16_t*)p = (uint16_t)pixel;
            break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                p[0] = (pixel >> 16) & 0xFF;
                p[1] = (pixel >> 8) & 0xFF;
                p[2] = pixel & 0xFF;
            } else {
                p[0] = pixel & 0xFF;
                p[1] = (pixel >> 8) & 0xFF;
                p[2] = (pixel >> 16) & 0xFF;
            }
            break;
        case 4:
            *(uint32_t*)p = pixel;
            break;
    }
}

// Safe wrapper: maps RGBA to surface format, locks/unlocks if needed, then writes.
void put_pixel(SDL_Surface *surface, int x, int y,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!surface) return;

    // Map color for this surface format
    Uint32 mapped = SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format), nullptr, r, g, b, a);

    bool locked = false;
    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) != 0) return; // failed to lock
        locked = true;
    }

    put_pixel_locked(surface, x, y, mapped);

    if (locked) SDL_UnlockSurface(surface);
}

//int i = 0;

// Draw a Pixel to the screen
void DrawPixel(PixelValue p, int x, int y) {
    //PrintInfo("\e[38;2;" << int(p.r) << ");" << int(p.g) << ");" << int(p.b) << "m" << "█");
    put_pixel(surf, x, y, p.r,p.g,p.b,255);
    /*
    if (i % 50 == 0) {
        SDL_KeepAliveAndUpdate();
    }
    i++;
    */
}

// Draw the framebuffer colors to the SDL Window
void UpdateScreen() {
    if (!frameBufferColor) return;
    //PrintInfo("\033[H");
    for (int y = 0; y < renderAreaHeight; y++) {
        for (int x = 0; x < renderAreaWidth; x++) {
            DrawPixel(frameBufferColor[x + y * renderAreaWidth], x, y);
        }
        //PrintInfo("\n");;
    }
    // Poll to make not crash
    SDL_KeepAliveAndUpdate();
    if (pauseForEveryRefresh) {
        int test;
        std::cin >> test;
    }
}