#ifndef ENGINE_GL_RMISC_H
#define ENGINE_GL_RMISC_H

extern cvar_t gl_clear;

void D_FlushCaches();

extern cvar_t gl_spriteblend;
extern cvar_t	gl_flashblend;
extern cvar_t r_wadtextures;

extern qboolean	filterMode; // GL_RMISC.C:85
extern float		filterColorRed;
extern float		filterColorGreen;
extern float		filterColorBlue;
extern float		filterBrightness;

extern cvar_t r_traceglow;
extern cvar_t gl_affinemodels;
extern cvar_t r_glowshellfreq;
extern cvar_t gl_dither;

#endif //ENGINE_GL_RMISC_H
