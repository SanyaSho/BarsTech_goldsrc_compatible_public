//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Win32-Dependant VGUI Rendering for Software mode
//
// $NoKeywords: $
//=============================================================================

#include "quakedef.h"
#include "vgui_EngineSurface.h"
#include "draw.h"
#include "screen.h"
#include "sys_getmodes.h"
#include "keys.h"
#include "vgui_int.h"
#include "vid.h"
#include "client.h"
#include "view.h"
#include "sbar.h"

extern SDL_Window* pmainwindow;
extern vrect_t scr_vrect;

EngineSurface::EngineSurface()
{
	createRenderPlat();
}

#if !defined ( GLQUAKE )

int g_TranslateX;
int g_TranslateY;

wrect_t scissor_rect;

void EngineSurface::createRenderPlat()
{
	_renderPlat = new EngineSurfaceRenderPlat;

	// Create a TextureManager object as well
	_renderPlat->_tmgr = new TextureManager;
}

void EngineSurface::deleteRenderPlat()
{
	delete _renderPlat->_tmgr;
	_renderPlat->_tmgr = NULL;

	delete _renderPlat;
	_renderPlat = NULL;
}

void EngineSurface::pushMakeCurrent(int* insets, int* absExtents, int* clipRect, bool translateToScreenSpace)
{
	RECT rect;
	POINT pnt;

	pnt.x = 0;
	pnt.y = 0;

	if (translateToScreenSpace)
	{
		if (VideoMode_IsWindowed())
			SDL_GetWindowPosition((SDL_Window*)pmainwindow, (int*)&pnt.x, (int*)&pnt.y);
	}

	rect.left = 0;
	rect.top = 0;

	if (VideoMode_IsWindowed())
		SDL_GetWindowSize((SDL_Window*)pmainwindow, (int*)&rect.right, (int*)&rect.bottom);
	else
		VideoMode_GetCurrentVideoMode((int*)&rect.right, (int*)&rect.bottom, NULL);

	rect.right += rect.left;
	rect.bottom += rect.top;

	_renderPlat->viewport[0] = absExtents[0] - pnt.x;
	_renderPlat->viewport[1] = absExtents[1] - pnt.y;
	_renderPlat->viewport[2] = absExtents[2] - pnt.x;
	_renderPlat->viewport[3] = absExtents[3] - pnt.y;

	int x, y;
	int width, height;

	x = clipRect[0] - pnt.x;
	y = clipRect[1] - pnt.y;
	width = clipRect[2] - pnt.x;
	height = clipRect[3] - pnt.y;

	g_TranslateX = insets[0];
	g_TranslateY = insets[1];

	if (x != rect.left || y != rect.top || width != rect.right || height != rect.bottom)
	{
		EnableScissorTest(x - insets[0], y - insets[1], width + insets[0] - x, height + insets[1] - y);
	}
}

void EngineSurface::popMakeCurrent()
{
	DisableScissorTest();
	resetViewPort();
}

