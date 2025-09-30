// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#define DIRECTINPUT_VERSION 0x700
#define WIN32_LEAN_AND_MEAN
#include "winsani_in.h"
#include <windows.h>
#include "winsani_out.h"
#define CINTERFACE
#include <dinput.h>
#include "winquake.h"
#include "quakedef.h"
#include <SDL2\SDL_syswm.h>
#include "IEngine.h"
#include "vid.h"

/*
#define DINPUT_BUFFERSIZE           32

int			mouse_buttons;
int			mouse_oldbuttonstate;
POINT		current_pos;
int			mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

static qboolean	restore_spi;
static int		originalmouseparms[3], newmouseparms[3] = { 0, 0, 1 };

qboolean	dinput_acquired;
qboolean mouseinitialized;
static LPDIRECTINPUTA		g_pdi;
static LPDIRECTINPUTDEVICE	g_pMouse;
static LPDIRECTINPUTDEVICE	g_pKeyboard;

static unsigned int		mstate_di;

static HINSTANCE hInstDI;

static qboolean	dinput;

qboolean input_called;
unsigned DirectInputVersion;

int g_maccel, g_mspeed;
int g_mlimits[2];
int g_xMouse, g_yMouse;
int g_MouseButtons;
int g_KeyLast, gKeyTicksRepeat, gKeyTicksPressed, gKeyboardDelay, g_KeyboardSpeed;*/

void DI_HandleEvents()
{
	// Nothing since HL25
}

