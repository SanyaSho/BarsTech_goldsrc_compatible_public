// gl_screen.cpp -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "glHud.h"
#include "host.h"
#include "sys_getmodes.h"
#include "draw.h"
#include "vid.h"
#include "screen.h"
#include "vgui_int.h"
#include "keys.h"
#include "wad.h"
#include "kbutton.h"
#include "sbar.h"
#include "sv_main.h"
#include "vgui2/text_draw.h"
#include "view.h"


// In other C files.
qboolean V_CheckGamma( void );

#define SCR_CENTERSTRING_MAX	40

viddef_t		vid;			// global video state

// TODO: check which ones are boolean
int clearnotify = 0;
int scr_center_lines = 0;
int scr_erase_lines = 0;
int scr_erase_center = 0;
int scr_fullupdate = 0;

float scr_centertime_off = 0;
float scr_centertime_start = 0;

char scr_centerstring[1024];

float scr_con_current = 0;
float scr_fov_value = 90;

int scr_copytop = 0;
int scr_copyeverything = 0;
qboolean scr_initialized = false;		// ready to draw
qboolean scr_drawloading = false;

int glx = 0;
int gly = 0;
int glwidth = 0;
int glheight = 0;

int                     sb_lines;

extern RECT window_rect;

extern int bDoScaledFBO;

qpic_t* scr_paused = NULL;

vrect_t scr_vrect;

cvar_t scr_viewsize = { const_cast<char*>("viewsize"), const_cast<char*>("120"), FCVAR_ARCHIVE };
cvar_t scr_graphheight = { const_cast<char*>("graphheight"), const_cast<char*>("64.0"), FCVAR_ARCHIVE };

cvar_t scr_showpause = {  const_cast<char*>("showpause"),	   const_cast<char*>("1") };
cvar_t scr_centertime = { const_cast<char*>("scr_centertime"), const_cast<char*>("2") };
cvar_t scr_printspeed = { const_cast<char*>("scr_printspeed"), const_cast<char*>("8") };

// Download slider
cvar_t scr_connectmsg = {  const_cast<char*>("scr_connectmsg"), const_cast<char*>("0") };
cvar_t scr_connectmsg1 = { const_cast<char*>("scr_connectmsg1"), const_cast<char*>("0") };
cvar_t scr_connectmsg2 = { const_cast<char*>("scr_connectmsg2"), const_cast<char*>("0") };

cvar_t scr_downloading = { const_cast<char*>("scr_downloading"), const_cast<char*>("-1.0") }; // gl_screen.c:102

void SCR_ScreenShot_f( void );
void SCR_SizeUp_f( void );
void SCR_SizeDown_f( void );
void SCR_CalcRefdef();

/*
=================
SCR_Init

Initializes Screen
=================
*/
void SCR_Init( void )
{
	Cvar_RegisterVariable(&scr_viewsize);
	Cvar_RegisterVariable(&scr_showpause);

	Cvar_RegisterVariable(&scr_centertime);
	Cvar_RegisterVariable(&scr_printspeed);
	Cvar_RegisterVariable(&scr_graphheight);

	Cvar_RegisterVariable(&scr_connectmsg);
	Cvar_RegisterVariable(&scr_connectmsg1);
	Cvar_RegisterVariable(&scr_connectmsg2);

	Cmd_AddCommand(const_cast<char*>("screenshot"), SCR_ScreenShot_f);
	Cmd_AddCommand(const_cast<char*>("sizeup"), SCR_SizeUp_f);
	Cmd_AddCommand(const_cast<char*>("sizedown"), SCR_SizeDown_f);

	scr_paused = Draw_PicFromWad(const_cast<char*>("paused"));
	scr_initialized = true;
}

void GL_BreakOnError( void )
{
}

void SCR_DrawConsole( void )
{
	Con_DrawNotify();
}

