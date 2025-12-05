#include "global.h"

bool forwardToSystemGl = true;

int vertexIndex = 0;

// If depth should be tested
bool depthTestActive = false;
bool fogActive = false;
bool colorMaterialActive = false;
bool texture2dActive = false;
bool blendActive = false;
bool lightingActive = false;
bool cullFaceActive = false;
bool normalizeActive = false;

// Lights
bool lightActive[MAX_LIGHTS];
Light lights[MAX_LIGHTS];

// Fog variables
int fogMode = 0;
Col4 fogColor = Col4{0,0,0,0};
float fogStart = 0.0;
float fogEnd = 0.0;

// The current object rendering mode
GLenum drawingMode = GL_POINTS;

// The current projection mode
GLenum projectionMode = 0;

// The current matrix mode
GLenum matrixMode = GL_MODELVIEW;

// Currently active color
Col3 currentColor = Col3{1,1,1};
Col3 clearColor = Col3{0,0,0};

// Matricies
int projMatrixPtr = 0;
int modelMatrixPtr = 0;
int texMatrixPtr = 0;
Mat4x4 projMatricies[MAX_PROJECTION_MATRICIES];
Mat4x4 modelMatricies[MAX_MODEL_MATRICIES];
Mat4x4 texMatricies[MAX_TEXTURE_MATRICIES];
Mat4x4* lastAccessedMatrix = &modelMatricies[0];

// Vertex buffer
Vertex vertices[MAX_VERTICES];

// Viewport size
int renderAreaWidth = DEFAULT_RENDER_AREA_WIDTH;
int renderAreaHeight = DEFAULT_RENDER_AREA_HEIGHT;
int renderAreaTotal = DEFAULT_RENDER_AREA_WIDTH * DEFAULT_RENDER_AREA_HEIGHT;

// Screen framebuffer
PixelValue* frameBufferColor;
float* frameBufferDepth;