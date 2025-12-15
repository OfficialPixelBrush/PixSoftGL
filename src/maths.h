#pragma once
#include <cmath>
#include <algorithm>
#include "global.h"
#include "include/datatypes.h"

Col4 lerp(Col4 a, Col4 b, float t);
Col3 lerp(Col3 a, Col3 b, float t);
float lerp(float a, float b, float t);
Mat4x4 Vec3ToMat4x4(Vec3 pos);
Vec4 ProjectPosition(Vec3 pos);
Triangle ProjectTriangle(Triangle tri);
PixelValue Col3ToPixelValue(Col3 color);
Col3 PixelValueToCol3(PixelValue color);
Vec3 Normalize(Vec3 v);
Vec3 Cross3D(Vec3 a, Vec3 b);
float Cross2D(Vec3 a, Vec3 b);
float Dot3D(Vec3 a, Vec3 b);
float Dot2D(Vec3 a, Vec3 b);
Vec3 Perpendicular2D(Vec3 vec);
bool PointOnRightSideOfLine(Vec3 a, Vec3 b, Vec3 p);
bool PointInTriangle(Triangle tri, Vec3 p);
Col4 BarycentricColor(Triangle tri, Vec3& p);
Col4 BarycentricTexture(Triangle tri, Vec3& p);
bool DetermineBounding(Triangle& tri, int& xMin, int& yMin, int& xMax, int& yMax);
Vec3 CalculateNormal(Triangle& tri);
float LinearizeDepth(float ndcZ, float near, float far);