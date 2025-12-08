#include "global.h"
#include <GL/gl.h>
#include <cstddef>

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
bool scissorTestActive = false;
bool counterClockWiseWindingActive = false;
bool depthWriteActive = true;
bool alphaTestActive = false;

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
Mat4x4 modelMatricies[MAX_MODELVIEW_STACK_DEPTH];
Mat4x4 projMatricies[MAX_PROJECTION_STACK_DEPTH];
Mat4x4 texMatricies[MAX_TEXTURE_STACK_DEPTH];
Mat4x4* lastAccessedMatrix = &modelMatricies[0];

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
DisplayList displayLists[MAX_DISPLAY_LIST_ENTRIES];
// If false, only compile
bool compileAndExecute = false;

// Textures
TextureSlot* lastAccessedTexture = nullptr;
TextureSlot textureArray[MAX_TEXTURES];

// Arrays
GLenum clientState = 0;
const GLvoid *vertexArrayPointer = nullptr;
GLint vertexArrayStride = 0;
const GLvoid *colorArrayPointer = nullptr;
GLint colorArrayStride = 0;
const GLvoid *textureArrayPointer = nullptr;
GLint textureArrayStride =  0;

// GL error global
GLenum errorState = 0;