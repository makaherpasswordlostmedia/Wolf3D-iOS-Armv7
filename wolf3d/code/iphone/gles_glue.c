#include "gles_glue.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
typedef struct { float xyz[3]; float st[2]; GLubyte c[4]; } Vertex;
#define MAX_VERTS 16384
static Vertex immediate[MAX_VERTS];
static int curr_vertex=0;
static GLenum curr_prim=GL_TRIANGLES;
static Vertex g_cur;
#define MAT_STACK_DEPTH 16
typedef enum{MM_MODELVIEW=0,MM_PROJECTION=1}MatMode;
static MatMode g_matMode=MM_MODELVIEW;
static float g_matMV[MAT_STACK_DEPTH][16];
static float g_matPR[MAT_STACK_DEPTH][16];
static int g_mvDepth=0,g_prDepth=0;
static float*CurMat(void){return(g_matMode==MM_PROJECTION)?g_matPR[g_prDepth]:g_matMV[g_mvDepth];}
static void mat_identity(float*m){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1.f;}
static void mat_mul(float*out,const float*a,const float*b){
  float t[16];
  for(int r=0;r<4;r++)for(int c=0;c<4;c++){
    float s=0;for(int k=0;k<4;k++)s+=a[r+k*4]*b[k+c*4];t[r+c*4]=s;}
  memcpy(out,t,64);}
static GLuint g_prog=0,g_vbo=0;
static GLint g_aPos=-1,g_aTC=-1,g_aCol=-1,g_uMVP=-1,g_uTex=-1,g_uMode=-1,g_uARef=-1;
static const char*VERT=
  "attribute vec3 a_position;\n"
  "attribute vec2 a_texcoord;\n"
  "attribute vec4 a_color;\n"
  "uniform   mat4 u_mvpMatrix;\n"
  "varying   vec2 v_texcoord;\n"
  "varying   vec4 v_color;\n"
  "void main(){\n"
  "  gl_Position=u_mvpMatrix*vec4(a_position,1.0);\n"
  "  v_texcoord=a_texcoord;\n"
  "  v_color=a_color;\n"
  "}\n";
static const char*FRAG=
  "precision mediump float;\n"
  "uniform sampler2D u_texture;\n"
  "uniform int u_renderMode;\n"
  "uniform float u_alphaRef;\n"
  "varying vec2 v_texcoord;\n"
  "varying vec4 v_color;\n"
  "void main(){\n"
  "  vec4 color;\n"
  "  if(u_renderMode==1){\n"
  "    color=v_color;\n"
  "  }else if(u_renderMode==2){\n"
  "    vec4 t=texture2D(u_texture,v_texcoord);\n"
  "    color=vec4(t.rgb*v_color.rgb,t.a*v_color.a);\n"
  "    if(color.a<=u_alphaRef)discard;\n"
  "  }else{\n"
  "    color=texture2D(u_texture,v_texcoord)*v_color;\n"
  "  }\n"
  "  gl_FragColor=color;\n"
  "}\n";
static GLuint mk_shader(GLenum t,const char*src){
  GLuint s=glCreateShader(t);
  glShaderSource(s,1,&src,NULL);glCompileShader(s);
  GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok){char l[512];glGetShaderInfoLog(s,512,NULL,l);printf("Shader:%s\n",l);}
  return s;}
