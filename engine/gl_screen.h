#ifndef ENGINE_GL_SCREEN_H
#define ENGINE_GL_SCREEN_H

#if defined(GLQUAKE)

#include "wad.h"

extern int clearnotify;

extern float scr_con_current;
extern float scr_fov_value;
extern cvar_t scr_downloading;
extern int scr_copyeverything;

extern int scr_copytop;

extern int glx;
extern int gly;
extern int glwidth;
extern int glheight;

extern cvar_t scr_viewsize;
extern cvar_t scr_connectmsg;
extern cvar_t scr_graphheight;
extern float scr_centertime_off;

void SCR_Init();

void SCR_DrawConsole();

void SCR_CenterPrint( const char* str );

void Draw_CenterPic( qpic_t* pPic );

void SCR_DrawConsole( void );
void SCR_DrawLoading();

void SCR_UpdateScreen();

void SCR_BeginLoadingPlaque( qboolean reconnect );
void SCR_EndLoadingPlaque();

void SCR_CalcRefdef( void );

#endif // defined(GLQUAKE)

#endif //ENGINE_GL_SCREEN_H
