//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#if defined( WIN32 ) && !defined( _X360 )
#define NOMINMAX
#include <wtypes.h>
#include <winuser.h>
#include <common\xbox\xboxstubs.h>
#endif
#include "tier0/dbg.h"
#include "vgui_key_translation.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#ifdef POSIX
#define VK_RETURN -1
#endif

#include <external\SDL2\include\SDL2\SDL.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct vgui_to_virtual_t
{
	vgui2::KeyCode vguiKeyCode;
	int32 windowsKeyCode;
	int32 sdlKeyCode;
};

static const vgui_to_virtual_t keyMap[] = 
{
	{ vgui2::KEY_0, '0', SDLK_0 },
	{ vgui2::KEY_1, '1', SDLK_1 },
	{ vgui2::KEY_2, '2', SDLK_2 },
	{ vgui2::KEY_3, '3', SDLK_3 },
	{ vgui2::KEY_4, '4', SDLK_4 },
	{ vgui2::KEY_5, '5', SDLK_5 },
	{ vgui2::KEY_6, '6', SDLK_6 },
	{ vgui2::KEY_7, '7', SDLK_7 },
	{ vgui2::KEY_8, '8', SDLK_8 },
	{ vgui2::KEY_9, '9', SDLK_9 },

	{ vgui2::KEY_A, 'A', SDLK_a },
	{ vgui2::KEY_B, 'B', SDLK_b },
	{ vgui2::KEY_C, 'C', SDLK_c },
	{ vgui2::KEY_D, 'D', SDLK_d },
	{ vgui2::KEY_E, 'E', SDLK_e },
	{ vgui2::KEY_F, 'F', SDLK_f },
	{ vgui2::KEY_G, 'G', SDLK_g },
	{ vgui2::KEY_H, 'H', SDLK_h },
	{ vgui2::KEY_I, 'I', SDLK_i },
	{ vgui2::KEY_J, 'J', SDLK_j },
	{ vgui2::KEY_K, 'K', SDLK_k },
	{ vgui2::KEY_L, 'L', SDLK_l },
	{ vgui2::KEY_M, 'M', SDLK_m },
	{ vgui2::KEY_N, 'N', SDLK_n },
	{ vgui2::KEY_O, 'O', SDLK_o },
	{ vgui2::KEY_P, 'P', SDLK_p },
	{ vgui2::KEY_Q, 'Q', SDLK_q },
	{ vgui2::KEY_R, 'R', SDLK_r },
	{ vgui2::KEY_S, 'S', SDLK_s },
	{ vgui2::KEY_T, 'T', SDLK_t },
	{ vgui2::KEY_U, 'U', SDLK_u },
	{ vgui2::KEY_V, 'V', SDLK_v },
	{ vgui2::KEY_W, 'W', SDLK_w },
	{ vgui2::KEY_X, 'X', SDLK_x },
	{ vgui2::KEY_Y, 'Y', SDLK_y },
	{ vgui2::KEY_Z, 'Z', SDLK_z },

	{ vgui2::KEY_PAD_0, 0, SDLK_KP_0 },
	{ vgui2::KEY_PAD_1, 0, SDLK_KP_1 },
	{ vgui2::KEY_PAD_2, 0, SDLK_KP_2 },
	{ vgui2::KEY_PAD_3, 0, SDLK_KP_3 },
	{ vgui2::KEY_PAD_4, 0, SDLK_KP_4 },
	{ vgui2::KEY_PAD_5, 0, SDLK_KP_5 },
	{ vgui2::KEY_PAD_6, 0, SDLK_KP_6 },
	{ vgui2::KEY_PAD_7, 0, SDLK_KP_7 },
	{ vgui2::KEY_PAD_8, 0, SDLK_KP_8 },
	{ vgui2::KEY_PAD_9, 0, SDLK_KP_9 },

	{ vgui2::KEY_PAD_DIVIDE, 0, SDLK_KP_DIVIDE },
	{ vgui2::KEY_PAD_MULTIPLY, 0, SDLK_KP_MULTIPLY },
	{ vgui2::KEY_PAD_MINUS, 0, SDLK_KP_MINUS },
	{ vgui2::KEY_PAD_PLUS, 0, SDLK_KP_PLUS },
	{ vgui2::KEY_PAD_ENTER, 0, SDLK_KP_ENTER },
	{ vgui2::KEY_PAD_DECIMAL, 0, SDLK_KP_DECIMAL },

	{ vgui2::KEY_LBRACKET, 0, SDLK_LEFTBRACKET },
	{ vgui2::KEY_RBRACKET, 0, SDLK_RIGHTBRACKET },
	{ vgui2::KEY_SEMICOLON, 0, SDLK_SEMICOLON },
	{ vgui2::KEY_APOSTROPHE, 0, SDLK_QUOTE },
	{ vgui2::KEY_BACKQUOTE, 0, SDLK_BACKQUOTE },
	{ vgui2::KEY_COMMA, 0, SDLK_COMMA },
	{ vgui2::KEY_PERIOD, 0, SDLK_PERIOD },
	{ vgui2::KEY_SLASH, 0, SDLK_SLASH },
	{ vgui2::KEY_BACKSLASH, 0, SDLK_BACKSLASH },
	{ vgui2::KEY_MINUS, 0, SDLK_MINUS },
	{ vgui2::KEY_EQUAL, 0, SDLK_EQUALS },
	{ vgui2::KEY_ENTER, 0, SDLK_RETURN },
	{ vgui2::KEY_SPACE, 0, SDLK_SPACE },
	{ vgui2::KEY_BACKSPACE, 0, SDLK_BACKSPACE },
	{ vgui2::KEY_TAB, 0, SDLK_TAB },
	{ vgui2::KEY_CAPSLOCK, 0, SDLK_CAPSLOCK },
	{ vgui2::KEY_NUMLOCK, 0, SDLK_NUMLOCKCLEAR },
	{ vgui2::KEY_ESCAPE, 0, SDLK_ESCAPE },
	{ vgui2::KEY_SCROLLLOCK, 0, SDLK_SCROLLLOCK },
	{ vgui2::KEY_INSERT, 0, SDLK_INSERT },
	{ vgui2::KEY_DELETE, 0, SDLK_DELETE },
	{ vgui2::KEY_HOME, 0, SDLK_HOME },
	{ vgui2::KEY_END, 0, SDLK_END },
	{ vgui2::KEY_PAGEUP, 0, SDLK_PAGEUP },
	{ vgui2::KEY_PAGEDOWN, 0, SDLK_PAGEDOWN },
	{ vgui2::KEY_BREAK, 0, SDLK_PAUSE },
	{ vgui2::KEY_RSHIFT, 0, SDLK_RSHIFT },
	{ vgui2::KEY_LSHIFT, 0, SDLK_LSHIFT },
	{ vgui2::KEY_RALT, 0, SDLK_RALT },
	{ vgui2::KEY_LALT, 0, SDLK_LALT },
	{ vgui2::KEY_RCONTROL, 0, SDLK_RCTRL },
	{ vgui2::KEY_LCONTROL, 0, SDLK_LCTRL },
	{ vgui2::KEY_LWIN, 0, SDLK_LGUI },
	{ vgui2::KEY_RWIN, 0, SDLK_RGUI },
	{ vgui2::KEY_APP, 0, SDLK_APPLICATION },

	{ vgui2::KEY_UP, 0, SDLK_UP },
	{ vgui2::KEY_LEFT, 0, SDLK_LEFT },
	{ vgui2::KEY_DOWN, 0, SDLK_DOWN },
	{ vgui2::KEY_RIGHT, 0, SDLK_RIGHT },

	{ vgui2::KEY_F1, 0, SDLK_F1 },
	{ vgui2::KEY_F2, 0, SDLK_F2 },
	{ vgui2::KEY_F3, 0, SDLK_F3 },
	{ vgui2::KEY_F4, 0, SDLK_F4 },
	{ vgui2::KEY_F5, 0, SDLK_F5 },
	{ vgui2::KEY_F6, 0, SDLK_F6 },
	{ vgui2::KEY_F7, 0, SDLK_F7 },
	{ vgui2::KEY_F8, 0, SDLK_F8 },
	{ vgui2::KEY_F9, 0, SDLK_F9 },
	{ vgui2::KEY_F10, 0, SDLK_F10 },
	{ vgui2::KEY_F11, 0, SDLK_F11 },
	{ vgui2::KEY_F12, 0, SDLK_F12 },
};

