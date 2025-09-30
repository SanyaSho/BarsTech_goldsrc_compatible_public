//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Engine Surface Implementation for HW renderer (OpenGL)
//
// $NoKeywords: $
//=============================================================================

#include "quakedef.h"
#include "glHUD.h"
#include "draw.h"
#include "screen.h"
#include "sys_getmodes.h"
#include "keys.h"
#include "vgui_EngineSurface.h"
#include "vgui_int.h"
#include "sbar.h"
#include "view.h"
#include "gl_screen.h"

#include <tier1/UtlRBTree.h>

RECT g_ScissorRect; // vgui_EngineSurfaceHW.cpp:110
bool g_bScissor = false; // vgui_EngineSurfaceHW.cpp:111

extern qboolean scr_drawloading;

extern SDL_Window* pmainwindow;

void SCR_DrawLoading( void );

struct TCoordRect
{
	float s0;
	float t0;
	float s1;
	float t1;
};

float InterpTCoord( int val, int min, int max, float tMin, float tMax )
{
	return tMin + (tMax - tMin) * ((float)(val - min) / (float)(max - min));
}

bool ScissorRect_TCoords( int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, RECT* pOut, TCoordRect* pTCoords )
{
	RECT rcChar;

	rcChar.left = x0;
	rcChar.top = y0;
	rcChar.right = x1;
	rcChar.bottom = y1;

	if (g_bScissor)
	{
		if (!IntersectRect(pOut, &g_ScissorRect, &rcChar))
			return false;

		if (pTCoords)
		{
			pTCoords->s0 = InterpTCoord(pOut->left, rcChar.left, rcChar.right, s0, s1);
			pTCoords->s1 = InterpTCoord(pOut->right, rcChar.left, rcChar.right, s0, s1);
			pTCoords->t0 = InterpTCoord(pOut->top, rcChar.top, rcChar.bottom, t0, t1);
			pTCoords->t1 = InterpTCoord(pOut->bottom, rcChar.top, rcChar.bottom, t0, t1);
		}
	}
	else
	{
		pOut->left = x0;
		pOut->top = y0;
		pOut->right = x1;
		pOut->bottom = y1;

		if (pTCoords)
		{
			pTCoords->s0 = s0;
			pTCoords->s1 = s1;
			pTCoords->t0 = t0;
			pTCoords->t1 = t1;
		}
	}

	return true;
}

bool ScissorRect( int x0, int y0, int x1, int y1, RECT* pOut )
{
	return ScissorRect_TCoords(x0, y0, x1, y1, 0, 0, 0, 0, pOut, NULL);
}

namespace
{
	struct Texture
	{
		int _id;
		int _wide;
		int _tall;
		float _s0;
		float _t0;
		float _s1;
		float _t1;
	};
}

struct VertexBuffer_t
{
	float texcoords[2];
	float vertex[2];
};

static bool VguiSurfaceTexturesLessFunc( const Texture& lhs, const Texture& rhs )
{
	return lhs._id < rhs._id;
}

static CUtlRBTree<Texture, int> g_VGuiSurfaceTextures(&VguiSurfaceTexturesLessFunc);
static Texture* staticTextureCurrent = NULL;

static VertexBuffer_t g_VertexBuffer[256];
static int g_iVertexBufferEntriesUsed = 0;
static float g_vgui_projection_matrix[16];

static Texture* staticGetTextureById( int id )
{
	Texture dummy;
	dummy._id = id;

	int index = g_VGuiSurfaceTextures.Find(dummy);

	if (index != g_VGuiSurfaceTextures.InvalidIndex())
		return &g_VGuiSurfaceTextures[index];

	return NULL;
}

void EngineSurface::createRenderPlat()
{
}

EngineSurface::EngineSurface()
{
	createRenderPlat();
}

void EngineSurface::deleteRenderPlat()
{
}

