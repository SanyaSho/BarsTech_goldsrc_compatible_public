#include "quakedef.h"
#include "client.h"
#include "dll_state.h"
#include "host_cmd.h"
#include "server.h"
#include "host.h"
#include "sys.h"
#include "r_studio.h"
#include "pmove.h"
#include "cd.h"
#include "buildnum.h"
#include "r_local.h"
#include "cl_main.h"
#include "sys_getmodes.h"
#include "pr_cmds.h"
#include "vgui_int.h"
#if defined(GLQUAKE)
#include "gl_screen.h"
#include "gl_model.h"
#else
#include "screen.h"
#endif
#include "sv_remoteaccess.h"
#include "world.h"
#include "pr_edict.h"
#include "voice.h"

CareerStateType g_careerState = CAREER_NONE;
qboolean noclip_anglehack;
bool g_iQuitCommandIssued = false;

bool g_bMajorMapChange = false;

int current_skill;
int gHostSpawnCount;

typedef int(*SV_BLENDING_INTERFACE_FUNC)(int, struct sv_blending_interface_s **, struct server_studio_api_s *, float *, float *);

void Host_SavegameComment(char* pszBuffer, int iSizeBuffer);

SV_SAVEGAMECOMMENT_FUNC g_pSaveGameCommentFunc = &Host_SavegameComment;

cvar_t voice_recordtofile = { const_cast<char*>("voice_recordtofile"), const_cast<char*>("0") };
cvar_t voice_inputfromfile = { const_cast<char*>("voice_inputfromfile"), const_cast<char*>("0") };
cvar_t gHostMap = { const_cast<char*>("HostMap"), const_cast<char*>("C1A0") };

TITLECOMMENT gTitleComments[] =
{
	{ const_cast<char*>("T0A0"), const_cast<char*>("#T0A0TITLE") },
	{ const_cast<char*>("C0A0"), const_cast<char*>("#C0A0TITLE") },
	{ const_cast<char*>("C1A0"), const_cast<char*>("#C0A1TITLE") },
	{ const_cast<char*>("C1A1"), const_cast<char*>("#C1A1TITLE") },
	{ const_cast<char*>("C1A2"), const_cast<char*>("#C1A2TITLE") },
	{ const_cast<char*>("C1A3"), const_cast<char*>("#C1A3TITLE") },
	{ const_cast<char*>("C1A4"), const_cast<char*>("#C1A4TITLE") },
	{ const_cast<char*>("C2A1"), const_cast<char*>("#C2A1TITLE") },
	{ const_cast<char*>("C2A2"), const_cast<char*>("#C2A2TITLE") },
	{ const_cast<char*>("C2A3"), const_cast<char*>("#C2A3TITLE") },
	{ const_cast<char*>("C2A4D"), const_cast<char*>("#C2A4TITLE2") },
	{ const_cast<char*>("C2A4E"), const_cast<char*>("#C2A4TITLE2") },
	{ const_cast<char*>("C2A4F"), const_cast<char*>("#C2A4TITLE2") },
	{ const_cast<char*>("C2A4G"), const_cast<char*>("#C2A4TITLE2") },
	{ const_cast<char*>("C2A4"), const_cast<char*>("#C2A4TITLE1") },
	{ const_cast<char*>("C2A5"), const_cast<char*>("#C2A5TITLE") },
	{ const_cast<char*>("C3A1"), const_cast<char*>("#C3A1TITLE") },
	{ const_cast<char*>("C3A2"), const_cast<char*>("#C3A2TITLE") },
	{ const_cast<char*>("C4A1A"), const_cast<char*>("#C4A1ATITLE") },
	{ const_cast<char*>("C4A1B"), const_cast<char*>("#C4A1ATITLE") },
	{ const_cast<char*>("C4A1C"), const_cast<char*>("#C4A1ATITLE") },
	{ const_cast<char*>("C4A1D"), const_cast<char*>("#C4A1ATITLE") },
	{ const_cast<char*>("C4A1E"), const_cast<char*>("#C4A1ATITLE") },
	{ const_cast<char*>("C4A1"), const_cast<char*>("#C4A1TITLE") },
	{ const_cast<char*>("C4A2"), const_cast<char*>("#C4A2TITLE") },
	{ const_cast<char*>("C4A3"), const_cast<char*>("#C4A3TITLE") },
	{ const_cast<char*>("C5A1"), const_cast<char*>("#C5TITLE") },
	{ const_cast<char*>("OFBOOT"), const_cast<char*>("#OF_BOOT0TITLE") },
	{ const_cast<char*>("OF0A"), const_cast<char*>("#OF1A1TITLE") },
	{ const_cast<char*>("OF1A1"), const_cast<char*>("#OF1A3TITLE") },
	{ const_cast<char*>("OF1A2"), const_cast<char*>("#OF1A3TITLE") },
	{ const_cast<char*>("OF1A3"), const_cast<char*>("#OF1A3TITLE") },
	{ const_cast<char*>("OF1A4"), const_cast<char*>("#OF1A3TITLE") },
	{ const_cast<char*>("OF1A"), const_cast<char*>("#OF1A5TITLE") },
	{ const_cast<char*>("OF2A1"), const_cast<char*>("#OF2A1TITLE") },
	{ const_cast<char*>("OF2A2"), const_cast<char*>("#OF2A1TITLE") },
	{ const_cast<char*>("OF2A3"), const_cast<char*>("#OF2A1TITLE") },
	{ const_cast<char*>("OF2A"), const_cast<char*>("#OF2A4TITLE") },
	{ const_cast<char*>("OF3A1"), const_cast<char*>("#OF3A1TITLE") },
	{ const_cast<char*>("OF3A2"), const_cast<char*>("#OF3A1TITLE") },
	{ const_cast<char*>("OF3A"), const_cast<char*>("#OF3A3TITLE") },
	{ const_cast<char*>("OF4A1"), const_cast<char*>("#OF4A1TITLE") },
	{ const_cast<char*>("OF4A2"), const_cast<char*>("#OF4A1TITLE") },
	{ const_cast<char*>("OF4A3"), const_cast<char*>("#OF4A1TITLE") },
	{ const_cast<char*>("OF4A"), const_cast<char*>("#OF4A4TITLE") },
	{ const_cast<char*>("OF5A"), const_cast<char*>("#OF5A1TITLE") },
	{ const_cast<char*>("OF6A1"), const_cast<char*>("#OF6A1TITLE") },
	{ const_cast<char*>("OF6A2"), const_cast<char*>("#OF6A1TITLE") },
	{ const_cast<char*>("OF6A3"), const_cast<char*>("#OF6A1TITLE") },
	{ const_cast<char*>("OF6A4b"), const_cast<char*>("#OF6A4TITLE") },
	{ const_cast<char*>("OF6A4"), const_cast<char*>("#OF6A1TITLE") },
	{ const_cast<char*>("OF6A5"), const_cast<char*>("#OF6A4TITLE") },
	{ const_cast<char*>("OF6A"), const_cast<char*>("#OF6A4TITLE") },
	{ const_cast<char*>("OF7A"), const_cast<char*>("#OF7A0TITLE") },
	{ const_cast<char*>("ba_tram"), const_cast<char*>("#BA_TRAMTITLE") },
	{ const_cast<char*>("ba_security"), const_cast<char*>("#BA_SECURITYTITLE") },
	{ const_cast<char*>("ba_main"), const_cast<char*>("#BA_SECURITYTITLE") },
	{ const_cast<char*>("ba_elevator"), const_cast<char*>("#BA_SECURITYTITLE") },
	{ const_cast<char*>("ba_canal"), const_cast<char*>("#BA_CANALSTITLE") },
	{ const_cast<char*>("ba_yard"), const_cast<char*>("#BA_YARDTITLE") },
	{ const_cast<char*>("ba_xen"), const_cast<char*>("#BA_XENTITLE") },
	{ const_cast<char*>("ba_hazard"), const_cast<char*>("#BA_HAZARD") },
	{ const_cast<char*>("ba_power"), const_cast<char*>("#BA_POWERTITLE") },
	{ const_cast<char*>("ba_teleport1"), const_cast<char*>("#BA_YARDTITLE") },
	{ const_cast<char*>("ba_teleport"), const_cast<char*>("#BA_TELEPORTTITLE") },
	{ const_cast<char*>("ba_outro"), const_cast<char*>("#BA_OUTRO") },
	{ const_cast<char*>("contamination"), const_cast<char*>("#contamination") },
	{ const_cast<char*>("crossfire"), const_cast<char*>("#crossfire") },
	{ const_cast<char*>("datacore"), const_cast<char*>("#datacore") },
	{ const_cast<char*>("disposal"), const_cast<char*>("#disposal") },
	{ const_cast<char*>("doublecross"), const_cast<char*>("#doublecross") },
	{ const_cast<char*>("frenzy"), const_cast<char*>("#frenzy") },
	{ const_cast<char*>("gasworks"), const_cast<char*>("#gasworks") },
	{ const_cast<char*>("lambda_bunker"), const_cast<char*>("#lambda_bunker") },
	{ const_cast<char*>("pool_party"), const_cast<char*>("#pool_party") },
	{ const_cast<char*>("rapidcore"), const_cast<char*>("#rapidcore") },
	{ const_cast<char*>("rocket_frenzy"), const_cast<char*>("#rocket_frenzy") },
	{ const_cast<char*>("rustmill"), const_cast<char*>("#rustmill") },
	{ const_cast<char*>("snark_pit"), const_cast<char*>("#snark_pit") },
	{ const_cast<char*>("stalkyard"), const_cast<char*>("#stalkyard") },
	{ const_cast<char*>("subtransit"), const_cast<char*>("#subtransit") },
	{ const_cast<char*>("undertow"), const_cast<char*>("#undertow") },
	{ const_cast<char*>("xen_dm"), const_cast<char*>("#xen_dm") },
	{ const_cast<char*>("bounce"), const_cast<char*>("#bounce") }
};

TYPEDESCRIPTION gGameHeaderDescription[] =
{
	DEFINE_FIELD(GAME_HEADER, mapCount, FIELD_INTEGER),
	DEFINE_ARRAY(GAME_HEADER, mapName, FIELD_CHARACTER, 32),
	DEFINE_ARRAY(GAME_HEADER, comment, FIELD_CHARACTER, 80),
};
TYPEDESCRIPTION gSaveHeaderDescription[] =
{
	DEFINE_FIELD(SAVE_HEADER, skillLevel, FIELD_INTEGER),
	DEFINE_FIELD(SAVE_HEADER, entityCount, FIELD_INTEGER),
	DEFINE_FIELD(SAVE_HEADER, connectionCount, FIELD_INTEGER),
	DEFINE_FIELD(SAVE_HEADER, lightStyleCount, FIELD_INTEGER),
	DEFINE_FIELD(SAVE_HEADER, time, FIELD_TIME),
	DEFINE_ARRAY(SAVE_HEADER, mapName, FIELD_CHARACTER, 32),
	DEFINE_ARRAY(SAVE_HEADER, skyName, FIELD_CHARACTER, 32),
	DEFINE_FIELD(SAVE_HEADER, skyColor_r, FIELD_INTEGER),
	DEFINE_FIELD(SAVE_HEADER, skyColor_g, FIELD_INTEGER),
	DEFINE_FIELD(SAVE_HEADER, skyColor_b, FIELD_INTEGER),
	DEFINE_FIELD(SAVE_HEADER, skyVec_x, FIELD_FLOAT),
	DEFINE_FIELD(SAVE_HEADER, skyVec_y, FIELD_FLOAT),
	DEFINE_FIELD(SAVE_HEADER, skyVec_z, FIELD_FLOAT),
};
TYPEDESCRIPTION gAdjacencyDescription[] =
{
	DEFINE_ARRAY(LEVELLIST, mapName, FIELD_CHARACTER, 32),
	DEFINE_ARRAY(LEVELLIST, landmarkName, FIELD_CHARACTER, 32),
	DEFINE_FIELD(LEVELLIST, pentLandmark, FIELD_EDICT),
	DEFINE_FIELD(LEVELLIST, vecLandmarkOrigin, FIELD_VECTOR),
};
TYPEDESCRIPTION gEntityTableDescription[] =
{
	DEFINE_FIELD(ENTITYTABLE, id, FIELD_INTEGER),
	DEFINE_FIELD(ENTITYTABLE, location, FIELD_INTEGER),
	DEFINE_FIELD(ENTITYTABLE, size, FIELD_INTEGER),
	DEFINE_FIELD(ENTITYTABLE, flags, FIELD_INTEGER),
	DEFINE_FIELD(ENTITYTABLE, classname, FIELD_STRING),
};
TYPEDESCRIPTION gLightstyleDescription[] =
{
	DEFINE_FIELD(SAVELIGHTSTYLE, index, FIELD_INTEGER),
	DEFINE_ARRAY(SAVELIGHTSTYLE, style, FIELD_CHARACTER, MAX_LIGHTSTYLES),
};

void SV_GetPlayerHulls(void)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		if (!gEntityInterface.pfnGetHullBounds(i, player_mins[i], player_maxs[i]))
			break;
	}
}

void SV_CheckBlendingInterface(void)
{
	int i;
	SV_BLENDING_INTERFACE_FUNC studio_interface;

	R_ResetSvBlending();
	for (i = 0; i < g_iextdllMac; i++)
	{
#ifdef _WIN32
		studio_interface = (SV_BLENDING_INTERFACE_FUNC)GetProcAddress((HMODULE)g_rgextdll[i].pDLLHandle, "Server_GetBlendingInterface");
#else
		studio_interface = (SV_BLENDING_INTERFACE_FUNC)dlsym(g_rgextdll[i].lDLLHandle, "Server_GetBlendingInterface");
#endif
		if (studio_interface)
		{
			if (studio_interface(SV_BLENDING_INTERFACE_VERSION, &g_pSvBlendingAPI, &server_studio_api, (float*)rotationmatrix, (float*)bonetransform))
				return;

			Con_DPrintf(const_cast<char*>("Couldn't get server .dll studio model blending interface. Version mismatch?\n"));
			R_ResetSvBlending();
		}
	}
}

