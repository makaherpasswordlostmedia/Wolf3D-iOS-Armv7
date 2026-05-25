/*
 * gles_glue.h  —  OpenGL ES 2.0 glue layer (Wolf3D-iOS)
 *
 * Подключается везде где раньше был #include <OpenGLES/ES1/gl.h>
 * плюс оригинальный gles_glue.h.
 */

#ifndef GLES_GLUE_H
#define GLES_GLUE_H

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Инициализация / завершение --------------------------------------- */
void R_GLES2_Init(void);
void R_GLES2_Shutdown(void);

/* ----- Управление режимами шейдера -------------------------------------- */
/*  mode 0 — texture * vertex color  (стены, HUD)
    mode 1 — vertex color only       (заливка потолка / пола)
    mode 2 — texture alpha * vertex  (спрайты с прозрачностью)   */
void R_GLES2_SetRenderMode(int mode);
void R_GLES2_SetAlphaRef(float ref);

/* ----- Immediate-mode эмуляция ----------------------------------------- */
void pfglBegin(GLenum prim);
void pfglEnd(void);
void pfglVertex3f(float x, float y, float z);
void pfglVertex2f(float x, float y);
void pfglTexCoord2f(float s, float t);
void pfglColor4f(float r, float g, float b, float a);
void pfglColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void pfglColor3f(float r, float g, float b);

/* ----- Матричный стек --------------------------------------------------- */
void qglMatrixMode(GLenum mode);
void qglLoadIdentity(void);
void qglLoadMatrixf(const float *m);
void qglMultMatrixf(const float *m);
void qglOrtho(float l, float r, float b, float t, float n, float f);
void qglFrustum(float l, float r, float b, float t, float n, float f);
void qglTranslatef(float x, float y, float z);
void qglScalef(float x, float y, float z);
void qglRotatef(float angle, float x, float y, float z);
void qglPushMatrix(void);
void qglPopMatrix(void);

/* ----- No-op заглушки (ES1 API) ---------------------------------------- */
void pfglEnableClientState(GLenum cap);
void pfglDisableClientState(GLenum cap);
void qglEnableClientState(GLenum cap);
void qglDisableClientState(GLenum cap);
void pfglVertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void pfglTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void pfglColorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void qglTexEnvf(GLenum target, GLenum pname, GLfloat param);
void qglTexEnvi(GLenum target, GLenum pname, GLint param);
void qglAlphaFunc(GLenum func, GLclampf ref);

/* ----- Макросы совместимости ------------------------------------------- */
/* В оригинале qgl* и pfgl* часто взаимозаменяемы через define */
#define qglBegin           pfglBegin
#define qglEnd             pfglEnd
#define qglVertex3f        pfglVertex3f
#define qglVertex2f        pfglVertex2f
#define qglTexCoord2f      pfglTexCoord2f
#define qglColor4f         pfglColor4f
#define qglColor4ub        pfglColor4ub
#define qglColor3f         pfglColor3f

/* ES2 не имеет этих enum, добавляем для совместимости заголовков */
#ifndef GL_MODELVIEW
#define GL_MODELVIEW  0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif

#ifdef __cplusplus
}
#endif

#endif /* GLES_GLUE_H */
