#include "displayLists.h"
#include "functions/function.h"
#include "global.h"
#include "include/commands.h"
#include <GL/gl.h>

void ExecuteDisplayList(const DisplayList& dl) {
    for (int i = 0; i < dl.numberOfCommands; i++) {
        const auto& c = dl.commands[i];
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
                Process_glColor3f(
                    c.data.PARAM_glColor3f.red,
                    c.data.PARAM_glColor3f.green,
                    c.data.PARAM_glColor3f.blue    
                );
                break;
            case CMD_glGetString:
                break;
            case CMD_glLightfv:
                break;
            case CMD_glGenLists:
                break;
            case CMD_glNewList:
                break;
            case CMD_glEndList:
                break;
            case CMD_glClear:
                break;
            case CMD_glMatrixMode:
                break;
            case CMD_glFrontFace:
                break;
            case CMD_glLoadMatrixf:
                break;
            case CMD_glDepthMask:
                break;
            case CMD_glEnable:
                break;
            case CMD_glDisable:
                break;
            case CMD_glBegin:
                Process_glBegin(c.data.PARAM_glBegin);
                break;
            case CMD_glFinish:
                break;
            case CMD_glFlush:
                break;
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
                break;
            case CMD_glFogfv:
                break;
            case CMD_glFogf:
                break;
            case CMD_glReadPixels:
                break;
            case CMD_glPushMatrix:
                Process_glPushMatrix();
                break;
            case CMD_glPopMatrix:
                Process_glPopMatrix();
                break;
            case CMD_glGetIntegerv:
              break;
            case CMD_glBindTexture:
                Process_glBindTexture(
                    c.data.PARAM_glBindTexture.target,
                    c.data.PARAM_glBindTexture.texture
                );
                break;
            case CMD_glDrawElements:
                Process_glDrawElements(
                    c.data.PARAM_glDrawElements.mode,
                    c.data.PARAM_glDrawElements.count,
                    c.data.PARAM_glDrawElements.type,
                    c.data.PARAM_glDrawElements.indices
                );
                break;
            case CMD_glMaterialfv:
                Process_glMaterialfv(
                    c.data.PARAM_glMaterialfv.face,
                    c.data.PARAM_glMaterialfv.pname,
                    c.data.PARAM_glMaterialfv.params
                );
                break;
            }
    }
}

void Record_glVector2f(GLfloat x, GLfloat y) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glVertex2f;
    c.data.PARAM_glVertex2f = {x,y};
};

void Record_glVector3f(GLfloat x, GLfloat y, GLfloat z) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glVertex3f;
    c.data.PARAM_glVertex3f = {x,y, z};
};

void Record_glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glColor3f;
    c.data.PARAM_glColor3f = {red, green, blue};
}

void Record_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glViewport;
    c.data.PARAM_glViewport = {x,y,width,height};
};

void Record_glBegin(GLenum mode) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glBegin;
    c.data.PARAM_glBegin = {mode};
}

void Record_glEnd() {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glEnd;
}

void Record_glLoadIdentity() {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glLoadIdentity;
}
void Record_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glTranslatef;
    c.data.PARAM_glTranslatef = {x,y,z};
}

void Record_glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glRotatef;
    c.data.PARAM_glRotatef = {angleDeg, x,y,z};
}
void Record_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glScalef;
    c.data.PARAM_glScalef = { x,y,z};
}

void Record_glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glFrustum;
    c.data.PARAM_glFrustum = {l,r,b,t,n,f};
}
void Record_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glOrtho;
    c.data.PARAM_glOrtho = {l,r,b,t,n,f};
}

void Record_glPushMatrix() {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glPushMatrix;
}

void Record_glPopMatrix() {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glPopMatrix;
}

void Record_glBindTexture(GLenum target, GLuint texture) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glBindTexture;
    c.data.PARAM_glBindTexture = {target, texture};
}

void Record_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glDrawElements;
    c.data.PARAM_glDrawElements = {mode,count,type,indices};
}

void Record_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    if (activeDisplayListIndex == 0) return;
    auto& c = displayLists[0].commands[++displayLists[0].numberOfCommands];
    c.type = CMD_glMaterialfv;
    c.data.PARAM_glMaterialfv = {face,pname,params};
}