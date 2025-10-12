#include "quakedef.h"
#include "cl_main.h"
#include "client.h"
#include "wad.h"
#include "draw.h"
#include "screen.h"
//#include "gl_draw.h"
//#include "gl_screen.h"
#include "vid.h"
//#include "glHud.h"
#include "host.h"
#include "sbar.h"
#include "net_chan.h"
//#include "qgl.h"
#include "d_iface.h"
#include "sound.h"
#include "vgui_int.h"
#include "vid.h"
#include "view.h"
#include "wad.h"
#include "vgui2/text_draw.h"
#include "render.h"
#include "progs.h"
#include "color.h"

/*

SCR_CenterPrint
SCR_EraseCenterString
SCR_DrawCenterString
SCR_CheckDrawCenterString
SCR_ConnectMsg
SCR_CalcRefdef
SCR_SizeUp_f
SCR_SizeDown_f
SCR_Init
SCR_DrawPause
SCR_SetUpToDrawConsole
SCR_DrawConsole
SCR_ScreenShot_f
SCR_BeginLoadingPlaque
SCR_EndLoadingPlaque
SCR_UpdateScreen

*/

#define SCR_CENTERSTRING_MAX 40

viddef_t	vid;				// global video state

// only the refresh window will be updated unless these variables are flagged 
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current = 0;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov, oldlcd_x;


int sb_lines = 0;

int clearnotify = 0;
int scr_center_lines = 0;
int scr_erase_lines = 0;
int scr_erase_center = 0;
int scr_fullupdate = 0;

float scr_centertime_off = 0;
float scr_centertime_start = 0;

char scr_centerstring[1024] = {};

float scr_fov_value = 90;

bool scr_initialized = false;
bool scr_drawloading = false;

qpic_t* scr_paused = nullptr;

vrect_t* pconupdate;
vrect_t scr_vrect = {};

cvar_t scr_viewsize = { const_cast<char*>("viewsize"), const_cast<char*>("120"), FCVAR_ARCHIVE };
cvar_t scr_showpause = { const_cast<char*>("showpause"), const_cast<char*>("1") };

cvar_t scr_centertime = { const_cast<char*>("scr_centertime"), const_cast<char*>("2") };
cvar_t scr_printspeed = { const_cast<char*>("scr_printspeed"), const_cast<char*>("8") };
cvar_t scr_graphheight = { const_cast<char*>("graphheight"), const_cast<char*>("64.0"), FCVAR_ARCHIVE};

cvar_t scr_downloading = { const_cast<char*>("scr_downloading"), const_cast<char*>("-1.0") };

cvar_t scr_connectmsg = { const_cast<char*>("scr_connectmsg"), const_cast<char*>("0") };
cvar_t scr_connectmsg1 = { const_cast<char*>("scr_connectmsg1"), const_cast<char*>("0") };
cvar_t scr_connectmsg2 = { const_cast<char*>("scr_connectmsg2"), const_cast<char*>("0") };

void SCR_SizeUp_f();
void SCR_SizeDown_f();
void SCR_ScreenShot_f();
void SCR_NetGraph(void);
void SCR_DrawFPS(void);

void SCR_Init()
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

void SCR_EraseCenterString()
{
	if (scr_erase_center++ > vid.numpages)
	{
		scr_erase_lines = 0;
	}
	else
	{
		int y = 48;

		if (scr_center_lines <= 4)
			y = vid.height * 0.35;

		scr_copytop = 1;
		Draw_TileClear(0, y, vid.width, 8 * scr_erase_lines);
	}
}

void SCR_DrawConsole()
{
	Con_DrawNotify();
}

void SCR_DrawCenterString()
{
	int		l;
	int		j;
	int		x, y;
	int		remaining;

	// the finale prints the characters one at a time
	if (cl.intermission)
	{
//		if (VGuiWrap2_IsInCareerMatch())
//			return;

		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	}
	else
		remaining = 9999;

	//Draw_GetDefaultColor();

	scr_erase_center = 0;
	const char* start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height * 0.35;
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

		Q_strncpy(line, (char*)start, l);
		line[l] = '\0';

		x = (vid.width - Draw_StringLen(line, VGUI2_GetConsoleFont())) / 2;

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
	} while (1);
}

