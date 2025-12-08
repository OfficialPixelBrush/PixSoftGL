#pragma once
#include "global.h"

#include <GL/gl.h>
#include <SDL3/SDL_init.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <string>

void PrintInfoHex(int s);
void PrintInfo(int s);
void PrintInfo(const std::string& s);

void ReCreateWindow();
void UpdateScreen();