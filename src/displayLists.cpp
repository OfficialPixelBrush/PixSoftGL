#include "displayLists.h"
#include "functions/function.h"
#include "global.h"
#include "include/commands.h"
#include <GL/gl.h>

void ExecuteDisplayList() {
    for (int i = 0; i < activeDisplayList->numberOfCommands; i++) {
        const auto& c = activeDisplayList->commands[i];
        switch(c.type) {
            case CMD_glVertex2f:
                Process_glVertex2f(
                    c.data.PARAM_glVertex2f.x, 
                    c.data.PARAM_glVertex2f.y
                );
                break;
            case CMD_glVertex3f:
                Process_glVertex3f(
                    c.data.PARAM_glVertex3f.x, 
                    c.data.PARAM_glVertex3f.y,
                     c.data.PARAM_glVertex3f.z
                );
                break;
            case CMD_glViewport:
                Process_glViewport(
                    c.data.PARAM_glViewport.x,
                    c.data.PARAM_glViewport.y,
                    c.data.PARAM_glViewport.width,
                    c.data.PARAM_glViewport.height
                );
                break;
            case CMD_glClearColor:
            case CMD_glColor3f:
            case CMD_glGetString:
            case CMD_glLightfv:
            case CMD_glGenLists:
            case CMD_glNewList:
            case CMD_glEndList:
            case CMD_glClear:
            case CMD_glMatrixMode:
            case CMD_glFrontFace:
            case CMD_glLoadMatrixf:
            case CMD_glDepthMask:
            case CMD_glEnable:
            case CMD_glDisable:
            case CMD_glBegin:
                Process_glBegin(c.data.PARAM_glBegin);
                break;
            case CMD_glFinish:
            case CMD_glFlush:
            case CMD_glEnd:
                Process_glEnd();
                break;
            case CMD_glLoadIdentity:
                Process_glLoadIdentity();
                break;
            case CMD_glTranslatef:
                Process_glTranslatef(
                    c.data.PARAM_glTranslatef.x,
                    c.data.PARAM_glTranslatef.y,
                    c.data.PARAM_glTranslatef.z
                );
                break;
            case CMD_glRotatef:
                Process_glRotatef(
                    c.data.PARAM_glRotatef.angleDeg,
                    c.data.PARAM_glRotatef.x,
                    c.data.PARAM_glRotatef.y,
                    c.data.PARAM_glRotatef.z
                );
                break;
            case CMD_glScalef:
                Process_glScalef(
                    c.data.PARAM_glScalef.x,
                    c.data.PARAM_glScalef.y,
                    c.data.PARAM_glScalef.z
                );
                break;
            case CMD_glFrustum:
                Process_glFrustum(
                    c.data.PARAM_glFrustum.l,
                    c.data.PARAM_glFrustum.r,
                    c.data.PARAM_glFrustum.t,
                    c.data.PARAM_glFrustum.b,
                    c.data.PARAM_glFrustum.n,
                    c.data.PARAM_glFrustum.f
                );
                break;
            case CMD_glOrtho:
                Process_glOrtho(
                    c.data.PARAM_glOrtho.l,
                    c.data.PARAM_glOrtho.r,
                    c.data.PARAM_glOrtho.t,
                    c.data.PARAM_glOrtho.b,
                    c.data.PARAM_glOrtho.n,
                    c.data.PARAM_glOrtho.f
                );
                break;
            case CMD_glFogi:
            case CMD_glFogfv:
            case CMD_glFogf:
            case CMD_glReadPixels:
            case CMD_glPushMatrix:
                Process_glPushMatrix();
                break;
            case CMD_glPopMatrix:
                Process_glPopMatrix();
                break;
            case CMD_glGetIntegerv:
              break;
            }
    }
}

void Record_glVector2f(GLfloat x, GLfloat y) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glVertex2f;
    c.data.PARAM_glVertex2f = {x,y};
};

void Record_glVector3f(GLfloat x, GLfloat y, GLfloat z) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glVertex3f;
    c.data.PARAM_glVertex3f = {x,y, z};
};

void Record_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glViewport;
    c.data.PARAM_glViewport = {x,y,width,height};
};

void Record_glBegin(GLenum mode) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glBegin;
    c.data.PARAM_glBegin = {mode};
}

void Record_glEnd() {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glEnd;
}

void Record_glLoadIdentity() {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glLoadIdentity;
}
void Record_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glTranslatef;
    c.data.PARAM_glTranslatef = {x,y,z};
}

void Record_glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glRotatef;
    c.data.PARAM_glRotatef = {angleDeg, x,y,z};
}
void Record_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glScalef;
    c.data.PARAM_glScalef = { x,y,z};
}

void Record_glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glFrustum;
    c.data.PARAM_glFrustum = {l,r,b,t,n,f};
}
void Record_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glOrtho;
    c.data.PARAM_glOrtho = {l,r,b,t,n,f};
}

void Record_glPushMatrix() {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glPushMatrix;
}

void Record_glPopMatrix() {
    if (!activeDisplayList) return;
    auto& c = activeDisplayList->commands[activeDisplayList->numberOfCommands++];
    c.type = CMD_glPopMatrix;
}