/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

// sys_getmodes.h
#ifndef SYS_GETMODES_H
#define SYS_GETMODES_H
#ifdef _WIN32
#pragma once
#endif

#include "IGameUIFuncs.h"

#define VM_Software		0
#define VM_OpenGL		1
#define VM_D3D			2

//-----------------------------------------------------------------------------
// Purpose: Manages the main window's video modes
//-----------------------------------------------------------------------------
// TODO: Move to ivideomode.h
class IVideoMode
{
public:
	virtual const char* GetName( void ) = 0;

	virtual bool Init( void* pvInstance ) = 0;
	virtual void PlayStartupSequence( void ) = 0;
	virtual void Shutdown( void ) = 0;

	virtual bool AddMode( int width, int height, int bpp ) = 0;
	virtual vmode_t* GetCurrentMode( void ) = 0;
	virtual vmode_t* GetMode( int num ) = 0;
	virtual int GetModeCount( void ) = 0;

	virtual bool IsWindowedMode( void ) const = 0;

	virtual bool GetInitialized( void ) const = 0;
	virtual void SetInitialized( bool init ) = 0;

	virtual void UpdateWindowPosition( void ) = 0;

	virtual void FlipScreen( void ) = 0;

	virtual void RestoreVideo( void ) = 0;
	virtual void ReleaseVideo( void ) = 0;
};

extern bool bNeedsFullScreenModeSwitch;

extern IVideoMode* videomode;

bool BUsesSDLInput( void );

void VideoMode_Create( void );
bool VideoMode_IsWindowed( void );
void VideoMode_GetVideoModes( vmode_t** liststart, int* count );
void VideoMode_GetCurrentVideoMode( int* wide, int* tall, int* bpp );
void VideoMode_GetCurrentRenderer( char* name, int namelen, int* windowed, int* hdmodels, int* addons_folder, int* vid_level );
void VideoMode_RestoreVideo( void );
void VideoMode_SetVideoMode( int width, int height, int bpp );
void VideoMode_SwitchMode( int hardware, int windowed );

#endif // SYS_GETMODES_H
