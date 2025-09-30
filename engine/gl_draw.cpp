// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#include "cdll_int.h"
#include "gl_model.h"
#include "decals.h"
#include "sv_main.h"
#include "vid.h"
#include "screen.h"
#include "wad.h"
//#include "ivideomode.h"
#include "sys_getmodes.h"
#include "vgui2/text_draw.h"
#include "gl_rmain.h"
#include "gl_rmisc.h"
#include "gl_vidnt.h"
#include "gl_screen.h"
#include "host.h"
#include "buildnum.h"
#include "view.h"
#include "host_cmd.h"
#include "glquake.h"


int gl_FilterTexture;

cvar_t gl_max_size = { const_cast<char*>("gl_max_size"), const_cast<char*>("256"), FCVAR_ARCHIVE };
cvar_t gl_round_down = { const_cast<char*>("gl_round_down"), const_cast<char*>("3"), FCVAR_ARCHIVE };
cvar_t gl_picmip = { const_cast<char*>("gl_picmip"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t gl_palette_tex = { const_cast<char*>("gl_palette_tex"), const_cast<char*>("1") };

qpic_t* draw_disc = NULL;
qpic_t* draw_backtile = NULL;

int translate_texture;

struct glpic_t
{
	int		texnum;
	float	sl, tl, sh, th;
};

int		gl_lightmap_format = GL_RGBA;
//int		gl_solid_format = 3;
//int		gl_alpha_format = 4;

int gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int gl_filter_max = GL_LINEAR;
void gl_texturemode_hook_callback( cvar_t* cvar );

cvar_t gl_texturemode = { const_cast<char*>("gl_texturemode"), const_cast<char*>("GL_LINEAR_MIPMAP_LINEAR"), FCVAR_ARCHIVE };
cvar_t gl_ansio = { const_cast<char*>("gl_ansio"), const_cast<char*>("16"), FCVAR_ARCHIVE };
cvarhook_t gl_texturemode_hook = { gl_texturemode_hook_callback, 0, 0 };

int		texels;

struct GL_PALETTE
{
	int tag;
	int referenceCount;

	byte colors[768];
};

GL_PALETTE gGLPalette[350];

#define	MAX_CACHED_PICS		16
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;


cachewad_t custom_wad;

struct gltexture_t
{
	int			texnum;
	short		servercount;
	short		paletteIndex;
	int			width, height;
	qboolean	mipmap;
	char		identifier[MAX_QPATH];
};

#define	MAX_GLTEXTURES	4800
gltexture_t gltextures[MAX_GLTEXTURES];
int numgltextures;

extern char szCustName[10];

extern qboolean gfCustomBuild;
extern qboolean g_bSupportsNPOTTextures;

extern cvar_t gl_dither;

extern cachewad_t* menu_wad;

qboolean m_bDrawInitialized = false;

void GL_PaletteSelect( int paletteIndex );
void GL_PaletteInit( void );
void GL_PaletteClear( int index );

qpic_t* LoadTransBMP( char* pszName );
qpic_t* LoadTransPic( char* pszName, qpic_t* ppic );

void Download4444( void )
{
}

//qboolean IsPowerOfTwo( int value )
//{
//	int i;
//
//	for (i = 0; i < 32; i++)
//	{
//		if (1 << i == value)
//			return TRUE;
//	}
//
//	return FALSE;
//}

void GL_Bind( int texnum )
{
	int paletteIndex;

	paletteIndex = (texnum >> 16) - 1;
	if (currenttexture == texnum)
		return;

	currenttexture = texnum;
	qglBindTexture(GL_TEXTURE_2D, texnum);

	if (paletteIndex >= 0)
		GL_PaletteSelect(paletteIndex);
}

void GL_Texels_f( void )
{
	Con_Printf(const_cast<char*>("Current uploaded texels: %i\n"), texels);
}

/****************************************/

GLenum oldtarget = TEXTURE0_SGIS;

void GL_SelectTexture( GLenum target )
{
	if (!gl_mtexable)
		return;

	qglSelectTextureSGIS(target);

	if (target == oldtarget)
		return;

	cnttextures[oldtarget - TEXTURE0_SGIS] = currenttexture;
	currenttexture = cnttextures[target - TEXTURE0_SGIS];
	oldtarget = target;
}

//=============================================================================
/* Support Routines */

qpic_t* Draw_PicFromWad( char* name )
{
	qpic_t* p;
	glpic_t* gl;

	p = (qpic_t*)W_GetLumpName(0, name);
	gl = (glpic_t*)p->data;

	gl->texnum = GL_LoadPicTexture(p, name);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}

qpic_t* Draw_CachePic( char* path )
{
	qpic_t* ret;
	int idx;

	idx = Draw_CacheIndex(menu_wad, path);
	ret = (qpic_t*)Draw_CacheGet(menu_wad, idx);

	return ret;
}

/*
===============
Draw_StringLen
===============
*/
int Draw_StringLen( const char* psz, unsigned int font )
{
	return VGUI2_Draw_StringLen(psz, font);
}

int Draw_MessageFontInfo( short* pWidth, unsigned int font )
{
	return VGUI2_GetFontTall(font);
}

/*
===============
Draw_MessageCharacterAdd

Draws a single character using the specified font, using additive rendering
===============
*/
int Draw_MessageCharacterAdd( int x, int y, int num, int rr, int gg, int bb, unsigned int font )
{
	return VGUI2_Draw_CharacterAdd(x, y, num, rr, gg, bb, font);
}

void Draw_CharToConback( int num, unsigned char* dest )
{
}

struct quake_mode_t
{
	char* name;
	int	minimize, maximize;
};

quake_mode_t modes[] =
{
	{ const_cast<char*>("GL_NEAREST"), GL_NEAREST, GL_NEAREST },
	{ const_cast<char*>("GL_LINEAR"), GL_LINEAR, GL_LINEAR },
	{ const_cast<char*>("GL_NEAREST_MIPMAP_NEAREST"), GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ const_cast<char*>("GL_LINEAR_MIPMAP_NEAREST"), GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ const_cast<char*>("GL_NEAREST_MIPMAP_LINEAR"), GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ const_cast<char*>("GL_LINEAR_MIPMAP_LINEAR"), GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

/*
===============
gl_texturemode_hook_callback
===============
*/
void gl_texturemode_hook_callback( cvar_t* cvar )
{
	int i;
	gltexture_t* glt;
	const char* mode;

	if (!cvar)
		return;

	for (i = 0; i < Q_ARRAYSIZE(modes); i++)
	{
		mode = modes[i].name;

		if (!Q_stricmp(mode, cvar->string))
			break;
	}

	if (i >= Q_ARRAYSIZE(modes))
	{
		Con_Printf(const_cast<char*>("bad filter name\n"));
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0; i < numgltextures; i++)
	{
		glt = &gltextures[i];

		if (glt->mipmap)
		{
			GL_Bind(glt->texnum);

			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
		}
	}
}

/*
===============
Draw_Init

Initialize Draw
===============
*/
void Draw_Init( void )
{
	int i;
	int vid_level;

	m_bDrawInitialized = TRUE;

	VGUI2_Draw_Init();

	Cmd_AddCommand(const_cast<char*>("gl_texels"), GL_Texels_f);

	Cvar_RegisterVariable(&gl_max_size);
	Cvar_RegisterVariable(&gl_round_down);
	Cvar_RegisterVariable(&gl_picmip);
	Cvar_RegisterVariable(&gl_palette_tex);
	Cvar_RegisterVariable(&gl_texturemode);
	Cvar_RegisterVariable(&gl_ansio);

	Cvar_HookVariable(gl_texturemode.name, &gl_texturemode_hook);

	vid_level = Host_GetVideoLevel();
	if (vid_level > 0)
	{
		Cvar_DirectSet(&gl_ansio, const_cast<char*>("4"));
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
	}

	GL_PaletteInit();

	menu_wad = (cachewad_t*)Mem_ZeroMalloc(sizeof(cachewad_t));
	Draw_CacheWadInit(const_cast<char*>("cached.wad"), 16, menu_wad);
	menu_wad->tempWad = 1;
	Draw_CacheWadHandler(&custom_wad, Draw_MiptexTexture, DECAL_EXTRASIZE);

	if (qglColorTableEXT && gl_palette_tex.value)
		qglEnable(GL_SHARED_TEXTURE_PALETTE_EXT);

	Q_memset(decal_names, 0, sizeof(decal_names));

	for (i = 0; i < 256; i++)
		texgammatable[i] = i;

	//
	// get the other pics we need
	//
	draw_disc = LoadTransBMP(const_cast<char*>("lambda"));

	Draw_ResetTextColor();
}

void Draw_ResetTextColor( void )
{
	Draw_GetDefaultColor();
}

/*
===============
Draw_SetTextColor

Sets the text color
===============
*/
void Draw_SetTextColor( float r, float g, float b )
{
	RecEngDrawSetTextColor(r, g, b);

	VGUI2_Draw_SetTextColor((int)(r * 255.0), (int)(g * 255.0), (int)(b * 255.0));
}

void Draw_GetDefaultColor( void )
{
	int ir, ig, ib;
	int num;

	num = sscanf(con_color.string, "%i %i %i", &ir, &ig, &ib);
	if (num == 3)
	{
		VGUI2_Draw_SetTextColor(ir, ig, ib);
	}
}

/*
===============
Draw_Character

Draws a single character
===============
*/
int Draw_Character( int x, int y, int num, unsigned int font )
{
	return VGUI2_Draw_Character(x, y, num, font);
}

/*
===============
Draw_String
===============
*/
int Draw_String( int x, int y, char* str )
{
	int wide;

	wide = VGUI2_DrawString(x, y, str, VGUI2_GetConsoleFont());
	Draw_ResetTextColor();

	return wide + x;
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar( char num )
{
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic( int x, int y, qpic_t* pic )
{
	glpic_t* gl;

	if (!pic)
		return;

	VGUI2_ResetCurrentTexture();

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
	qglDisable(GL_DEPTH_TEST);
	qglEnable(GL_ALPHA_TEST);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	gl = (glpic_t*)pic->data;
	qglColor4f(1, 1, 1, 1);

	GL_Bind(gl->texnum);

	qglBegin(GL_QUADS);

	qglTexCoord2f(gl->sl, gl->tl);
	qglVertex2f(x, y);

	qglTexCoord2f(gl->sh, gl->tl);
	qglVertex2f(x + pic->width, y);

	qglTexCoord2f(gl->sh, gl->th);
	qglVertex2f(x + pic->width, y + pic->height);

	qglTexCoord2f(gl->sl, gl->th);
	qglVertex2f(x, y + pic->height);

	qglEnd();
}

void Draw_AlphaSubPic( int xDest, int yDest, int xSrc, int ySrc, int iWidth, int iHeight, qpic_t* pPic, colorVec* pc, int iAlpha )
{
	glpic_t* gl;

	float	flX, flY;
	float	flHeight;
	float	flWidth;
	float	alpha;

	if (!pPic)
		return;

	VGUI2_ResetCurrentTexture();

	qglBlendFunc(1, 1);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglEnable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);

	flWidth = (float)iWidth / (float)pPic->width;
	flHeight = (float)iHeight / (float)pPic->height;

	flX = (float)xSrc / (float)pPic->width;
	flY = (float)ySrc / (float)pPic->height;

	alpha = iAlpha / 255.0;

	gl = (glpic_t*)pPic->data;
	qglColor4f((pc->r * alpha) / 256.0, (pc->g * alpha) / 256.0, (pc->b * alpha) / 256.0, 1);

	GL_Bind(gl->texnum);

	qglBegin(GL_QUADS);

	qglTexCoord2f(flX, flY);
	qglVertex2f(xDest, yDest);

	qglTexCoord2f(flX + flWidth, flY);
	qglVertex2f(iWidth + xDest, yDest);

	qglTexCoord2f(flX + flWidth, flY + flHeight);
	qglVertex2f(iWidth + xDest, yDest + iHeight);

	qglTexCoord2f(flX, flY + flHeight);
	qglVertex2f(xDest, yDest + iHeight);

	qglEnd();

	qglDisable(GL_BLEND);
}

void Draw_AlphaAddPic( int x, int y, qpic_t* pic, colorVec* pc, int iAlpha )
{
	glpic_t* gl;

	if (!pic)
		return;

	VGUI2_ResetCurrentTexture();

	qglEnable(GL_TEXTURE_2D);

	qglBlendFunc(GL_SRC_ALPHA, 1);

	qglEnable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (pc)
		qglColor4f((float)pc->r / 255.0, (float)pc->g / 255.0, (float)pc->b / 255.0, (float)iAlpha / 255.0);
	else
		qglColor4f(1, 1, 1, (float)iAlpha / 255.0);

	gl = (glpic_t*)pic->data;
	GL_Bind(gl->texnum);

	qglBegin(GL_QUADS);

	qglTexCoord2f(gl->sl, gl->tl);
	qglVertex2f(x, y);

	qglTexCoord2f(gl->sh, gl->tl);
	qglVertex2f(x + pic->width, y);

	qglTexCoord2f(gl->sh, gl->th);
	qglVertex2f(x + pic->width, y + pic->height);

	qglTexCoord2f(gl->sl, gl->th);
	qglVertex2f(x, y + pic->height);

	qglEnd();

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
}

void Draw_Pic2( int x, int y, int w, int h, qpic_t* pic )
{
	glpic_t* gl;

	if (!pic)
		return;

	VGUI2_ResetCurrentTexture();

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	gl = (glpic_t*)pic->data;
	GL_Bind(gl->texnum);

	qglBegin(GL_QUADS);

	qglTexCoord2f(gl->sl, gl->tl);
	qglVertex2f(x, y);

	qglTexCoord2f(gl->sh, gl->tl);
	qglVertex2f(x + w, y);

	qglTexCoord2f(gl->sh, gl->th);
	qglVertex2f(x + w, h - 1 + y);

	qglTexCoord2f(gl->sl, gl->th);
	qglVertex2f(x, h - 1 + y);

	qglEnd();
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic( int x, int y, qpic_t* pic )
{
	if (!pic)
		return;

	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		(unsigned)(y + pic->height) > vid.height)
	{
		Sys_Error("Draw_TransPic: bad coordinates");
	}

	Draw_Pic(x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate( int x, int y, qpic_t* pic, unsigned char* translation )
{
	int				v, u, c;
	unsigned		trans[64 * 64], * dest;
	byte* src;
	int				p;

	if (!pic)
		return;

	VGUI2_ResetCurrentTexture();

	translate_texture = GL_GenTexture();

	GL_Bind(translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v = 0; v < 64; v++, dest += 64)
	{
		src = &menuplyr_pixels[((v * pic->height) >> 6) * pic->width];
		for (u = 0; u < 64; u++)
		{
			p = src[(u * pic->width) >> 6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] = 0xFF0000FF;
		}
	}

	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8_EXT, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);

	qglColor3f(1, 1, 1);
	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(x, y);
	qglTexCoord2f(1, 0);
	qglVertex2f(x + pic->width, y);
	qglTexCoord2f(1, 1);
	qglVertex2f(x + pic->width, y + pic->height);
	qglTexCoord2f(0, 1);
	qglVertex2f(x, y + pic->height);
	qglEnd();
}

// Sprites are clipped to this rectangle (x,y,width,height) if ScissorTest is enabled
int scissor_x, scissor_y, scissor_width, scissor_height;
qboolean giScissorTest;

/*
===============
EnableScissorTest

Set the scissor
 the coordinate system for gl is upsidedown (inverted-y) as compared to software, so the
 specified clipping rect must be flipped
===============
*/
void EnableScissorTest( int x, int y, int width, int height )
{
	// Added casts to int because these warnings are so annoying
	x = clamp(x, 0, (int)vid.width);
	y = clamp(y, 0, (int)vid.height);
	width = clamp(width, 0, (int)vid.width - x);
	height = clamp(height, 0, (int)vid.height - y);

	scissor_x = x;
	scissor_width = width;
	scissor_y = y;
	scissor_height = height;

	giScissorTest = TRUE;
}

/*
===============
DisableScissorTest
===============
*/
void DisableScissorTest( void )
{
	scissor_x = 0;
	scissor_y = 0;

	scissor_width = 0;
	scissor_height = 0;

	giScissorTest = FALSE;
}

/*
===============
ValidateWRect

Verify that this is a valid, properly ordered rectangle.
===============
*/
int ValidateWRect( const wrect_t* prc )
{
	if (!prc)
		return FALSE;

	if ((prc->left >= prc->right) || (prc->top >= prc->bottom))
	{
		//!!!UNDONE Dev only warning msg
		return FALSE;
	}

	return TRUE;
}

/*
===============
IntersectWRect

classic interview question
===============
*/
int IntersectWRect( const wrect_t* prc1, const wrect_t* prc2, wrect_t* prc )
{
	wrect_t rc;

	if (!prc)
		prc = &rc;

	prc->left = max(prc1->left, prc2->left);
	prc->right = min(prc1->right, prc2->right);

	if (prc->left < prc->right)
	{
		prc->top = max(prc1->top, prc2->top);
		prc->bottom = min(prc1->bottom, prc2->bottom);

		if (prc->top < prc->bottom)
			return TRUE;
	}

	return FALSE;
}

/*
===============
AdjustSubRect
===============
*/
void AdjustSubRect( mspriteframe_t* pFrame, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom, int* pw, int* ph, const wrect_t* prcSubRect )
{
	wrect_t rc;
	float f;

	if (!ValidateWRect(prcSubRect))
		return;

	// clip sub rect to sprite

	rc.top = rc.left = 0;
	rc.right = *pw;
	rc.bottom = *ph;

	if (!IntersectWRect(prcSubRect, &rc, &rc))
		return;

	*pw = rc.right - rc.left;
	*ph = rc.bottom - rc.top;

	f = 1.0 / (float)pFrame->width;
	*pfLeft = ((float)rc.left + 0.5) * f;
	*pfRight = ((float)rc.right - 0.5) * f;

	f = 1.0 / (float)pFrame->height;
	*pfTop = ((float)rc.top + 0.5) * f;
	*pfBottom = ((float)rc.bottom - 0.5) * f;
}

/*
===============
Draw_Frame
===============
*/
void Draw_Frame( mspriteframe_t* pFrame, int ix, int iy, const wrect_t* prcSubRect )
{
	float	x, y;
	float	fLeft = 0;
	float	fRight = 1;
	float	fTop = 0;
	float	fBottom = 1;
	int		iWidth, iHeight;

	iWidth = pFrame->width;
	iHeight = pFrame->height;

	x = (float)ix + 0.5;
	y = (float)iy + 0.5;

	VGUI2_ResetCurrentTexture();

	if (giScissorTest)
	{
		qglScissor(scissor_x, scissor_y, scissor_width, scissor_height);
		qglEnable(GL_SCISSOR_TEST);
	}

	if (prcSubRect)
	{
		AdjustSubRect(pFrame, &fLeft, &fRight, &fTop, &fBottom, &iWidth, &iHeight, prcSubRect);
	}

	qglDepthMask(GL_FALSE);

	GL_Bind(pFrame->gl_texturenum);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglBegin(GL_QUADS);

	qglTexCoord2f(fLeft, fTop);
	qglVertex2f(x, y);

	qglTexCoord2f(fRight, fTop);
	qglVertex2f((GLfloat)(iWidth + x), y);

	qglTexCoord2f(fRight, fBottom);
	qglVertex2f((GLfloat)(iWidth + x), (GLfloat)iHeight + y);

	qglTexCoord2f(fLeft, fBottom);
	qglVertex2f(x, (GLfloat)iHeight + y);

	qglEnd();

	qglDepthMask(GL_TRUE);
	qglDisable(GL_SCISSOR_TEST);
}

void Draw_SpriteFrame( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect )
{
	Draw_Frame(pFrame, x, y, prcSubRect);
}

void Draw_SpriteFrameHoles( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect )
{
	qglEnable(GL_ALPHA_TEST);

	if (gl_spriteblend.value)
	{
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable(GL_BLEND);
	}

	Draw_Frame(pFrame, x, y, prcSubRect);

	qglDisable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
}

void Draw_SpriteFrameAdditive( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect )
{
	qglEnable(GL_BLEND);
	qglBlendFunc(1, 1);

	Draw_Frame(pFrame, x, y, prcSubRect);

	qglDisable(GL_BLEND);
}

void Draw_SpriteFrameGeneric( mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect, int src, int dest, int width, int height )
{
	int		oldWidth, oldHeight;

	oldWidth = pFrame->width;
	oldHeight = pFrame->height;

	pFrame->width = width;
	pFrame->height = height;

	qglEnable(GL_BLEND);
	qglBlendFunc(src, dest);

	Draw_Frame(pFrame, x, y, prcSubRect);

	qglDisable(GL_BLEND);

	pFrame->width = oldWidth;
	pFrame->height = oldHeight;
}

/*
================
Draw_ConsoleBackground
================
*/
void Draw_ConsoleBackground( int lines )
{
	char ver[100];

	qglDisable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);

	qglColor4f(0, 0, 0, 1);

	qglBegin(GL_QUADS);

	qglVertex2f(0, lines - glheight);
	qglVertex2f(glwidth, lines - glheight);
	qglVertex2f(glwidth, lines);
	qglVertex2f(0, lines);

	qglEnd();

	qglEnable(GL_TEXTURE_2D);
	qglEnable(GL_ALPHA_TEST);

	snprintf(ver, sizeof(ver), "Half-Life %i/%s (hw build %d)", PROTOCOL_VERSION, gpszVersionString, build_number());

	if (console.value && (giSubState & 4) == 0)
		Draw_String(vid.conwidth - VGUI2_Draw_StringLen(ver, VGUI2_GetConsoleFont()), 0, ver);
}

/*
===============
Draw_FillRGBA

Fills the given rectangle with a given color
===============
*/
void Draw_FillRGBA( int x, int y, int w, int h, int r, int g, int b, int a )
{
	RecEngDraw_FillRGBA(x, y, w, h, r, g, b, a);

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_BLEND);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, 1);

	qglColor4f(r / 255.0, g / 255.0, b / 255.0, a / 255.0);

	qglBegin(GL_QUADS);
	qglVertex2f(x, y);
	qglVertex2f(w + x, y);
	qglVertex2f(w + x, h + y);
	qglVertex2f(x, h + y);
	qglEnd();

	qglColor3f(1, 1, 1);

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}

/*
===============
Draw_FillRGBABlend

Fills the given rectangle with a given color
Blends with existing pixel data
===============
*/
void Draw_FillRGBABlend( int x, int y, int w, int h, int r, int g, int b, int a )
{
	RecEngDraw_FillRGBA(x, y, w, h, r, g, b, a);

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_BLEND);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglColor4f(r / 255.0, g / 255.0, b / 255.0, a / 255.0);

	qglBegin(GL_QUADS);
	qglVertex2f(x, y);
	qglVertex2f(w + x, y);
	qglVertex2f(w + x, h + y);
	qglVertex2f(x, h + y);
	qglEnd();

	qglColor3f(1, 1, 1);

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear( int x, int y, int w, int h )
{
	Draw_FillRGBABlend(x, y, w, h, 0, 0, 0, 255);
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill( int x, int y, int w, int h, int c )
{
	Draw_FillRGBA(x, y, w, h,
		host_basepal[c * 4 + 0],
		host_basepal[c * 4 + 1],
		host_basepal[c * 4 + 2],
		255);
}
//=============================================================================

/*
================
Draw_FadeScreen
================
*/
void Draw_FadeScreen( void )
{
	qglEnable(GL_BLEND);
	qglDisable(GL_TEXTURE_2D);

	qglColor4f(0, 0, 0, 0.8);

	qglBegin(GL_QUADS);

	qglVertex2f(0, 0);
	qglVertex2f(glwidth, 0);
	qglVertex2f(glwidth, glheight);
	qglVertex2f(0, glheight);

	qglEnd();

	qglColor4f(1, 1, 1, 1);

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc( void )
{
	if (!draw_disc)
		return;
	
	Draw_CenterPic(draw_disc);
}

/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc( void )
{
}

void ComputeScaledSize( int* wscale, int* hscale, int width, int height )
{
	int scaled_width, scaled_height;
	int max_size;

	if (g_bSupportsNPOTTextures)
	{
		if (wscale)
			*wscale = width;
		if (hscale)
			*hscale = height;
		return;
	}

	max_size = max(128, (int)gl_max_size.value);

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1) 
		;

	if (gl_round_down.value > 0 && 
		width < scaled_width && 
		(gl_round_down.value == 1 || (scaled_width - width) > (scaled_width >> (int)gl_round_down.value)))
		scaled_width >>= 1;

	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1) 
		;

	if (gl_round_down.value > 0 && 
		height < scaled_height && 
		(gl_round_down.value == 1 || (scaled_height - height) > (scaled_height >> (int)gl_round_down.value)))
		scaled_height >>= 1;

	if (wscale)
		*wscale = min(scaled_width >> (int)gl_picmip.value, max_size);

	if (hscale)
		*hscale = min(scaled_height >> (int)gl_picmip.value, max_size);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture( char* identifier )
{
	int i;
	gltexture_t* glt;

	for (i = 0; i < numgltextures; i++)
	{
		glt = &gltextures[i];

		if (!Q_strcmp(identifier, glt->identifier))
			return glt->texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture( unsigned int* in, int inwidth, int inheight, unsigned int* out, int outwidth, int outheight )
{
	int		i, j;
	unsigned* inrow, * inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[1024], p2[1024];
	byte* pix1, * pix2, * pix3, * pix4;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);
		
		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte*)inrow + p1[j];
			pix2 = (byte*)inrow + p2[j];
			pix3 = (byte*)inrow2 + p1[j];
			pix4 = (byte*)inrow2 + p2[j];
			((byte*)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte*)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte*)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte*)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/*
================
GL_ResampleAlphaTexture
================
*/
void GL_ResampleAlphaTexture( byte* in, int inwidth, int inheight, byte* out, int outwidth, int outheight )
{
	int		i, j;
	byte* inrow, * inrow2;
	unsigned	frac, fracstep;
	byte	p1[1024], p2[1024];
	byte* pix1, * pix2, * pix3, * pix4;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++)
	{
		p1[i] = frac >> 16;
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++)
	{
		p2[i] = frac >> 16;
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);

		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte*)inrow + p1[j];
			pix2 = (byte*)inrow + p2[j];
			pix3 = (byte*)inrow2 + p1[j];
			pix4 = (byte*)inrow2 + p2[j];
			((byte*)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte*)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte*)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte*)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