void SV_CheckSaveGameCommentInterface(void)
{
	int i;
	SV_SAVEGAMECOMMENT_FUNC pTemp = NULL;
	for (i = 0; i < g_iextdllMac; i++)
	{
#ifdef _WIN32
		pTemp = (SV_SAVEGAMECOMMENT_FUNC)GetProcAddress((HMODULE)g_rgextdll[i].pDLLHandle, "SV_SaveGameComment");
#else
		pTemp = (SV_SAVEGAMECOMMENT_FUNC)dlsym(g_rgextdll[i].lDLLHandle, "SV_SaveGameComment");
#endif
		if (pTemp)
		{
			g_pSaveGameCommentFunc = pTemp;
			break;
		}
	}
}

void Host_InitializeGameDLL()
{
	Cbuf_Execute();
	NET_Config( svs.maxclients > 1 );

	if( svs.dll_initialized )
		return Con_DPrintf( const_cast<char*>("Sys_InitializeGameDLL called twice, skipping second call\n") );
		

	svs.dll_initialized = true;
	LoadEntityDLLs( host_parms.basedir );
	
	gEntityInterface.pfnGameInit();
	gEntityInterface.pfnPM_Init( &g_svmove );
	gEntityInterface.pfnRegisterEncoders();

	SV_InitEncoders();

	SV_GetPlayerHulls();
	SV_CheckBlendingInterface();
	SV_CheckSaveGameCommentInterface();
	Cbuf_Execute();
}

void Host_ClearSaveDirectory()
{
	char szName[MAX_PATH];
	const char* pfn;

	_snprintf(szName, sizeof(szName), "%s", Host_SaveGameDirectory());
	strncat(szName, "*.HL?", sizeof(szName) - Q_strlen(szName) - 1);
	COM_FixSlashes(szName);

	if (Sys_FindFirstPathID(szName, const_cast<char*>("GAMECONFIG")) != NULL)
	{
		Sys_FindClose();
		_snprintf(szName, sizeof(szName), "%s", Host_SaveGameDirectory());
		COM_FixSlashes(szName);
		FS_CreateDirHierarchy(szName, "GAMECONFIG");
		strncat(szName, "*.HL?", sizeof(szName) - Q_strlen(szName) - 1);

		for (pfn = Sys_FindFirstPathID(szName, const_cast<char*>("GAMECONFIG")); pfn; pfn = Sys_FindNext(NULL))
		{
			_snprintf(szName, sizeof(szName), "%s%s", Host_SaveGameDirectory(), pfn);
			FS_RemoveFile(szName, const_cast<char*>("GAMECONFIG"));
		}
	}
	Sys_FindClose();
}

void Host_Quit_f()
{
	if( Cmd_Argc() == 1 )
	{
		giActive = DLL_CLOSE;
		g_iQuitCommandIssued = true;

		extern void DbgPrint(FILE*, const char* format, ...);
		extern FILE* m_fMessages;
		DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
		if( cls.state != ca_dedicated )
			CL_Disconnect();
		Host_ShutdownServer( 0 );
		
		Sys_Quit();
	}
	else
	{
		giActive = DLL_PAUSED;
		giStateInfo = 4;
	}
}

void CareerAudio_Command_f()
{
	s_careerAudioPaused = 0;
}

void Host_KillServer_f(void)
{
	if (cls.state != ca_dedicated)
		CL_Disconnect_f();

	else if (sv.active)
	{
		Host_ShutdownServer(FALSE);

		if (cls.state != ca_dedicated)
			NET_Config(FALSE);
	}
}

void Host_Soundfade_f(void)
{
	int percent;
	int inTime;
	int holdTime;
	int outTime;

	if (Cmd_Argc() != 3 && Cmd_Argc() != 5)
	{
		Con_Printf(const_cast<char*>("soundfade <percent> <hold> [<out> <int>]\n"));
		return;
	}

	percent = Q_atoi(Cmd_Argv(1));

	if (percent > 100)
		percent = 100;
	if (percent < 0)
		percent = 0;

	holdTime = Q_atoi(Cmd_Argv(2));
	if (holdTime > 255)
		holdTime = 255;

	if (Cmd_Argc() == 5)
	{
		outTime = Q_atoi(Cmd_Argv(3));
		if (outTime > 255)
			outTime = 255;

		inTime = Q_atoi(Cmd_Argv(4));
		if (inTime > 255)
			inTime = 255;
	}
	else
	{
		outTime = 0;
		inTime = 0;
	}

	cls.soundfade.nStartPercent = percent;
	cls.soundfade.soundFadeStartTime = realtime;
	cls.soundfade.soundFadeOutTime = outTime;
	cls.soundfade.soundFadeHoldTime = holdTime;
	cls.soundfade.soundFadeInTime = inTime;
}

void Host_Status_Printf(qboolean conprint, qboolean log, char* fmt, ...)
{
	va_list argptr;
	char string[4096];
	char szfile[260];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	if (conprint)
		Con_Printf(const_cast<char*>("%s"), string);

	else SV_ClientPrintf("%s", string);

	if (log)
	{
		_snprintf(szfile, sizeof(szfile), "%s", "status.log");
		COM_Log(szfile, const_cast<char*>("%s"), string);
	}
}

void Host_Status_f(void)
{
	client_t* client;
	int seconds;
	int minutes;
	int hltv_slots = 0;
	int hltv_specs = 0;
	int hltv_delay = 0;
	const char* val;
	int hours;
	int j;
	int nClients;
	qboolean log = FALSE;
	qboolean conprint = FALSE;
	qboolean bIsSecure;
	qboolean bWantsToBeSecure;
	qboolean bConnectedToSteam3;
	const char* pchConnectionString = "";
	const char* pchSteamUniverse = "";

	if (cmd_source == src_command)
	{
		conprint = TRUE;

		if (!sv.active)
		{
			Cmd_ForwardToServer();
			return;
		}
	}

	char szfile[260];
	if (Cmd_Argc() == 2 && !Q_strcasecmp(Cmd_Argv(1), "log"))
	{
		log = TRUE;
		_snprintf(szfile, sizeof(szfile), "%s", "status.log");
		_unlink(szfile);
	}

	Host_Status_Printf(conprint, log, const_cast<char*>("hostname:  %s\n"), Cvar_VariableString(const_cast<char*>("hostname")));

	bIsSecure = Steam_GSBSecure();
	bWantsToBeSecure = Steam_GSBSecurePreference();
	bConnectedToSteam3 = Steam_GSBLoggedOn();

	if (!bIsSecure && bWantsToBeSecure)
	{
		pchConnectionString = "(secure mode enabled, connected to Steam3)";
		if (!bConnectedToSteam3)
		{
			pchConnectionString = "(secure mode enabled, disconnected from Steam3)";
		}
	}
	if (sv.active)
	{
		pchSteamUniverse = Steam_GetGSUniverse();
	}

	val = "insecure";
	if (bIsSecure)
		val = "secure";

	Host_Status_Printf(conprint, log, const_cast<char*>("version :  %i/%s %d %s %s%s (%d)\n"), PROTOCOL_VERSION, gpszVersionString, build_number(), val, pchConnectionString, pchSteamUniverse, GetGameAppID());
	if (!noip)
	{
		Host_Status_Printf(conprint, log, const_cast<char*>("tcp/ip  :  %s\n"), NET_AdrToString(net_local_adr));
	}
#ifdef _WIN32
	if (!noipx)
	{
		Host_Status_Printf(conprint, log, const_cast<char*>("ipx     :  %s\n"), NET_AdrToString(net_local_ipx_adr));
	}
#endif // _WIN32
	Host_Status_Printf(conprint, log, const_cast<char*>("map     :  %s at: %d x, %d y, %d z\n"), sv.name, r_origin[0], r_origin[1], r_origin[2]);
	SV_CountPlayers(&nClients);
	Host_Status_Printf(conprint, log, const_cast<char*>("players :  %i active (%i max)\n\n"), nClients, svs.maxclients);
	Host_Status_Printf(conprint, log, const_cast<char*>("#      name userid uniqueid frag time ping loss adr\n"));

	int count = 1;
	client = svs.clients;
	for (j = 0; j < svs.maxclients; j++, client++)
	{
		if (!client->active)
		{
			continue;
		}

		hours = 0;
		seconds = realtime - client->netchan.connect_time;
		minutes = seconds / 60;
		if (minutes)
		{
			seconds %= 60;
			hours = minutes / 60;
			if (hours)
				minutes %= 60;
		}

		if (!client->fakeclient)
			val = SV_GetClientIDString(client);
		else val = "BOT";

		Host_Status_Printf(conprint, log, const_cast<char*>("#%2i %8s %i %s"), count++, va(const_cast<char*>("\"%s\""), client->name), client->userid, val);
		if (client->proxy)
		{
			const char* userInfo = Info_ValueForKey(client->userinfo, "hspecs");
			if (Q_strlen(userInfo))
				hltv_specs = Q_atoi(userInfo);

			userInfo = Info_ValueForKey(client->userinfo, "hslots");
			if (Q_strlen(userInfo))
				hltv_slots = Q_atoi(userInfo);

			userInfo = Info_ValueForKey(client->userinfo, "hdelay");
			if (Q_strlen(userInfo))
				hltv_delay = Q_atoi(userInfo);

			Host_Status_Printf(conprint, log, const_cast<char*>(" hltv:%u/%u delay:%u"), hltv_specs, hltv_slots, hltv_delay);
		}
		else
			Host_Status_Printf(conprint, log, const_cast<char*>(" %3i"), (int)client->edict->v.frags);

		if (hours)
			Host_Status_Printf(conprint, log, const_cast<char*>(" %2i:%02i:%02i"), hours, minutes, seconds);
		else Host_Status_Printf(conprint, log, const_cast<char*>(" %02i:%02i"), minutes, seconds);

		Host_Status_Printf(conprint, log, const_cast<char*>(" %4i  %3i"), SV_CalcPing(client), (int)client->packet_loss);
		if ((conprint || client->proxy) && client->netchan.remote_address.type == NA_IP)
		{
			Host_Status_Printf(conprint, log, const_cast<char*>(" %s\n"), NET_AdrToString(client->netchan.remote_address));
		}
		else Host_Status_Printf(conprint, log, const_cast<char*>("\n"));
	}
	Host_Status_Printf(conprint, log, const_cast<char*>("%i users\n"), nClients);
}

