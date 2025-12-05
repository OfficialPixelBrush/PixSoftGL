#pragma once

// The current vertex index
#include "include/datatypes.h"
#include "include/defines.h"
#include <GL/gl.h>

static bool forwardToSystemGl = true;

static int vertexIndex = 0;

// If depth should be tested
static bool depthTestActive = false;
static bool fogActive = false;
static bool colorMaterialActive = false;
static bool texture2dActive = false;
static bool blendActive = false;
static bool lightingActive = false;
static bool cullFaceActive = false;
static bool normalizeActive = false;

// Lights
static bool lightActive[MAX_LIGHTS];
static Light lights[MAX_LIGHTS];

// Fog variables
static int fogMode = 0;
static Col4 fogColor = Col4{0,0,0,0};
static float fogStart = 0.0;
static float fogEnd = 0.0;

// The current object rendering mode
static GLenum drawingMode = GL_POINTS;

// The current projection mode
static GLenum projectionMode = 0;

// The current matrix mode
static GLenum matrixMode = GL_MODELVIEW;

// Currently active color
static Col3 currentColor = Col3{1,1,1};
static Col3 clearColor = Col3{0,0,0};

// Matricies
static int projMatrixPtr = 0;
static int modelMatrixPtr = 0;
static int texMatrixPtr = 0;
static Mat4x4 projMatricies[MAX_PROJECTION_MATRICIES];
static Mat4x4 modelMatricies[MAX_MODEL_MATRICIES];
static Mat4x4 texMatricies[MAX_TEXTURE_MATRICIES];
static Mat4x4* lastAccessedMatrix = &modelMatricies[0];

// Vertex buffer
static Vertex vertices[MAX_VERTICES];

// Viewport size
static int renderAreaWidth = DEFAULT_RENDER_AREA_WIDTH;
static int renderAreaHeight = DEFAULT_RENDER_AREA_HEIGHT;
static int renderAreaTotal = DEFAULT_RENDER_AREA_WIDTH * DEFAULT_RENDER_AREA_HEIGHT;

// Screen framebuffer
static PixelValue* frameBufferColor;
static float* frameBufferDepth;