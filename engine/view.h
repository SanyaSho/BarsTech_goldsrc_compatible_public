/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef ENGINE_VIEW_H
#define ENGINE_VIEW_H

#include "shake.h"

extern	cvar_t		v_gamma;
extern	cvar_t		v_lambert;
extern	cvar_t		v_lightgamma;

extern	byte		texgammatable[256];	// palette is sent through this
extern	int			lightgammatable[1024];
extern	int			screengammatable[1024];
extern	int			lineargammatable[1024];
extern	float		v_lambert1;
#if defined(GLQUAKE)
extern	byte		ramps[3][256];
extern	float		v_blend[4];
#endif

extern cvar_t lcd_x;
extern cvar_t v_direct;

void V_RenderView(void);
void V_UpdatePalette(void);

void V_Init();

qboolean V_CheckGamma();
void V_InitLevel();

int V_FadeAlpha();
#if defined(GLQUAKE)
void V_CalcBlend();
#endif

#endif //ENGINE_VIEW_H
