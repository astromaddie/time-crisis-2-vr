/* Small stereo options panel. Reuses the upstream font; one draw and texture
 * uploads only when the displayed setting changes. No per-frame font rasterizing. */
#include "quest_gpu.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "quest_ui.h"
#include "quest_panel.h"
#include "quest_lobby.h"
static unsigned last_lobby_revision;
static bool last_lobby;

enum { WIDTH=640,HEIGHT=480 };
static GLuint program,vao,vbo,texture;
static GLint u_view,u_projection,u_texture;
static int last_state=-1;
static int last_pitch;
static uint8_t pixels[WIDTH*HEIGHT*4];

static void text(int x,int y,const char *s,uint8_t r,uint8_t g,uint8_t b){qpanel_text(pixels,x,y,s,r,g,b);}
static void panel(bool laser,bool physical_crouch,bool left_handed,bool frame_profile_active,int gun_pitch,bool saved){
#ifndef TCVR_STEAM_FRAME
    (void)frame_profile_active;
#endif
    for(int y=0;y<HEIGHT;y++)for(int x=0;x<WIDTH;x++){
        uint8_t *p=pixels+(y*WIDTH+x)*4;bool border=x<2||x>=WIDTH-2||y<2||y>=HEIGHT-2;
        p[0]=border?54:12;p[1]=border?183:19;p[2]=border?207:29;p[3]=border?255:240;
    }
    text(24,20,"PAUSE / OPTIONS",181,207,220);
    text(24,54,left_handed?"DEFAULT HAND: LEFT":"DEFAULT HAND: RIGHT",255,255,255);
    text(24,78,"RIGHT STICK CLICK: CHANGE DEFAULT",181,207,220);
    text(24,102,"EITHER TRIGGER: SELECT HAND + FIRE",181,207,220);
    char angle[40];snprintf(angle,sizeof angle,"GUN ANGLE: %+d DEG",gun_pitch);
    text(24,140,angle,255,255,255);
    text(24,164,"RIGHT STICK UP / DOWN: ADJUST",181,207,220);
    text(24,188,"LEFT STICK CLICK: RESET TO 0",181,207,220);
    text(24,224,"LASER:",255,255,255);text(160,224,laser?"ON":"OFF",laser?94:228,laser?226:235,laser?151:239);
#ifdef TCVR_STEAM_FRAME
    text(336,224,frame_profile_active?"RIGHT B: ON / OFF":(left_handed?"Y: ON / OFF":"B: ON / OFF"),181,207,220);
#else
    text(336,224,left_handed?"Y: ON / OFF":"B: ON / OFF",181,207,220);
#endif
    text(24,262,physical_crouch?"COVER: PHYSICAL DUCKING":"COVER: GRIP BUTTONS",255,255,255);
#ifdef TCVR_STEAM_FRAME
    text(24,286,frame_profile_active?"RIGHT X: CHANGE MODE":(left_handed?"B RIGHT: CHANGE MODE":"Y LEFT: CHANGE MODE"),181,207,220);
#else
    text(24,286,left_handed?"B RIGHT: CHANGE MODE":"Y LEFT: CHANGE MODE",181,207,220);
#endif
    text(24,320,physical_crouch?"UPRIGHT: OUT / DUCK: COVER":"HOLD EITHER GRIP: LEAVE COVER",255,255,255);
    if(physical_crouch){
#ifdef TCVR_STEAM_FRAME
        text(24,344,frame_profile_active?"VIEW HOLD 3s: RECENTER + HEIGHT":(left_handed?"A RIGHT: RESET UPRIGHT HEIGHT":"X LEFT: RESET UPRIGHT HEIGHT"),181,207,220);
        text(24,368,frame_profile_active?"VIEW HOLD 1s: RECENTER": "STAND OR SIT UPRIGHT FIRST",181,207,220);
#else
        text(24,344,left_handed?"A RIGHT: RESET UPRIGHT HEIGHT":"X LEFT: RESET UPRIGHT HEIGHT",181,207,220);
        text(24,368,"STAND OR SIT UPRIGHT FIRST",181,207,220);
#endif
    }else{
        text(24,344,"RELEASE BOTH: COVER / RELOAD",181,207,220);
#ifdef TCVR_STEAM_FRAME
        text(24,368,frame_profile_active?"VIEW HOLD 1s: RECENTER":(left_handed?"A RIGHT: RECENTER":"X LEFT: RECENTER"),181,207,220);
#else
        text(24,368,left_handed?"A RIGHT: RECENTER":"X LEFT: RECENTER",181,207,220);
#endif
    }
#ifdef TCVR_STEAM_FRAME
    text(24,404,"RIGHT STICK RIGHT: LAN PLAY",181,207,220);
    text(24,438,saved?(frame_profile_active?"VIEW TAP: BACK / RIGHT MENU: RESUME":"LEFT MENU: RESUME"):"SAVING FAILED",saved?170:255,saved?193:150,saved?206:150);
#else
    text(24,404,"RIGHT STICK RIGHT: LAN PLAY",181,207,220);
    text(24,438,saved?"LEFT MENU: RESUME":"SAVING FAILED",saved?170:255,saved?193:150,saved?206:150);
#endif
}
static GLuint shader(GLenum type,const char *source){
    GLuint s=glCreateShader(type);qgpu_shader_source(s,source);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char message[1024];glGetShaderInfoLog(s,sizeof message,NULL,message);fprintf(stderr,"[UI] shader failed: %s\n",message);glDeleteShader(s);return 0;}return s;
}
bool qui_init(void){
    GLuint vs=shader(GL_VERTEX_SHADER,"#version 300 es\nprecision highp float;layout(location=0) in vec3 position;layout(location=1) in vec2 texcoord;uniform mat4 view,projection;out vec2 uv;void main(){gl_Position=projection*view*vec4(position,1);uv=texcoord;}");
    GLuint fs=shader(GL_FRAGMENT_SHADER,"#version 300 es\nprecision highp float;in vec2 uv;uniform sampler2D panelTex;out vec4 frag;void main(){frag=texture(panelTex,uv);}");
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
    program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);
    GLint linked;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked){qui_shutdown();return false;}
    u_view=glGetUniformLocation(program,"view");u_projection=glGetUniformLocation(program,"projection");u_texture=glGetUniformLocation(program,"panelTex");
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenTextures(1,&texture);last_state=-1;
    return true;
}
#ifdef TCVR_STEAM_FRAME
void qui_draw(const float view[16],const float projection[16],V3 head,Q4 rotation,bool laser,bool physical_crouch,bool left_handed,bool frame_profile_active,int gun_pitch,bool saved){
#else
void qui_draw(const float view[16],const float projection[16],V3 head,Q4 rotation,bool laser,bool physical_crouch,bool left_handed,int gun_pitch,bool saved){
    const bool frame_profile_active=false;
#endif
    if(!program)return;
    GLint bound,unit;glGetIntegerv(GL_ACTIVE_TEXTURE,&unit);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);glBindTexture(GL_TEXTURE_2D,texture);
    int state=(physical_crouch?1:0)|(laser?2:0)|(saved?4:0)|(left_handed?8:0)|(frame_profile_active?16:0);
    bool lobby=qlobby_visible(); unsigned lobby_revision=qlobby_revision();
    if(last_state!=state||last_pitch!=gun_pitch||last_lobby!=lobby||(lobby&&last_lobby_revision!=lobby_revision)){
        if(lobby)qlobby_render(pixels);else panel(laser,physical_crouch,left_handed,frame_profile_active,gun_pitch,saved);
        last_lobby=lobby;last_lobby_revision=lobby_revision;last_pitch=gun_pitch;
        GLint row_length;glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row_length);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,WIDTH,HEIGHT,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);glPixelStorei(GL_UNPACK_ROW_LENGTH,row_length);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);last_state=state;
    }
    /* One panel 1.6 m from the head, with each eye's real view/projection. */
    const float xy[4][2]={{-.65f,.4875f},{.65f,.4875f},{.65f,-.4875f},{-.65f,-.4875f}};
    const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};const int order[6]={0,1,2,0,2,3};float vertices[6][5];
    for(int i=0;i<6;i++){int j=order[i];V3 p=add(head,rotate(rotation,v3(xy[j][0],xy[j][1],-1.6f)));vertices[i][0]=p.x;vertices[i][1]=p.y;vertices[i][2]=p.z;vertices[i][3]=uv[j][0];vertices[i][4]=uv[j][1];}
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);glUniformMatrix4fv(u_view,1,GL_FALSE,view);glUniformMatrix4fv(u_projection,1,GL_FALSE,projection);glUniform1i(u_texture,0);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glDrawArrays(GL_TRIANGLES,0,6);glDisable(GL_BLEND);glBindTexture(GL_TEXTURE_2D,bound);glActiveTexture(unit);
}
void qui_shutdown(void){glDeleteProgram(program);glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&vbo);glDeleteTextures(1,&texture);program=vao=vbo=texture=0;last_state=-1;}