void R_GLES2_Init(void){
  GLuint vs=mk_shader(GL_VERTEX_SHADER,VERT);
  GLuint fs=mk_shader(GL_FRAGMENT_SHADER,FRAG);
  g_prog=glCreateProgram();
  glAttachShader(g_prog,vs);glAttachShader(g_prog,fs);
  glBindAttribLocation(g_prog,0,"a_position");
  glBindAttribLocation(g_prog,1,"a_texcoord");
  glBindAttribLocation(g_prog,2,"a_color");
  glLinkProgram(g_prog);glDeleteShader(vs);glDeleteShader(fs);
  glUseProgram(g_prog);
  g_aPos=glGetAttribLocation(g_prog,"a_position");
  g_aTC=glGetAttribLocation(g_prog,"a_texcoord");
  g_aCol=glGetAttribLocation(g_prog,"a_color");
  g_uMVP=glGetUniformLocation(g_prog,"u_mvpMatrix");
  g_uTex=glGetUniformLocation(g_prog,"u_texture");
  g_uMode=glGetUniformLocation(g_prog,"u_renderMode");
  g_uARef=glGetUniformLocation(g_prog,"u_alphaRef");
  glUniform1i(g_uTex,0);glUniform1i(g_uMode,0);glUniform1f(g_uARef,0.f);
  glGenBuffers(1,&g_vbo);
  glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
  glBufferData(GL_ARRAY_BUFFER,sizeof(immediate),NULL,GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,0);
  mat_identity(g_matMV[0]);mat_identity(g_matPR[0]);
  g_mvDepth=g_prDepth=0;
  g_cur.c[0]=g_cur.c[1]=g_cur.c[2]=g_cur.c[3]=255;}
void R_GLES2_Shutdown(void){
  if(g_vbo){glDeleteBuffers(1,&g_vbo);g_vbo=0;}
  if(g_prog){glDeleteProgram(g_prog);g_prog=0;}}
void R_GLES2_SetRenderMode(int m){glUniform1i(g_uMode,m);}
void R_GLES2_SetAlphaRef(float r){glUniform1f(g_uARef,r);}
static void upload_mvp(void){
  float mvp[16];mat_mul(mvp,g_matPR[g_prDepth],g_matMV[g_mvDepth]);
  glUniformMatrix4fv(g_uMVP,1,GL_FALSE,mvp);}
void pfglBegin(GLenum p){curr_vertex=0;curr_prim=p;}
void pfglEnd(void){
  if(!curr_vertex)return;
  upload_mvp();
  glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
  glBufferSubData(GL_ARRAY_BUFFER,0,curr_vertex*sizeof(Vertex),immediate);
  glEnableVertexAttribArray(g_aPos);
  glEnableVertexAttribArray(g_aTC);
  glEnableVertexAttribArray(g_aCol);
  glVertexAttribPointer(g_aPos,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
  glVertexAttribPointer(g_aTC,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)12);
  glVertexAttribPointer(g_aCol,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vertex),(void*)20);
  glDrawArrays(curr_prim,0,curr_vertex);
  glDisableVertexAttribArray(g_aPos);
  glDisableVertexAttribArray(g_aTC);
  glDisableVertexAttribArray(g_aCol);
  glBindBuffer(GL_ARRAY_BUFFER,0);
  curr_vertex=0;}
void pfglTexCoord2f(float s,float t){g_cur.st[0]=s;g_cur.st[1]=t;}
void pfglVertex3f(float x,float y,float z){
  if(curr_vertex>=MAX_VERTS)return;
  g_cur.xyz[0]=x;g_cur.xyz[1]=y;g_cur.xyz[2]=z;
  immediate[curr_vertex++]=g_cur;}
void pfglVertex2f(float x,float y){pfglVertex3f(x,y,0.f);}
void pfglColor4f(float r,float g,float b,float a){
  g_cur.c[0]=(GLubyte)(r*255);g_cur.c[1]=(GLubyte)(g*255);
  g_cur.c[2]=(GLubyte)(b*255);g_cur.c[3]=(GLubyte)(a*255);}
void pfglColor4ub(GLubyte r,GLubyte g,GLubyte b,GLubyte a){
  g_cur.c[0]=r;g_cur.c[1]=g;g_cur.c[2]=b;g_cur.c[3]=a;}
