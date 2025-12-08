#pragma once
#include <cmath>
#include <algorithm>
#include "global.h"

Col3 lerp(Col3 a, Col3 b, float t);
float lerp(float a, float b, float t);
Mat4x4 Vec3ToMat4x4(Vec3 pos);
Vec3 ProjectPosition(Vec3 pos);
Triangle ProjectTriangle(Triangle tri);
PixelValue Col3ToPixelValue(Col3 color);
Vec3 Normalize(Vec3 v);
float Cross2D(Vec3 a, Vec3 b);
float Dot2D(Vec3 a, Vec3 b);
Vec3 Perpendicular2D(Vec3 vec);
bool PointOnRightSideOfLine(Vec3 a, Vec3 b, Vec3 p);
bool PointInTriangle(Triangle tri, Vec3 p);
Col3 BarycentricColor(Triangle tri, Vec3& p);
Col4 BarycentricTexture(Triangle tri, Vec3& p);