static vgui2::KeyCode SDLKeysumToKeyCode( int keyCode )
{
	switch ( keyCode )
	{
	case SDLK_a: return vgui2::KEY_A;
	case SDLK_b: return vgui2::KEY_B;
	case SDLK_c: return vgui2::KEY_C;
	case SDLK_d: return vgui2::KEY_D;
	case SDLK_e: return vgui2::KEY_E;
	case SDLK_f: return vgui2::KEY_F;
	case SDLK_g: return vgui2::KEY_G;
	case SDLK_h: return vgui2::KEY_H;
	case SDLK_i: return vgui2::KEY_I;
	case SDLK_j: return vgui2::KEY_J;
	case SDLK_k: return vgui2::KEY_K;
	case SDLK_l: return vgui2::KEY_L;
	case SDLK_m: return vgui2::KEY_M;
	case SDLK_n: return vgui2::KEY_N;
	case SDLK_o: return vgui2::KEY_O;
	case SDLK_p: return vgui2::KEY_P;
	case SDLK_q: return vgui2::KEY_Q;
	case SDLK_r: return vgui2::KEY_R;
	case SDLK_s: return vgui2::KEY_S;
	case SDLK_t: return vgui2::KEY_T;
	case SDLK_u: return vgui2::KEY_U;
	case SDLK_v: return vgui2::KEY_V;
	case SDLK_w: return vgui2::KEY_W;
	case SDLK_x: return vgui2::KEY_X;
	case SDLK_y: return vgui2::KEY_Y;
	case SDLK_z: return vgui2::KEY_Z;

	case SDLK_BACKSPACE: return vgui2::KEY_BACKSPACE;
	case SDLK_TAB: return vgui2::KEY_TAB;
	case SDLK_RETURN: return vgui2::KEY_ENTER;
	case SDLK_ESCAPE: return vgui2::KEY_ESCAPE;
	case SDLK_SPACE: return vgui2::KEY_SPACE;
	case SDLK_QUOTE: return vgui2::KEY_APOSTROPHE;
	case SDLK_COMMA: return vgui2::KEY_COMMA;
	case SDLK_MINUS: return vgui2::KEY_MINUS;
	case SDLK_PERIOD: return vgui2::KEY_PERIOD;
	case SDLK_SLASH: return vgui2::KEY_SLASH;

	case SDLK_0: return vgui2::KEY_0;
	case SDLK_1: return vgui2::KEY_1;
	case SDLK_2: return vgui2::KEY_2;
	case SDLK_3: return vgui2::KEY_3;
	case SDLK_4: return vgui2::KEY_4;
	case SDLK_5: return vgui2::KEY_5;
	case SDLK_6: return vgui2::KEY_6;
	case SDLK_7: return vgui2::KEY_7;
	case SDLK_8: return vgui2::KEY_8;
	case SDLK_9: return vgui2::KEY_9;

	case SDLK_SEMICOLON: return vgui2::KEY_SEMICOLON;
	case SDLK_EQUALS: return vgui2::KEY_EQUAL;
	case SDLK_LEFTBRACKET: return vgui2::KEY_LBRACKET;
	case SDLK_BACKSLASH: return vgui2::KEY_BACKSLASH;
	case SDLK_RIGHTBRACKET: return vgui2::KEY_RBRACKET;
	case SDLK_BACKQUOTE: return vgui2::KEY_BACKQUOTE;

	case SDLK_DELETE: return vgui2::KEY_DELETE;

	case 1073741867: return vgui2::KEY_TAB;
	case 1073741869: return vgui2::KEY_MINUS;
	case 1073741870: return vgui2::KEY_EQUAL;
	case 1073741877: return vgui2::KEY_BACKQUOTE;
	case 1073741900: return vgui2::KEY_DELETE;

	case SDLK_CAPSLOCK: return vgui2::KEY_CAPSLOCK;

	case SDLK_F1: return vgui2::KEY_F1;
	case SDLK_F2: return vgui2::KEY_F2;
	case SDLK_F3: return vgui2::KEY_F3;
	case SDLK_F4: return vgui2::KEY_F4;
	case SDLK_F5: return vgui2::KEY_F5;
	case SDLK_F6: return vgui2::KEY_F6;
	case SDLK_F7: return vgui2::KEY_F7;
	case SDLK_F8: return vgui2::KEY_F8;
	case SDLK_F9: return vgui2::KEY_F9;
	case SDLK_F10: return vgui2::KEY_F10;
	case SDLK_F11: return vgui2::KEY_F11;
	case SDLK_F12: return vgui2::KEY_F12;

	case SDLK_SCROLLLOCK: return vgui2::KEY_SCROLLLOCK;
	case SDLK_PAUSE: return vgui2::KEY_BREAK;
	case SDLK_INSERT: return vgui2::KEY_INSERT;
	case SDLK_HOME: return vgui2::KEY_HOME;
	case SDLK_PAGEUP: return vgui2::KEY_PAGEUP;
	case SDLK_END: return vgui2::KEY_END;
	case SDLK_PAGEDOWN: return vgui2::KEY_PAGEDOWN;
	case SDLK_RIGHT: return vgui2::KEY_RIGHT;
	case SDLK_LEFT: return vgui2::KEY_LEFT;
	case SDLK_DOWN: return vgui2::KEY_DOWN;
	case SDLK_UP: return vgui2::KEY_UP;

	case SDLK_NUMLOCKCLEAR: return vgui2::KEY_NUMLOCK;
	case SDLK_KP_DIVIDE: return vgui2::KEY_PAD_DIVIDE;
	case SDLK_KP_MULTIPLY: return vgui2::KEY_PAD_MULTIPLY;
	case SDLK_KP_MINUS: return vgui2::KEY_PAD_MINUS;
	case SDLK_KP_PLUS: return vgui2::KEY_PAD_PLUS;
	case SDLK_KP_ENTER: return vgui2::KEY_PAD_ENTER;
	case SDLK_KP_1: return vgui2::KEY_PAD_1;
	case SDLK_KP_2: return vgui2::KEY_PAD_2;
	case SDLK_KP_3: return vgui2::KEY_PAD_3;
	case SDLK_KP_4: return vgui2::KEY_PAD_4;
	case SDLK_KP_5: return vgui2::KEY_PAD_5;
	case SDLK_KP_6: return vgui2::KEY_PAD_6;
	case SDLK_KP_7: return vgui2::KEY_PAD_7;
	case SDLK_KP_8: return vgui2::KEY_PAD_8;
	case SDLK_KP_9: return vgui2::KEY_PAD_9;
	case SDLK_KP_0: return vgui2::KEY_PAD_0;
	case SDLK_KP_PERIOD: return vgui2::KEY_PAD_DECIMAL;

	case SDLK_KP_DECIMAL: return vgui2::KEY_PAD_DECIMAL;

	case SDLK_LCTRL: return vgui2::KEY_LCONTROL;
	case SDLK_LSHIFT: return vgui2::KEY_LSHIFT;
	case SDLK_LALT: return vgui2::KEY_LALT;
	case SDLK_LGUI: return vgui2::KEY_LWIN;
	case SDLK_RCTRL: return vgui2::KEY_RCONTROL;
	case SDLK_RSHIFT: return vgui2::KEY_RSHIFT;
	case SDLK_RALT: return vgui2::KEY_RALT;
	case SDLK_RGUI: return vgui2::KEY_RWIN;

	default: return vgui2::KEY_NONE;
	}
}