void EngineSurface::pushMakeCurrent( int* insets, int* absExtents, int* clipRect, bool translateToScreenSpace )
{
	RECT rect;
	POINT pnt;

	int wide, tall;

	pnt.x = 0;
	pnt.y = 0;

	if (translateToScreenSpace)
	{
		if (VideoMode_IsWindowed())
			SDL_GetWindowPosition((SDL_Window*)pmainwindow, (int*)&pnt.x, (int*)&pnt.y);
	}

	if (VideoMode_IsWindowed())
		SDL_GetWindowSize((SDL_Window*)pmainwindow, (int*)&rect.right, (int*)&rect.bottom);
	else
		VideoMode_GetCurrentVideoMode((int*)&rect.right, (int*)&rect.bottom, NULL);

	qglPushMatrix();
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, rect.right, rect.bottom, 0, -1, 1);

	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	qglTranslatef(insets[0], insets[1], 0);

	wide = absExtents[0] - pnt.x;
	tall = absExtents[1] - pnt.y;

	qglTranslatef(wide, tall, 0);

	g_bScissor = true;

	g_ScissorRect.left = clipRect[0] - pnt.x - (insets[0] + wide);
	g_ScissorRect.top = clipRect[1] - pnt.y - (insets[1] + tall);
	g_ScissorRect.right = clipRect[2] - pnt.x - (insets[0] + wide);
	g_ScissorRect.bottom = clipRect[3] - pnt.y - (insets[1] + tall);

	qglEnable(GL_BLEND);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void EngineSurface::popMakeCurrent()
{
	drawFlushText();

	qglMatrixMode(GL_PROJECTION);
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();
	qglPopMatrix();

	g_bScissor = false;

	qglEnable(GL_TEXTURE_2D);
}

/*
=================
EngineSurface::drawFilledRect

Draws a filled rectangle
=================
*/
void EngineSurface::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	RECT rcOut;

	if (_drawColor[3] == 255)
		return;

	if (ScissorRect(x0, y0, x1, y1, &rcOut))
	{
		qglDisable(GL_TEXTURE_2D);

		qglColor4ub(_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);

		qglBegin(GL_QUADS);

		qglVertex2f(rcOut.left, rcOut.top);
		qglVertex2f(rcOut.right, rcOut.top);
		qglVertex2f(rcOut.right, rcOut.bottom);
		qglVertex2f(rcOut.left, rcOut.bottom);

		qglEnd();

		qglEnable(GL_TEXTURE_2D);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws an unfilled rectangle in the current drawcolor
//-----------------------------------------------------------------------------
void EngineSurface::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	if (_drawColor[3] != 255)
	{
		qglDisable(GL_TEXTURE_2D);
		qglColor4ub(_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);

		// draw an outline of a rectangle using 4 filledRect
		drawFilledRect(x0, y0, x1, y0 + 1);			// top
		drawFilledRect(x0, y1 - 1, x1, y1);			// bottom
		drawFilledRect(x0, y0 + 1, x0 + 1, y1 - 1); // left
		drawFilledRect(x1 - 1, y0 + 1, x1, y1 - 1); // right
	}
}

/*
=================
EngineSurface::drawLine

Draws a line
=================
*/
void EngineSurface::drawLine( int x1, int y1, int x2, int y2 )
{
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_BLEND);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE);

	qglColor4ub(_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);

	qglBegin(GL_LINES);

	qglVertex2f(x1, y1);
	qglVertex2f(x2, y2);

	qglEnd();

	qglColor3f(1, 1, 1);

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}

void EngineSurface::drawPolyLine( int* px, int* py, int numPoints )
{
	int i;

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_BLEND);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE);

	qglColor4ub(_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);

	qglBegin(GL_LINES);

	for (i = 1; i < numPoints; i++)
	{
		qglVertex2f(px[i - 1], py[i - 1]);
		qglVertex2f(px[i], py[i]);
	}

	qglEnd();

	qglColor3f(1, 1, 1);

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}

