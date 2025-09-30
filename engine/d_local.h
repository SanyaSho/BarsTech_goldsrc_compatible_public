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
// d_local.h:  private rasterization driver defs

#ifndef _D_LOCAL_H_
#define _D_LOCAL_H_

#include "r_shared.h"

//
// TODO: fine-tune this; it's based on providing some overage even if there
// is a 2k-wide scan, with subdivision every 8, for 256 spans of 12 bytes each
//
#define SCANBUFFERPAD		0x1000

#define R_SKY_SMASK	0x007F0000
#define R_SKY_TMASK	0x007F0000

#define DS_SPAN_LIST_END	-128

#define SURFCACHE_SIZE_AT_320X200	/*600*1024*/ /*3072*1024*/ 1024*1024

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s 	**owner;		// NULL is an empty chunk of memory
	int					lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int					dlight;
	int					size;		// including header
	unsigned			width;
	unsigned			height;		// DEBUG only needed for debug
	float				mipscale;
	struct texture_s	*texture;	// checked for animating textures
	byte				data[4];	// width*height elements
} surfcache_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct sspan_s
{
	int				u, v, count;
} sspan_t;

// TODO: put in span spilling to shrink list size
// !!! if this is changed, it must be changed in d_polysa.s too !!!
#define DPS_MAXSPANS			MAXHEIGHT+1	
// 1 extra for spanpackage that marks end

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
	void			*pdest;
	short			*pz;
	int				count;
	byte			*ptex;
	int				sfrac, tfrac, light, zi;
	//int zsize;
} spanpackage_t;

extern cvar_t	d_subdiv16;

extern float	scale_for_mip;

extern qboolean		d_roverwrapped;
extern surfcache_t	*sc_rover;
extern surfcache_t	*d_initial_rover;

extern "C" float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
extern "C" float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
extern "C" float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

extern "C" fixed16_t	sadjust, tadjust;
extern "C" fixed16_t	bbextents, bbextentt;
extern word *gWaterLastPalette, gWaterTextureBuffer[CYCLE * CYCLE];
extern colorVec d_fogtable[64][64];
extern short watertex[CYCLE * CYCLE];
extern short watertex2[CYCLE * CYCLE];

EXTERN_C void D_DrawSpans8 (espan_t *pspans);
EXTERN_C void D_DrawSpans16 (espan_t *pspans);
EXTERN_C void D_DrawZSpans (espan_t *pspans);
void WaterTextureUpdate (word * pPalette, float dropTime, texture_t* texture);
void D_DrawTiled8(espan_t* pspans);
void D_DrawTiled8Trans(espan_t* pspans);
void D_SpriteDrawSpans (sspan_t *pspan);

void D_DrawTranslucentColor(espan_t *pspan);
void D_DrawTranslucentTexture(espan_t *pspan);
void D_DrawTransHoles(espan_t *pspan);
void D_DrawTranslucentAdd(espan_t *pspan);

void D_SpriteDrawSpans(sspan_t* pspan);
void D_SpriteDrawSpansAdd(sspan_t* pspan);
void D_SpriteDrawSpansAlpha(sspan_t* pspan);
void D_SpriteDrawSpansGlow(sspan_t* pspan);
void D_SpriteDrawSpansTrans(sspan_t* pspan);

void D_PolysetDrawSpans8(spanpackage_t *pspanpackage);
void D_PolysetDrawSpansTransAdd(spanpackage_t *pspanpackage);
void D_PolysetDrawHole(spanpackage_t *pspanpackage);
void D_PolysetDrawSpansTrans(spanpackage_t *pspanpackage);

void D_DrawSkyScans8 (espan_t *pspan);
void D_DrawSkyScans16 (espan_t *pspan);

void R_ShowSubDiv (void);
surfcache_t	*D_CacheSurface (msurface_t *surface, int miplevel);
void D_SetFadeColor(int r, int g, int b, int fog);
void D_SetScreenFade(int r, int g, int b, int alpha, int type);
int LeafId(msurface_t* psurf);

extern int D_MipLevelForScale (float scale);
extern void TilingSetup(int sMask, int tMask, int tShift);

#if id386
extern "C" void D_PolysetAff8Start (void);
extern "C" void D_PolysetAff8End (void);
#endif

extern "C" short *d_pzbuffer;
extern "C" unsigned int d_zrowbytes, d_zwidth;

extern int	*d_pscantable;
extern "C" int	d_scantable[MAXHEIGHT];

extern "C" int	d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

extern "C" int	d_y_aspect_shift, d_pix_min, d_pix_max, d_pix_shift;
extern "C" int d_spanzi;
extern "C" int d_vox_min, d_vox_max, d_vrectright_vox, d_vrectbottom_vox;

extern float fp_64kx64k;

extern "C" pixel_t	*d_viewbuffer;

extern "C" short	*zspantable[MAXHEIGHT];

extern int d_depth;

extern int		d_minmip;
extern float	d_scalemip[3];

extern void (*d_drawspans) (espan_t *pspan);
extern void (*spritedraw) (sspan_t* pspan);
extern void (*polysetdraw)(spanpackage_t *pspanpackage);
extern cvar_t	d_spriteskip;

typedef int (*SurfaceCacheForResFn)(int, int);

extern SurfaceCacheForResFn D_SurfaceCacheForRes;

#endif
