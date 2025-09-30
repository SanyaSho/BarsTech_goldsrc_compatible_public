//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#if !defined( ILAUNCHERDIRECTDRAW_H )
#define ILAUNCHERDIRECTDRAW_H
#ifdef _WIN32
#pragma once
#endif

class ILauncherDirectDraw
{
public:
	virtual				~ILauncherDirectDraw( void ) { }

	virtual bool		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	virtual void		EnumDisplayModes( int bpp ) = 0;
	virtual char*		ErrorToString( unsigned int code ) = 0;
};

extern ILauncherDirectDraw* directdraw;


#endif // ILAUNCHERDIRECTDRAW_H