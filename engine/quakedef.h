#ifndef ENGINE_QUAKEDEF_H
#define ENGINE_QUAKEDEF_H

#define QUAKEDEF_H

/**
*	@file
*
*	primary header for client
*/

#include <cmath>

#define MAX_NUM_ARGVS	50

#if defined(_M_IX86)
#define __i386__	1
#endif

#if defined __i386__ // && !defined __sun__
#define id386	1
#else
#define id386	0
#endif

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2

#define	MAX_STYLESTRING	64

#define VSTDLIB_BACKWARD_COMPAT

#include "tier0/platform.h"
#include "winsani_in.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "winsani_out.h"
#include "commonmacros.h"
#include "filesystem.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "mathlib.h"
#include "usercmd.h"
#include "common.h"
#include "net.h"
#include "const.h"
#if defined(GLQUAKE)
#include "gl_model.h"
#else
#include "model.h"
#endif
#include "console.h"
#include "sv_main.h"
#include "sys.h"
#include "strtools.h"
#include "protocol.h"
#include "sv_log.h"
#include "com_model.h"
#include "info.h"
#include "sv_steam3.h"
#include "keys.h"
#include "wrect.h"
#include "cdll_int.h"
#include "qlimits.h"


#if defined(_WIN32)
#define Q_ARRAYSIZE ARRAYSIZE
#endif

#if defined(GLQUAKE)
#include <GL/glew.h>
#include <gl/GL.h>
#include "qgl.h"
#include "glquake.h"
#endif

/**
*	the host system specifies the base of the directory tree, the
*	command line parms passed to the program, and the amount of memory
*	available for the program to use
*/
struct quakeparms_t
{
	char* basedir;
	char* cachedir;			// for development over ISDN lines
	int argc;
	char** argv;
	void *membase;
	int memsize;
};


extern quakeparms_t host_parms;

/**
*	true if into command execution
*/
extern qboolean host_initialized;

/**
*	not bounded in any way, changed at
*	start of every frame, never reset
*/
extern double realtime;

void Host_Error( const char* error, ... );

int Host_Init( quakeparms_t* parms );
void Host_Shutdown();

/**
*	Loads the server dll if needed.
*/
void Host_InitializeGameDLL();

void Host_ClearSaveDirectory();

#endif //ENGINE_QUAKEDEF_H