/*
=================
EngineSurface::drawTexturedPolygon

Draws a textured poly
=================
*/
void EngineSurface::drawTexturedPolygon( vgui2::VGuiVertex* pVertices, int n )
{
	int i;

	if (_drawColor[3] == 255)
		return;

	qglColor4ub(_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);

	qglEnable(GL_TEXTURE_2D);

	qglBegin(GL_POLYGON);

	for (i = 0; i < n; i++, pVertices++)
	{
		qglTexCoord2f(pVertices->u, pVertices->v);
		qglVertex2f(pVertices->x, pVertices->y);
	}

	qglEnd();
}

void EngineSurface::drawSetTextureRGBA( int id, const byte* rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel )
{
	Texture* pTexture = staticGetTextureById(id);

	if (pTexture == NULL)
	{
		Texture newTexture;
		memset(&newTexture, 0, sizeof(newTexture));

		newTexture._id = id;
		pTexture = &g_VGuiSurfaceTextures[g_VGuiSurfaceTextures.Insert(newTexture)];
	}

	if (!pTexture)
		return;

	pTexture->_id = id;
	pTexture->_wide = wide;
	pTexture->_tall = tall;

	int pow2Wide;
	int pow2Tall;

	for (int i = 0; i < 32; ++i)
	{
		pow2Wide = 1 << i;

		if (wide <= pow2Wide)
			break;
	}

	for (int i = 0; i < 32; ++i)
	{
		pow2Tall = 1 << i;

		if (tall <= pow2Tall)
			break;
	}

	int* pExpanded = NULL;

	const void* pData = rgba;

	// Convert to power of 2 texture
	if (wide != pow2Wide || tall != pow2Tall)
	{
		pExpanded = new int[pow2Wide * pow2Tall];

		pData = pExpanded;

		memset(pExpanded, 0, 4 * pow2Wide * pow2Tall);

		const int* pSrc = reinterpret_cast<const int*>(rgba);
		int* pDest = pExpanded;

		for (int y = 0; y < tall; ++y)
		{
			for (int x = 0; x < wide; ++x)
			{
				pDest[x] = pSrc[x];
			}

			pDest += pow2Wide;
			pSrc += wide;
		}
	}

	pTexture->_s0 = 0;
	pTexture->_t0 = 0;
	pTexture->_s1 = (double)(wide) / pow2Wide;
	pTexture->_t1 = (double)(tall) / pow2Tall;

	staticTextureCurrent = pTexture;

	currenttexture = id;

	qglBindTexture(GL_TEXTURE_2D, id);

	if (hardwareFilter)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	int w, h, bpp;
	VideoMode_GetCurrentVideoMode(&w, &h, &bpp);

	if (hasAlphaChannel || bpp == 32)
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pow2Wide, pow2Tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, pData);
	else
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, pow2Wide, pow2Tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, pData);

	if (pExpanded)
		delete[] pExpanded;
}

void EngineSurface::drawSetTexture( int id )
{
	if (id != currenttexture)
	{
		drawFlushText();
		currenttexture = id;
	}

	staticTextureCurrent = staticGetTextureById(id);

	qglBindTexture(GL_TEXTURE_2D, id);
}

/*
=================
EngineSurface::drawTexturedRect

Draws a textured rectangle
=================
*/
void EngineSurface::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	if (!staticTextureCurrent || _drawColor[3] == 255)
		return;

	RECT rcOut;
	TCoordRect tRect;

	if (!ScissorRect_TCoords(x0, y0, x1, y1, staticTextureCurrent->_s0, staticTextureCurrent->_t0, staticTextureCurrent->_s1, staticTextureCurrent->_t1, &rcOut, &tRect))
		return;

	qglEnable(GL_TEXTURE_2D);

	qglBindTexture(GL_TEXTURE_2D, staticTextureCurrent->_id);

	currenttexture = staticTextureCurrent->_id;

	qglColor4ub(_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);

	qglBegin(GL_QUADS);

	qglTexCoord2f(tRect.s0, tRect.t0);
	qglVertex2f(rcOut.left, rcOut.top);

	qglTexCoord2f(tRect.s1, tRect.t0);
	qglVertex2f(rcOut.right, rcOut.top);

	qglTexCoord2f(tRect.s1, tRect.t1);
	qglVertex2f(rcOut.right, rcOut.bottom);

	qglTexCoord2f(tRect.s0, tRect.t1);
	qglVertex2f(rcOut.left, rcOut.bottom);

	qglEnd();
}