void Host_Status_Formatted_f(void)
{
	client_t* client;
	int seconds;
	int minutes;
	int hours;
	int j;
	int nClients;
	qboolean log = FALSE;
	qboolean conprint = FALSE;
	qboolean bIsSecure;
	char sz[32];
	char szfile[MAX_PATH];
	char* szIDString;

	if (cmd_source == src_command)
	{
		conprint = TRUE;
		if (!sv.active)
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	if (Cmd_Argc() == 2 && !Q_strcasecmp(Cmd_Argv(1), "log"))
	{
		_snprintf(szfile, sizeof(szfile), "%s", "status.log");
		_unlink(szfile);
		log = TRUE;
	}

	bIsSecure = Steam_GSBSecure();
	Host_Status_Printf(conprint, log, const_cast<char*>("hostname:  %s\n"), Cvar_VariableString(const_cast<char*>("hostname")));

	char* szSecure = const_cast<char*>("insecure");
	if (bIsSecure)
		szSecure = const_cast<char*>("secure");

	Host_Status_Printf(conprint, log, const_cast<char*>("version :  %i/%s %d %s\n"), PROTOCOL_VERSION, gpszVersionString, build_number(), szSecure);

	if (!noip)
	{
		Host_Status_Printf(conprint, log, const_cast<char*>("tcp/ip  :  %s\n"), NET_AdrToString(net_local_adr));
	}
#ifdef _WIN32
	if (!noipx)
	{
		Host_Status_Printf(conprint, log, const_cast<char*>("ipx     :  %s\n"), NET_AdrToString(net_local_ipx_adr));
	}
#endif // _WIN32

	Host_Status_Printf(conprint, log, const_cast<char*>("map     :  %s at: %d x, %d y, %d z\n"), sv.name, r_origin[0], r_origin[1], r_origin[2]);
	SV_CountPlayers(&nClients);
	Host_Status_Printf(conprint, log, const_cast<char*>("players :  %i active (%i max)\n\n"), nClients, svs.maxclients);
	Host_Status_Printf(conprint, log, const_cast<char*>("%-2.2s\t%-9.9s\t%-7.7s\t%-20.20s\t%-4.4s\t%-8.8s\t%-4.4s\t%-4.4s\t%-21.21s\n"), "# ", "name", "userid   ", "uniqueid ", "frag", "time    ", "ping", "loss", "adr");

	int count = 1;
	char* szRemoteAddr;
	client = svs.clients;
	for (j = 0; j < svs.maxclients; j++, client++)
	{
		if (!client->active)
		{
			continue;
		}

		seconds = realtime - client->netchan.connect_time;
		minutes = seconds / 60;
		hours = minutes / 60;

		if (minutes && hours)
			_snprintf(sz, sizeof(sz), "%-2i:%02i:%02i", hours, minutes % 60, seconds % 60);
		else
			_snprintf(sz, sizeof(sz), "%02i:%02i", minutes, seconds % 60);

		if (conprint)
			szRemoteAddr = NET_AdrToString(client->netchan.remote_address);
		else szRemoteAddr = const_cast<char*>("");

		szIDString = SV_GetClientIDString(client);
		Host_Status_Printf(conprint, log, const_cast<char*>("%-2.2s\t%-9.9s\t%-7.7s\t%-20.20s\t%-4.4s\t%-8.8s\t%-4.4s\t%-4.4s\t%-21.21s\n"),
			va(const_cast<char*>("%-2i"), count++), va(const_cast<char*>("\"%s\""), client->name), va(const_cast<char*>("%-7i"), client->userid), szIDString,
			va(const_cast<char*>("%-4i"), (int)client->edict->v.frags), sz, va(const_cast<char*>("%-4i"), SV_CalcPing(client)), va(const_cast<char*>("%-4i"), (int)client->packet_loss), szRemoteAddr);
	}
	Host_Status_Printf(conprint, log, const_cast<char*>("%i users\n"), nClients);
}

void Host_Quit_Restart_f(void)
{
	giActive = DLL_RESTART;
	giStateInfo = 4;

	if (sv.active)
	{
		if (svs.maxclients == 1 && cls.state == ca_active && g_pPostRestartCmdLineArgs)
		{
			Cbuf_AddText(const_cast<char*>("save quick\n"));
			Cbuf_Execute();
			Q_strcat(g_pPostRestartCmdLineArgs, const_cast<char*>(" +load quick"));
		}
	}
	else if (cls.state == ca_active && cls.trueaddress[0] && g_pPostRestartCmdLineArgs)
	{
		Q_strcat(g_pPostRestartCmdLineArgs, const_cast<char*>(" +connect "));
		Q_strcat(g_pPostRestartCmdLineArgs, cls.servername);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Change to hardware renderer
//-----------------------------------------------------------------------------
void Host_SetRenderer_f(void)
{
	bool fullscreen = true;
	int mode = 0;

	if (cls.state == ca_dedicated)
		return;

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
	{
		return;
	}

	if (stricmp(Cmd_Argv(1), "software"))
	{
		mode = 1;
		if (stricmp(Cmd_Argv(1), "gl"))
		{
			mode = 2 - stricmp(Cmd_Argv(5), "d3d");
		}
	}

	if (Cmd_Argc() == 3 && !stricmp(Cmd_Argv(2), "windowed"))
	{
		fullscreen = false;
	}

	VideoMode_SwitchMode(mode, fullscreen);
}

void Host_SetVideoMode_f(void)
{
	int w, h, bpp = 16;

	if (cls.state == ca_dedicated)
		return;

	if (Cmd_Argc() != 3 &&
		Cmd_Argc() != 4)
	{
		return;
	}

	w = Q_atoi(Cmd_Argv(1));
	h = Q_atoi(Cmd_Argv(2));
	if (Cmd_Argc() == 4)
	{
		bpp = Q_atoi(Cmd_Argv(3));
	}

	VideoMode_SetVideoMode(w, h, bpp);
}

void Host_SetGameDir_f()
{
	char* psz;
	
	if (Cmd_Argc() < 1 || !g_pPostRestartCmdLineArgs)
		return;

	psz = (char*)Cmd_Argv(1);

	if (!psz[0] || psz[0] == '/' || strstr(psz, ":") || strstr(psz, "..") || strstr(psz, "\\"))
		return Con_Printf(const_cast<char*>("Couldn't set gamedir to %s (contains illegal characters)\n"), psz);

	strcpy(&g_pPostRestartCmdLineArgs[strlen(g_pPostRestartCmdLineArgs)], " -game ");
	strcat(g_pPostRestartCmdLineArgs, Cmd_Argv(1));
}

void Host_ClearGameState(void)
{
	S_StopAllSounds(TRUE);
	Host_ClearSaveDirectory();
	if (gEntityInterface.pfnResetGlobalState)
		gEntityInterface.pfnResetGlobalState();
}

SAVERESTOREDATA* LoadSaveData(const char* level)
{
	char name[MAX_PATH];
	char* pszTokenList;
	FILE* pFile;
	int i;
	int size;
	int tokenCount;
	int tokenSize;
	int tableCount;
	int chunkSize;
	SAVERESTOREDATA* pSaveData;

	_snprintf(name, sizeof(name), "%s%s.HL1", Host_SaveGameDirectory(), level);
	COM_FixSlashes(name);
	Con_Printf(const_cast<char*>("Loading game from %s...\n"), name);
	pFile = (FILE*)FS_OpenPathID(name, "rb", "GAMECONFIG");
	if (!pFile)
	{
		Con_Printf(const_cast<char*>("ERROR: couldn't open.\n"));
		return NULL;
	}
	FS_Read(&i, sizeof(int), 1, pFile);
	if (i != SAVEFILE_HEADER)
	{
		FS_Close(pFile);
		return NULL;
	}
	FS_Read(&i, sizeof(int), 1, pFile);
	if (i != SAVEGAME_VERSION)
	{
		FS_Close(pFile);
		return NULL;
	}

	FS_Read(&size, sizeof(int), 1, pFile);
	FS_Read(&tableCount, sizeof(int), 1, pFile);
	FS_Read(&tokenCount, sizeof(int), 1, pFile);
	FS_Read(&tokenSize, sizeof(int), 1, pFile);

	chunkSize = size + tokenSize + sizeof(ENTITYTABLE) * tableCount;

	if (size < 0 || tokenCount < 0 || tokenSize < 0 || 0x7FFFFFFF - size < tokenSize || 0x7FFFFFFF - (size + tokenSize) < sizeof(ENTITYTABLE) * tableCount || chunkSize < sizeof(SAVERESTOREDATA) || 
		(pSaveData = (SAVERESTOREDATA*)Mem_Calloc(chunkSize + sizeof(SAVERESTOREDATA), 1)) == NULL)
	{
		FS_Close(pFile);
		return NULL;
	}

//	pSaveData = (SAVERESTOREDATA*)Mem_Calloc(size + tokenSize + sizeof(ENTITYTABLE) * tableCount + sizeof(SAVERESTOREDATA), 1u);
	pSaveData->tableCount = tableCount;
	pSaveData->tokenCount = tokenCount;
	pSaveData->tokenSize = tokenSize;
	Q_strncpy(pSaveData->szCurrentMapName, level, sizeof(pSaveData->szCurrentMapName) - 1);
	pSaveData->szCurrentMapName[sizeof(pSaveData->szCurrentMapName) - 1] = 0;

	pszTokenList = (char*)(pSaveData + 1);
	if (tokenSize > 0)
	{
		FS_Read(pszTokenList, tokenSize, 1, pFile);
		if (!pSaveData->pTokens)
		{
			pSaveData->pTokens = (char**)Mem_Calloc(tokenCount, sizeof(char*));
		}
		for (i = 0; i < tokenCount; i++)
		{
			if (*pszTokenList)
				pSaveData->pTokens[i] = pszTokenList;
			else
				pSaveData->pTokens[i] = NULL;
			while (*pszTokenList++);
		}
	}

	pSaveData->pTable = (ENTITYTABLE*)pszTokenList;
	pSaveData->connectionCount = 0;
	pSaveData->size = 0;

	pSaveData->pBaseData = pszTokenList;
	pSaveData->pCurrentData = pszTokenList;

	pSaveData->fUseLandmark = 1;
	pSaveData->bufferSize = size;
	pSaveData->time = 0.0f;
	pSaveData->vecLandmarkOffset[0] = 0.0f;
	pSaveData->vecLandmarkOffset[1] = 0.0f;
	pSaveData->vecLandmarkOffset[2] = 0.0f;
	gGlobalVariables.pSaveData = pSaveData;

	FS_Read(pSaveData->pBaseData, size, 1, pFile);
	FS_Close(pFile);

	return pSaveData;
}

void ParseSaveTables(SAVERESTOREDATA* pSaveData, SAVE_HEADER* pHeader, int updateGlobals)
{
	int i;
	SAVELIGHTSTYLE light;
	for (i = 0; i < pSaveData->tableCount; i++)
	{
		gEntityInterface.pfnSaveReadFields(pSaveData, "ETABLE", &(pSaveData->pTable[i]), gEntityTableDescription, ARRAYSIZE(gEntityTableDescription));
		pSaveData->pTable[i].pent = NULL;
	}

	pSaveData->size = 0;
	pSaveData->pBaseData = pSaveData->pCurrentData;
	gEntityInterface.pfnSaveReadFields(pSaveData, "Save Header", pHeader, gSaveHeaderDescription, ARRAYSIZE(gSaveHeaderDescription));
	pSaveData->connectionCount = pHeader->connectionCount;
	pSaveData->fUseLandmark = 1;
	pSaveData->time = pHeader->time;
	pSaveData->vecLandmarkOffset[0] = 0;
	pSaveData->vecLandmarkOffset[1] = 0;
	pSaveData->vecLandmarkOffset[2] = 0;

	for (i = 0; i < pSaveData->connectionCount; i++)
	{
		gEntityInterface.pfnSaveReadFields(pSaveData, "ADJACENCY", &(pSaveData->levelList[i]), gAdjacencyDescription, ARRAYSIZE(gAdjacencyDescription));
	}
	for (i = 0; i < pHeader->lightStyleCount; i++)
	{
		gEntityInterface.pfnSaveReadFields(pSaveData, "LIGHTSTYLE", &light, gLightstyleDescription, ARRAYSIZE(gLightstyleDescription));
		if (updateGlobals)
		{
			char* val = (char*)Hunk_Alloc(Q_strlen(light.style) + 1);
			Q_strncpy(val, light.style, ARRAYSIZE(val) - 1);
			val[ARRAYSIZE(val) - 1] = '\0';
			sv.lightstyles[light.index] = val;
		}
	}
}

void EntityPatchWrite(SAVERESTOREDATA* pSaveData, const char* level)
{
	char name[MAX_PATH];
	FILE* pFile;
	int i;
	int size = 0;

	_snprintf(name, sizeof(name), "%s%s.HL3", Host_SaveGameDirectory(), level);
	COM_FixSlashes(name);
	pFile = (FILE*)FS_OpenPathID(name, "wb", "GAMECONFIG");
	if (!pFile)
		return;

	for (i = 0; i < pSaveData->tableCount; i++)
	{
		if (pSaveData->pTable[i].flags & FENTTABLE_REMOVED)
			size++;
	}
	FS_Write(&size, sizeof(int), 1, pFile);
	for (i = 0; i < pSaveData->tableCount; i++)
	{
		if (pSaveData->pTable[i].flags & FENTTABLE_REMOVED)
			FS_Write(&i, sizeof(int), 1, pFile);
	}
	FS_Close(pFile);
}

void EntityPatchRead(SAVERESTOREDATA* pSaveData, const char* level)
{
	char name[MAX_PATH];
	FILE* pFile;
	int i;
	int size;
	int entityId;

	_snprintf(name, sizeof(name), "%s%s.HL3", Host_SaveGameDirectory(), level);
	COM_FixSlashes(name);
	pFile = (FILE*)FS_OpenPathID(name, "rb", "GAMECONFIG");
	if (!pFile)
		return;

	FS_Read(&size, 4, 1, pFile);
	for (i = 0; i < size; i++)
	{
		FS_Read(&entityId, 4, 1, pFile);
		pSaveData->pTable[entityId].flags = FENTTABLE_REMOVED;
	}
}

void EntityInit(edict_t* pEdict, int className)
{
	ENTITYINIT pEntityInit;
	if (!className)
		Sys_Error("Bad class!!\n");

	ReleaseEntityDLLFields(pEdict);
	InitEntityDLLFields(pEdict);
	pEdict->v.classname = className;
	pEntityInit = GetEntityInit(&pr_strings[className]);
	if (pEntityInit)
		pEntityInit(&pEdict->v);
}

void SaveExit(SAVERESTOREDATA* save)
{
	if (save->pTokens)
	{
		Mem_Free(save->pTokens);
		save->pTokens = NULL;
		save->tokenCount = 0;
	}
	Mem_Free(save);
	gGlobalVariables.pSaveData = NULL;
}

int LoadGamestate(char* level, int createPlayers)
{
	int i;
	edict_t* pent;
	SAVE_HEADER header;
	SAVERESTOREDATA* pSaveData;
	ENTITYTABLE* pEntInfo;

	pSaveData = LoadSaveData(level);
	if (!pSaveData)
		return 0;

	ParseSaveTables(pSaveData, &header, 1);
	EntityPatchRead(pSaveData, level);

	Q_strncpy(sv.name, header.mapName, sizeof(sv.name) - 1);
	sv.name[sizeof(sv.name) - 1] = 0;

	Cvar_Set(const_cast<char*>("sv_skyname"), header.skyName);
	Cvar_SetValue(const_cast<char*>("skill"), header.skillLevel);
	Cvar_SetValue(const_cast<char*>("sv_skycolor_r"), header.skyColor_r);
	Cvar_SetValue(const_cast<char*>("sv_skycolor_g"), header.skyColor_g);
	Cvar_SetValue(const_cast<char*>("sv_skycolor_b"), header.skyColor_b);
	Cvar_SetValue(const_cast<char*>("sv_skyvec_x"), header.skyVec_x);
	Cvar_SetValue(const_cast<char*>("sv_skyvec_y"), header.skyVec_y);
	Cvar_SetValue(const_cast<char*>("sv_skyvec_z"), header.skyVec_z);

	for (i = 0; i < pSaveData->tableCount; i++)
	{
		pent = NULL;
		pEntInfo = &pSaveData->pTable[i];
		if (pEntInfo->classname && pEntInfo->size && !(pEntInfo->flags & FENTTABLE_REMOVED))
		{
			if (pEntInfo->id)
			{
				if (pEntInfo->id > svs.maxclients)
					pEntInfo->pent = CreateNamedEntity(pEntInfo->classname);
				else
				{
					if (!(pEntInfo->flags & FENTTABLE_PLAYER))
						Sys_Error("ENTITY IS NOT A PLAYER: %d\n", i);

					pent = svs.clients[pEntInfo->id - 1].edict;
					if (createPlayers && pent)
						EntityInit(pent, pEntInfo->classname);
					else
						pent = NULL;
				}
			}
			else
			{
				pent = sv.edicts;
				EntityInit(pent, pEntInfo->classname);
			}
		}
		pEntInfo->pent = pent;
	}
	for (i = 0; i < pSaveData->tableCount; i++)
	{
		pEntInfo = &pSaveData->pTable[i];
		pent = pEntInfo->pent;

		pSaveData->size = pEntInfo->location;
		pSaveData->pCurrentData = &pSaveData->pBaseData[pEntInfo->location];

		if (pent)
		{
			if (gEntityInterface.pfnRestore(pent, pSaveData, 0) < 0)
			{
				ED_Free(pent);
				pEntInfo->pent = NULL;
			}
			else
				SV_LinkEdict(pent, FALSE);
		}
	}
	SaveExit(pSaveData);
	sv.time = header.time;
	return 1;
}

void Host_Map(qboolean bIsDemo, char* mapstring, char* mapName, qboolean loadGame)
{
	int i;
	UserMsg* pMsg;
	Host_ShutdownServer(FALSE);
	key_dest = key_game;
	SCR_BeginLoadingPlaque(FALSE);
	if (!loadGame)
	{
		Host_ClearGameState();
		SV_InactivateClients();
		svs.serverflags = 0;
	}
	Q_strncpy(cls.mapstring, mapstring, sizeof(cls.mapstring) - 1);
	cls.mapstring[sizeof(cls.mapstring) - 1] = 0;
	if (SV_SpawnServer(bIsDemo, mapName, NULL))
	{
		ContinueLoadingProgressBar("Server", 7, 0.0);
		if (loadGame)
		{
			if (!LoadGamestate(mapName, 1))
				SV_LoadEntities();
			sv.paused = TRUE;
			sv.loadgame = TRUE;
			SV_ActivateServer(0);
		}
		else
		{
			SV_LoadEntities();
			SV_ActivateServer(1);
			if (!sv.active)
				return;

			if (cls.state != ca_dedicated)
			{
				Q_strcpy(cls.spawnparms, "");
				for (i = 0; i < Cmd_Argc(); i++)
					strncat(cls.spawnparms, Cmd_Argv(i), sizeof(cls.spawnparms) - Q_strlen(cls.spawnparms) - 1);
			}
		}
		if (sv_gpNewUserMsgs)
		{
			pMsg = sv_gpUserMsgs;
			if (pMsg)
			{
				while (pMsg->next)
					pMsg = pMsg->next;
				pMsg->next = sv_gpNewUserMsgs;
			}
			else
				sv_gpUserMsgs = sv_gpNewUserMsgs;

			sv_gpNewUserMsgs = NULL;
		}
		if (cls.state)
			Cmd_ExecuteString(const_cast<char*>("connect local"), src_command);
	}
}

void Host_Map_f(void)
{
	int i;
	char mapstring[64];
	char name[64];
	CareerStateType careerState = g_careerState;
	if (cmd_source != src_command)
	{
		g_careerState = CAREER_NONE;
		return;
	}
	if (Cmd_Argc() > 1 && Q_strlen(Cmd_Args()) > 54)
	{
		g_careerState = CAREER_NONE;
		Con_Printf(const_cast<char*>("map change failed: command string is too long.\n"));
		return;
	}
	if (Cmd_Argc() < 2)
	{
		g_careerState = CAREER_NONE;
		Con_Printf(const_cast<char*>("map <levelname> : changes server to specified map\n"));
		return;
	}

	extern void DbgPrint(FILE*, const char* format, ...);
	extern FILE* m_fMessages;
	DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
	CL_Disconnect();

	// состояние career сохраняется перед разрывом соединения и затем восстанавливается
	if (careerState == CAREER_LOADING)
		g_careerState = CAREER_LOADING;

	if (COM_CheckParm(const_cast<char*>("-steam")) && PF_IsDedicatedServer())
		g_bMajorMapChange = TRUE;

	FS_LogLevelLoadStarted("Map_Common");
	mapstring[0] = 0;
	for (i = 0; i < Cmd_Argc(); i++)
	{
		strncat(mapstring, Cmd_Argv(i), 62 - Q_strlen(mapstring));
		strncat(mapstring, " ", 62 - Q_strlen(mapstring));
	}
	Q_strcat(mapstring, const_cast<char*>("\n"));
	Q_strncpy(name, Cmd_Argv(1), sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;

	if (!svs.dll_initialized)
		Host_InitializeGameDLL();

	int iLen = Q_strlen(name);
	if (iLen > 4 && !Q_strcasecmp(&name[iLen - 4], ".bsp"))
		name[iLen - 4] = 0;

	FS_LogLevelLoadStarted(name);
	VGuiWrap2_LoadingStarted("level", name);
	SCR_UpdateScreen();
	SCR_UpdateScreen();

	if (!PF_IsMapValid_I(name))
	{
		Con_Printf(const_cast<char*>("map change failed: '%s' not found on server.\n"), name);
		if (COM_CheckParm(const_cast<char*>("-steam")))
		{
			if (PF_IsDedicatedServer())
			{
				g_bMajorMapChange = FALSE;
				Sys_Printf("\n");
			}
		}
		return;
	}

	StartLoadingProgressBar("Server", 24);
	SetLoadingProgressBarStatusText("#GameUI_StartingServer");
	ContinueLoadingProgressBar("Server", 1, 0.0);
	Cvar_Set(const_cast<char*>("HostMap"), name);
	Host_Map(FALSE, mapstring, name, FALSE);
	if (COM_CheckParm(const_cast<char*>("-steam")) && PF_IsDedicatedServer())
	{
		g_bMajorMapChange = FALSE;
		Sys_Printf("\n");
	}
	ContinueLoadingProgressBar("Server", 11, 0.0);
	NotifyDedicatedServerUI("UpdateMap");

	if (g_careerState == CAREER_LOADING)
	{
		g_careerState = CAREER_PLAYING;
		SetCareerAudioState(true);
	}
	else
		SetCareerAudioState(false);
}

void Host_Career_f(void)
{
	if (cmd_source == src_command)
	{
		g_careerState = CAREER_LOADING;
		Host_Map_f();
	}
}

void Host_Maps_f(void)
{
	char* pszSubString;

	if (Cmd_Argc() != 2)
	{
		Con_Printf(const_cast<char*>("Usage:  maps <substring>\nmaps * for full listing\n"));
		return;
	}

	pszSubString = Cmd_Argv(1);

	if (pszSubString != NULL && pszSubString[0] != 0)
	{
		if (pszSubString[0] == '*')
			pszSubString = NULL;

		COM_ListMaps(pszSubString);
	}
}

void Host_Restart_f(void)
{
	char name[MAX_PATH];
	if (cls.demoplayback || !sv.active || cmd_source != src_command)
		return;
	if (cls.state)
		cls.state = ca_disconnected;

	Host_ClearGameState();
	SV_InactivateClients();
	Q_strncpy(name, sv.name, sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;

	SV_ServerShutdown();
	SV_SpawnServer(FALSE, name, NULL);
	SV_LoadEntities();
	SV_ActivateServer(1);
}

const char* Host_FindRecentSave(char* pNameBuf)
{
	const char* findfn;
	char basefilename[MAX_PATH];
	int found;
	int32 newest;
	int32 ft;
	char szPath[MAX_PATH];

	sprintf(pNameBuf, "%s*.sav", Host_SaveGameDirectory());
	_snprintf(szPath, sizeof(szPath), "%s", Host_SaveGameDirectory());

	found = 0;
	findfn = Sys_FindFirst(pNameBuf, basefilename);
	while (findfn != NULL)
	{
		if (Q_strlen(findfn) && Q_strcasecmp(findfn, "HLSave.sav"))
		{
			_snprintf(szPath, sizeof(szPath), "%s%s", Host_SaveGameDirectory(), findfn);
			ft = FS_GetFileTime(szPath);
			if (ft > 0 && (!found || newest < ft))
			{
				found = 1;
				newest = ft;
				Q_strcpy(pNameBuf, findfn);
			}
		}
		findfn = Sys_FindNext(basefilename);
	}
	Sys_FindClose();
	return found != 0 ? pNameBuf : NULL;
}

SAVERESTOREDATA* SaveInit(int size)
{
	int i;
	edict_t* pEdict;
	SAVERESTOREDATA* pSaveData;

	if (size <= 0)
		size = 0x80000;

	pSaveData = (SAVERESTOREDATA*)Mem_Calloc(sizeof(SAVERESTOREDATA) + (sizeof(ENTITYTABLE) * sv.num_edicts) + size, sizeof(char));
	pSaveData->pTable = (ENTITYTABLE*)(pSaveData + 1);
	pSaveData->tokenSize = 0;
	pSaveData->tokenCount = 0xFFF;
	pSaveData->pTokens = (char**)Mem_Calloc(0xFFF, sizeof(char*));

	for (i = 0; i < sv.num_edicts; i++)
	{
		pEdict = &sv.edicts[i];

		pSaveData->pTable[i].id = i;
		pSaveData->pTable[i].pent = pEdict;
		pSaveData->pTable[i].flags = 0;
		pSaveData->pTable[i].location = 0;
		pSaveData->pTable[i].size = 0;
		pSaveData->pTable[i].classname = 0;
	}

	pSaveData->tableCount = sv.num_edicts;
	pSaveData->connectionCount = 0;
	pSaveData->size = 0;
	pSaveData->bufferSize = size;
	pSaveData->fUseLandmark = 0;

	pSaveData->pBaseData = (char*)(pSaveData->pTable + sv.num_edicts);
	pSaveData->pCurrentData = (char*)(pSaveData->pTable + sv.num_edicts);

	gGlobalVariables.pSaveData = pSaveData;
	pSaveData->time = gGlobalVariables.time;

	VectorCopy(vec3_origin, pSaveData->vecLandmarkOffset);

	return pSaveData;
}

void CL_Save(const char* name)
{
	FileHandle_t file;
	DECALLIST decalList[MAX_DECALS];
	int decalCount;
	int temp;

	decalCount = DecalListCreate(decalList);
	file = FS_OpenPathID(name, "wb", "GAMECONFIG");

	if (!file)
		return;

	temp = SAVEFILE_HEADER;
	FS_Write(&temp, sizeof(temp), 1, file);
	temp = SAVEGAME_VERSION;
	FS_Write(&temp, sizeof(temp), 1, file);
	FS_Write(&decalCount, sizeof(decalCount), 1, file);

	for (int i = 0; i < decalCount; i++)
	{
		// указатель name в структуре ссылается на имя кэшированной текстуры, состоящей из 16 байт
		FS_Write(decalList[i].name, offsetof(texture_t, width) -  offsetof(texture_t, name), 1, file);
		FS_Write(&decalList[i].entityIndex, sizeof(decalList[i].entityIndex), 1, file);
		FS_Write(&decalList[i].depth, sizeof(decalList[i].depth), 1, file);
		FS_Write(&decalList[i].flags, sizeof(decalList[i].flags), 1, file);
		FS_Write(decalList[i].position, sizeof(decalList[i].position), 1, file);
	}

	FS_Close(file);
}

SAVERESTOREDATA* SaveGamestate(void)
{
	char name[256];
	char* pTableData;
	char* pTokenData;
	FILE* pFile;
	int i;
	int dataSize;
	int tableSize;
	edict_t* pent;
	SAVE_HEADER header;
	SAVERESTOREDATA* pSaveData;
	SAVELIGHTSTYLE light;

	if (!gEntityInterface.pfnParmsChangeLevel)
		return NULL;

	FS_CreateDirHierarchy(Host_SaveGameDirectory(), "GAMECONFIG");
	pSaveData = SaveInit(0);
	_snprintf(name, sizeof(name), "%s%s.HL1", Host_SaveGameDirectory(), sv.name);
	COM_FixSlashes(name);
	gEntityInterface.pfnParmsChangeLevel();
	header.version = build_number();
	header.skillLevel = (int)skill.value;
	header.entityCount = pSaveData->tableCount;
	header.time = sv.time;
	header.connectionCount = pSaveData->connectionCount;
	Q_strncpy(header.skyName, sv_skyname.string, sizeof(header.skyName) - 1);
	header.skyName[sizeof(header.skyName) - 1] = 0;
	pSaveData->time = 0.0f;

	header.skyColor_r = (int)sv_skycolor_r.value;
	header.skyColor_g = (int)sv_skycolor_g.value;
	header.skyColor_b = (int)sv_skycolor_b.value;
	header.skyVec_x = sv_skyvec_x.value;
	header.skyVec_y = sv_skyvec_y.value;
	header.skyVec_z = sv_skyvec_z.value;

	Q_strncpy(header.mapName, sv.name, sizeof(header.mapName) - 1);
	header.mapName[sizeof(header.mapName) - 1] = 0;

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
			header.lightStyleCount++;
	}
	gEntityInterface.pfnSaveWriteFields(pSaveData, "Save Header", &header, gSaveHeaderDescription, ARRAYSIZE(gSaveHeaderDescription));
	pSaveData->time = header.time;

	for (i = 0; i < pSaveData->connectionCount; i++)
		gEntityInterface.pfnSaveWriteFields(pSaveData, "ADJACENCY", &pSaveData->levelList[i], gAdjacencyDescription, ARRAYSIZE(gAdjacencyDescription));

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
		{
			light.index = i;
			Q_strncpy(light.style, sv.lightstyles[i], sizeof(light.style) - 1);
			light.style[sizeof(light.style) - 1] = 0;
			gEntityInterface.pfnSaveWriteFields(pSaveData, "LIGHTSTYLE", &light, gLightstyleDescription, ARRAYSIZE(gLightstyleDescription));
		}
	}

	dataSize = pSaveData->size;
	pTableData = pSaveData->pCurrentData;
	tableSize = 0;

	for (i = 0; i < sv.num_edicts; i++)
	{
		pent = &sv.edicts[i];
		pSaveData->currentIndex = i;
		pSaveData->pTable[i].location = pSaveData->size;
		if (!pent->free)
		{
			gEntityInterface.pfnSave(pent, pSaveData);
			if (SV_IsPlayerIndex(i))
				pSaveData->pTable[i].flags |= FENTTABLE_PLAYER;
		}
	}
	for (i = 0; i < sv.num_edicts; i++)
		gEntityInterface.pfnSaveWriteFields(pSaveData, "ETABLE", &pSaveData->pTable[i], gEntityTableDescription, ARRAYSIZE(gEntityTableDescription));

	pTokenData = NULL;
	for (i = 0; i < pSaveData->tokenCount; i++)
	{
		if (pSaveData->pTokens[i])
		{
			pTokenData = pSaveData->pCurrentData;
			do
			{
				*pTokenData++ = *pSaveData->pTokens[i]++;
			} while (*pSaveData->pTokens[i]);
			pSaveData->pCurrentData = pTokenData;
		}
		else
		{
			pTokenData = pSaveData->pCurrentData;
			*pTokenData = 0;
			pSaveData->pCurrentData = pTokenData + 1;
		}
	}
	pSaveData->tokenSize = (int)(pSaveData->pCurrentData - pTokenData);

	COM_CreatePath(name);
	pFile = (FILE*)FS_OpenPathID(name, "wb", "GAMECONFIG");
	if (!pFile)
	{
		Con_Printf(const_cast<char*>("Unable to open save game file %s."), name);
		return NULL;
	}

	i = SAVEFILE_HEADER;
	FS_Write(&i, sizeof(int), 1, pFile);
	i = SAVEGAME_VERSION;
	FS_Write(&i, sizeof(int), 1, pFile);
	FS_Write(&pSaveData->size, sizeof(int), 1, pFile);
	FS_Write(&pSaveData->tableCount, sizeof(int), 1, pFile);
	FS_Write(&pSaveData->tokenCount, sizeof(int), 1, pFile);
	FS_Write(&pSaveData->tokenSize, sizeof(int), 1, pFile);
	FS_Write(pSaveData->pCurrentData, pSaveData->tokenSize, 1, pFile);
	FS_Write(pTableData, tableSize, 1, pFile);
	FS_Write(pSaveData->pBaseData, dataSize, 1, pFile);
	FS_Close(pFile);
	EntityPatchWrite(pSaveData, sv.name);

	_snprintf(name, sizeof(name), "%s%s.HL2", Host_SaveGameDirectory(), sv.name);
	COM_FixSlashes(name);
	CL_Save(name);

	return pSaveData;
}

void Host_SaveAgeList(const char* pName, int count)
{
	char newName[MAX_PATH];
	char oldName[MAX_PATH];

	_snprintf(newName, sizeof(newName), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count);
	COM_FixSlashes(newName);
	FS_RemoveFile(newName, "GAMECONFIG");

	while (count > 0)
	{
		if (count == 1)
			_snprintf(oldName, sizeof(oldName), "%s%s.sav", Host_SaveGameDirectory(), pName);
		else
			_snprintf(oldName, sizeof(oldName), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count - 1);

		COM_FixSlashes(oldName);
		_snprintf(newName, sizeof(newName), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count);
		COM_FixSlashes(newName);
		FS_Rename(oldName, newName);
		count--;
	}
}

int FileSize(FileHandle_t pFile)
{
	if (!pFile)
		return 0;
	return FS_Size(pFile);
}

void FileCopy(FileHandle_t pOutput, FileHandle_t pInput, int fileSize)
{
	char buf[1024];
	int size;
	while (fileSize > 0)
	{
		if (fileSize > sizeof(buf))
			size = sizeof(buf);
		else
			size = fileSize;

		FS_Read(buf, size, 1, pInput);
		FS_Write(buf, size, 1, pOutput);
		fileSize -= size;
	}
}

void DirectoryCopy(const char* pPath, FileHandle_t pFile)
{
	const char* findfn;
	char basefindfn[MAX_PATH];
	int fileSize;
	FILE* pCopy;
	char szName[MAX_PATH];

	findfn = Sys_FindFirst(pPath, basefindfn);
	while (findfn != NULL)
	{
		_snprintf(szName, sizeof(szName), "%s%s", Host_SaveGameDirectory(), findfn);
		COM_FixSlashes(szName);
		pCopy = (FILE*)FS_OpenPathID(szName, "rb", "GAMECONFIG");
		fileSize = FS_Size(pCopy);
		FS_Write(findfn, MAX_PATH, 1, pFile);
		FS_Write(&fileSize, sizeof(int), 1, pFile);
		FileCopy(pFile, pCopy, fileSize);
		FS_Close(pCopy);
		findfn = Sys_FindNext(basefindfn);
	}
	Sys_FindClose();
}

void DirectoryExtract(FileHandle_t pFile, int fileCount)
{
	int i;
	int fileSize;
	FILE* pCopy;
	char szName[MAX_PATH];
	char fileName[MAX_PATH];

	for (i = 0; i < fileCount; i++)
	{
		FS_Read(fileName, sizeof(fileName), 1, pFile);
		FS_Read(&fileSize, sizeof(int), 1, pFile);
		_snprintf(szName, sizeof(szName), "%s%s", Host_SaveGameDirectory(), fileName);
		COM_FixSlashes(szName);
		pCopy = (FILE*)FS_OpenPathID(szName, "wb", "GAMECONFIG");
		FileCopy(pCopy, pFile, fileCount);
		FS_Close(pCopy);
	}
}

int DirectoryCount(const char* pPath)
{
	int count;
	const char* findfn;

	count = 0;
	findfn = Sys_FindFirstPathID(pPath, const_cast<char*>("GAMECONFIG"));

	while (findfn != NULL)
	{
		findfn = Sys_FindNext(NULL);
		count++;
	}
	Sys_FindClose();
	return count;
}

qboolean SaveGameSlot(const char* pSaveName, const char* pSaveComment)
{
	char hlPath[256];
	char name[256];
	char* pTokenData;
	int tag;
	int i;
	FILE* pFile;
	SAVERESTOREDATA* pSaveData;
	GAME_HEADER gameHeader;

	FS_CreateDirHierarchy(Host_SaveGameDirectory(), "GAMECONFIG");
	pSaveData = SaveGamestate();
	if (!pSaveData)
		return FALSE;

	SaveExit(pSaveData);
	pSaveData = SaveInit(0);

	_snprintf(hlPath, sizeof(hlPath), "%s*.HL?", Host_SaveGameDirectory());
	COM_FixSlashes(hlPath);
	gameHeader.mapCount = DirectoryCount(hlPath);
	Q_strncpy(gameHeader.mapName, sv.name, 31);
	gameHeader.mapName[31] = 0;
	Q_strncpy(gameHeader.comment, pSaveComment, 79);
	gameHeader.comment[79] = 0;

	gEntityInterface.pfnSaveWriteFields(pSaveData, "GameHeader", &gameHeader, gGameHeaderDescription, ARRAYSIZE(gGameHeaderDescription));
	gEntityInterface.pfnSaveGlobalState(pSaveData);

	pTokenData = NULL;
	for (i = 0; i < pSaveData->tokenCount; i++)
	{
		if (pSaveData->pTokens[i])
		{
			pSaveData->size += Q_strlen(pSaveData->pTokens[i]) + 1;
			if (pSaveData->size > pSaveData->bufferSize)
			{
				Con_Printf(const_cast<char*>("Token Table Save/Restore overflow!"));
				pSaveData->size = pSaveData->bufferSize;
				break;
			}
			pTokenData = pSaveData->pCurrentData;
			do
			{
				*pTokenData++ = *pSaveData->pTokens[i]++;
			} while (*pSaveData->pTokens[i]);
			pSaveData->pCurrentData = pTokenData;
		}
		else
		{
			if (pSaveData->size + 1 > pSaveData->bufferSize)
			{
				Con_Printf(const_cast<char*>("Token Table Save/Restore overflow!"));
				pSaveData->size = pSaveData->bufferSize;
				break;
			}
			pTokenData = pSaveData->pCurrentData;
			*pTokenData = 0;
			pSaveData->pCurrentData = pTokenData + 1;
		}
	}
	pSaveData->tokenSize = (int)(pSaveData->pCurrentData - pTokenData);

	if (pSaveData->size < pSaveData->bufferSize)
		pSaveData->size -= pSaveData->tokenSize;

	_snprintf(name, 252, "%s%s", Host_SaveGameDirectory(), pSaveName);
	COM_DefaultExtension(name, const_cast<char*>(".sav"));
	COM_FixSlashes(name);
	Con_DPrintf(const_cast<char*>("Saving game to %s...\n"), name);

	if (Q_strcasecmp(pSaveName, "quick") || Q_strcasecmp(pSaveName, "autosave"))
		Host_SaveAgeList(pSaveName, 1);

	pFile = (FILE*)FS_OpenPathID(name, "wb", "GAMECONFIG");

	tag = SAVEGAME_HEADER;
	FS_Write(&tag, sizeof(int), 1, pFile);
	tag = SAVEGAME_VERSION;
	FS_Write(&tag, sizeof(int), 1, pFile);
	FS_Write(&pSaveData->size, sizeof(int), 1, pFile);
	FS_Write(&pSaveData->tokenCount, sizeof(int), 1, pFile);
	FS_Write(&pSaveData->tokenSize, sizeof(int), 1, pFile);
	FS_Write(pSaveData->pCurrentData, pSaveData->tokenSize, 1, pFile);
	FS_Write(pSaveData->pBaseData, pSaveData->size, 1, pFile);
	DirectoryCopy(hlPath, pFile);
	FS_Close(pFile);
	SaveExit(pSaveData);

	return TRUE;
}

qboolean SaveGame(const char* pszSlot, const char* pszComment)
{
	qboolean qret;
	qboolean q;

	q = scr_skipupdate;
	scr_skipupdate = TRUE;
	qret = SaveGameSlot(pszSlot, pszComment);
	scr_skipupdate = q;
	return qret;
}

int SaveReadHeader(FileHandle_t pFile, GAME_HEADER* pHeader, int readGlobalState)
{
	int i;
	int tag;
	int size;
	int tokenCount;
	int tokenSize;
	char* pszTokenList;
	SAVERESTOREDATA* pSaveData;

	FS_Read(&tag, sizeof(int), 1, pFile);
	if (tag != SAVEGAME_HEADER)
	{
		FS_Close(pFile);
		return 0;
	}
	FS_Read(&tag, sizeof(int), 1, pFile);
	if (tag != SAVEGAME_VERSION)
	{
		FS_Close(pFile);
		return 0;
	}
	FS_Read(&size, sizeof(int), 1, pFile);
	FS_Read(&tokenCount, sizeof(int), 1, pFile);
	FS_Read(&tokenSize, sizeof(int), 1, pFile);

	pSaveData = (SAVERESTOREDATA*)Mem_Calloc(sizeof(SAVERESTOREDATA) + tokenSize + size, sizeof(char));
	pSaveData->tableCount = 0;
	pSaveData->pTable = NULL;
	pSaveData->connectionCount = 0;

	pszTokenList = (char*)(pSaveData + 1);
	if (tokenSize > 0)
	{
		pSaveData->tokenCount = tokenCount;
		pSaveData->tokenSize = tokenSize;

		FS_Read(pszTokenList, tokenSize, 1, pFile);

		if (!pSaveData->pTokens)
			pSaveData->pTokens = (char**)Mem_Calloc(tokenCount, sizeof(char*));

		for (i = 0; i < tokenCount; i++)
		{
			if (*pszTokenList)
				pSaveData->pTokens[i] = pszTokenList;
			else
				pSaveData->pTokens[i] = NULL;
			while (*pszTokenList++);
		}
	}
	pSaveData->size = 0;
	pSaveData->bufferSize = size;
	pSaveData->fUseLandmark = 0;
	pSaveData->time = 0.0f;

	pSaveData->pCurrentData = pszTokenList;
	pSaveData->pBaseData = pszTokenList;

	FS_Read(pSaveData->pBaseData, size, 1, pFile);
	gEntityInterface.pfnSaveReadFields(pSaveData, "GameHeader", pHeader, gGameHeaderDescription, ARRAYSIZE(gGameHeaderDescription));
	if (readGlobalState)
		gEntityInterface.pfnRestoreGlobalState(pSaveData);
	SaveExit(pSaveData);
	return 1;
}

int Host_Load(const char* pName)
{
	FILE* pFile;
	GAME_HEADER gameHeader;
	char name[256];
	int nSlot;

	if (!pName || !pName[0])
		return 0;

	if (strstr(pName, ".."))
	{
		Con_Printf(const_cast<char*>("Relative pathnames are not allowed.\n"));
		return 0;
	}

	if (*pName == '_' && COM_TokenWaiting((char*)&pName[1]))
	{
		nSlot = Q_atoi(pName);
		if (nSlot < 1 || nSlot > 12)
			return 0;

		_snprintf(name, 252, "%sHalf-Life-%i", Host_SaveGameDirectory(), nSlot);
	}
	else
		_snprintf(name, 252, "%s%s", Host_SaveGameDirectory(), pName);
	name[251] = 0;

	if (!svs.dll_initialized)
		Host_InitializeGameDLL();

	COM_DefaultExtension(name, const_cast<char*>(".sav"));
	COM_FixSlashes(name);
	name[255] = 0;

	Con_Printf(const_cast<char*>("Loading game from %s...\n"), name);
	pFile = (FILE*)FS_OpenPathID(name, "rb", "GAMECONFIG");
	if (!pFile)
		return 0;
	Host_ClearGameState();
	if (!SaveReadHeader(pFile, &gameHeader, 1))
	{
		giStateInfo = 1;
		Cbuf_AddText(const_cast<char*>("\ndisconnect\n"));
		return 0;
	}

	cls.demonum = -1;
	SV_InactivateClients();
	SCR_BeginLoadingPlaque(FALSE);
	DirectoryExtract(pFile, gameHeader.mapCount);
	FS_Close(pFile);

	Cvar_SetValue(const_cast<char*>("deathmatch"), 0.0);
	Cvar_SetValue(const_cast<char*>("coop"), 0.0);

	if (!Q_strcasecmp(com_gamedir, "valve")
		|| !Q_strcasecmp(com_gamedir, "bshift")
		|| !Q_strcasecmp(com_gamedir, "gearbox"))
	{
		svs.maxclients = 1;
		Cvar_SetValue(const_cast<char*>("maxplayers"), 1.0);
	}

	_snprintf(name, sizeof(name), "map %s\n", gameHeader.mapName);
	extern void DbgPrint(FILE*, const char* format, ...);
	extern FILE* m_fMessages;
	DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
	CL_Disconnect();
	Host_Map(FALSE, name, gameHeader.mapName, TRUE);
	return 1;
}

int LoadGame(const char* pName)
{
	int iRet;
	qboolean q;

	q = scr_skipupdate;
	scr_skipupdate = TRUE;
	iRet = Host_Load(pName);
	scr_skipupdate = q;
	return iRet;
}

void Host_Reload_f(void)
{
	const char* pSaveName;
	char name[MAX_PATH];
	if (cls.demoplayback || !sv.active || cmd_source != src_command)
		return;

	Host_ClearGameState();
	SV_InactivateClients();
	SV_ServerShutdown();

	pSaveName = Host_FindRecentSave(name);
	if (!pSaveName || !Host_Load(pSaveName))
	{
		SV_SpawnServer(FALSE, gHostMap.string, NULL);
		SV_LoadEntities();
		SV_ActivateServer(1);
	}
}

void Host_Changelevel_f(void)
{
	char _level[64];
	char _startspot[64];

	char* level = NULL;
	char* startspot = NULL;

	if (Cmd_Argc() < 2)
	{
		Con_Printf(const_cast<char*>("changelevel <levelname> : continue game on a new level\n"));
		return;
	}
	if (!sv.active || cls.demoplayback)
	{
		Con_Printf(const_cast<char*>("Only the server may changelevel\n"));
		return;
	}
	level = (char*)Cmd_Argv(1);
	if (!PF_IsMapValid_I(level))
	{
		Con_Printf(const_cast<char*>("changelevel failed: '%s' not found on server.\n"), level);
		return;
	}

	Q_strncpy(_level, level, sizeof(level) - 1);
	_level[sizeof(level) - 1] = 0;

	if (Cmd_Argc() != 2)
	{
		startspot = &_startspot[0];
		Q_strncpy(_startspot, Cmd_Argv(2), 63);
		_startspot[63] = 0;
	}
	else
		_startspot[0] = 0;

	SCR_BeginLoadingPlaque(FALSE);
	S_StopAllSounds(1);
	SV_InactivateClients();
	SV_ServerShutdown();
	SV_SpawnServer(FALSE, _level, startspot);
	SV_LoadEntities();
	SV_ActivateServer(1);
}

int EntryInTable(SAVERESTOREDATA* pSaveData, const char* pMapName, int index)
{
	int i;
	for (i = index + 1; i < pSaveData->connectionCount; i++)
	{
		if (!Q_strcmp(pSaveData->levelList[i].mapName, pMapName))
			return i;
	}
	return -1;
}

void LandmarkOrigin(SAVERESTOREDATA* pSaveData, vec_t* output, const char* pLandmarkName)
{
	int i;
	for (i = 0; i < pSaveData->connectionCount; i++)
	{
		if (!Q_strcmp(pSaveData->levelList[i].landmarkName, pLandmarkName))
		{
			output[0] = pSaveData->levelList[i].vecLandmarkOrigin[0];
			output[1] = pSaveData->levelList[i].vecLandmarkOrigin[1];
			output[2] = pSaveData->levelList[i].vecLandmarkOrigin[2];
			return;
		}
	}
	output[0] = output[1] = output[2] = 0.0f;
}

int EntityInSolid(edict_t* pent)
{
	vec3_t point;
	if (pent->v.movetype == MOVETYPE_FOLLOW)
	{
		edict_t* pAimEnt = pent->v.aiment;
		if (pAimEnt && pAimEnt->v.flags & FL_CLIENT)
			return 0;
	}
	g_groupmask = pent->v.groupinfo;
	point[0] = (pent->v.absmin[0] + pent->v.absmax[0]) * 0.5f;
	point[1] = (pent->v.absmin[1] + pent->v.absmax[1]) * 0.5f;
	point[2] = (pent->v.absmin[2] + pent->v.absmax[2]) * 0.5f;
	return (SV_PointContents(point) == CONTENTS_SOLID);
}

int CreateEntityList(SAVERESTOREDATA* pSaveData, int levelMask)
{
	int i;
	int movedCount = 0;
	int active;
	edict_t* pent;
	ENTITYTABLE* pEntInfo;

	if (pSaveData->tableCount < 1)
		return 0;

	for (i = 0; i < pSaveData->tableCount; i++)
	{
		pent = NULL;
		pEntInfo = &pSaveData->pTable[i];
		if (pEntInfo->classname && pEntInfo->size && pEntInfo->id)
		{
			active = !!(pEntInfo->flags & levelMask);
			if (SV_IsPlayerIndex(pEntInfo->id))
			{
				client_t* cl = &svs.clients[pEntInfo->id - 1];
				if (active && cl)
				{
					pent = cl->edict;
					if (pent && !pent->free)
					{
						if (!(pEntInfo->flags & FENTTABLE_PLAYER))
							Sys_Error("ENTITY IS NOT A PLAYER: %d\n", i);

						if (cl->active)
							EntityInit(pent, pEntInfo->classname);
					}
				}
			}
			else if (active)
				pent = CreateNamedEntity(pEntInfo->classname);
		}
		pEntInfo->pent = pent;
	}
	for (i = 0; i < pSaveData->tableCount; i++)
	{
		pEntInfo = &pSaveData->pTable[i];
		pent = pEntInfo->pent;
		pSaveData->currentIndex = i;
		pSaveData->size = pEntInfo->location;
		pSaveData->pCurrentData = &pSaveData->pBaseData[pEntInfo->location];

		if (pent && (pEntInfo->flags & levelMask))
		{
			if (pEntInfo->flags & FENTTABLE_GLOBAL)
			{
				Con_DPrintf(const_cast<char*>("Merging changes for global: %s\n"), &pr_strings[pEntInfo->classname]);
				gEntityInterface.pfnRestore(pent, pSaveData, 1);
				ED_Free(pent);
			}
			else
			{
				Con_DPrintf(const_cast<char*>("Transferring %s (%d)\n"), &pr_strings[pEntInfo->classname], NUM_FOR_EDICT(pent));
				if (gEntityInterface.pfnRestore(pent, pSaveData, 0) < 0)
					ED_Free(pent);
				else
				{
					SV_LinkEdict(pent, FALSE);
					if (!(pEntInfo->flags & FENTTABLE_PLAYER) && EntityInSolid(pent))
					{
						Con_DPrintf(const_cast<char*>("Suppressing %s\n"), &pr_strings[pEntInfo->classname]);
						ED_Free(pent);
					}
					else
					{
						movedCount++;
						pEntInfo->flags = FENTTABLE_REMOVED;
					}
				}
			}
		}
	}
	return movedCount;
}

void LoadAdjacentEntities(const char* pOldLevel, const char* pLandmarkName)
{
	int i;
	int test;
	int flags;
	int index;
	int movedCount = 0;
	SAVE_HEADER header;
	vec3_t landmarkOrigin;
	SAVERESTOREDATA* pSaveData;
	SAVERESTOREDATA currentLevelData;

	Q_memset(&currentLevelData, 0, sizeof(SAVERESTOREDATA));
	gGlobalVariables.pSaveData = &currentLevelData;
	gEntityInterface.pfnParmsChangeLevel();

	for (i = 0; i < currentLevelData.connectionCount; i++)
	{
		for (test = 0; test < i; test++)
		{
			// Only do maps once
			if (!Q_strcmp(currentLevelData.levelList[i].mapName, currentLevelData.levelList[test].mapName))
				break;
		}
		// Map was already in the list
		if (test < i)
			continue;

		pSaveData = LoadSaveData(currentLevelData.levelList[i].mapName);
		if (pSaveData)
		{
			ParseSaveTables(pSaveData, &header, 0);
			EntityPatchRead(pSaveData, currentLevelData.levelList[i].mapName);
			pSaveData->time = sv.time;
			pSaveData->fUseLandmark = 1;
			flags = 0;

			LandmarkOrigin(&currentLevelData, landmarkOrigin, pLandmarkName);
			LandmarkOrigin(pSaveData, pSaveData->vecLandmarkOffset, pLandmarkName);

			pSaveData->vecLandmarkOffset[0] -= landmarkOrigin[0];
			pSaveData->vecLandmarkOffset[1] -= landmarkOrigin[1];
			pSaveData->vecLandmarkOffset[2] -= landmarkOrigin[2];

			if (!Q_strcmp(currentLevelData.levelList[i].mapName, pOldLevel))
				flags |= FENTTABLE_PLAYER;

			index = -1;
			while (1)
			{
				index = EntryInTable(pSaveData, sv.name, index);
				if (index < 0)
					break;

				flags |= (1 << index);
			}

			if (flags)
				movedCount = CreateEntityList(pSaveData, flags);
			// If ents were moved, rewrite entity table to save file
			if (movedCount)
				EntityPatchWrite(pSaveData, currentLevelData.levelList[i].mapName);
			SaveExit(pSaveData);
		}
	}
	gGlobalVariables.pSaveData = NULL;
}

void Host_Changelevel2_f(void)
{
	char level[64];
	char oldlevel[64];
	char _startspot[64];
	char* startspot;

	SAVERESTOREDATA* pSaveData;
	qboolean newUnit;

	giActive = DLL_TRANS;

	if (Cmd_Argc() < 2)
	{
		Con_Printf(const_cast<char*>("changelevel2 <levelname> : continue game on a new level in the unit\n"));
		return;
	}
	if (!sv.active || cls.demoplayback || sv.paused)
	{
		Con_Printf(const_cast<char*>("Only the server may changelevel\n"));
		return;
	}
	if (svs.maxclients > 1)
	{
		Con_Printf(const_cast<char*>("changelevel2 <levelname> : not for use with multiplayer games\n"));
		return;
	}

	SCR_BeginLoadingPlaque(FALSE);
	S_StopAllSounds(TRUE);

	Q_strncpy(level, Cmd_Argv(1), sizeof(level) - 1);
	level[sizeof(level) - 1] = 0;

	if (Cmd_Argc() != 2)
	{
		startspot = &_startspot[0];
		Q_strncpy(_startspot, Cmd_Argv(2), sizeof(_startspot) - 1);
		_startspot[sizeof(_startspot) - 1] = 0;
	}
	else
		startspot = NULL;

	Q_strncpy(oldlevel, sv.name, sizeof(oldlevel) - 1);
	oldlevel[sizeof(oldlevel) - 1] = 0;

	pSaveData = SaveGamestate();
	SV_ServerShutdown();
	FS_LogLevelLoadStarted(level);

	if (!SV_SpawnServer(FALSE, level, startspot))
		Sys_Error(__FUNCTION__ ": Couldn't load map %s\n", level);

	if (pSaveData)
		SaveExit(pSaveData);

	newUnit = FALSE;
	if (!LoadGamestate(level, 0))
	{
		newUnit = TRUE;
		SV_LoadEntities();
	}

	LoadAdjacentEntities(oldlevel, startspot);
	gGlobalVariables.time = sv.time;

	sv.paused = TRUE;
	sv.loadgame = TRUE;

	if (newUnit && sv_newunit.value != 0.0f)
		Host_ClearSaveDirectory();

	SV_ActivateServer(0);
}

void Host_Reconnect_f(void)
{
	char cmdString[128];
	if (cls.state < ca_connected)
		return;

	if (cls.passive)
	{
		_snprintf(cmdString, sizeof(cmdString), "listen %s\n", NET_AdrToString(cls.connect_stream));
		Cbuf_AddText(cmdString);
		return;
	}

	SCR_BeginLoadingPlaque(FALSE);
	cls.signon = 0;
	cls.state = ca_connected;
	sys_timescale.value = 1.0f;

	Netchan_Clear(&cls.netchan);
	SZ_Clear(&cls.netchan.message);
	MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, "new");
}

void Host_Version_f(void)
{
	Con_Printf(const_cast<char*>("Protocol version %i\nExe version %s (%s)\n"), PROTOCOL_VERSION, gpszVersionString, gpszProductString);
	Con_Printf(const_cast<char*>("Exe build: " __TIME__ " " __DATE__ " (%i)\n"), build_number());
}

void Host_Say(qboolean teamonly)
{
	client_t* client;
	client_t* save;
	int j;
	char* p;
	char text[128];
	qboolean fromServer;

	if (cls.state != ca_dedicated)
	{
		if (cmd_source != src_command)
			return;

		Cmd_ForwardToServer();
		return;
	}

	if (Cmd_Argc() < 2)
		return;

	p = (char*)Cmd_Args();
	if (!p)
		return;

	save = host_client;
	// Removes quotes, "text" -> text
	if (*p == '"')
	{
		p++;
		p[Q_strlen(p) - 1] = '\0';
	}

	_snprintf(text, sizeof(text), "%c<%s> ", 1, Cvar_VariableString(const_cast<char*>("hostname")));

	if (Q_strlen(p) > 63)
		p[63] = '\0';

	// 1 cell for '\n'
	j = (sizeof(text) - 1) - (Q_strlen(text) + 1);
	if (Q_strlen(p) > (unsigned int)j)
		p[j] = '\0';

	Q_strcat(text, p);
	Q_strcat(text, const_cast<char*>("\n"));

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active || !client->spawned || client->fakeclient)
			continue;

		host_client = client;

		PF_MessageBegin_I(MSG_ONE, RegUserMsg("SayText", -1), NULL, &sv.edicts[j + 1]);
		PF_WriteByte_I(0);
		PF_WriteString_I(text);
		PF_MessageEnd_I();
	}

	host_client = save;
	// Cause we have '\x01' in the beginning
	Sys_Printf("%s", &text[1]);
	Log_Printf("Server say \"%s\"\n", p);
}

