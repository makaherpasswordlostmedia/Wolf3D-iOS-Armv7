/*
 * gles_glue.c  —  OpenGL ES 2.0 glue layer for Wolf3D-iOS
 *
 * Заменяет оригинальный gles_glue.c (который был написан под ES 1.1).
 *
 * Эмулирует:
 *   pfglBegin / pfglEnd
 *   pfglVertex3f / pfglTexCoord2f / pfglColor4f / pfglColor4ub
 *   pfglEnableClientState / pfglDisableClientState  (no-op)
 *   pfglVertexPointer / pfglTexCoordPointer / pfglColorPointer (no-op)
 *   qglMatrixMode / qglLoadIdentity / qglOrtho / qglFrustum
 *   qglLoadMatrixf / qglMultMatrixf
 *   qglEnableClientState / qglDisableClientState    (no-op)
 *   qglTranslatef / qglRotatef / qglScalef
 *   qglPushMatrix / qglPopMatrix
 *
 * Новые функции для управления шейдером:
 *   R_GLES2_Init()          — компилирует шейдеры, создаёт VBO
 *   R_GLES2_Shutdown()      — освобождает ресурсы
 *   R_GLES2_SetRenderMode(int mode)
 *   R_GLES2_SetAlphaRef(float ref)
 *   R_GLES2_SetMVP(const float m[16])
 */

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* =========================================================================
 * Vertex структура — должна совпадать с оригинальным gles_glue.h
 * ========================================================================= */
typedef struct {
    float    xyz[3];
    float    st[2];
    GLubyte  c[4];
} Vertex;

#define MAX_VERTS 16384

static Vertex  immediate[MAX_VERTS];
static int     curr_vertex = 0;
static GLenum  curr_prim   = GL_TRIANGLES;

/* =========================================================================
 * Матричный стек (заменяет glMatrixMode / glPushMatrix / glOrtho и т.д.)
 * ========================================================================= */
#define MAT_STACK_DEPTH 16

typedef enum { MM_MODELVIEW = 0, MM_PROJECTION = 1 } MatrixMode;
static MatrixMode  g_matMode = MM_MODELVIEW;

static float g_matModelView[MAT_STACK_DEPTH][16];
static float g_matProjection[MAT_STACK_DEPTH][16];
static int   g_mvDepth = 0;
static int   g_prDepth = 0;

/* текущие верхние матрицы */
static float *MV(void) { return g_matModelView[g_mvDepth]; }
static float *PR(void) { return g_matProjection[g_prDepth]; }

static void mat_identity(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat_multiply(float *out, const float *a, const float *b)
{
    float tmp[16];
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            float s = 0;
            for (int k = 0; k < 4; k++)
                s += a[row + k*4] * b[k + col*4];
            tmp[row + col*4] = s;
        }
    memcpy(out, tmp, 16 * sizeof(float));
}

/* =========================================================================
 * Шейдерная программа
 * ========================================================================= */
static GLuint g_program     = 0;
static GLuint g_vbo         = 0;

/* attribute locations */
static GLint  g_loc_pos     = -1;
static GLint  g_loc_tc      = -1;
static GLint  g_loc_color   = -1;

/* uniform locations */
static GLint  g_loc_mvp     = -1;
static GLint  g_loc_tex     = -1;
static GLint  g_loc_mode    = -1;
static GLint  g_loc_aref    = -1;

/* ---- inline шейдерные исходники (чтобы не грузить файлы с диска) -------- */
static const char *k_vertSrc =
    "attribute vec3 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "attribute vec4 a_color;\n"
    "uniform   mat4 u_mvpMatrix;\n"
    "varying   vec2 v_texcoord;\n"
    "varying   vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_mvpMatrix * vec4(a_position, 1.0);\n"
    "    v_texcoord  = a_texcoord;\n"
    "    v_color     = a_color;\n"
    "}\n";

static const char *k_fragSrc =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "uniform int       u_renderMode;\n"
    "uniform float     u_alphaRef;\n"
    "varying vec2 v_texcoord;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    vec4 color;\n"
    "    if (u_renderMode == 1) {\n"
    "        color = v_color;\n"
    "    } else if (u_renderMode == 2) {\n"
    "        vec4 t = texture2D(u_texture, v_texcoord);\n"
    "        color = vec4(t.rgb * v_color.rgb, t.a * v_color.a);\n"
    "        if (color.a <= u_alphaRef) discard;\n"
    "    } else {\n"
    "        color = texture2D(u_texture, v_texcoord) * v_color;\n"
    "    }\n"
    "    gl_FragColor = color;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        printf("Shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

/* =========================================================================
 * Публичные функции инициализации
 * ========================================================================= */