void SCR_DrawCenterString( void )
{
	int		l;
	int		j;
	int		x, y;
	int		remaining;

	// the finale prints the characters one at a time
	if (cl.intermission)
	{
		if (VGuiWrap2_IsInCareerMatch())
			return;

		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	}
	else
		remaining = 9999;

	Draw_GetDefaultColor();

	scr_erase_center = 0;
	const char* start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = glheight * 0.35;
	else
		y = 48;

	char line[SCR_CENTERSTRING_MAX * 2];

	do
	{
		// scan the width of the line
		for (l = 0; l < SCR_CENTERSTRING_MAX; ++l)
		{
			if (start[l] == '\n' || !start[l])
				break;
		}

		strncpy(line, start, l);
		line[l] = '\0';

		x = (glwidth - Draw_StringLen(line, VGUI2_GetConsoleFont())) / 2;

		for (j = 0; j < l; ++j)
		{
			x += Draw_Character(x, y, start[j], VGUI2_GetConsoleFont());
			if (!remaining--)
				return;
		}

		y += 12;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;

		start++;		// skip the \n
	} while (true);
}

void SCR_CheckDrawCenterString( void )
{
	scr_copytop = true;

	if (scr_erase_lines < scr_center_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;

	if (key_dest != key_game)
		return;

	SCR_DrawCenterString();
}

/*
=================
SCR_CenterPrint
=================
*/
void SCR_CenterPrint( const char* str )
{
	Q_strncpy(scr_centerstring, str, Q_ARRAYSIZE(scr_centerstring) - 1);
	scr_centerstring[Q_ARRAYSIZE(scr_centerstring) - 1] = '\0';
	scr_center_lines = 1;
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	for (const char* pszStr = str; *pszStr; ++pszStr)
	{
		if (*pszStr == '\n')
			++scr_center_lines;
	}
}

void D_FillRect( vrect_t* r, byte* color )
{
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_BLEND);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglEnable(GL_ALPHA_TEST);

	qglColor4f(color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, 1.0);

	qglDisable(GL_DEPTH_TEST);

	qglBegin(GL_QUADS);
	qglVertex2f(r->x, r->y);
	qglVertex2f(r->x + r->width, r->y);
	qglVertex2f(r->x + r->width, r->y + r->height);
	qglVertex2f(r->x, r->y + r->height);
	qglEnd();

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);

	qglColor3f(1, 1, 1);

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}

void SCR_ConnectMsg( void )
{
	if (*scr_connectmsg.string == '0')
		return;

	int wid[3];
	wid[0] = Draw_StringLen(scr_connectmsg.string, VGUI2_GetConsoleFont());
	wid[1] = Draw_StringLen(scr_connectmsg1.string, VGUI2_GetConsoleFont());
	int iWidth = Draw_StringLen(scr_connectmsg2.string, VGUI2_GetConsoleFont());

	if (scr_vrect.width > 270)
	{
		auto iWidest = -1;

		if (wid[0] >= 0)
			iWidest = wid[0];

		if (iWidest < wid[1])
			iWidest = wid[1];

		if (iWidth < iWidest)
			iWidth = iWidest;

		if (iWidth == -1)
			return;

		if (iWidth >= scr_vrect.width - 1)
			iWidth = scr_vrect.width - 2;
	}
	else
	{
		iWidth = scr_vrect.width - 2;
	}

	const int iLineHeight = (scr_vrect.height <= 14) ? (scr_vrect.height - 2) : 14;
	const int iYStart = iLineHeight / -2;

	int v10 = scr_vrect.x + (scr_vrect.width - iWidth) / 2;

	int iXPos = v10 + 1;
	int iYPos = iYStart + (-5 * iLineHeight) + scr_vrect.height;

	vrect_t rcFill;
	rcFill.x = v10 - 4;
	rcFill.y = iYPos + iYStart;
	rcFill.width = iWidth + 10;
	rcFill.height = 4 * iLineHeight;

	byte color[3] = { 0, 0, 0 };

	D_FillRect(&rcFill, color);

	// TODO: shouldn't these be in the range 0..1?
	Draw_SetTextColor(192.0, 192.0, 192.0);
	Draw_String(iXPos, iYPos, scr_connectmsg.string);

	if (*scr_connectmsg1.string != '0')
	{
		iYPos += iLineHeight;
		Draw_SetTextColor(192.0, 192.0, 192.0);
		Draw_String(iXPos, iYPos, scr_connectmsg1.string);
	}

	if (*scr_connectmsg2.string != '0')
	{
		iYPos += iLineHeight;
		Draw_SetTextColor(192.0, 192.0, 192.0);
		Draw_String(iXPos, iYPos, scr_connectmsg2.string);
	}
}