void SCR_CheckDrawCenterString()
{
	scr_copytop = true;

	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;

	if (key_dest != key_game)
		return;

	SCR_DrawCenterString();
}

static void SCR_CalcRefdef(void)
{
	vrect_t		vrect;
	float		size;

	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

	//========================================

	// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set(const_cast<char*>("viewsize"), const_cast<char*>("30"));
	if (scr_viewsize.value > 120)
		Cvar_Set(const_cast<char*>("viewsize"), const_cast<char*>("120"));

	// bound field of view
	if (scr_fov_value < 10)
		scr_fov_value = 10;
	if (scr_fov_value > 150)
		scr_fov_value = 150;

	//r_refdef.fov_x = scr_fov.value;
	//r_refdef.fov_y = CalcFov(r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	// intermission is always full screen	
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;		// no status bar at all
	else if (size >= 110)
		sb_lines = 24;		// no inventory
	else
		sb_lines = 24 + 16 + 8;

	// these calculations mirror those in R_Init() for r_refdef, but take no
	// account of water warping
	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	R_SetVrect(&vrect, &scr_vrect, sb_lines);

	// guard against going from one mode to another that's less than half the
	// vertical resolution
	if (scr_con_current > vid.height)
		scr_con_current = vid.height;

	// notify the refresh of the change
	R_ViewChanged(&vrect, sb_lines, vid.aspect);
}

void SCR_CenterPrint(const char* str)
{
	Q_strncpy(scr_centerstring, (char*)str, ARRAYSIZE(scr_centerstring) - 1);
	scr_centerstring[ARRAYSIZE(scr_centerstring) - 1] = '\0';
	scr_center_lines = 1;
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	for (auto pszStr = str; *pszStr; ++pszStr)
	{
		if (*pszStr == '\n')
			++scr_center_lines;
	}
}

