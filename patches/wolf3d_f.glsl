/* wolf3d_f.glsl — Fragment Shader
 *
 * Поддерживает три режима (u_renderMode):
 *
 *   0 — Textured + vertex color (стены, спрайты, HUD-иконки)
 *   1 — Color only, без текстуры (R_Draw_Fill: потолок/пол)
 *   2 — Textured, alpha из текстуры * vertex alpha (прозрачные спрайты)
 *
 * Заменяет:
 *   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE/GL_REPLACE)
 *   glAlphaFunc(GL_GREATER, ...)
 *   glColor4f / glColor4ub
 */

precision mediump float;

uniform sampler2D u_texture;
uniform int       u_renderMode;   /* 0 = tex*color, 1 = color only, 2 = tex alpha */
uniform float     u_alphaRef;     /* порог отсечения альфы (заменяет glAlphaFunc) */

varying vec2 v_texcoord;
varying vec4 v_color;

void main()
{
    vec4 color;

    if (u_renderMode == 1) {
        /* --- режим заливки (потолок/пол, R_Draw_Fill) --- */
        color = v_color;

    } else if (u_renderMode == 2) {
        /* --- спрайты: альфа из текстуры, цвет из vertex color --- */
        vec4 texel = texture2D(u_texture, v_texcoord);
        color = vec4(texel.rgb * v_color.rgb, texel.a * v_color.a);

        /* Alpha test — заменяет glAlphaFunc(GL_GREATER, u_alphaRef) */
        if (color.a <= u_alphaRef) {
            discard;
        }

    } else {
        /* --- режим 0: стены и всё остальное textured --- */
        vec4 texel = texture2D(u_texture, v_texcoord);
        color = texel * v_color;   /* GL_MODULATE */
    }

    gl_FragColor = color;
}
