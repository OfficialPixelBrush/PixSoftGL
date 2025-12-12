#include "global.h"
#include <GL/gl.h>
#include <cstddef>

bool forwardToSystemGl = true;

int vertexIndex = 0;

// If depth should be tested
bool depthTestActive = false;
bool fogActive = false;
bool colorMaterialActive = false;
bool blendActive = false;
bool lightingActive = false;
bool cullFaceActive = false;
bool normalizeActive = false;
bool scissorTestActive = false;
bool counterClockWiseWindingActive = true;
bool depthWriteActive = true;
bool alphaTestActive = false;
bool texture2dActive = false;

GLenum depthFunction = GL_GEQUAL;

// Lights
bool lightActive[MAX_LIGHTS];
Light lights[MAX_LIGHTS];

// Fog variables
int fogMode = 0;
Col4 fogColor = Col4{0,0,0,1};
float fogStart = 0.0;
float fogEnd = 0.0;
float fogDensity = 0.0;

// The current object rendering mode
GLenum drawingMode = GL_POINTS;

// The current matrix mode
GLenum matrixMode = GL_MODELVIEW;

// Currently active color
Col4 currentColor = Col4{1,1,1,1};
Col4 clearColor = Col4{0,0,0,1};

// Matrices
int projMatrixPtr = 0;
int modelMatrixPtr = 0;
int texMatrixPtr = 0;
Mat4x4 modelMatrices[MAX_MODELVIEW_STACK_DEPTH];
Mat4x4 projMatrices[MAX_PROJECTION_STACK_DEPTH];
Mat4x4 texMatrices[MAX_TEXTURE_STACK_DEPTH];
Mat4x4* lastAccessedMatrix = &modelMatrices[0];

// Vertex buffer
Vertex vertices[MAX_VERTICES];

// Render-surface size
int renderAreaWidth = DEFAULT_RENDER_AREA_WIDTH;
int renderAreaHeight = DEFAULT_RENDER_AREA_HEIGHT;
int renderAreaTotal = renderAreaWidth*renderAreaHeight;

// Viewport size
int viewportOffsetX = 0;
int viewportOffsetY = 0;
int viewportAreaWidth = renderAreaWidth;
int viewportAreaHeight = renderAreaHeight;
int viewportAreaTotal = viewportAreaWidth*viewportAreaHeight;

// Screen framebuffer
PixelValue* frameBufferColor;
float* frameBufferDepth;

// Display lists
GLint activeDisplayListIndex = 0;
// Currently active Display list is display List 0!!!
DisplayList displayListBuffer;
std::vector<DisplayList> displayLists;
// If false, only compile
bool compileAndExecute = false;

// Textures
Vec2 currentTextureUV = Vec2{0,0};
TextureSlot* lastAccessedTexture = nullptr;
std::vector<TextureSlot> textureArray;

// Arrays
bool vertexArrayActive = false;
const GLvoid *vertexArrayPointer = nullptr;
GLint vertexArrayStride = 0;

bool colorArrayActive = false;
const GLvoid *colorArrayPointer = nullptr;
GLint colorArrayStride = 0;

bool textureArrayActive = false;
const GLvoid *textureArrayPointer = nullptr;
GLint textureArrayStride =  0;

// GL error global
GLenum errorState = 0;