void pfglColor3f(float r,float g,float b){pfglColor4f(r,g,b,1.f);}
void qglMatrixMode(GLenum m){g_matMode=(m==GL_PROJECTION)?MM_PROJECTION:MM_MODELVIEW;}
void qglLoadIdentity(void){mat_identity(CurMat());}
void qglLoadMatrixf(const float*m){memcpy(CurMat(),m,64);}
void qglMultMatrixf(const float*m){float*c=CurMat(),t[16];memcpy(t,c,64);mat_mul(c,t,m);}
void qglOrtho(float l,float r,float b,float t,float n,float f){
  float m[16]={0};
  m[0]=2.f/(r-l);m[5]=2.f/(t-b);m[10]=-2.f/(f-n);
  m[12]=-(r+l)/(r-l);m[13]=-(t+b)/(t-b);m[14]=-(f+n)/(f-n);m[15]=1.f;
  qglMultMatrixf(m);}
void qglFrustum(float l,float r,float b,float t,float n,float f){
  float m[16]={0};
  m[0]=(2.f*n)/(r-l);m[5]=(2.f*n)/(t-b);
  m[8]=(r+l)/(r-l);m[9]=(t+b)/(t-b);
  m[10]=-(f+n)/(f-n);m[11]=-1.f;m[14]=-(2.f*f*n)/(f-n);
  qglMultMatrixf(m);}
void qglTranslatef(float x,float y,float z){
  float m[16];mat_identity(m);m[12]=x;m[13]=y;m[14]=z;qglMultMatrixf(m);}
void qglScalef(float x,float y,float z){
  float m[16];mat_identity(m);m[0]=x;m[5]=y;m[10]=z;qglMultMatrixf(m);}
void qglRotatef(float a,float x,float y,float z){
  float len=sqrtf(x*x+y*y+z*z);if(!len)return;
  x/=len;y/=len;z/=len;
  float rad=a*3.14159265f/180.f,c=cosf(rad),s=sinf(rad),ic=1.f-c;
  float m[16]={0};
  m[0]=x*x*ic+c;m[1]=y*x*ic+z*s;m[2]=z*x*ic-y*s;
  m[4]=x*y*ic-z*s;m[5]=y*y*ic+c;m[6]=z*y*ic+x*s;
  m[8]=x*z*ic+y*s;m[9]=y*z*ic-x*s;m[10]=z*z*ic+c;m[15]=1.f;
  qglMultMatrixf(m);}
void qglPushMatrix(void){
  if(g_matMode==MM_PROJECTION){
    if(g_prDepth<MAT_STACK_DEPTH-1){memcpy(g_matPR[g_prDepth+1],g_matPR[g_prDepth],64);g_prDepth++;}}
  else{if(g_mvDepth<MAT_STACK_DEPTH-1){memcpy(g_matMV[g_mvDepth+1],g_matMV[g_mvDepth],64);g_mvDepth++;}}}
void qglPopMatrix(void){
  if(g_matMode==MM_PROJECTION){if(g_prDepth>0)g_prDepth--;}
  else{if(g_mvDepth>0)g_mvDepth--;}}
void pfglEnableClientState(GLenum c){(void)c;}
void pfglDisableClientState(GLenum c){(void)c;}
void qglEnableClientState(GLenum c){(void)c;}
void qglDisableClientState(GLenum c){(void)c;}
void pfglVertexPointer(GLint s,GLenum t,GLsizei st,const void*p){(void)s;(void)t;(void)st;(void)p;}
void pfglTexCoordPointer(GLint s,GLenum t,GLsizei st,const void*p){(void)s;(void)t;(void)st;(void)p;}
void pfglColorPointer(GLint s,GLenum t,GLsizei st,const void*p){(void)s;(void)t;(void)st;(void)p;}
void qglTexEnvf(GLenum a,GLenum b,GLfloat c){(void)a;(void)b;(void)c;}
void qglTexEnvi(GLenum a,GLenum b,GLint c){(void)a;(void)b;(void)c;}
void qglAlphaFunc(GLenum f,GLclampf r){R_GLES2_SetAlphaRef((f==GL_GREATER)?r:0.f);}
