#pragma once

// The current vertex index
#include "displayLists.h"
#include "datatypes.h"
#include "defines.h"
#include <GL/gl.h>
#include <vector>

extern bool forwardToSystemGl;

extern int vertexIndex;

// If depth should be tested
extern bool depthTestActive;
extern bool fogActive;
extern bool colorMaterialActive;
extern bool blendActive;
extern bool lightingActive;
extern bool cullFaceActive;
extern bool counterClockWiseWindingActive;
extern bool normalizeActive;
extern bool scissorTestActive;
extern bool depthWriteActive;
extern bool alphaTestActive;
extern bool texture2dActive;
extern GLenum textureEnvMode;

extern GLenum depthFunction;
extern GLenum alphaFunc;
extern GLfloat alphaRef;
extern GLenum blendSrcFactor;
extern GLenum blendDstFactor;
extern GLint unpackAlignment;

extern GLint scissorX;
extern GLint scissorY;
extern GLsizei scissorWidth;
extern GLsizei scissorHeight;

// Original GL-convention viewport (origin bottom-left), for glGetIntegerv.
extern GLint glViewportX;
extern GLint glViewportY;
extern GLsizei glViewportW;
extern GLsizei glViewportH;

// Lights
extern bool lightActive[MAX_LIGHTS];
extern Light lights[MAX_LIGHTS];

// Fog variables
extern GLenum fogMode;
extern Col4 fogColor;
extern float fogStart;
extern float fogEnd;
extern float fogDensity;

// The current object rendering mode
extern GLenum drawingMode;

// The current matrix mode
extern GLenum matrixMode;

// Currently active color
extern Col4 currentColor;
extern Col4 clearColor;

// Matrices
extern int projMatrixPtr;
extern int modelMatrixPtr;
extern int texMatrixPtr;
extern Mat4x4 modelMatrices[MAX_MODELVIEW_STACK_DEPTH];
extern Mat4x4 projMatrices[MAX_PROJECTION_STACK_DEPTH];
extern Mat4x4 texMatrices[MAX_TEXTURE_STACK_DEPTH];
extern Mat4x4* lastAccessedMatrix;

// Vertex buffer
extern Vertex vertices[MAX_VERTICES];

// Render-surface size
extern int renderAreaWidth;
extern int renderAreaHeight;
extern int renderAreaTotal;

// Viewport size
extern int viewportOffsetX;
extern int viewportOffsetY;
extern int viewportAreaWidth;
extern int viewportAreaHeight;
extern int viewportAreaTotal;

// Screen framebuffer
extern PixelValue* frameBufferColor;
extern float* frameBufferDepth;

// Display lists
extern GLint activeDisplayListIndex;
extern GLuint listBase;
// Currently active Display list is display List 0!!!
extern DisplayList displayListBuffer;
extern std::vector<DisplayList> displayLists;
// If false, only compile
extern bool compileAndExecute;

// Textures
extern Vec2 currentTextureUV;
extern GLenum textureType;
extern TextureSlot* lastAccessedTexture;
extern std::vector<TextureSlot> textureArray;

// Arrays
extern bool vertexArrayActive;
extern const GLvoid *vertexArrayPointer;
extern GLint vertexArrayStride;
extern GLenum vertexArrayType;
extern GLenum vertexArrayTypeSize;

extern bool colorArrayActive;
extern const GLvoid *colorArrayPointer;
extern GLint colorArrayStride;
extern GLenum colorArrayType;
extern GLenum colorArrayTypeSize;

extern bool textureArrayActive;
extern const GLvoid *textureArrayPointer;
extern GLint textureArrayStride;
extern GLenum textureArrayType;
extern GLenum textureArrayTypeSize;

// GL error global
extern GLenum errorState;