//-----------------------------------------------------------------------------
// Purpose: Draws a filled rectangle
//-----------------------------------------------------------------------------
void EngineSurface::drawFilledRect(int x0, int y0, int x1, int y1)
{
	if (_drawColor[3])
	{
		if (_drawColor[3] != 255)
		{
			Draw_FillRGBABlend(x0 + g_TranslateX + _renderPlat->viewport[0], y0 + g_TranslateY + _renderPlat->viewport[1], x1 - x0, y1 - y0,
				_drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3]);
		}
	}
	else
	{
		Draw_FillRGB(x0 + g_TranslateX + _renderPlat->viewport[0], y0 + g_TranslateY + _renderPlat->viewport[1], x1 - x0, y1 - y0,
			_drawColor[0], _drawColor[1], _drawColor[2]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws an unfilled rectangle in the current drawcolor
//-----------------------------------------------------------------------------
void EngineSurface::drawOutlinedRect(int x0, int y0, int x1, int y1)
{
	// draw an outline of a rectangle using 4 filledRect
	drawFilledRect(g_TranslateX + x0, g_TranslateY + y0, g_TranslateX + x1, g_TranslateY + y0 + 1);			// top
	drawFilledRect(g_TranslateX + x0, g_TranslateY + y1 - 1, g_TranslateX + x1, g_TranslateY + y1);			// bottom
	drawFilledRect(g_TranslateX + x0, g_TranslateY + y0 + 1, g_TranslateX + x0 + 1, g_TranslateY + y1 - 1); // left
	drawFilledRect(g_TranslateX + x1 - 1, g_TranslateY + y0 + 1, g_TranslateX + x1, g_TranslateY + y1 - 1); // right
}

//-----------------------------------------------------------------------------
// Purpose: Draws a line
//-----------------------------------------------------------------------------
void EngineSurface::drawLine(int x1, int y1, int x2, int y2)
{
	Draw_LineRGB(g_TranslateX + x1 + _renderPlat->viewport[0],
		g_TranslateY + y1 + _renderPlat->viewport[1],
		g_TranslateX + x2 + _renderPlat->viewport[0],
		g_TranslateY + y2 + _renderPlat->viewport[1],
		_drawTextColor[0], _drawTextColor[1], _drawTextColor[2]);
}

void EngineSurface::drawPolyLine(int* px, int* py, int numPoints)
{
	int i;

	for (i = 0; i < numPoints - 1; i++)
	{
		Draw_LineRGB(g_TranslateX + px[i] + _renderPlat->viewport[0],
			g_TranslateY + py[i] + _renderPlat->viewport[1],
			g_TranslateX + px[i + 1] + _renderPlat->viewport[0],
			g_TranslateY + py[i + 1] + _renderPlat->viewport[1],
			_drawTextColor[0], _drawTextColor[1], _drawTextColor[2]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws a textured poly
//-----------------------------------------------------------------------------
void EngineSurface::drawTexturedPolygon(vgui2::VGuiVertex* pVertices, int n)
{
}

void EngineSurface::drawSetTextureRGBA(int id, const byte* rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel)
{
	if (_renderPlat->_tmgr)
		_renderPlat->_tmgr->SetTexture(id, wide, tall, reinterpret_cast<const char*>(rgba));
}

void EngineSurface::drawSetTexture(int id)
{
	if (_renderPlat->_tmgr)
		_renderPlat->_tdata = _renderPlat->_tmgr->GetTextureById(id);
	else
		_renderPlat->_tdata = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Draws a textured rectangle
//-----------------------------------------------------------------------------
void EngineSurface::drawTexturedRect(int x0, int y0, int x1, int y1)
{
	int x, y;
	int wide, tall;

	if (!_renderPlat->_tdata)
		return;

	wide = x1 - x0;
	tall = y1 - y0;

	x = x0 + g_TranslateX + _renderPlat->viewport[0];
	y = y0 + g_TranslateY + _renderPlat->viewport[1];

	if (wide == _renderPlat->_tdata->_wide && tall == _renderPlat->_tdata->_tall)
	{
		Draw_RGBAImage(x, y, 0, 0, wide, y1 - y0, _renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall,
			_renderPlat->_tdata->_rgba, false, _drawColor[0], _drawColor[1], _drawColor[2], 255 - (byte)_drawColor[3]);
	}
	else
	{
		Draw_ScaledRGBAImage(x, y, wide, y1 - y0, 0, 0, _renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall,
			_renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall, _renderPlat->_tdata->_rgba);
	}
}

void EngineSurface::drawTexturedRectAdd(int x0, int y0, int x1, int y1)
{
	int x, y;
	int wide, tall;

	if (!_renderPlat->_tdata)
		return;

	wide = x1 - x0;
	tall = y1 - y0;

	x = x0 + g_TranslateX + _renderPlat->viewport[0];
	y = y0 + g_TranslateY + _renderPlat->viewport[1];

	if (wide == _renderPlat->_tdata->_wide && tall == _renderPlat->_tdata->_tall)
	{
		Draw_RGBAImageAdditive(x, y, 0, 0, wide, y1 - y0, _renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall,
			_renderPlat->_tdata->_rgba, false, _drawColor[0], _drawColor[1], _drawColor[2], 255 - (byte)_drawColor[3]);
	}
	else
	{
		Draw_ScaledRGBAImage(x, y, wide, y1 - y0, 0, 0, _renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall,
			_renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall, _renderPlat->_tdata->_rgba);
	}
}

void EngineSurface::drawPrintChar(int x, int y, int wide, int tall, float s0, float t0, float s1, float t1)
{
	if (!_renderPlat->_tdata)
		return;

	Draw_RGBAImage(x + g_TranslateX + _renderPlat->viewport[0], y + g_TranslateY + _renderPlat->viewport[1],
		(float)_renderPlat->_tdata->_wide * s0, (float)_renderPlat->_tdata->_tall * t0,
		wide, tall, _renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall,
		_renderPlat->_tdata->_rgba, true, _drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255);
}

void EngineSurface::drawPrintCharAdd(int x, int y, int wide, int tall, float s0, float t0, float s1, float t1)
{
	if (!_renderPlat->_tdata)
		return;

	Draw_RGBAImageAdditive(x + g_TranslateX + _renderPlat->viewport[0], y + g_TranslateY + _renderPlat->viewport[1],
		(float)_renderPlat->_tdata->_wide * s0, (float)_renderPlat->_tdata->_tall * t0, wide, tall, _renderPlat->_tdata->_wide, _renderPlat->_tdata->_tall,
		_renderPlat->_tdata->_rgba, 1, _drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - (byte)_drawTextColor[3]);
}

//-----------------------------------------------------------------------------
// Purpose: Associates a texture with a material file (also binds it)
//-----------------------------------------------------------------------------
extern qboolean LoadTGA(char* szFilename, byte* buffer, int bufferSize, int* width, int* height);
void EngineSurface::drawSetTextureFile(int id, const char* filename, int hardwareFilter, bool forceReload)
{
	char name[260];

	byte buf[512 * 512];

	int width, height;
	bool bLoaded = false;

	FileHandle_t hFile;
	Texture* ptex = NULL;

	ptex = _renderPlat->_tmgr->GetTextureById(id);

	if (!ptex || forceReload)
	{
		snprintf(name, sizeof(name), "%s.tga", filename);

		if (LoadTGA(name, buf, sizeof(buf), &width, &height))
		{
			// Loaded!
			bLoaded = true;
		}
		else
		{
			// Can't load TGA? Load the BMP then
			snprintf(name, sizeof(name), "%s.bmp", filename);

			hFile = FS_Open(name, "rb");

			if (hFile)
			{
				VGui_LoadBMP(hFile, buf, sizeof(buf), &width, &height);

				// Loaded!
				bLoaded = true;
			}
		}

		if (bLoaded)
		{
			drawSetTextureRGBA(id, buf, width, height, hardwareFilter, false);
		}

		ptex = _renderPlat->_tmgr->GetTextureById(id);
	}

	if (ptex)
		_renderPlat->_tdata = ptex;
}

void EngineSurface::drawGetTextureSize(int id, int& wide, int& tall)
{
	Texture* ptex = NULL;

	ptex = _renderPlat->_tmgr->GetTextureById(id);

	if (ptex)
	{
		wide = ptex->_wide;
		tall = ptex->_tall;
	}
	else
	{
		wide = 0;
		tall = 0;
	}
}

int EngineSurface::isTextureIdValid(int id)
{
	return _renderPlat->_tmgr->GetTextureById(id) != 0;
}

void EngineSurface::drawSetSubTextureRGBA(int textureID, int drawX, int drawY, const byte* rgba, int subTextureWide, int subTextureTall)
{
	int i;
	Texture* ptex = NULL;

	ptex = _renderPlat->_tmgr->GetTextureById(textureID);

	for (i = 0; i < subTextureTall; i++)
	{
		memcpy(&ptex->_rgba[4 * drawX + 4 * ptex->_wide * (i + drawY)], rgba, subTextureWide * 4);
		rgba += subTextureWide * 4;
	}

	_renderPlat->_tdata = ptex;
}

void EngineSurface::drawFlushText()
{
}

//-----------------------------------------------------------------------------
// Purpose: Resets View Port
//-----------------------------------------------------------------------------
void EngineSurface::resetViewPort()
{
	_renderPlat->viewport[0] = 0;
	_renderPlat->viewport[1] = 0;
	_renderPlat->viewport[2] = 0;
	_renderPlat->viewport[3] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Draws a texture
//-----------------------------------------------------------------------------
void EngineSurface::drawSetTextureBGRA(int id, const byte* rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel)
{
	if (_renderPlat->_tmgr)
		_renderPlat->_tmgr->SetTexture(id, wide, tall, (char*)(rgba));
}

void EngineSurface::drawUpdateRegionTextureBGRA(int nTextureID, int x, int y, const byte* pchData, int wide, int tall)
{
}

/*
=================
VGui_ViewportPaintBackground

Paints background, called every frame
=================
*/
void VGui_ViewportPaintBackground(int* extents)
{
	RecEngVGui_ViewportPaintBackground(extents);

	V_RenderView();

	FillBackGround();

	if (!scr_drawloading && (cl.intermission != 1 || key_dest))
		SCR_DrawConsole();

	if (cls.state == ca_active)
		ClientDLL_HudRedraw(cl.intermission == 1);
}

#endif