void Host_Say_f(void)
{
	Host_Say(FALSE);
}

void Host_Say_Team_f(void)
{
	Host_Say(TRUE);
}

void Host_Tell_f(void)
{
	client_t* client;
	client_t* save;
	int j;
	char* p;
	char text[64];
	char* tellmsg;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}
	if (Cmd_Argc() < 3)
		return;

	p = (char*)Cmd_Args();
	if (!p)
		return;

	_snprintf(text, sizeof(text), "%s TELL: ", host_client->name);

	if (*p == '"')
	{
		p++;
		p[Q_strlen(p) - 1] = 0;
	}

	j = ARRAYSIZE(text) - 2 - Q_strlen(text);
	if (Q_strlen(p) > (unsigned int)j)
		p[j] = 0;

	tellmsg = strstr(p, Cmd_Argv(1));
	if (tellmsg != NULL)
		Q_strcat(text, &tellmsg[Q_strlen(Cmd_Argv(1))]);
	else
		Q_strcat(text, p);

	Q_strcat(text, const_cast<char*>("\n"));
	save = host_client;

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active || !client->spawned || client->fakeclient)
			continue;
		if (Q_strcasecmp(client->name, Cmd_Argv(1)))
			continue;

		host_client = client;

		PF_MessageBegin_I(MSG_ONE, RegUserMsg("SayText", -1), NULL, &sv.edicts[j + 1]);
		PF_WriteByte_I(0);
		PF_WriteString_I(text);
		PF_MessageEnd_I();
		break;
	}
	host_client = save;
}

