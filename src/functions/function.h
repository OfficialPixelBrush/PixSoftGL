#pragma once
#include <GL/gl.h>

void Process_glVertex2f(GLfloat x, GLfloat y);
void Process_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void Process_glColor3f(GLfloat red, GLfloat green, GLfloat blue);
void Process_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void Process_glBegin(GLenum mode);
void Process_glEnd();
void Process_glLoadIdentity();
void Process_glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void Process_glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z);
void Process_glScalef(GLfloat x, GLfloat y, GLfloat z);
void Process_glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void Process_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void Process_glPushMatrix();
void Process_glPopMatrix();
void Process_glNewList(GLuint list, GLenum mode);
void Process_glEndList();
void Process_glCallList(GLuint list);
GLuint Process_glGenLists(GLsizei range);
GLboolean Process_IsList(GLuint list);
void Process_glDeleteLists(GLuint list, GLsizei range);
void Process_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void Process_glGenTextures(GLsizei n, GLuint *textures);
void Process_glBindTexture(GLenum target, GLuint texture);