void GL_ResampleTexturePoint( byte* in, int inwidth, int inheight, byte* out, int outwidth, int outheight )
{
	int i, j;
	unsigned ufrac, vfrac;
	unsigned ufracstep, vfracstep;
	byte* src, * dest;

	src = in;
	dest = out;
	ufracstep = inwidth * 0x10000 / outwidth;
	vfracstep = inheight * 0x10000 / outheight;

	vfrac = vfracstep >> 2;

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		ufrac = ufracstep >> 2;

		for (j = 0; j < outwidth; j++)
		{
			*dest = src[ufrac >> 16];
			ufrac += ufracstep;
			dest++;
		}

		vfrac += vfracstep;
		src += inwidth * (vfrac >> 16);
		vfrac = vfrac & 0xFFFF;
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap( byte* in, int width, int height )
{
	int		i, j;
	byte* out;

	width <<= 2;
	height >>= 1;
	out = in;
	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

void BoxFilter3x3( byte* out, byte* in, int w, int h, int x, int y )
{
	int		i, j;
	int		a = 0, r = 0, g = 0, b = 0;
	int		count = 0, acount = 0;
	int		u, v;
	byte* pixel;

	for (i = 0; i < 3; i++)
	{
		u = (i - 1) + x;

		for (j = 0; j < 3; j++)
		{
			v = (j - 1) + y;

			if (u >= 0 && u < w && v >= 0 && v < h)
			{
				pixel = &in[(u + v * w) * 4];

				if (pixel[3] != 0)
				{
					r += pixel[0];
					g += pixel[1];
					b += pixel[2];
					a += pixel[3];
					acount++;
				}
			}
		}
	}

	if (acount == 0)
		acount = 1;

	out[0] = r / acount;
	out[1] = g / acount;
	out[2] = b / acount;
	out[3] = 0;
}

int giTotalTextures;
int giTotalTexBytes;

void GL_Upload32( unsigned int* data, int width, int height, qboolean mipmap, int iType, int filter )
{
	int			iComponent = 0, iFormat = 0;
	int			i;

	static	unsigned	scaled[1024 * 512];	// [512*256];
	int			scaled_width, scaled_height;
	qboolean	f4444 = FALSE;
	int			w, h, bpp;

	VideoMode_GetCurrentVideoMode(&w, &h, &bpp);

	giTotalTexBytes += height * width;

	if (iType != TEX_TYPE_LUM)
		giTotalTexBytes += width * height * 2;

	giTotalTextures++;

	if (gl_spriteblend.value && TEX_IS_ALPHA(iType))
	{
		for (i = 0; i < width * height; i++)
		{
			if (!data[i])
				BoxFilter3x3((byte*)&data[i], (byte*)data, width, height, i % width, i / width);
		}
	}

	ComputeScaledSize(&scaled_width, &scaled_height, width, height);

	if (scaled_width * scaled_height > sizeof(scaled) / 4)
		Sys_Error("GL_LoadTexture: too big");

	switch (iType)
	{
		case TEX_TYPE_NONE:
			iFormat = GL_RGBA;

			if (bpp == 16)
				iComponent = GL_RGB8;
			else
				iComponent = GL_RGBA8;
			break;

		case TEX_TYPE_ALPHA:
		case TEX_TYPE_ALPHA_GRADIENT:
			iFormat = GL_RGBA;

			if (bpp == 16)
				iComponent = GL_RGBA4;
			else
				iComponent = GL_RGBA8;
			break;

		case TEX_TYPE_LUM:
			iFormat = GL_LUMINANCE;
			iComponent = GL_LUMINANCE8;
			break;

		case TEX_TYPE_RGBA:
			iFormat = GL_RGBA;

			if (bpp == 16)
				iComponent = GL_RGB5_A1;
			else
				iComponent = GL_RGBA8;
			break;

		default:
			Sys_Error("GL_Upload32: Bad texture type");
			break;
	}

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			if (f4444)
			{
				Download4444();
			}

			{
				// JAY: No paletted textures for now
				qglTexImage2D(GL_TEXTURE_2D, GL_ZERO, iComponent, width, height, GL_ZERO, iFormat, GL_UNSIGNED_BYTE, data);
			}
			goto done;
		}

		if (iType == TEX_TYPE_LUM)
			Q_memcpy(scaled, data, width * height);
		else
			Q_memcpy(scaled, data, width * height * 4);			
	}
	else
	{
		if (iType == TEX_TYPE_LUM)
			GL_ResampleAlphaTexture((byte*)data, width, height, (byte*)scaled, scaled_width, scaled_height);
		else
			GL_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);
	}

	texels += scaled_width * scaled_height;
	qglTexImage2D(GL_TEXTURE_2D, GL_ZERO, iComponent, scaled_width, scaled_height, GL_ZERO, iFormat, GL_UNSIGNED_BYTE, scaled);
	
	if (mipmap)
	{
		int	miplevel = 0;

		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap((byte*)scaled, scaled_width, scaled_height);

			scaled_width >>= 1;
			scaled_height >>= 1;

			if (scaled_width < 1)
				scaled_width = 1;

			if (scaled_height < 1)
				scaled_height = 1;

			texels += scaled_width * scaled_height;

			miplevel++;
			qglTexImage2D(GL_TEXTURE_2D, miplevel, iComponent, scaled_width, scaled_height, GL_ZERO, iFormat, GL_UNSIGNED_BYTE, scaled);
		}
	}

