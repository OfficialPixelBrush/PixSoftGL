#include "global.h"
#include <GL/gl.h>
#include <cstddef>
#include <cstdlib>

namespace {

Mat4x4 makeIdentity() {
    return Mat4x4{
        Vec4{1, 0, 0, 0},
        Vec4{0, 1, 0, 0},
        Vec4{0, 0, 1, 0},
        Vec4{0, 0, 0, 1}
    };
}

void fillIdentity(Mat4x4* mats, int count) {
    for (int i = 0; i < count; ++i) {
        mats[i] = makeIdentity();
    }
}

} // namespace

bool forwardToSystemGl = false;

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
GLenum textureEnvMode = GL_MODULATE;

GLenum depthFunction = GL_LESS;
GLenum alphaFunc = GL_ALWAYS;
GLfloat alphaRef = 0.0f;
GLenum blendSrcFactor = GL_SRC_ALPHA;
GLenum blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
GLint unpackAlignment = 4;

GLint scissorX = 0;
GLint scissorY = 0;
GLsizei scissorWidth = DEFAULT_RENDER_AREA_WIDTH;
GLsizei scissorHeight = DEFAULT_RENDER_AREA_HEIGHT;

GLint glViewportX = 0;
GLint glViewportY = 0;
GLsizei glViewportW = DEFAULT_RENDER_AREA_WIDTH;
GLsizei glViewportH = DEFAULT_RENDER_AREA_HEIGHT;

// Lights
bool lightActive[MAX_LIGHTS];
Light lights[MAX_LIGHTS];

// Fog variables
GLenum fogMode = GL_EXP;
Col4 fogColor = Col4{0,0,0,0};
float fogStart = 0.0f;
float fogEnd = 1.0f;
float fogDensity = 1.0f;

// The current object rendering mode
GLenum drawingMode = GL_POINTS;

// The current matrix mode
GLenum matrixMode = GL_MODELVIEW;

// Currently active color
Col4 currentColor = Col4{1,1,1,1};
Col4 clearColor = Col4{0,0,0,1};

// Matrices — OpenGL stacks start as identity.
int projMatrixPtr = 0;
int modelMatrixPtr = 0;
int texMatrixPtr = 0;
Mat4x4 modelMatrices[MAX_MODELVIEW_STACK_DEPTH];
Mat4x4 projMatrices[MAX_PROJECTION_STACK_DEPTH];
Mat4x4 texMatrices[MAX_TEXTURE_STACK_DEPTH];
Mat4x4* lastAccessedMatrix = &modelMatrices[0];

struct MatrixInit {
    MatrixInit() {
        fillIdentity(modelMatrices, MAX_MODELVIEW_STACK_DEPTH);
        fillIdentity(projMatrices, MAX_PROJECTION_STACK_DEPTH);
        fillIdentity(texMatrices, MAX_TEXTURE_STACK_DEPTH);
    }
};
static MatrixInit matrixInit;

// Vertex buffer
Vertex vertices[MAX_VERTICES];

// Render-surface size
int renderAreaWidth = DEFAULT_RENDER_AREA_WIDTH;
int renderAreaHeight = DEFAULT_RENDER_AREA_HEIGHT;
int renderAreaTotal = renderAreaWidth*renderAreaHeight;

int presentWidth = DEFAULT_RENDER_AREA_WIDTH;
int presentHeight = DEFAULT_RENDER_AREA_HEIGHT;

// Viewport size
int viewportOffsetX = 0;
int viewportOffsetY = 0;
int viewportAreaWidth = renderAreaWidth;
int viewportAreaHeight = renderAreaHeight;
int viewportAreaTotal = viewportAreaWidth*viewportAreaHeight;

// Screen framebuffer
PixelValue* frameBufferColor = nullptr;
float* frameBufferDepth = nullptr;
uint16_t* frameBufferDepth16 = nullptr;

// Display lists
GLint activeDisplayListIndex = 0;
GLuint listBase = 0;
// Currently active Display list is display List 0!!!
DisplayList displayListBuffer;
std::vector<DisplayList> displayLists;
// If false, only compile
bool compileAndExecute = false;

// Textures
Vec2 currentTextureUV = Vec2{0,0};
GLenum textureType = 0;
GLuint boundTexture2D = 0;
TextureSlot* lastAccessedTexture = nullptr;
std::vector<TextureSlot> textureArray;

// Arrays
bool vertexArrayActive = false;
const GLvoid *vertexArrayPointer = nullptr;
GLint vertexArrayStride = 0;
GLenum vertexArrayType = 0;
GLenum vertexArrayTypeSize = 0;

bool colorArrayActive = false;
const GLvoid *colorArrayPointer = nullptr;
GLint colorArrayStride = 0;
GLenum colorArrayType = 0;
GLenum colorArrayTypeSize = 0;

bool textureArrayActive = false;
const GLvoid *textureArrayPointer = nullptr;
GLint textureArrayStride = 0;
GLenum textureArrayType = 0;
GLenum textureArrayTypeSize = 0;

// GL error global
GLenum errorState = GL_NO_ERROR;