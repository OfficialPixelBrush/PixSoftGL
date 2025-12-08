#pragma once
#include "defines.h"
#include <GL/gl.h>

// I hate display lists
enum CommandType {
    CMD_glVertex2f,
    CMD_glVertex3f,
    CMD_glViewport,
    CMD_glClearColor,
    CMD_glColor3f,
    CMD_glGetString,
    CMD_glLightfv,
    CMD_glGenLists,
    CMD_glNewList,
    CMD_glEndList,
    CMD_glClear,
    CMD_glMatrixMode,
    CMD_glFrontFace,
    CMD_glLoadMatrixf,
    CMD_glDepthMask,
    CMD_glEnable,
    CMD_glDisable,
    CMD_glBegin,
    CMD_glFinish,
    CMD_glFlush,
    CMD_glEnd,
    CMD_glLoadIdentity,
    CMD_glTranslatef,
    CMD_glRotatef,
    CMD_glScalef,
    CMD_glFrustum,
    CMD_glOrtho,
    CMD_glFogi,
    CMD_glFogfv,
    CMD_glFogf,
    CMD_glReadPixels,
    CMD_glPushMatrix,
    CMD_glPopMatrix,
    CMD_glGetIntegerv,
    CMD_glBindTexture,
    CMD_glDrawElements,
};

struct CMD_PARAM_glVertex2f {
    GLfloat x, y;
};

struct CMD_PARAM_glVertex3f {
    GLfloat x, y, z;
};

struct CMD_PARAM_glViewport {
    GLint x, y;
    GLsizei width, height;
};

struct CMD_PARAM_glTranslatef {
    GLfloat x,y,z;
};

struct CMD_PARAM_glRotatef {
    GLfloat angleDeg, x, y, z;
};

struct CMD_PARAM_glScalef {
    GLfloat x,y,z;
};

struct CMD_PARAM_glFrustum {
    GLdouble l,r,t,b,n,f;
};

struct CMD_PARAM_glOrtho {
    GLdouble l,r,t,b,n,f;
};

struct CMD_PARAM_glBindTexture {
    GLenum target;
    GLuint texture;
};

struct CMD_PARAM_glColor3f {
    GLfloat red, green, blue;
};

struct CMD_PARAM_glDrawElements {
    GLenum mode;
    GLsizei count;
    GLenum type;
    const GLvoid *indices;
};