void SCR_ConnectMsg()
{
	if (*scr_connectmsg.string == '0')
		return;

	int wid[3];
	wid[0] = Draw_StringLen(scr_connectmsg.string, VGUI2_GetConsoleFont());
	wid[1] = Draw_StringLen(scr_connectmsg1.string, VGUI2_GetConsoleFont());
	int iWidth = wid[2] = Draw_StringLen(scr_connectmsg2.string, VGUI2_GetConsoleFont());

	if (scr_vrect.width > 270)
	{
		auto iWidest = -1;

		/*for (int i = 0; i < 3; i++)
		{
			if (iWidest < wid[i])
				iWidest = wid[i];
		}*/

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

	int iXStart = scr_vrect.x + (scr_vrect.width - iWidth) / 2;

	int iXPos = iXStart + 1;
	int iYPos = iYStart + (-5 * iLineHeight) + scr_vrect.height;

	vrect_t rcFill;
	rcFill.x = iXStart - 4;
	rcFill.y = iYPos + iYStart;
	rcFill.width = iWidth + 10;
	rcFill.height = 4 * iLineHeight;

	color24 color = { 0, 0, 0 };

	D_FillRect(&rcFill, color);

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

void SCR_DrawDownloadText()
{
	if (cls.state != ca_active && Q_strlen(gDownloadFile) > 0)
	{
		char szStatusText[128];
		_snprintf(szStatusText, ARRAYSIZE(szStatusText), "Downloading %s", gDownloadFile);

		const auto iWidth = Draw_StringLen(szStatusText, VGUI2_GetConsoleFont());

		vrect_t rcFill;
		rcFill.x = (vid.width - iWidth) / 2;
		rcFill.y = max(22, static_cast<int>(vid.height) - 40);
		rcFill.width = iWidth + 2;
		rcFill.height = vid.height <= 10 ? vid.height - 2 : 14;

		color24 color = { 0, 0, 0 };
		D_FillRect(&rcFill, color);

		Draw_SetTextColor(0.9, 0.9, 0.9);
		Draw_String(rcFill.x + 1, rcFill.y, szStatusText);
	}
}

void SCR_DrawPause()
{
	if (scr_showpause.value != 0.0 && cl.paused)
	{
		if (!g_bIsCZeroRitual || !gGlobalVariables.mapname || !pr_strings || stricmp((const char*)(gGlobalVariables.mapname + pr_strings), "cz_worldmap"))
		{
			qpic_t* pausepic = Draw_PicFromWad(const_cast<char*>("paused"));
			if (!VGuiWrap2_IsGameUIVisible())
				Draw_TransPic((vid.width - pausepic->width) / 2, (vid.height - pausepic->height - 48) / 2, pausepic);
		}
	}
}

void SCR_SetUpToDrawConsole()
{
	Con_CheckResize();
}

void SCR_UpdateScreen()
{
	static bool recursionGuard = false;

	if (recursionGuard)
		return;

	recursionGuard = true;
	
	V_CheckGamma();

	if (!gfBackground && !scr_skipupdate)
	{
		if (scr_skiponeupdate)
		{
			scr_skiponeupdate = false;
		}
		else
		{
			scr_copytop = false;
			scr_copyeverything = false;

			if (cls.state != ca_dedicated && scr_initialized && con_initialized)
			{
				double fCurrentTime = Sys_FloatTime();

				if ((cls.state == ca_connecting ||
					cls.state == ca_connected ||
					cls.state == ca_active) &&
					g_LastScreenUpdateTime > 0 &&
					(fCurrentTime - g_LastScreenUpdateTime) > 120)
				{
					Con_Printf(const_cast<char*>("load failed.\n"));

					COM_ExplainDisconnection( true, const_cast<char*>("Connection to server lost during level change.") );
					extern void DbgPrint(FILE*, const char* format, ...);
					extern FILE* m_fMessages;
					DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
					CL_Disconnect();
					
				}

				//
				// check for vid changes
				//
				if (oldfov != scr_fov_value)
				{
					oldfov = scr_fov_value;
					vid.recalc_refdef = 1;
				}

				if (oldlcd_x != lcd_x.value)
				{
					oldlcd_x = lcd_x.value;
					vid.recalc_refdef = 1;
				}

				if (oldscreensize != scr_viewsize.value)
				{
					oldscreensize = scr_viewsize.value;
					vid.recalc_refdef = 1;
				}

				//
				// determine size of refresh window
				//
				if (vid.recalc_refdef)
				{
					SCR_CalcRefdef();
				}

				if (scr_fullupdate++ < vid.numpages)
				{
					// clear the entire screen
					scr_copyeverything = 1;
					Draw_TileClear(0, 0, vid.width, vid.height);
				}

				pconupdate = NULL;


				SCR_SetUpToDrawConsole();
				SCR_EraseCenterString();
				
				if (scr_drawloading)
				{
					//Nothing
				}
				else if (cl.intermission == 1 && key_dest == key_game)
				{
					//Nothing
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
					
					if (cls.state != ca_active)
					{
						SCR_DrawConsole();
					}
				}

				SCR_NetGraph();
				VGui_Paint();
				SCR_DrawFPS();


				if (!VGuiWrap2_IsGameUIVisible())
				{
					SCR_DrawPause();
					if (scr_con_current < (float)vid.height && cls.state == ca_active)
						Sbar_Draw();
					SCR_CheckDrawCenterString();
				}

				SCR_ConnectMsg();

				if (pconupdate)
				{
					D_UpdateRects(pconupdate);
				}

				VID_FlipScreen();

				g_LastScreenUpdateTime = fCurrentTime;
			}
		}
	}

	recursionGuard = false;
}

void SCR_BeginLoadingPlaque(bool reconnect)
{
	S_StopAllSounds(true);

	if ((cls.state == ca_connected ||
		cls.state == ca_uninitialized ||
		cls.state == ca_active) &&
		cls.signon == 2)
	{
		Con_ClearNotify();
		scr_drawloading = true;
		scr_fullupdate = false;
		scr_centertime_off = false;
		scr_con_current = false;

		StartLoadingProgressBar("Connecting", 12);
		SetLoadingProgressBarStatusText("#GameUI_EstablishingConnection");

		SCR_UpdateScreen();

		scr_fullupdate = false;
		VGuiWrap2_LoadingStarted("transition", "");
	}
}

void SCR_EndLoadingPlaque()
{
	scr_fullupdate = false;
	scr_drawloading = false;
	Con_ClearNotify();
	VGuiWrap2_LoadingFinished("transition", "");
}

void SCR_SizeUp_f()
{
	HudSizeUp();
	vid.recalc_refdef = true;
}

void SCR_SizeDown_f()
{
	HudSizeDown();
	vid.recalc_refdef = true;
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

void SCR_ScreenShot_f()
{
	int     i;
	char		tganame[80];
	TargaHeader targa_header{};
	//char		checkname[MAX_PATH];

	// 
	// find a file name to save it to 
	// 
	strcpy(tganame, "HalfLife__.tga");

	for (i = 0; i <= 99; i++)
	{
		tganame[8] = i / 10 + '0';
		tganame[9] = i % 10 + '0';
		//sprintf(checkname, "%s/%s", com_gamedir, pcxname);
		if (FS_FileExists(tganame) == FALSE)
			break;	// file doesn't exist
	}
	if (i == 100)
	{
		Con_Printf(const_cast<char*>("SCR_ScreenShot_f: Couldn't create a TGA file\n"));
		return;
	}

	FileHandle_t file = FS_OpenPathID(tganame, "wb", "GAMECONFIG");

	if (!file)
		Sys_Error("Couldn't create file for snapshot.\n");

	targa_header.id_length = 0;
	targa_header.colormap_type = 0;
	targa_header.colormap_index = 0;
	targa_header.colormap_length = 0;
	targa_header.colormap_size = 0;
	targa_header.x_origin = 0;
	targa_header.y_origin = 0;
	targa_header.attributes = 0;

	targa_header.image_type = 0x02;
	targa_header.width = vid.width;
	targa_header.height = vid.height;
	targa_header.pixel_size = 24;

	if (FS_Write(&targa_header, 18, 1, file) != 18)
		Sys_Error("Couldn't write tga header screenshot.\n");

	color24 t[2048];
	for (int i = vid.height - 1; i >= 0 ; i--)
	{
		if (r_pixbytes == 1)
		{
			unsigned short* start = (WORD*)&vid.buffer[i * vid.rowbytes];
			for (int j = 0; j < vid.width; j++)
			{
				// FIXME : 15-bit depth support
				if (is15bit)
				{
					t[j].b = RGB_BLUE555(*start) << 3;
					t[j].g = RGB_GREEN555(*start) << 3;
					t[j].r = RGB_RED555(*start) << 3;
				}
				else
				{
					t[j].b = RGB_BLUE565(*start) << 3;
					t[j].g = RGB_GREEN565(*start) << 2;
					t[j].r = RGB_RED565(*start) << 3;
				}
				start++;
			}
		}
		else
		{
			unsigned int* start = (unsigned int*)&vid.buffer[i * vid.rowbytes];
			for (int j = 0; j < vid.width; j++)
			{
				t[j].r = RGB_BLUE888(*start);
				t[j].g = RGB_GREEN888(*start);
				t[j].b = RGB_RED888(*start);
				start++;
			}
		}

		int compatible_size = vid.width * 3;
		if (FS_Write(t, compatible_size, 1, file) != compatible_size)
			Sys_Error("Couldn't write bitmap data snapshot.\n");
	}
	
	FS_Close(file);

	// 
	// save the pcx file 
	// 
	/*D_EnableBackBufferAccess();	// enable direct drawing of console to back
									//  rgba

	WritePCXfile(pcxname, vid.rgba, vid.width, vid.height, vid.rowbytes,
		host_basepal);

	D_DisableBackBufferAccess();	// for adapters that can't stay mapped in
									//  for linear writes all the time
									*/
	Con_Printf(const_cast<char*>("Wrote %s\n"), tganame);

}