done:
	if (mipmap)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	}

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
}

/*
===============
GL_Upload16

Alpha textures require additional methods
===============
*/
void GL_Upload16( unsigned char* data, int width, int height, qboolean mipmap, int iType, unsigned char* pPal, int filter )
{
	static	unsigned	trans[640 * 480];		// FIXME, temporary
	int			i, s;
	qboolean noalpha = TRUE;
	int			p;
	unsigned char* pb;

	s = width * height;

	if (s > sizeof(trans))
	{
		Con_Printf(const_cast<char*>("Can't upload (%ix%i) texture, it's > 640*480 bytes\n"), width, height);
		return;
	}

	if (iType != TEX_TYPE_LUM)
	{
		if (!pPal)
			return;

		for (i = 0; i < 768; i++)
			pPal[i] = texgammatable[pPal[i]];
	}

	// Alpha textures
	if (TEX_IS_ALPHA(iType))
	{
		if (iType == TEX_TYPE_ALPHA_GRADIENT)
		{
			for (i = 0; i < s; i++)
			{
				p = data[i];
				pb = (byte*)&trans[i];

				pb[0] = pPal[765];
				pb[1] = pPal[766];
				pb[2] = pPal[767];
				pb[3] = p;

				noalpha = FALSE;
			}
		}
		else if (iType == TEX_TYPE_RGBA)
		{
			for (i = 0; i < s; i++)
			{
				p = data[i];
				pb = (byte*)&trans[i];

				pb[0] = pPal[p * 3 + 0];
				pb[1] = pPal[p * 3 + 1];
				pb[2] = pPal[p * 3 + 2];
				pb[3] = p;

				noalpha = FALSE;
			}
		}
		else if (iType == TEX_TYPE_ALPHA)
		{
			for (i = 0; i < s; i++)
			{
				p = data[i];
				pb = (byte*)&trans[i];

				if (p == 255)
				{
					pb[0] = 0;
					pb[1] = 0;
					pb[2] = 0;
					pb[3] = 0;

					noalpha = FALSE;
				}
				else
				{
					pb[0] = pPal[p * 3 + 0];
					pb[1] = pPal[p * 3 + 1];
					pb[2] = pPal[p * 3 + 2];
					pb[3] = 255;
				}
			}
		}

		if (noalpha)
			iType = TEX_TYPE_NONE;
	}
	else if (iType == TEX_TYPE_NONE)
	{
		if (s & 3)
			Sys_Error("GL_Upload16: s&3");

		if (gl_dither.value)
		{
			for (i = 0; i < s; i++)
			{
				unsigned char r, g, b;
				unsigned char* ppix;

				p = data[i];
				pb = (byte*)&trans[i];
				ppix = &pPal[p * 3];
				r = ppix[0] |= (ppix[0] >> 6);
				g = ppix[1] |= (ppix[1] >> 6);
				b = ppix[2] |= (ppix[2] >> 6);

				pb[0] = r;
				pb[1] = g;
				pb[2] = b;
				pb[3] = 255;
			}
		}
		else
		{
			for (i = 0; i < s; i += 4)
			{
				trans[i + 0] = *(unsigned int*)&pPal[3 * data[i + 0]] | 0xFF000000;
				trans[i + 1] = *(unsigned int*)&pPal[3 * data[i + 1]] | 0xFF000000;
				trans[i + 2] = *(unsigned int*)&pPal[3 * data[i + 2]] | 0xFF000000;
				trans[i + 3] = *(unsigned int*)&pPal[3 * data[i + 3]] | 0xFF000000;
			}
		}
	}
	else if (iType == TEX_TYPE_LUM)
	{
		Q_memcpy(trans, data, s);
	}
	else
	{
		Con_Printf(const_cast<char*>("Upload16:Bogus texture type!/n"));
	}

	GL_Upload32(trans, width, height, mipmap, iType, filter);
}