void Host_Kill_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (sv_player->v.health <= 0.0f)
	{
		SV_ClientPrintf("Can't suicide -- already dead!\n");
		return;
	}
	gGlobalVariables.time = sv.time;
	gEntityInterface.pfnClientKill(sv_player);
}

void Host_TogglePause_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}
	if (!pausable.value)
	{
		SV_ClientPrintf("Pause not allowed.\n");
		return;
	}
	sv.paused ^= TRUE;
	if (sv.paused)
		SV_BroadcastPrintf("%s paused the game\n", &pr_strings[sv_player->v.netname]);
	else
		SV_BroadcastPrintf("%s unpaused the game\n", &pr_strings[sv_player->v.netname]);

	MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
	MSG_WriteByte(&sv.reliable_datagram, sv.paused);
}

void Host_Pause_f(void)
{
	// pause only singleplayer when console or main menu opens
	if (!cl.levelname[0])
		return;
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}
	if (!pausable.value)
		return;

	sv.paused = TRUE;

	MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
	MSG_WriteByte(&sv.reliable_datagram, sv.paused);
}

void Host_Unpause_f(void)
{
	// unpause only singleplayer when console or main menu opens
	if (!cl.levelname[0])
		return;
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}
	if (!pausable.value)
		return;

	sv.paused = FALSE;

	MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
	MSG_WriteByte(&sv.reliable_datagram, sv.paused);
}

