//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid rgba

#ifndef DRAW_H
#define DRAW_H
#ifdef _WIN32
#pragma once
#endif

#include "wad.h"
#include "wrect.h"
#include "vgui/VGUI2.h"

struct cacheentry_t
{
	char			name[64];
	cache_user_t	cache;
};

//#define TRANSPARENT_COLOR 0xff

typedef struct cachewad_s cachewad_t;

typedef void (*PFNCACHE)(cachewad_t*, byte*);

#if !defined(GLQUAKE)
const DWORD overflow15 = 0x80200400, 
			overflow16 = 0x200800, 
			overflow16withred = 0x80100400; // shifted right by 1 to handle red overflow
const WORD mask_r_15 = ((1 << 5) - 1) << 10, mask_g_15 = ((1 << 5) - 1) << 5, mask_b_15 = ((1 << 5) - 1) << 0;
const DWORD mask_r_16 = ((1 << 5) - 1) << 11, mask_g_16 = ((1 << 6) - 1) << 5, mask_b_16 = ((1 << 5) - 1) << 0;
const unsigned int mask_r_32 = 0x00FF0000, mask_g_32 = 0x0000FF00, mask_b_32 = 0x000000FF, mask_a_32 = 0xFF000000;
#define ROR32(x) (((x & 1) << 31) | (x >> 1))
#define colorsplit(source, maskr, maskg, maskb) ( (WORD)(source & maskg) | (source & (maskr | maskb)) << 16 )
#define lowfrac16(color, mask) (color & mask)
#define highfrac16(color, mask) (color & mask) << 16
#define lowcleanmask(bits) ~((1 << bits) - 1)
#define diffcolor(color1, color2, topmask) 
// const int color6bitmax = (1 << 6) - 1, color5bitmax = (1 << 5) - 1;
#define colorsplitcustom(source, splitcolormask) ( lowfrac16(source, splitcolormask) | highfrac16(source, ~splitcolormask) )
#endif

const int lightmax16ub = (BYTE)~((1 << 6) - 1);
const float lightmax16f = (float(lightmax16ub) + 0.5f) * 256;
const float studiolightmax16f = float(lightmax16ub + 1) * 256 - 1;

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	cache_user_t	cache;
} cachepic_t;

typedef struct cachewad_s
{
	char* name;

#if defined ( GLQUAKE )
	cacheentry_t* cache;
#else
	cachepic_t* cache;
#endif

	int				cacheCount;
	int				cacheMax;
	lumpinfo_t* lumps;
	int				lumpCount;
	int				cacheExtra;
	PFNCACHE		pfnCacheBuild;
	int				numpaths;
	char** basedirs;
	int* lumppathindices;

#if defined ( GLQUAKE )
	int				tempWad;
#endif
} cachewad_t;

// Scissor
void	EnableScissorTest( int x, int y, int width, int height );
void	DisableScissorTest( void );

void	Draw_Init( void );
void	Draw_Shutdown( void );
void	Draw_SetTextColor( float r, float g, float b );
void	Draw_GetDefaultColor( void );
void	Draw_ResetTextColor( void );
int		Draw_Character( int x, int y, int num, unsigned int font );
void	Draw_DebugChar( char num );
void	Draw_Pic( int x, int y, qpic_t* pic );
void	Draw_TransPic( int x, int y, qpic_t* pic );
void	Draw_TransPicTranslate( int x, int y, qpic_t* pic, unsigned char* translation );
void	Draw_SpriteFrame( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect );
void	Draw_SpriteFrameHoles( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect );
void	Draw_SpriteFrameAdditive( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect );
void	Draw_SpriteFrameGeneric( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect, int src, int dest, int width, int height );
void	Draw_ConsoleBackground( int lines );
void	Draw_BeginDisc( void );
void	Draw_EndDisc( void );
void	Draw_FillRGB( int x, int y, int width, int height, int r, int g, int b );
void	Draw_FillRGBA( int x, int y, int width, int height, int r, int g, int b, int a );
void	Draw_FillRGBABlend( int x, int y, int width, int height, int r, int g, int b, int a );
void	Draw_LineRGB( int x1, int y1, int x2, int y2, int r, int g, int b );
void	Draw_ScaledRGBAImage( int x, int y, int wide, int tall, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba );
void	Draw_RGBAImage( int x, int y, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba, int istext, byte textr, byte textg, byte textb, byte globalAlphaScale );
void	Draw_RGBAImageAdditive( int x, int y, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba, int istext, byte textr, byte textg, byte textb, byte globalAlphaScale );
void	Draw_TileClear( int x, int y, int w, int h );
void	Draw_Fill( int x, int y, int w, int h, int c );
int		Draw_StringLen( const char* psz, unsigned int font );
int		Draw_MessageCharacterAdd( int x, int y, int num, int rr, int gg, int bb, unsigned int font );
int		Draw_String( int x, int y, char* str );
qpic_t* Draw_PicFromWad( char* name );
qpic_t* Draw_CachePic( char* path );
void	Draw_MiptexTexture( cachewad_t* wad, unsigned char* data );

#if !defined ( GLQUAKE )
void	GetRGB( short pal, colorVec* pcv );
short	RGBToPacked( byte* col );
void	Draw_FadeScreen( );

extern "C" short hlRGB(word* p, int i);

// 32-bit software stuff
unsigned int hlRGB32(word* p, int i);
unsigned int PutRGB32(colorVec* pcv);
unsigned int RGBToPacked32(byte* p);
unsigned int PackedRGB32(byte* p, unsigned char i);
void	GetRGB32( unsigned int c, colorVec* pcv );

void	Draw_SpriteFrame32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect);
void	Draw_SpriteFrameHoles32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect);
void	Draw_SpriteFrameAdditive32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect);
void	Draw_SpriteFrameGeneric32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect, int src, int dest, int width, int height);
#endif

extern qboolean m_bDrawInitialized;

#endif // DRAW_H