static vgui2::KeyCode s_pVirtualKeyToVGUI[ 256 ];
static int s_pVGUIToVirtualKey[ 256 ];

namespace vgui2
{
void InitKeyTranslationTable()
{
	static bool bInitted = false;

	if( !bInitted )
	{
		bInitted = true;

		memset ( s_pVirtualKeyToVGUI, 0, sizeof( s_pVirtualKeyToVGUI ) );

		for ( int i = 0; i < ARRAYSIZE( keyMap ); ++i )
		{
			s_pVirtualKeyToVGUI[ keyMap[ i ].sdlKeyCode ] = keyMap[ i ].vguiKeyCode;
		}
	}
}

void InitVGUIToVKTranslationTable()
{
	static bool bInitted = false;

	if( !bInitted )
	{
		bInitted = true;

		memset ( s_pVGUIToVirtualKey, 0, sizeof( s_pVGUIToVirtualKey ) );

		for ( int i = 0; i < ARRAYSIZE( keyMap ); ++i )
		{
			s_pVGUIToVirtualKey[ keyMap[ i ].vguiKeyCode ] = keyMap[ i ].sdlKeyCode;
		}
	}
}
}

vgui2::KeyCode KeyCode_VirtualKeyToVGUI( int keyCode )
{
	vgui2::InitKeyTranslationTable();

	return SDLKeysumToKeyCode( keyCode );
}

int KeyCode_VGUIToVirtualKey( vgui2::KeyCode keyCode )
{
	vgui2::InitVGUIToVKTranslationTable();

	if ( keyCode >= 0 && keyCode <= 255 )
		return s_pVGUIToVirtualKey[keyCode];

	return -1;
}

bool IsKeyCode( int vk_code )
{
	return vk_code <= 255 && vk_code > 0;
}

bool IsMouseCode( int vk_code )
{
	return false;
}

vgui2::MouseCode MouseCode_VirtualKeyToVGUI( int vk_code )
{
	return vgui2::MOUSE_LAST;
}

int32 MouseCode_VGUIToVirtualKey( vgui2::MouseCode keyCode )
{
	return 0;
}