void EngineSurface::drawTexturedRectAdd(int x0, int y0, int x1, int y1)
{
	if (!staticTextureCurrent || _drawColor[3] == 255)
		return;

	RECT rcOut;
	TCoordRect tRect;

	if (!ScissorRect_TCoords(x0, y0, x1, y1, staticTextureCurrent->_s0, staticTextureCurrent->_t0, staticTextureCurrent->_s1, staticTextureCurrent->_t1, &rcOut, &tRect))
		return;

	qglEnable(GL_TEXTURE_2D);

	qglBlendFunc(GL_SRC_ALPHA, 1);
	qglEnable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);

	qglBindTexture(GL_TEXTURE_2D, staticTextureCurrent->_id);

	currenttexture = staticTextureCurrent->_id;

	qglColor4ub(_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);

	qglBegin(GL_QUADS);

	qglTexCoord2f(tRect.s0, tRect.t0);
	qglVertex2f(rcOut.left, rcOut.top);

	qglTexCoord2f(tRect.s1, tRect.t0);
	qglVertex2f(rcOut.right, rcOut.top);

	qglTexCoord2f(tRect.s1, tRect.t1);
	qglVertex2f(rcOut.right, rcOut.bottom);

	qglTexCoord2f(tRect.s0, tRect.t1);
	qglVertex2f(rcOut.left, rcOut.bottom);

	qglEnd();

	qglDisable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
}

void EngineSurface::drawPrintChar(int x, int y, int wide, int tall, float s0, float t0, float s1, float t1)
{
	RECT rcOut;
	TCoordRect tRect;

	if (_drawTextColor[3] == 255 || !ScissorRect_TCoords(x, y, x + wide, y + tall, s0, t0, s1, t1, &rcOut, &tRect))
		return;

	if (g_iVertexBufferEntriesUsed >= Q_ARRAYSIZE(g_VertexBuffer))
	{
		drawFlushText();
	}

	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s0;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t0;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.left;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.top;
	g_iVertexBufferEntriesUsed++;

	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s1;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t0;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.right;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.top;
	g_iVertexBufferEntriesUsed++;

	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s1;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t1;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.right;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.bottom;
	g_iVertexBufferEntriesUsed++;
	
	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s0;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t1;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.left;
	g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.bottom;
	g_iVertexBufferEntriesUsed++;
}

void EngineSurface::drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	RECT rcOut;
	TCoordRect tRect;

	if (_drawTextColor[3] == 255 || !ScissorRect_TCoords(x, y, x + wide, y + tall, s0, t0, s1, t1, &rcOut, &tRect))
		return;

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
	qglEnable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);

	qglColor4ub(_drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - _drawTextColor[3]);

	qglBegin(GL_QUADS);

	qglTexCoord2f(tRect.s0, tRect.t0);
	qglVertex2f(rcOut.left, rcOut.top);

	qglTexCoord2f(tRect.s1, tRect.t0);
	qglVertex2f(rcOut.right, rcOut.top);

	qglTexCoord2f(tRect.s1, tRect.t1);
	qglVertex2f(rcOut.right, rcOut.bottom);

	qglTexCoord2f(tRect.s0, tRect.t1);
	qglVertex2f(rcOut.left, rcOut.bottom);

	qglEnd();

	qglDisable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
}

