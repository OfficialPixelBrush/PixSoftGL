#pragma once
#include "global.h"
#include <algorithm>

void RenderPixel(Vec3 screenPos, Col3 color);
void RenderLine(Vec3 posA, Col3 colA, Vec3 posB, Col3 colB);
void RenderTriangle(Triangle tri);