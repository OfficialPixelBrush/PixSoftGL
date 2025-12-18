#include "global.h"
#include "maths.h"
#include <cassert>
#include <cmath>
#include "sdl.h"

void ClearFramebuffers(GLenum mask);
void RenderPixel(Vec3 screenPos, Col4 color);
void RenderLine(Vec3 posA, Col4 colA, Vec3 posB, Col4 colB);
void RenderTriangle(Triangle tri);