//-----------------------------------------------------------------------------
// Associates a texture with a material file (also binds it)
//-----------------------------------------------------------------------------
void EngineSurface::drawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload )
{
	Texture* pTexture = staticGetTextureById(id);

	if (!pTexture || forceReload)
	{
		char name[512];

		byte buf[512 * 512];

		int width, height;

		snprintf(name, Q_ARRAYSIZE(name), "%s.tga", filename);

		bool bLoaded = false;

		if (LoadTGA(name, buf, sizeof(buf), &width, &height))
		{
			bLoaded = true;
		}
		else
		{
			snprintf(name, Q_ARRAYSIZE(name), "%s.bmp", filename);

			FileHandle_t hFile = FS_Open(name, "rb");

			if (hFile != FILESYSTEM_INVALID_HANDLE)
			{
				VGui_LoadBMP(hFile, buf, sizeof(buf), &width, &height);
				// TODO: check result of call
				bLoaded = true;
			}
		}

		if (bLoaded)
		{
			drawSetTextureRGBA(id, buf, width, height, hardwareFilter, false);

			pTexture = staticGetTextureById(id);
		}

		if (fs_perf_warnings.value && (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)))
		{
			Con_DPrintf(const_cast<char*>("fs_perf_warnings: Resampling non-power-of-2 image '%s' (%dx%d)\n"), filename, width, height);
		}
	}

	if (pTexture)
		drawSetTexture(id);
}

void EngineSurface::drawGetTextureSize( int id, int& wide, int& tall )
{
	Texture* pTexture = staticGetTextureById(id);

	if (pTexture)
	{
		wide = pTexture->_wide;
		tall = pTexture->_tall;
	}
	else
	{
		wide = 0;
		tall = 0;
	}
}

int EngineSurface::isTextureIdValid( int id )
{
	return staticGetTextureById(id) != NULL;
}

