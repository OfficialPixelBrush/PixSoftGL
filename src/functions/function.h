#pragma once
#include <GL/gl.h>

void Process_glVertex2f(GLfloat x, GLfloat y);
void Process_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
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