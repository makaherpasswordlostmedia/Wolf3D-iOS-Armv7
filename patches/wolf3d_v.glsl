/* wolf3d_v.glsl — Vertex Shader
 *
 * Заменяет fixed-function pipeline:
 *   glMatrixMode / glLoadIdentity / glOrtho / glFrustum
 *   glVertexPointer / glTexCoordPointer / glColorPointer
 *
 * Атрибуты соответствуют структуре Vertex в gles_glue.c:
 *   struct Vertex { float xyz[3]; float st[2]; GLubyte c[4]; };
 *
 * Используется для ВСЕХ проходов рендера:
 *   - 3D мир (стены, спрайты)   — u_mode = 0
 *   - 2D оверлеи / HUD          — u_mode = 1
 */

attribute vec3 a_position;   /* Vertex.xyz  */
attribute vec2 a_texcoord;   /* Vertex.st   */
attribute vec4 a_color;      /* Vertex.c / 255.0 */

uniform mat4 u_mvpMatrix;    /* projection * modelview */

varying vec2 v_texcoord;
varying vec4 v_color;

void main()
{
    gl_Position = u_mvpMatrix * vec4(a_position, 1.0);
    v_texcoord  = a_texcoord;
    v_color     = a_color;
}