/*
================
GL_FreeGLTex
================
*/
void GL_FreeGLTex( gltexture_t* glt )
{
	int texnum;

	qglDeleteTextures(1, (const GLuint*)glt);

	texnum = glt->paletteIndex;
	if (texnum >= 0)
	{
		if (gGLPalette[texnum].referenceCount <= 1)
			GL_PaletteClear(texnum);
		else
			gGLPalette[texnum].referenceCount--;
	}

	Q_memset(glt, 0, sizeof(*glt));
	glt->servercount = -1;
}

/*
================
GL_UnloadTextures

Unload all loaded textures
We do this every time we load the map
================
*/
void GL_UnloadTextures( void )
{
	int i;
	gltexture_t* glt;

	for (i = 0; i < numgltextures; i++)
	{
		glt = &gltextures[i];

		if (glt->servercount > 0 && glt->servercount != gHostSpawnCount)
			GL_FreeGLTex(glt);
	}
}

/*
================
GL_UnloadTexture

Unload a single texture
================
*/
void GL_UnloadTexture( char* identifier )
{
	int i;
	gltexture_t* glt;

	for (i = 0; i < numgltextures; i++)
	{
		glt = &gltextures[i];

		// Try to find texture we want to unload
		if (!Q_strcmp(identifier, glt->identifier))
		{
			if (glt->servercount >= 0)
				GL_FreeGLTex(glt);

			return;
		}
	}
}

