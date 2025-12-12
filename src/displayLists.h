#pragma once

#include "include/commands.h"
#include <GL/gl.h>
#include <vector>

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
        CMD_PARAM_glBindTexture PARAM_glBindTexture;
        CMD_PARAM_glColor3f PARAM_glColor3f;
        CMD_PARAM_glDrawElements PARAM_glDrawElements;
        CMD_PARAM_glMaterialfv PARAM_glMaterialfv;
        GLenum PARAM_glEnable;
        GLenum PARAM_glDisable;
        GLenum PARAM_glMatrixMode;
        GLenum PARAM_glFrontFace;
        CMD_PARAM_glDrawArrays PARAM_glDrawArrays;
        CMD_PARAM_glTexCoord2f PARAM_glTexCoord2f;
        CMD_PARAM_glFogi PARAM_glFogi;
        CMD_PARAM_glFogfv PARAM_glFogfv;
        CMD_PARAM_glFogf PARAM_glFogf;
        CMD_PARAM_glLoadMatrixf PARAM_glLoadMatrixf;
        GLenum PARAM_glDepthFunc;
    } data;
};

struct DisplayList {
    std::vector<DisplayListCommand> commands;
};

void ExecuteDisplayList(const DisplayList& dl);

void Record_glVector2f(GLfloat x, GLfloat y);
void Record_glVector3f(GLfloat x, GLfloat y, GLfloat z);
void Record_glColor3f(GLfloat red, GLfloat green, GLfloat blue);
void Record_glTexCoord2f(GLfloat s, GLfloat t);
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
void Record_glBindTexture(GLenum target, GLuint texture);
void Record_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void Record_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
void Record_glEnable(GLenum cap);
void Record_glDisable(GLenum cap);
void Record_glMatrixMode(GLenum mode);
void Record_glFrontFace(GLenum mode);
void Record_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void Record_glFogi(GLenum pname, GLint param);
void Record_glFogfv(GLenum pname, const GLfloat *params);
void Record_glFogf(GLenum pname, GLfloat param);
void Record_glDepthFunc(GLenum func);