#ifndef GLES_GLUE_H
#define GLES_GLUE_H
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#ifdef __cplusplus
extern "C" {
#endif
void R_GLES2_Init(void);
void R_GLES2_Shutdown(void);
void R_GLES2_SetRenderMode(int mode);
void R_GLES2_SetAlphaRef(float ref);
void pfglBegin(GLenum prim);
void pfglEnd(void);
void pfglVertex3f(float x,float y,float z);
void pfglVertex2f(float x,float y);
void pfglTexCoord2f(float s,float t);
void pfglColor4f(float r,float g,float b,float a);
void pfglColor4ub(GLubyte r,GLubyte g,GLubyte b,GLubyte a);
void pfglColor3f(float r,float g,float b);
void qglMatrixMode(GLenum mode);
void qglLoadIdentity(void);
void qglLoadMatrixf(const float *m);
void qglMultMatrixf(const float *m);
void qglOrtho(float l,float r,float b,float t,float n,float f);
void qglFrustum(float l,float r,float b,float t,float n,float f);
void qglTranslatef(float x,float y,float z);
void qglScalef(float x,float y,float z);
void qglRotatef(float a,float x,float y,float z);
void qglPushMatrix(void);
void qglPopMatrix(void);
void pfglEnableClientState(GLenum cap);
void pfglDisableClientState(GLenum cap);
void qglEnableClientState(GLenum cap);
void qglDisableClientState(GLenum cap);
void pfglVertexPointer(GLint s,GLenum t,GLsizei st,const void *p);
void pfglTexCoordPointer(GLint s,GLenum t,GLsizei st,const void *p);
void pfglColorPointer(GLint s,GLenum t,GLsizei st,const void *p);
void qglTexEnvf(GLenum tgt,GLenum pname,GLfloat param);
void qglTexEnvi(GLenum tgt,GLenum pname,GLint param);
void qglAlphaFunc(GLenum func,GLclampf ref);
#define qglBegin      pfglBegin
#define qglEnd        pfglEnd
#define qglVertex3f   pfglVertex3f
#define qglVertex2f   pfglVertex2f
#define qglTexCoord2f pfglTexCoord2f
#define qglColor4f    pfglColor4f
#define qglColor4ub   pfglColor4ub
#define qglColor3f    pfglColor3f
#ifndef GL_MODELVIEW
#define GL_MODELVIEW  0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif
#ifdef __cplusplus
}
#endif
#endif
