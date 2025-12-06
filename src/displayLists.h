#pragma once

#include "include/commands.h"
#include <GL/gl.h>

struct DisplayListCommand {
    CommandType type;
    union {
        CMD_PARAM_glVertex2f PARAM_glVertex2f;
        CMD_PARAM_glVertex3f PARAM_glVertex3f;
        CMD_PARAM_glViewport PARAM_glViewport;
        CMD_PARAM_glTranslatef PARAM_glTranslatef;
        CMD_PARAM_glRotatef PARAM_glRotatef;
        CMD_PARAM_glScalef PARAM_glScalef;
        CMD_PARAM_glFrustum PARAM_glFrustum;
        CMD_PARAM_glOrtho PARAM_glOrtho;
        GLenum PARAM_glBegin;
    } data;
};

struct DisplayList {
    int numberOfCommands = 0;
    DisplayListCommand commands[MAX_DISPLAY_LIST_COMMANDS];
};

void Record_glVector2f(GLfloat x, GLfloat y);
void Record_glVector3f(GLfloat x, GLfloat y, GLfloat z);
void Record_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void Record_glBegin(GLenum mode);
void Record_glEnd();
void Record_glLoadIdentity();
void Record_glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void Record_glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z);
void Record_glScalef(GLfloat x, GLfloat y, GLfloat z);
void Record_glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void Record_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void Record_glPushMatrix();
void Record_glPopMatrix();