void Host_Interp_f(void)
{
	r_dointerp ^= 1;
	if (!r_dointerp)
		Con_Printf(const_cast<char*>("Frame Interpolation OFF\n"));
	else
		Con_Printf(const_cast<char*>("Frame Interpolation ON\n"));
}

void Host_Ping_f(void)
{
	int i;
	client_t* client;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}
	SV_ClientPrintf("Client ping times:\n");

	client = svs.clients;
	for (i = 0; i < svs.maxclients; i++, client++)
	{
		if (client->active)
			SV_ClientPrintf("%4i %s\n", SV_CalcPing(client), client->name);
	}
}

void Host_Motd_f(void)
{
	int length;
	FileHandle_t pFile;
	char* pFileList;
	char* next;

	pFileList = motdfile.string;
	if (*pFileList == '/' || strstr(pFileList, ":") || strstr(pFileList, "..") || strstr(pFileList, "\\"))
	{
		Con_Printf(const_cast<char*>("Unable to open %s (contains illegal characters)\n"), pFileList);
		return;
	}
	pFile = FS_Open(pFileList, "rb");
	if (!pFile)
	{
		Con_Printf(const_cast<char*>("Unable to open %s\n"), pFileList);
		return;
	}
	length = FS_Size(pFile);
	if (length > 0)
	{
		char* buf = (char*)Mem_Malloc(length + 1);
		if (buf)
		{
			FS_Read(buf, length, 1, pFile);
			buf[length] = 0;
			char* now = buf;
			Con_Printf(const_cast<char*>("motd:"));
			next = strchr(now, '\n');
			while (next != NULL)
			{
				*next = 0;
				Con_Printf(const_cast<char*>("%s\n"), now);
				now = next + 1;
				next = strchr(now, '\n');
			}

			Con_Printf(const_cast<char*>("%s\n"), now);

			Mem_Free(buf);
		}
	}
	FS_Close(pFile);
}