void R_GLES2_Init(void)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   k_vertSrc);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, k_fragSrc);

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);

    /* bind attribute locations до линковки */
    glBindAttribLocation(g_program, 0, "a_position");
    glBindAttribLocation(g_program, 1, "a_texcoord");
    glBindAttribLocation(g_program, 2, "a_color");

    glLinkProgram(g_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(g_program, sizeof(log), NULL, log);
        printf("Program link error: %s\n", log);
        return;
    }

    glUseProgram(g_program);

    g_loc_pos   = glGetAttribLocation (g_program, "a_position");
    g_loc_tc    = glGetAttribLocation (g_program, "a_texcoord");
    g_loc_color = glGetAttribLocation (g_program, "a_color");
    g_loc_mvp   = glGetUniformLocation(g_program, "u_mvpMatrix");
    g_loc_tex   = glGetUniformLocation(g_program, "u_texture");
    g_loc_mode  = glGetUniformLocation(g_program, "u_renderMode");
    g_loc_aref  = glGetUniformLocation(g_program, "u_alphaRef");

    /* значения по умолчанию */
    glUniform1i(g_loc_tex,  0);
    glUniform1i(g_loc_mode, 0);
    glUniform1f(g_loc_aref, 0.0f);

    /* создаём VBO для immediate-mode буфера */
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(immediate), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* инициализируем матричный стек */
    mat_identity(MV());
    mat_identity(PR());
}

void R_GLES2_Shutdown(void)
{
    if (g_vbo)     { glDeleteBuffers(1, &g_vbo);     g_vbo = 0; }
    if (g_program) { glDeleteProgram(g_program);      g_program = 0; }
}

void R_GLES2_SetRenderMode(int mode)
{
    glUniform1i(g_loc_mode, mode);
}

void R_GLES2_SetAlphaRef(float ref)
{
    glUniform1f(g_loc_aref, ref);
}

/* Вызывается автоматически перед каждым glDrawArrays */
static void upload_mvp(void)
{
    float mvp[16];
    mat_multiply(mvp, PR(), MV());
    glUniformMatrix4fv(g_loc_mvp, 1, GL_FALSE, mvp);
}

/* =========================================================================
 * Эмуляция immediate mode (pfglBegin / pfglVertex3f / pfglEnd)
 * ========================================================================= */

void pfglBegin(GLenum prim)
{
    curr_vertex = 0;
    curr_prim   = prim;
}

void pfglEnd(void)
{
    if (curr_vertex == 0) return;

    upload_mvp();

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    curr_vertex * sizeof(Vertex), immediate);

    glEnableVertexAttribArray(g_loc_pos);
    glEnableVertexAttribArray(g_loc_tc);
    glEnableVertexAttribArray(g_loc_color);

    glVertexAttribPointer(g_loc_pos,   3, GL_FLOAT,         GL_FALSE,
                          sizeof(Vertex), (void*)offsetof(Vertex, xyz));
    glVertexAttribPointer(g_loc_tc,    2, GL_FLOAT,         GL_FALSE,
                          sizeof(Vertex), (void*)offsetof(Vertex, st));
    glVertexAttribPointer(g_loc_color, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                          sizeof(Vertex), (void*)offsetof(Vertex, c));

    glDrawArrays(curr_prim, 0, curr_vertex);

    glDisableVertexAttribArray(g_loc_pos);
    glDisableVertexAttribArray(g_loc_tc);
    glDisableVertexAttribArray(g_loc_color);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    curr_vertex = 0;
}

/* текущий накопленный вертекс */
static Vertex g_cur;

void pfglTexCoord2f(float s, float t)
{
    g_cur.st[0] = s;
    g_cur.st[1] = t;
}

void pfglVertex3f(float x, float y, float z)
{
    if (curr_vertex >= MAX_VERTS) return;
    g_cur.xyz[0] = x;
    g_cur.xyz[1] = y;
    g_cur.xyz[2] = z;
    immediate[curr_vertex++] = g_cur;
}

void pfglVertex2f(float x, float y)
{
    pfglVertex3f(x, y, 0.0f);
}

void pfglColor4f(float r, float g, float b, float a)
{
    g_cur.c[0] = (GLubyte)(r * 255.0f);
    g_cur.c[1] = (GLubyte)(g * 255.0f);
    g_cur.c[2] = (GLubyte)(b * 255.0f);
    g_cur.c[3] = (GLubyte)(a * 255.0f);
}

void pfglColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    g_cur.c[0] = r;
    g_cur.c[1] = g;
    g_cur.c[2] = b;
    g_cur.c[3] = a;
}

void pfglColor3f(float r, float g, float b)
{
    pfglColor4f(r, g, b, 1.0f);
}

/* =========================================================================
 * Матричные операции (заменяют glMatrixMode / glOrtho и т.д.)
 * ========================================================================= */

void qglMatrixMode(GLenum mode)
{
    g_matMode = (mode == GL_PROJECTION) ? MM_PROJECTION : MM_MODELVIEW;
}