void SCR_DrawDownloadText( void )
{
	if (cls.state != ca_active && Q_strlen(gDownloadFile) > 0)
	{
		char szStatusText[128];
		snprintf(szStatusText, Q_ARRAYSIZE(szStatusText), "Downloading %s", gDownloadFile);

		const auto iWidth = Draw_StringLen(szStatusText, VGUI2_GetConsoleFont());

		vrect_t rcFill;
		rcFill.x = (vid.width - iWidth) / 2;
		rcFill.y = max(22, (int)(vid.height) - 40);
		rcFill.width = iWidth + 2;
		rcFill.height = vid.height <= 10 ? vid.height - 2 : 14;

		byte color[3] = { 0, 0, 0 };
		D_FillRect(&rcFill, color);

		Draw_SetTextColor(0.9, 0.9, 0.9);
		Draw_String(rcFill.x + 1, rcFill.y, szStatusText);
	}
}

void Draw_CenterPic( qpic_t* pPic )
{
	Draw_Pic(glwidth / 2 - pPic->width / 2, glheight / 2 - pPic->height / 2, pPic);
}

void SCR_DrawLoading( void )
{
	if (scr_drawloading)
		Draw_BeginDisc();
}

void SCR_DrawPause( void )
{
	if (scr_showpause.value == 0.0)
		return;

	if (!cl.paused)
		return;

	if (VGuiWrap2_IsGameUIVisible())
		return;

	if (!g_bIsCZeroRitual || !pr_strings || !gGlobalVariables.mapname || Q_stricmp(&pr_strings[gGlobalVariables.mapname], "cz_worldmap"))
		Draw_CenterPic(scr_paused);
}

void SCR_SetUpToDrawConsole( void )
{
	Con_CheckResize();
}

void SCR_TileClear( void );