void EngineSurface::drawSetSubTextureRGBA( int textureID, int drawX, int drawY, const byte* rgba, int subTextureWide, int subTextureTall )
{
	qglTexSubImage2D(GL_TEXTURE_2D, 0, drawX, drawY, subTextureWide, subTextureTall, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

void EngineSurface::drawFlushText()
{
	if (g_iVertexBufferEntriesUsed > 0)
	{
		qglEnableClientState(GL_VERTEX_ARRAY);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

		qglEnable(GL_TEXTURE_2D);
		qglEnable(GL_BLEND);

		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		qglColor4ub(_drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - _drawTextColor[3]);

		qglTexCoordPointer(2, GL_FLOAT, sizeof(float) * 4, g_VertexBuffer[0].texcoords);
		qglVertexPointer(2, GL_FLOAT, sizeof(float) * 4, g_VertexBuffer[0].vertex);

		qglDrawArrays(GL_QUADS, 0, g_iVertexBufferEntriesUsed);

		qglDisable(GL_BLEND);

		g_iVertexBufferEntriesUsed = 0;
	}
}

/*
=================
EngineSurface::resetViewPort

Resets View Port
=================
*/
void EngineSurface::resetViewPort()
{
}

/*
=================
EngineSurface::drawSetTextureBGRA

Draws a texture
=================
*/
void EngineSurface::drawSetTextureBGRA( int id, const byte* rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel )
{
	auto pTexture = staticGetTextureById(id);

	if (!pTexture)
	{
		Texture newTexture;
		memset(&newTexture, 0, sizeof(newTexture));

		newTexture._id = id;

		pTexture = &g_VGuiSurfaceTextures[g_VGuiSurfaceTextures.Insert(newTexture)];
	}

	if (!pTexture)
		return;

	pTexture->_id = id;
	pTexture->_wide = wide;
	pTexture->_tall = tall;

	int pow2Wide;
	int pow2Tall;

	for (int i = 0; i < 32; ++i)
	{
		pow2Wide = 1 << i;

		if (wide <= pow2Wide)
			break;
	}

	for (int i = 0; i < 32; ++i)
	{
		pow2Tall = 1 << i;

		if (tall <= pow2Tall)
			break;
	}

	int* pExpanded = NULL;

	const void* pData = rgba;

	// Convert to power of 2 texture
	if (wide != pow2Wide || tall != pow2Tall)
	{
		pExpanded = new int[pow2Wide * pow2Tall];

		pData = pExpanded;

		memset(pExpanded, 0, 4 * pow2Wide * pow2Tall);

		const int* pSrc = reinterpret_cast<const int*>(rgba);
		int* pDest = pExpanded;

		for (int y = 0; y < tall; ++y)
		{
			for (int x = 0; x < wide; ++x)
			{
				pDest[x] = pSrc[x];
			}

			pDest += pow2Wide;
			pSrc += wide;
		}
	}

	pTexture->_s0 = 0;
	pTexture->_t0 = 0;
	pTexture->_s1 = (double)(wide) / pow2Wide;
	pTexture->_t1 = (double)(tall) / pow2Tall;

	staticTextureCurrent = pTexture;

	currenttexture = id;

	qglBindTexture(GL_TEXTURE_2D, id);

	if (hardwareFilter)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pow2Wide, pow2Tall, 0, GL_BGRA, GL_UNSIGNED_BYTE, pData);

	if (pExpanded)
		delete[] pExpanded;
}

void EngineSurface::drawUpdateRegionTextureBGRA( int nTextureID, int x, int y, const byte* pchData, int wide, int tall )
{
	Texture* pTexture = staticGetTextureById(nTextureID);

	qglBindTexture(GL_TEXTURE_2D, nTextureID);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	qglPixelStorei(GL_UNPACK_ROW_LENGTH, pTexture->_wide);

	qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, pTexture->_wide, tall, GL_BGRA, GL_UNSIGNED_BYTE, &pchData[4 * y * pTexture->_wide]);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

/*
=================
VGui_ViewportPaintBackground

Paints background, called every frame
=================
*/
void VGui_ViewportPaintBackground( int* extents )
{
	RECT rect;
	POINT pnt;

	int initGLX, initGLY;
	int initGLWidth, initGLHeight;

	g_engdstAddrs.VGui_ViewportPaintBackground(&extents);

	GLFinishHud();

	initGLX = glx;
	initGLY = gly;
	initGLWidth = glwidth;
	initGLHeight = glheight;

	pnt.x = 0;
	pnt.y = 0;

	SDL_GetWindowPosition((SDL_Window*)pmainwindow, (int*)&pnt.x, (int*)&pnt.y);

	if (VideoMode_IsWindowed())
		SDL_GetWindowSize((SDL_Window*)pmainwindow, (int*)&rect.right, (int*)&rect.bottom);
	else
		VideoMode_GetCurrentVideoMode((int*)&rect.right, (int*)&rect.bottom, NULL);

	glx = extents[0] - pnt.x;
	gly = pnt.y + glheight - extents[3];
	glwidth = extents[2] - extents[0];
	glheight = extents[3] - extents[1];

	SCR_CalcRefdef();

	// Draw world, etc.
	V_RenderView();

	GLBeginHud();

	if (cls.state == ca_active && cls.signon == SIGNONS)
	{
		AllowFog(false);
		ClientDLL_HudRedraw(cl.intermission == 1);
		AllowFog(true);
	}

	if (scr_drawloading == false && (cl.intermission != 1 || key_dest))
		SCR_DrawConsole();

	SCR_DrawLoading();

	if (cls.state == ca_active && cls.signon == SIGNONS)
		Sbar_Draw();

	GLFinishHud();

	glx = initGLX;
	gly = initGLY;
	glwidth = initGLWidth;
	glheight = initGLHeight;

	SCR_CalcRefdef();
	GLBeginHud();
}