void qglLoadIdentity(void)
{
    mat_identity((g_matMode == MM_PROJECTION) ? PR() : MV());
}

void qglLoadMatrixf(const float *m)
{
    memcpy((g_matMode == MM_PROJECTION) ? PR() : MV(), m, 16 * sizeof(float));
}

void qglMultMatrixf(const float *m)
{
    float *cur = (g_matMode == MM_PROJECTION) ? PR() : MV();
    float tmp[16];
    memcpy(tmp, cur, 16 * sizeof(float));
    mat_multiply(cur, tmp, m);
}

void qglOrtho(float l, float r, float b, float t, float n, float f)
{
    float m[16] = {0};
    m[0]  =  2.0f / (r - l);
    m[5]  =  2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] =  1.0f;
    float *cur = (g_matMode == MM_PROJECTION) ? PR() : MV();
    float tmp[16];
    memcpy(tmp, cur, 16 * sizeof(float));
    mat_multiply(cur, tmp, m);
}

void qglFrustum(float l, float r, float b, float t, float n, float f)
{
    float m[16] = {0};
    m[0]  =  (2.0f * n) / (r - l);
    m[5]  =  (2.0f * n) / (t - b);
    m[8]  =  (r + l) / (r - l);
    m[9]  =  (t + b) / (t - b);
    m[10] = -(f + n) / (f - n);
    m[11] = -1.0f;
    m[14] = -(2.0f * f * n) / (f - n);
    float *cur = (g_matMode == MM_PROJECTION) ? PR() : MV();
    float tmp[16];
    memcpy(tmp, cur, 16 * sizeof(float));
    mat_multiply(cur, tmp, m);
}

void qglTranslatef(float x, float y, float z)
{
    float m[16]; mat_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
    qglMultMatrixf(m);
}

void qglScalef(float x, float y, float z)
{
    float m[16]; mat_identity(m);
    m[0] = x; m[5] = y; m[10] = z;
    qglMultMatrixf(m);
}

void qglRotatef(float angle, float x, float y, float z)
{
    /* нормализуем ось */
    float len = sqrtf(x*x + y*y + z*z);
    if (len == 0.0f) return;
    x /= len; y /= len; z /= len;

    float rad = angle * (float)M_PI / 180.0f;
    float c = cosf(rad), s = sinf(rad), ic = 1.0f - c;

    float m[16] = {0};
    m[0]  = x*x*ic + c;    m[1]  = y*x*ic + z*s;  m[2]  = z*x*ic - y*s;
    m[4]  = x*y*ic - z*s;  m[5]  = y*y*ic + c;    m[6]  = z*y*ic + x*s;
    m[8]  = x*z*ic + y*s;  m[9]  = y*z*ic - x*s;  m[10] = z*z*ic + c;
    m[15] = 1.0f;
    qglMultMatrixf(m);
}

void qglPushMatrix(void)
{
    if (g_matMode == MM_PROJECTION) {
        if (g_prDepth < MAT_STACK_DEPTH - 1) {
            memcpy(g_matProjection[g_prDepth+1],
                   g_matProjection[g_prDepth], 16*sizeof(float));
            g_prDepth++;
        }
    } else {
        if (g_mvDepth < MAT_STACK_DEPTH - 1) {
            memcpy(g_matModelView[g_mvDepth+1],
                   g_matModelView[g_mvDepth], 16*sizeof(float));
            g_mvDepth++;
        }
    }
}

void qglPopMatrix(void)
{
    if (g_matMode == MM_PROJECTION) {
        if (g_prDepth > 0) g_prDepth--;
    } else {
        if (g_mvDepth > 0) g_mvDepth--;
    }
}

/* =========================================================================
 * No-op заглушки — ES1 client state API больше не нужна
 * ========================================================================= */

void pfglEnableClientState(GLenum cap)  { (void)cap; }
void pfglDisableClientState(GLenum cap) { (void)cap; }
void qglEnableClientState(GLenum cap)   { (void)cap; }
void qglDisableClientState(GLenum cap)  { (void)cap; }

void pfglVertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
    { (void)size; (void)type; (void)stride; (void)ptr; }
void pfglTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
    { (void)size; (void)type; (void)stride; (void)ptr; }
void pfglColorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
    { (void)size; (void)type; (void)stride; (void)ptr; }

/* glTexEnvf / glAlphaFunc — эмулированы через uniform в шейдере */
void qglTexEnvf(GLenum target, GLenum pname, GLfloat param)  { (void)target; (void)pname; (void)param; }
void qglTexEnvi(GLenum target, GLenum pname, GLint param)    { (void)target; (void)pname; (void)param; }
void qglAlphaFunc(GLenum func, GLclampf ref)
{
    /* GL_GREATER → u_alphaRef в шейдере */
    if (func == GL_GREATER)
        R_GLES2_SetAlphaRef(ref);
    else
        R_GLES2_SetAlphaRef(0.0f);
}