/*
=================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
=================
*/
void SCR_UpdateScreen( void )
{
	static bool recursionGuard = false;

	if (recursionGuard)
		return;

	recursionGuard = true;

	// Always force the Gamma Table to be rebuilt. Otherwise,
	// we'll load textures with an all white gamma lookup table.
	V_CheckGamma();

	if (gfBackground || scr_skipupdate)
	{
		recursionGuard = false;
		return;
	}

	if (scr_skiponeupdate)
	{
		scr_skiponeupdate = false;
		recursionGuard = false;
		return;
	}

	scr_copytop = false;
	scr_copyeverything = false;

	// No screen refresh on dedicated servers & make sure it is initialized
	if (cls.state != ca_dedicated && scr_initialized && con_initialized)
	{
		double fCurrentTime = Sys_FloatTime();

		if ((cls.state == ca_connecting || cls.state == ca_connected || cls.state == ca_active) && g_LastScreenUpdateTime > 0 && (fCurrentTime - g_LastScreenUpdateTime) > 120)
		{
			Con_Printf(const_cast<char*>("load failed.\n"));

			COM_ExplainDisconnection(true, const_cast<char*>("Connection to server lost during level change."));
			CL_Disconnect();
		}

		if (g_modfuncs.m_pfnFrameRender1)
			g_modfuncs.m_pfnFrameRender1();

		V_UpdatePalette();

		GL_BeginRendering(&glx, &gly, &glwidth, &glheight);

		//
		// determine size of refresh window
		//
		if (vid.recalc_refdef)
		{
			SCR_CalcRefdef();
		}

		SCR_SetUpToDrawConsole();

		GLBeginHud();

		SCR_TileClear();

		// Most of these cases are now obsolete,
		// but keep them to maintain the exclusion logic for the working cases
		if (scr_drawloading)
		{

		}
		else if (cl.intermission == 1 && key_dest == key_game)
		{

		}
		else if (cl.intermission == 2 && key_dest == key_game)
		{
			SCR_CheckDrawCenterString();
		}
		else if (cl.intermission == 3 && key_dest == key_game)
		{
			SCR_CheckDrawCenterString();
		}
		else
		{
			GL_Bind(r_notexture_mip->gl_texturenum);

			if (vid.height > scr_con_current)
				Sbar_Draw();

			if (developer.value != 0.0)
			{
				GL_Bind(r_notexture_mip->gl_texturenum);
				Con_DrawNotify();
			}
		}

		SCR_ConnectMsg();
		VGui_Paint();

		GLFinishHud();
		GLBeginHud();

		// Draw FPS and net graph
		SCR_DrawFPS();
		SCR_NetGraph();

		if (!VGuiWrap2_IsGameUIVisible())
		{
			SCR_DrawPause();
			SCR_CheckDrawCenterString();
		}

		GLFinishHud();

		if (g_modfuncs.m_pfnFrameRender2)
			g_modfuncs.m_pfnFrameRender2();

		GL_EndRendering();

		g_LastScreenUpdateTime = fCurrentTime;
	}

	recursionGuard = false;
}

/*
=================
SCR_BeginLoadingPlaque
=================
*/
void SCR_BeginLoadingPlaque( qboolean reconnect )
{
	// Stop all sounds
	S_StopAllSounds(true);

	if ((cls.state == ca_connected || cls.state == ca_uninitialized || cls.state == ca_active) && cls.signon == SIGNONS)
	{
		// redraw with no console and the loading plaque
		Con_ClearNotify();

		scr_drawloading = true;
		scr_fullupdate = false;
		scr_centertime_off = false;
		scr_con_current = false;

		StartLoadingProgressBar("Connecting", 12);
		SetLoadingProgressBarStatusText("#GameUI_EstablishingConnection");

		SCR_UpdateScreen();
		SCR_UpdateScreen();

		scr_fullupdate = false;
		VGuiWrap2_LoadingStarted("transition", "");
	}
}