void GL_PaletteInit( void )
{
	int i;

	for (i = 0; i < 350; i++)
		GL_PaletteClear(i);
}

void GL_PaletteClear( int index )
{
	if (index >= 0 && index < 350)
	{
		gGLPalette[index].tag = -1;
		gGLPalette[index].referenceCount = 0;
	}
}

int GL_PaletteTag( byte* pPal )
{
	int tag;
	int i;

	tag = *pPal;

	for (i = 0; i < 768; i++)
	{
		tag = (tag + pPal[1]) ^ *pPal;
		pPal++;
	}

	if (tag < 0)
		tag = -tag;

	return tag;
}

int GL_PaletteEqual( byte* pPal1, int tag1, byte* pPal2, int tag2 )
{
	int i;

	if (tag1 != tag2)
		return 0;

	for (i = 0; i < 768; i++)
	{
		if (pPal1[i] != pPal2[i])
			return 0;
	}

	return 1;
}

void GL_PaletteClearSky( void )
{
	int i;

	for (i = 344; i < 350; i++)
	{
		gGLPalette[i].tag = -1;
		gGLPalette[i].referenceCount = 0;
	}
}

/*
===============
GL_PaletteAdd
===============
*/
short GL_PaletteAdd( unsigned char* pPal, qboolean isSky )
{
	if (!qglColorTableEXT)
		return -1;
	
	int i, tag;
	int limit;
	byte gammaPal[768];

	for (i = 0; i < 768; i++)
	{
		gammaPal[i] = texgammatable[pPal[i]];
	}

	tag = GL_PaletteTag(gammaPal);

	if (isSky)
	{
		i = 344;
		limit = 350;
	}
	else
	{
		i = 0;
		limit = 344;
	}

	for (; i < limit; i++)
	{
		if (gGLPalette[i].tag < 0)
		{
			Q_memcpy(gGLPalette[i].colors, gammaPal, sizeof(gGLPalette[i].colors));
			gGLPalette[i].tag = tag;
			gGLPalette[i].referenceCount = 1;
			return i;
		}

		if (GL_PaletteEqual(gammaPal, tag, gGLPalette[i].colors, gGLPalette[i].tag))
		{
			gGLPalette[i].referenceCount++;
			return i;
		}
	}
	
	return -1;
}

