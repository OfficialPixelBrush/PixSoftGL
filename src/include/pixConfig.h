#pragma once

#include <cstdlib>
#include <cstring>

// Runtime quality / speed knobs (cached after first read).
//
// PIXSOFTGL_SCALE=N       Render at 1/N resolution, nearest-upscale on present
//                         (Doom/Quake-style; N=1..8, default 1)
// PIXSOFTGL_FIXED=1       Integer/fixed-point triangle raster
// PIXSOFTGL_FIXED_BITS=N  Sub-pixel bits for edges (default 4; try 2..8)
// PIXSOFTGL_DEPTH16=1     16-bit depth buffer instead of float
// PIXSOFTGL_AFFINE=1      Force affine texturing
// PIXSOFTGL_NO_FOG=1      Skip fog shading (big fill win)
// PIXSOFTGL_FAST=1        Preset: SCALE=2 FIXED DEPTH16 AFFINE (unless overridden)

namespace pix {

inline bool envFlag(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

inline int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    return std::atoi(v);
}

inline bool fastPreset() {
    static const bool on = envFlag("PIXSOFTGL_FAST");
    return on;
}

inline int renderScale() {
    static const int s = [] {
        int v = 1;
        if (std::getenv("PIXSOFTGL_SCALE")) {
            v = envInt("PIXSOFTGL_SCALE", 1);
        } else if (fastPreset()) {
            v = 2;
        }
        if (v < 1) v = 1;
        if (v > 8) v = 8;
        return v;
    }();
    return s;
}

inline bool fixedRaster() {
    static const bool on = envFlag("PIXSOFTGL_FIXED") || fastPreset();
    return on;
}

inline int fixedBits() {
    static const int b = [] {
        int v = envInt("PIXSOFTGL_FIXED_BITS", 4);
        if (v < 0) v = 0;
        if (v > 12) v = 12;
        return v;
    }();
    return b;
}

inline bool depth16() {
    static const bool on = envFlag("PIXSOFTGL_DEPTH16") || fastPreset();
    return on;
}

inline bool forceAffine() {
    static const bool on = envFlag("PIXSOFTGL_AFFINE") || fastPreset() || fixedRaster();
    return on;
}

inline bool noFog() {
    static const bool on = envFlag("PIXSOFTGL_NO_FOG");
    return on;
}

inline bool fogEnabled(bool glFogActive) {
    return glFogActive && !noFog();
}

} // namespace pix
