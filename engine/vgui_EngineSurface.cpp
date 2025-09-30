#include "quakedef.h"
//#include "gl_draw.h"
#include "vgui_EngineSurface.h"

EngineSurface* EngineSurface::s_pEngineSurface = nullptr;

EXPOSE_SINGLE_INTERFACE( EngineSurface, IEngineSurface, ENGINESURFACE_INTERFACE_VERSION );

#if !defined ( GLQUAKE )
int staticTextureID = 4000;
#endif

EngineSurface::~EngineSurface()
{
	deleteRenderPlat();
	_renderPlat = nullptr;
	s_pEngineSurface = nullptr;
}

int EngineSurface::createNewTextureID()
{
#if defined ( GLQUAKE )
    return GL_GenTexture();
#else
    staticTextureID++;
    return staticTextureID;
#endif
}

void EngineSurface::drawGetTextPos( int& x, int& y )
{
	x = _drawTextPos[ 0 ];
	y = _drawTextPos[ 1 ];
}

void EngineSurface::drawSetColor( int r, int g, int b, int a )
{
	_drawColor[ 0 ] = r;
	_drawColor[ 1 ] = g;
	_drawColor[ 2 ] = b;
	_drawColor[ 3 ] = a;
}

void EngineSurface::drawSetTextColor( int r, int g, int b, int a )
{
	if( _drawTextColor[ 0 ] != r ||
		_drawTextColor[ 1 ] != g ||
		_drawTextColor[ 2 ] != b ||
		_drawTextColor[ 3 ] != a )
	{
		drawFlushText();

		_drawTextColor[ 0 ] = r;
		_drawTextColor[ 1 ] = g;
		_drawTextColor[ 2 ] = b;
		_drawTextColor[ 3 ] = a;
	}
}

void EngineSurface::drawSetTextPos( int x, int y )
{
	_drawTextPos[ 0 ] = x;
	_drawTextPos[ 1 ] = y;
}

void EngineSurface::freeEngineSurface()
{
	if( s_pEngineSurface )
	{
		delete s_pEngineSurface;
		s_pEngineSurface = nullptr;
	}
}