void Host_Motd_Write_f(void)
{
	char newFile[2048] = "";
	unsigned int i;
	FileHandle_t pFile;

	if (!sv.active || cmd_source != src_command || cls.state)
		return;

	if (!IsSafeFileToDownload(motdfile.string) || !strstr(motdfile.string, ".txt"))
	{
		Con_Printf(const_cast<char*>("Invalid motdfile name (%s)\n"), motdfile.string);
		return;
	}
	pFile = FS_Open(motdfile.string, "wb+");
	if (!pFile)
	{
		Con_Printf(const_cast<char*>("Unable to open %s\n"), motdfile.string);
		return;
	}

	Q_strncpy(newFile, Cmd_Args(), ARRAYSIZE(newFile));

	auto len = Q_strlen(newFile);
	for (i = 0; i < len; i++)
	{
		if (newFile[i] == '\\' && newFile[i + 1] == 'n')
		{
			newFile[i] = '\n';
			memmove(&newFile[i + 1], &newFile[i + 2], Q_strlen(&newFile[i + 2]) + 1);
		}
	}
	FS_Write(newFile, Q_strlen(newFile), 1, pFile);
	FS_Close(pFile);
	Con_Printf(const_cast<char*>("Done.\n"));
}

void Host_Loadgame_f(void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf(const_cast<char*>("load <savename> : load a game\n"));
		return;
	}
	if (!Host_Load(Cmd_Argv(1)))
		Con_Printf(const_cast<char*>("Error loading saved game\n"));
}

int Host_ValidSave(void)
{
	if (cmd_source != src_command)
		return 0;

	if (!sv.active)
	{
		Con_Printf(const_cast<char*>("Not playing a local game.\n"));
		return 0;
	}
	if (svs.maxclients != 1)
	{
		Con_Printf(const_cast<char*>("Can't save multiplayer games.\n"));
		return 0;
	}
	if (cls.state != ca_active || cls.signon != 2)
	{
		Con_Printf(const_cast<char*>("Can't save during transition.\n"));
		return 0;
	}
	if (cl.intermission)
	{
		Con_Printf(const_cast<char*>("Can't save in intermission.\n"));
		return 0;
	}
	if (svs.clients->active && svs.clients->edict->v.health <= 0.0)
	{
		Con_Printf(const_cast<char*>("Can't savegame with a dead player\n"));
		return 0;
	}
	return 1;
}

void Host_Savegame_f(void)
{
	char szComment[80];
	char szTemp[80];

	if (!Host_ValidSave())
		return;

	if (Cmd_Argc() != 2)
	{
		Con_DPrintf(const_cast<char*>("save <savename> : save a game\n"));
		return;
	}
	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_DPrintf(const_cast<char*>("Relative pathnames are not allowed.\n"));
		return;
	}
	g_pSaveGameCommentFunc(szTemp, 80);
	_snprintf(szComment, sizeof(szComment) - 1, "%-64.64s %02d:%02d", szTemp, (int)(sv.time / 60.0), (int)fmod(sv.time, 60.0));
	SaveGameSlot(Cmd_Argv(1), szComment);
	CL_HudMessage("GAMESAVED");
}

void Host_AutoSave_f(void)
{
	char szComment[80];
	char szTemp[80];

	if (Host_ValidSave())
	{
		g_pSaveGameCommentFunc(szTemp, 80);
		_snprintf(szComment, sizeof(szComment) - 1, "%-64.64s %02d:%02d", szTemp, (int)(sv.time / 60.0), (int)fmod(sv.time, 60.0));
		szComment[sizeof(szComment) - 1] = 0;
		SaveGameSlot("autosave", szComment);
	}
}

void Host_VoiceRecordStart_f(void)
{
	if (cls.state == ca_active)
	{
		const char* pUncompressedFile = NULL;
		const char* pDecompressedFile = NULL;
		const char* pInputFile = NULL;

		if (voice_recordtofile.value)
		{
			pUncompressedFile = "voice_micdata.wav";
			pDecompressedFile = "voice_decompressed.wav";
		}

		if (voice_inputfromfile.value)
		{
			pInputFile = "voice_input.wav";
		}

		if (Voice_RecordStart(pUncompressedFile, pDecompressedFile, pInputFile))
		{
		}
	}
}


void Host_VoiceRecordStop_f(void)
{
	if (cls.state == ca_active)
	{
		if (Voice_IsRecording())
		{
			CL_AddVoiceToDatagram(true);
			Voice_RecordStop();
		}
	}
}

void Host_NextDemo(void)
{
	char str[1024];
	if (cls.demonum == -1)
		return;

	SCR_BeginLoadingPlaque(FALSE);
	if (cls.demos[cls.demonum][0])
	{
		if (cls.demonum == MAX_DEMOS)
			cls.demonum = 0;

		_snprintf(str, sizeof(str), "playdemo %s\n", cls.demos[cls.demonum]);
		Cbuf_InsertText(str);
		cls.demonum++;
	}
	Con_Printf(const_cast<char*>("No demos listed with startdemos\n"));
	cls.demonum = -1;
}

void Host_Startdemos_f(void)
{
	int i;
	int c;

	if (cls.state == ca_dedicated)
	{
		if (!sv.active)
			Con_Printf(const_cast<char*>("Cannot play demos on a dedicated server.\n"));
		return;
	}
	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		c = MAX_DEMOS;
		Con_Printf(const_cast<char*>("Max %i demos in demoloop\n"), MAX_DEMOS);
		Con_Printf(const_cast<char*>("%i demo(s) in loop\n"), MAX_DEMOS);
	}
	Con_Printf(const_cast<char*>("%i demo(s) in loop\n"), c);
	for (i = 1; i < c + 1; i++)
	{
		Q_strncpy(cls.demos[i - 1], Cmd_Argv(i), 15);
		cls.demos[i - 1][15] = 0;
	}
	if (sv.active || cls.demonum == -1 || cls.demoplayback)
		cls.demonum = -1;
	else
	{
		cls.demonum = 0;
		Host_NextDemo();
	}
}

void Host_Demos_f(void)
{
	if (cls.state != ca_dedicated)
	{
		if (cls.demonum == -1)
			cls.demonum = 0;
		CL_Disconnect_f();
		Host_NextDemo();
	}
}

void Host_Stopdemo_f(void)
{
	if (cls.state != ca_dedicated)
	{
		if (cls.demoplayback)
		{
			CL_StopPlayback();
			extern void DbgPrint(FILE*, const char* format, ...);
			extern FILE* m_fMessages;
			DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
			CL_Disconnect();
		}
	}
}

void Host_SetInfo_f(void)
{
	if (Cmd_Argc() == 1)
	{
		Info_Print(cls.userinfo);
		return;
	}
	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("usage: setinfo [ <key> <value> ]\n"));
		return;
	}
	if (cmd_source == src_command)
	{
		Info_SetValueForKey(cls.userinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING);
		Cmd_ForwardToServer();
		return;
	}
	Info_SetValueForKey(host_client->userinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING);
	host_client->sendinfo = TRUE;
}

void Host_FullInfo_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf(const_cast<char*>("fullinfo <complete info string>\n"));
		return;
	}

	char key[512];
	char value[512];
	char* o;
	char* s;

	s = (char*)Cmd_Argv(1);
	if (*s == '\\')
		s++;

	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (!*s)
		{
			Con_Printf(const_cast<char*>("MISSING VALUE\n"));
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;
		if (*s)
			s++;

		if (cmd_source == src_command)
		{
			Info_SetValueForKey(cls.userinfo, key, value, MAX_INFO_STRING);
			Cmd_ForwardToServer();
			return;
		}
		Info_SetValueForKey(host_client->userinfo, key, value, MAX_INFO_STRING);
		host_client->sendinfo = TRUE;
	}
	}

void Host_KillVoice_f(void)
{
	Voice_Deinit();
}

void Master_Heartbeat_f()
{
	;
}

void Host_God_f()
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
	}
	else if (!gGlobalVariables.deathmatch && sv_cheats.value)
	{
		sv_player->v.flags ^= FL_GODMODE;
		if (sv_player->v.flags & FL_GODMODE)
			SV_ClientPrintf("godmode ON\n");
		else
			SV_ClientPrintf("godmode OFF\n");
	}
}

void Host_Notarget_f()
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
	}
	else if (!gGlobalVariables.deathmatch && sv_cheats.value)
	{
		sv_player->v.flags ^= FL_NOTARGET;
		if (sv_player->v.flags & FL_NOTARGET)
			SV_ClientPrintf("notarget ON\n");
		else
			SV_ClientPrintf("notarget OFF\n");
	}
}

void Host_Fly_f()
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
	}
	else if (!gGlobalVariables.deathmatch)
	{
		if (sv_player->v.movetype == MOVETYPE_FLY)
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf("flymode OFF\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf("flymode ON\n");
		}
	}
}

int FindPassableSpace(edict_t* pEdict, vec_t* direction, float step)
{
	for (int i = 0; i < 100; i++)
	{
		VectorMA(pEdict->v.origin, step, direction, pEdict->v.origin);
		if (!SV_TestEntityPosition(pEdict))
		{
			VectorCopy(pEdict->v.origin, pEdict->v.oldorigin);
			return true;;
		}
	}

	return false;
}