int g_currentpalette = -1;

void GL_PaletteSelect( int paletteIndex )
{
	if (g_currentpalette == paletteIndex)
		return;

	if (qglColorTableEXT)
	{
		g_currentpalette = paletteIndex;
		qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, GL_DYNAMIC_STORAGE_BIT, GL_RGB, GL_UNSIGNED_BYTE, gGLPalette[paletteIndex].colors);
	}
}

GLuint GL_GenTexture( void )
{
	GLuint tex;
	qglGenTextures(1, &tex);

	return tex;
}

/*
================
GL_LoadTexture2

The main texture loading function
================
*/
int GL_LoadTexture2( char* identifier, GL_TEXTURETYPE textureType, int width, int height, unsigned char* data, qboolean mipmap, int iType, unsigned char* pPal, int filter )
{
	int			i;
	int			scaled_width, scaled_height;
	gltexture_t* glt, * slot;
	BOOL		mustRescale;

	glt = NULL;

	// see if the texture is allready present
	if (identifier[0])
	{
	tryagain:
		for (i = 0; i < numgltextures; i++)
		{
			slot = &gltextures[i];

			if (slot->servercount < 0)
			{
				if (!glt)
					glt = slot;

				continue;
			}

			if (!Q_strcmp(identifier, slot->identifier))
			{
				if ((slot->width == width) && (slot->height == height))
				{
					if (slot->servercount > 0)
						slot->servercount = gHostSpawnCount;

					if (slot->paletteIndex >= 0)
						return slot->texnum | ((slot->paletteIndex + 1) << 16);
					else
						return slot->texnum;
				}

				++identifier[3];

				if (*identifier == '\0')
				{
					Con_DPrintf(const_cast<char*>("NULL Texture\n"));
					break;
				}

				goto tryagain;	// check again
			}
		}
	}
	else
		Con_DPrintf(const_cast<char*>("NULL Texture\n"));

	if (!glt)
	{
		glt = &gltextures[numgltextures];
		numgltextures++;

		if (numgltextures >= MAX_GLTEXTURES)
			Sys_Error("Texture Overflow: MAX_GLTEXTURES");
	}

	if (!glt->texnum)
		glt->texnum = GL_GenTexture();

	Q_strncpy(glt->identifier, identifier, sizeof(glt->identifier) - 1);
	glt->identifier[sizeof(glt->identifier) - 1] = 0;

	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	if (textureType == GLT_WORLD)
		glt->servercount = gHostSpawnCount;
	else
		glt->servercount = 0;

	glt->paletteIndex = -1;

	GL_Bind(glt->texnum);

	currenttexture = -1;

	ComputeScaledSize(&scaled_width, &scaled_height, width, height);

	if (scaled_width == width && scaled_height == height)
		mustRescale = FALSE;
	else
		mustRescale = TRUE;

	if (!mipmap)
	{
		unsigned char* pTexture = NULL;
		byte scaled[16384];

		if (mustRescale)
		{
			if (scaled_width <= 128 && scaled_height <= 128)
			{
				if (fs_perf_warnings.value)
				{
					Con_DPrintf(const_cast<char*>("fs_perf_warnings: resampling non-power-of-2 texture %s (%dx%d)\n"), identifier, width, height);
				}

				GL_ResampleTexturePoint(data, width, height, scaled, scaled_width, scaled_height);
				pTexture = scaled;
			}
		}
		else
		{
			pTexture = data;
		}

		if (pTexture && qglColorTableEXT)
		{
			if (gl_palette_tex.value && (iType == TEX_TYPE_NONE))
			{
				glt->paletteIndex = GL_PaletteAdd(pPal, FALSE);

				if (glt->paletteIndex >= 0)
				{
					qglTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, GL_FALSE, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, pTexture);

					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);

					if (glt->paletteIndex < 0)
						return glt->texnum;
					else
						return glt->texnum | ((glt->paletteIndex + 1) << 16);
				}
			}
		}
	}

	if (g_modfuncs.m_pfnTextureLoad)
		g_modfuncs.m_pfnTextureLoad(identifier, width, height, (char*)data);

	if (textureType == GLT_SPRITE && iType == TEX_TYPE_RGBA)
		GL_Upload32((unsigned int*)data, width, height, mipmap, TEX_TYPE_RGBA, filter);
	else
		GL_Upload16(data, width, height, mipmap, iType, pPal, filter);

	if (glt->paletteIndex < 0)
		return glt->texnum;
	else
		return glt->texnum | ((glt->paletteIndex + 1) << 16);
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture( char* identifier, GL_TEXTURETYPE textureType, int width, int height, unsigned char* data, int mipmap, int iType, unsigned char* pPal )
{
	return GL_LoadTexture2(identifier, textureType, width, height, data, (qboolean)mipmap, iType, pPal, gl_filter_max);
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture( qpic_t* pic, char* pszName )
{
	unsigned char* pPal;

	pPal = &pic->data[pic->height * pic->width + 2];

	return GL_LoadTexture(pszName, GLT_SYSTEM, pic->width, pic->height, pic->data, FALSE, TEX_TYPE_ALPHA, pPal);
}

qpic_t* LoadTransBMP( char* pszName )
{
	return LoadTransPic(pszName, (qpic_t*)W_GetLumpName(0, pszName));
}

qpic_t* LoadTransPic( char* pszName, qpic_t* ppic )
{
	int		i;
	int		width, height;

	gltexture_t* glt;
	int* pbuf;
	unsigned char* pPal;
	glpic_t* gl;
	qpic_t* ppicNew;

	if (!ppic)
		return ppic;

	ppicNew = (qpic_t*)Mem_Malloc(sizeof(glpic_t));
	gl = (glpic_t*)ppicNew;

	ppicNew->width = ppic->width;
	ppicNew->height = ppic->height;

	if (*pszName)
	{
	tryagain:
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
		{
			if (!Q_strcmp(pszName, glt->identifier))
			{
				if (ppicNew->width == glt->width && ppicNew->height == glt->height)
					return ppic;

				++pszName[3];

				if (*pszName == '\0')
				{
					glt = &gltextures[numgltextures];
					break;
				}

				goto tryagain;
			}
		}
	}
	else
	{
		glt = &gltextures[numgltextures];
	}

	numgltextures++;

	Q_strncpy(glt->identifier, pszName, sizeof(glt->identifier) - 1);
	glt->identifier[sizeof(glt->identifier) - 1] = 0;

	glt->texnum = GL_GenTexture();

	glt->width = ppicNew->width;
	glt->height = ppicNew->height;

	glt->servercount = gHostSpawnCount;
	glt->mipmap = FALSE;

	GL_Bind(glt->texnum);

	pbuf = (int*)Mem_Malloc(sizeof(uint32) * ppic->width * ppic->height);

	pPal = &ppic->data[ppic->height * ppic->width + 2];

	for (i = 0; i < 768; i++)
		pPal[i] = texgammatable[pPal[i]];

	for (i = 0; i < ppic->width * ppic->height; i++)
	{
		pbuf[i] = *(ulong*)&pPal[3 * ppic->data[i]] & 0xFFFFFF;

		if (ppic->data[i] != 0xFF)
			pbuf[i] |= 0xFF000000;
	}

	width = ppic->width;
	height = ppic->height;
	GL_Upload32((unsigned int*)pbuf, width, height, FALSE, TEX_TYPE_ALPHA, gl_filter_max);

	gl->texnum = glt->texnum;
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	Mem_Free(pbuf);

	return (qpic_t*)gl;
}

/*
===============
Draw_MiptexTexture

Draw mip texture
Mip texturing is a texturing method that uses multiple copies of the same texture with different details
===============
*/
void Draw_MiptexTexture( cachewad_t* wad, unsigned char* data )
{	
	texture_t* tex;
	miptex_t* mip, tmp;

	int		i, pix;
	int		paloffset;
	int		palettesize;

	unsigned char* pal;
	unsigned char* bitmap;

	if (wad->cacheExtra != DECAL_EXTRASIZE)
		Sys_Error("Draw_MiptexTexture: Bad cached wad %s\n", wad->name);

	mip = (miptex_t*)(data + wad->cacheExtra);
	tmp = *mip;

	tex = (texture_t*)data;
	Q_memcpy(tex->name, tmp.name, sizeof(tex->name));

	tex->width = LittleLong(tmp.width);
	tex->height = LittleLong(tmp.height);
	tex->anim_max = 0;
	tex->anim_min = 0;
	tex->anim_total = 0;
	tex->alternate_anims = 0;
	tex->anim_next = 0;

	for (i = 0; i < MIPLEVELS; i++)
		tex->offsets[i] = wad->cacheExtra + LittleLong(tmp.offsets[i]);

	pix = tex->width * tex->height;
	palettesize = tex->offsets[0];
	paloffset = palettesize + (pix >> 4) + (pix >> 2) + (pix >> 6) + pix + 2;
	bitmap = (byte*)&tex->name[palettesize];
	pal = (byte*)&tex->name[paloffset];

	if (gfCustomBuild)
	{
		Q_strncpy(tex->name, szCustName, sizeof(tex->name) - 1);
		tex->name[sizeof(tex->name) - 1] = 0;
	}

	if (pal[765] || pal[766] || pal[767] != 0xFF)
	{
		tex->name[0] = '}';

		if (gfCustomBuild)
			GL_UnloadTexture(tex->name);

		tex->gl_texturenum = GL_LoadTexture(tex->name, GLT_DECAL, tex->width, tex->height, bitmap, TRUE, TEX_TYPE_ALPHA_GRADIENT, pal);
	}
	else
	{
		tex->name[0] = '{';
		tex->gl_texturenum = GL_LoadTexture(tex->name, GLT_DECAL, tex->width, tex->height, bitmap, TRUE, TEX_TYPE_ALPHA, pal);
	}
}

void GL_LoadFilterTexture( float r, float g, float b, float brightness )
{
	unsigned char* data;
	int pixelIndex;

	if (gl_FilterTexture)
	{
		data = (byte*)malloc(0xC0);

		for (pixelIndex = 0; pixelIndex < 0xC0 / 3; pixelIndex++)
		{
			data[pixelIndex + 0] = r * brightness * 255.0;
			data[pixelIndex + 1] = g * brightness * 255.0;
			data[pixelIndex + 2] = b * brightness * 255.0;
		}

		GL_Bind(gl_FilterTexture);

		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, data);

		free(data);
	}
	else
	{
		gl_FilterTexture = GL_GenTexture();

		data = (byte*)malloc(0xC0);

		for (pixelIndex = 0; pixelIndex < 0xC0 / 3; pixelIndex++)
		{
			data[pixelIndex + 0] = r * brightness * 255.0;
			data[pixelIndex + 1] = g * brightness * 255.0;
			data[pixelIndex + 2] = b * brightness * 255.0;
		}

		GL_Bind(gl_FilterTexture);

		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);

		free(data);
	}
}