/*
=================
SCR_EndLoadingPlaque
=================
*/
void SCR_EndLoadingPlaque( void )
{
	scr_fullupdate = false;
	scr_drawloading = false;
	Con_ClearNotify();
	VGuiWrap2_LoadingFinished("transition", "");
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
=================
*/
void SCR_CalcRefdef( void )
{
	scr_fullupdate = 0;             // force a background redraw
	vid.recalc_refdef = 0;

	scr_viewsize.value = 120.0;

	if (scr_fov_value < 10.0)
		scr_fov_value = 10.0;

	if (scr_fov_value > 150.0)
		scr_fov_value = 150.0;

	sb_lines = 0;           // no status bar at all

	r_refdef.vrect.width = glwidth;

	float size = 1.0f;

	if (glwidth < 96)
	{
		size = 96.0 / glwidth;
		r_refdef.vrect.width = 96;      // min for icons
	}

	r_refdef.vrect.height = glheight * size;

	if (glheight >= (glheight * size))
	{
		r_refdef.vrect.y = (glheight - (glheight * size)) / 2;
		scr_vrect.y = (glheight - (glheight * size)) / 2;
	}
	else
	{
		r_refdef.vrect.height = glheight;
		r_refdef.vrect.y = 0;
		scr_vrect.y = 0;
	}

	r_refdef.vrect.x = (glwidth - r_refdef.vrect.width) / 2;
	scr_vrect.x = (glwidth - r_refdef.vrect.width) / 2;

	scr_vrect.width = r_refdef.vrect.width;
	scr_vrect.height = r_refdef.vrect.height;
	scr_vrect.pnext = r_refdef.vrect.pnext;
}

void SCR_TileClear( void )
{
	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear(0, 0, r_refdef.vrect.x, 152);
		// right
		Draw_TileClear(r_refdef.vrect.x + r_refdef.vrect.width, 0, r_refdef.vrect.width - r_refdef.vrect.x + 320, 152);
	}

	if (r_refdef.vrect.height < 152)
	{
		Draw_TileClear(r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y);
		Draw_TileClear(r_refdef.vrect.x, r_refdef.vrect.height + r_refdef.vrect.y, r_refdef.vrect.width, 152 - (r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

#pragma pack(push, 1)

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

#pragma pack(pop)

void SCR_ScreenShot_f( void )
{ 
	int height = vid.height;
	int width = vid.width;

	if (bDoScaledFBO || VideoMode_IsWindowed())
	{
		width = window_rect.right - window_rect.left;
		height = window_rect.bottom - window_rect.top;
	}

	char tganame[80];
	Q_strcpy(tganame, "HalfLife__.tga");

	bool failed = true;
	int index = 0;

	for (int i = 0; i < 100; i++)
	{
		// Replace '__' with incrementing numbers.
		tganame[8] = i / 10 + '0';
		tganame[9] = i % 10 + '0';

		// File doens't exist, it's fine, break from the loop.
		if (!FS_FileExists(tganame))
		{
			failed = false;
			index = i;
			break;
		}
	}

	// All preserved names already exists
	if (failed)
	{
		Con_Printf(const_cast<char*>("SCR_ScreenShot_f: Couldn't create a TGA file\n"));
		return;
	}

	int imageSize = 3 * height * width + sizeof(TargaHeader);

	// Open file handle
	FileHandle_t file = FS_OpenPathID(tganame, "wb", "GAMECONFIG");

	if (!file)
		Sys_Error("Couldn't create file for snapshot.\n");

	TargaHeader targa_header;
	memset(&targa_header, 0, sizeof(targa_header));

	targa_header.image_type = 2;
	targa_header.width = width;
	targa_header.height = height;
	targa_header.pixel_size = 24;

	if (FS_Write(&targa_header, sizeof(targa_header), 1, file) != sizeof(targa_header))
		Sys_Error("Couldn't write tga header screenshot.\n");

	byte* imgBuffer = (byte*)Mem_Malloc(imageSize);

	// Malloc couldn't find a free memory block for allocation
	if (!imgBuffer)
		Sys_Error("Couldn't allocate bitmap header to snapshot.\n");

	// Read screen pixels
	qglPixelStorei(GL_PACK_ALIGNMENT, GL_ONE);
	qglReadPixels(GL_ZERO, GL_ZERO, width, height, GL_RGB, GL_UNSIGNED_BYTE, imgBuffer);

	// swap rgb to bgr
	for (int i = sizeof(targa_header); i < imageSize; i += sizeof(byte) * 3)
	{
		int temp = imgBuffer[i];
		imgBuffer[i] = imgBuffer[i + 2];
		imgBuffer[i + 2] = temp;
	}

	if (FS_Write(imgBuffer, imageSize, 1, file) != imageSize)
		Sys_Error("Couldn't write bitmap data screenshot.\n");

	// Cleanup
	Mem_Free(imgBuffer);
	FS_Close(file);

	Con_Printf(const_cast<char*>("Wrote %s\n"), tganame);
}

void SCR_SizeUp_f( void )
{
	HudSizeUp();
	vid.recalc_refdef = true;
}

void SCR_SizeDown_f( void )
{
	HudSizeDown();
	vid.recalc_refdef = true;
}