void Host_Noclip_f()
{
	vec3_t vforward, right, up;

	if (cmd_source == src_command)
		return Cmd_ForwardToServer();

	if (gGlobalVariables.deathmatch || !sv_cheats.value)
		return;

	if (sv_player->v.movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		noclip_anglehack = true;
		return SV_ClientPrintf("noclip ON\n");
	}

	sv_player->v.movetype = MOVETYPE_WALK;
	noclip_anglehack = false;
	VectorCopy(sv_player->v.origin, sv_player->v.oldorigin);
	SV_ClientPrintf("noclip OFF\n");

	if (SV_TestEntityPosition(sv_player))
	{
		AngleVectors(sv_player->v.v_angle, vforward, right, up);

		if (!FindPassableSpace(sv_player, vforward, 1.0)
			&& !FindPassableSpace(sv_player, right, 1.0)
			&& !FindPassableSpace(sv_player, right, -1.0)
			&& !FindPassableSpace(sv_player, up, 1.0)
			&& !FindPassableSpace(sv_player, up, -1.0)
			&& !FindPassableSpace(sv_player, vforward, -1.0))
		{
			Con_DPrintf(const_cast<char*>("Can't find the world\n"));
		}

		VectorCopy(sv_player->v.oldorigin, sv_player->v.origin);
	}
}

edict_t* FindViewthing(void)
{
	int		i;
	edict_t* e;
	for (i = 0; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM(i);
		if (!strcmp(pr_strings + e->v.classname, "viewthing"))
			return e;
	}
	Con_Printf(const_cast<char*>("No viewthing on map\n"));
	return NULL;
}
/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f(void)
{
	edict_t* e;
	model_t* m;
	e = FindViewthing();
	if (!e)
		return;
	m = Mod_ForName(Cmd_Argv(1), false, false);
	if (!m)
	{
		Con_Printf(const_cast<char*>("Can't load %s\n"), Cmd_Argv(1));
		return;
	}
	e->v.frame = 0;
	cl.model_precache[e->v.modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f(void)
{
	edict_t* e;
	int		f;
	model_t* m;
	e = FindViewthing();
	if (!e)
		return;
	m = CL_GetModelByIndex(e->v.modelindex);
	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes - 1;
	e->v.frame = f;
}

void PrintFrameName(model_t* m, int frame)
{
	aliashdr_t* hdr;
	maliasframedesc_t* pframedesc;
	hdr = (aliashdr_t*)Mod_Extradata(m);
	if (!hdr)
		return;
	pframedesc = &hdr->frames[frame];
	Con_Printf(const_cast<char*>("frame %i: %s\n"), frame, pframedesc->name);
}
/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f(void)
{
	edict_t* e;
	model_t* m;
	e = FindViewthing();
	if (!e)
		return;
	m = CL_GetModelByIndex(e->v.modelindex);
	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;
	PrintFrameName(m, (int)e->v.frame);
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f(void)
{
	edict_t* e;
	model_t* m;
	e = FindViewthing();
	if (!e)
		return;
	m = CL_GetModelByIndex(e->v.modelindex);
	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;
	PrintFrameName(m, (int)e->v.frame);
}


void Host_InitCommands()
{
	if (!g_bIsDedicatedServer)
	{
		Cmd_AddCommandWithFlags(const_cast<char*>("cd"), CD_Command_f, CMD_PRIVILEGED);
		Cmd_AddCommand(const_cast<char*>("mp3"), MP3_Command_f);
		Cmd_AddCommand(const_cast<char*>("_careeraudio"), CareerAudio_Command_f);
	}

	Cmd_AddCommand(const_cast<char*>("shutdownserver"), Host_KillServer_f);
	Cmd_AddCommand(const_cast<char*>("soundfade"), Host_Soundfade_f);
	Cmd_AddCommand(const_cast<char*>("status"), Host_Status_f);
	Cmd_AddCommand(const_cast<char*>("stat"), Host_Status_Formatted_f);
	Cmd_AddCommandWithFlags( const_cast<char*>("quit"), Host_Quit_f, CMD_PRIVILEGED );


	if (!g_bIsDedicatedServer)
	{
		Cmd_AddCommandWithFlags(const_cast<char*>("_restart"), Host_Quit_Restart_f, CMD_PRIVILEGED);
		Cmd_AddCommandWithFlags(const_cast<char*>("_setrenderer"), Host_SetRenderer_f, CMD_PRIVILEGED);
		Cmd_AddCommandWithFlags(const_cast<char*>("_setvideomode"), Host_SetVideoMode_f, CMD_PRIVILEGED);
		Cmd_AddCommandWithFlags(const_cast<char*>("_setgamedir"), Host_SetGameDir_f, CMD_PRIVILEGED);
		Cmd_AddCommandWithFlags(const_cast<char*>("_sethdmodels"), Host_SetHDModels_f, CMD_PRIVILEGED);
		Cmd_AddCommandWithFlags(const_cast<char*>("_setaddons_folder"), Host_SetAddonsFolder_f, CMD_PRIVILEGED);
		Cmd_AddCommandWithFlags(const_cast<char*>("_set_vid_level"), Host_SetVideoLevel_f, CMD_PRIVILEGED);
	}

	Cmd_AddCommand( const_cast<char*>("exit"), Host_Quit_f );
	Cmd_AddCommand(const_cast<char*>("map"), Host_Map_f);
	Cmd_AddCommand(const_cast<char*>("career"), Host_Career_f);
	Cmd_AddCommand(const_cast<char*>("maps"), Host_Maps_f);
	Cmd_AddCommand(const_cast<char*>("restart"), Host_Restart_f);
	Cmd_AddCommand(const_cast<char*>("reload"), Host_Reload_f);
	Cmd_AddCommand(const_cast<char*>("changelevel"), Host_Changelevel_f);
	Cmd_AddCommand(const_cast<char*>("changelevel2"), Host_Changelevel2_f);
	Cmd_AddCommand(const_cast<char*>("reconnect"), Host_Reconnect_f);
	Cmd_AddCommand(const_cast<char*>("version"), Host_Version_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("say"), Host_Say_f, CMD_PRIVILEGED);
	Cmd_AddCommand(const_cast<char*>("say_team"), Host_Say_Team_f);
	Cmd_AddCommand(const_cast<char*>("tell"), Host_Tell_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("kill"), Host_Kill_f, CMD_PRIVILEGED);
	Cmd_AddCommand(const_cast<char*>("pause"), Host_TogglePause_f);
	Cmd_AddCommand(const_cast<char*>("setpause"), Host_Pause_f);
	Cmd_AddCommand(const_cast<char*>("unpause"), Host_Unpause_f);
	Cmd_AddCommand(const_cast<char*>("kick"), Host_Kick_f);
	Cmd_AddCommand(const_cast<char*>("ping"), Host_Ping_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("motd"), Host_Motd_f, CMD_PRIVILEGED);
	Cmd_AddCommandWithFlags(const_cast<char*>("motd_write"), Host_Motd_Write_f, CMD_PRIVILEGED);
	Cmd_AddCommand(const_cast<char*>("stats"), Host_Stats_f);
	Cmd_AddCommand(const_cast<char*>("load"), Host_Loadgame_f);
	Cmd_AddCommand(const_cast<char*>("save"), Host_Savegame_f);
	Cmd_AddCommand(const_cast<char*>("autosave"), Host_AutoSave_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("writecfg"), Host_WriteCustomConfig, CMD_PRIVILEGED);

	if (!g_bIsDedicatedServer)
	{
		Cmd_AddCommand(const_cast<char*>("+voicerecord"), Host_VoiceRecordStart_f);
		Cmd_AddCommand(const_cast<char*>("-voicerecord"), Host_VoiceRecordStop_f);
	}

	Cmd_AddCommand(const_cast<char*>("startdemos"), Host_Startdemos_f);
	Cmd_AddCommand(const_cast<char*>("demos"), Host_Demos_f);
	Cmd_AddCommand(const_cast<char*>("stopdemo"), Host_Stopdemo_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("setinfo"), Host_SetInfo_f, CMD_PRIVILEGED);
	Cmd_AddCommand(const_cast<char*>("fullinfo"), Host_FullInfo_f);

	Cmd_AddCommand(const_cast<char*>("god"), Host_God_f);
	Cmd_AddCommand(const_cast<char*>("notarget"), Host_Notarget_f);
	Cmd_AddCommand(const_cast<char*>("fly"), Host_Fly_f);
	Cmd_AddCommand(const_cast<char*>("noclip"), Host_Noclip_f);

	if (!g_bIsDedicatedServer)
	{
		Cmd_AddCommand(const_cast<char*>("viewmodel"), Host_Viewmodel_f);
		Cmd_AddCommand(const_cast<char*>("viewframe"), Host_Viewframe_f);
		Cmd_AddCommand(const_cast<char*>("viewnext"), Host_Viewnext_f);
		Cmd_AddCommand(const_cast<char*>("viewprev"), Host_Viewprev_f);
	}

	Cmd_AddCommand(const_cast<char*>("mcache"), Mod_Print);
	Cmd_AddCommand(const_cast<char*>("interp"), Host_Interp_f);
	Cmd_AddCommand(const_cast<char*>("setmaster"), Master_SetMaster_f);
	Cmd_AddCommand(const_cast<char*>("heartbeat"), Master_Heartbeat_f);

	Cvar_RegisterVariable(&gHostMap);
	Cvar_RegisterVariable(&voice_recordtofile);
	Cvar_RegisterVariable(&voice_inputfromfile);
}

void Host_UpdateStats(void)
{
	uint32 runticks = 0;
	uint32 cputicks = 0;

	static float last = 0.0f;
	static float lastAvg = 0.0f;

	static uint64 lastcputicks = 0;
	static uint64 lastrunticks = 0;

#ifdef _WIN32

	struct _FILETIME ExitTime;
	struct _FILETIME UserTime;
	struct _FILETIME KernelTime;
	struct _FILETIME CreationTime;
	struct _FILETIME SystemTimeAsFileTime;

	if (!startTime)
		startTime = Sys_FloatTime();

	if (last + 1.0 < Sys_FloatTime())
	{
		GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime);
		GetSystemTimeAsFileTime(&SystemTimeAsFileTime);

		if (!lastcputicks)
		{
			cputicks = CreationTime.dwLowDateTime;
			runticks = CreationTime.dwHighDateTime;

			lastcputicks = FILETIME_TO_QWORD(CreationTime);
		}
		else
		{
			cputicks = (uint32)(lastcputicks & 0xFFFFFFFF);
			runticks = (uint32)(lastcputicks >> 32);
		}

		cpuPercent =
			(FILETIME_TO_QWORD(UserTime) + FILETIME_TO_QWORD(KernelTime) - lastrunticks)
			/ (FILETIME_TO_QWORD(SystemTimeAsFileTime)
			- (double)FILETIME_TO_PAIR(runticks, cputicks));

		if (lastAvg + 5.0f < Sys_FloatTime())
		{
			lastcputicks = FILETIME_TO_QWORD(SystemTimeAsFileTime);
			lastrunticks = FILETIME_TO_QWORD(UserTime) + FILETIME_TO_QWORD(KernelTime);
			lastAvg = last;
		}
		last = Sys_FloatTime();
	}

#else // _WIN32

	FILE *pFile;
	int32 dummy;
	int32 ctime;
	int32 stime;
	int32 start_time;
	char statFile[4096];
	struct sysinfo infos;

	if (!startTime)
		startTime = Sys_FloatTime();

	if (Sys_FloatTime() > last + 1.0f)
	{
		time(NULL);
		pid_t pid = getpid();
		Q_snprintf(statFile, sizeof(statFile), "/proc/%i/stat", pid);
		pFile = fopen(statFile, "rt");
		if (!pFile)
		{
			last = Sys_FloatTime();
			return;
		}
		sysinfo(&infos);
		fscanf(pFile, "%d %s %c %d %d %d %d %d %lu %lu \t\t\t%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu \t\t\t%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu \t\t\t%lu %lu %lu %lu %lu %lu",
			&dummy,
			statFile,
			&dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
			&ctime,
			&stime,
			&dummy, &dummy, &dummy, &dummy, &dummy,
			&start_time,
			&dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
			&dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy
			);
		fclose(pFile);

		runticks = 100 * infos.uptime - start_time;
		cputicks = ctime + stime;

		if (!lastcputicks)
			lastcputicks = cputicks;

		if (lastrunticks)
			cpuPercent = (double)(cputicks - lastcputicks) / (double)(runticks - lastrunticks);
		else
			lastrunticks = runticks;

		if (lastAvg + 5.0f < Sys_FloatTime())
		{
			lastcputicks = cputicks;
			lastrunticks = runticks;
			lastAvg = Sys_FloatTime();
		}
		if (cpuPercent > 0.999)
			cpuPercent = 0.999;
		else if (cpuPercent < 0.0)
			cpuPercent = 0.0;
		last = Sys_FloatTime();
	}

#endif // _WIN32
}

void Host_SavegameComment(char *pszBuffer, int iSizeBuffer)
{
	int i;
	const char *pszName = NULL;
	const char *pszMapName = (const char *)&pr_strings[gGlobalVariables.mapname];

	for (i = 0; i < ARRAYSIZE(gTitleComments) && !pszName; i++)
	{
		if (!Q_strncasecmp(pszMapName, gTitleComments[i].pBSPName, Q_strlen(gTitleComments[i].pBSPName)))
			pszName = gTitleComments[i].pTitleName;
	}
	if (!pszName)
	{
		if (!pszMapName || !pszMapName[0])
		{
			pszName = pszMapName;
			if (!Q_strlen(cl.levelname))
				pszName = cl.levelname;
		}
	}
	Q_strncpy(pszBuffer, pszName, iSizeBuffer - 1);
	pszBuffer[iSizeBuffer - 1] = 0;
}
