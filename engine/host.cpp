#include <ctime>

#include "quakedef.h"
#include "buildnum.h"
#include "cd.h"
#include "cdll_int.h"
#include "chase.h"
#include "cl_main.h"
#include "cl_parsefn.h"
#include "client.h"
#include "cmodel.h"
#include "decals.h"
#include "delta.h"
#include "DemoPlayerWrapper.h"
#include "dll_state.h"

#if defined(GLQUAKE)
#include "gl_draw.h"
#include "gl_model.h"
#include "gl_rmisc.h"
#include "gl_screen.h"
#include "gl_rmain.h"
#include "gl_vidnt.h"
#else
#include "model.h"
#include "draw.h"
#include "screen.h"
#include "r_local.h"
#include "render.h"
#endif

#include "hashpak.h"
#include "host.h"
#include "host_cmd.h"
#include "net_chan.h"
#include "pmove.h"
#include "qgl.h"
#include "server.h"
#include "sound.h"
#include "sv_main.h"
#include "sv_upld.h"
#include "SystemWrapper.h"
#include "vgui_int.h"
#include "view.h"
#include "voice.h"
#include "wad.h"
#include "sv_remoteaccess.h"
#include "cl_ents.h"
#include "download.h"
#include "cl_demo.h"
#include "cl_spectator.h"
#include "cl_pred.h"
#include "sv_log.h"
#include "../cl_dll/kbutton.h"
#include "pr_edict.h"
#include "cl_parse.h"
#include <io.h>

quakeparms_t host_parms = {};

jmp_buf host_abortserver;
jmp_buf host_enddemo;

// смерть тебе хуйло которое рекламу в картах придумало
qboolean g_bUsingInGameAdvertisements;

bool host_initialized = false;
double realtime = 0;
double oldrealtime = 0;
double host_frametime = 0;
double rolling_fps = 0;
int host_framecount;

client_t* host_client = nullptr;

cvar_t console = { const_cast<char*>("console"), const_cast<char*>("0.0"), FCVAR_ARCHIVE };

static cvar_t host_profile = { const_cast<char*>("host_profile"), const_cast<char*>("0") };

cvar_t fps_max = { const_cast<char*>("fps_max"), const_cast<char*>("100.0"), FCVAR_ARCHIVE };
cvar_t fps_override = { const_cast<char*>("fps_override"), const_cast<char*>("0") };

cvar_t host_framerate = { const_cast<char*>("host_framerate"), const_cast<char*>("0") };

cvar_t sys_ticrate = { const_cast<char*>("sys_ticrate"), const_cast<char*>("100.0") };
cvar_t sys_timescale = { const_cast<char*>("sys_timescale"), const_cast<char*>("1.0") };

cvar_t developer = { const_cast<char*>("developer"), const_cast<char*>("0") };

cvar_t deathmatch = { const_cast<char*>("deathmatch"), const_cast<char*>("0"), FCVAR_SERVER };
cvar_t coop = { const_cast<char*>("coop"), const_cast<char*>("0"), FCVAR_SERVER };

cvar_t host_limitlocal = { const_cast<char*>("host_limitlocal"), const_cast<char*>("0") };

cvar_t skill = { const_cast<char*>("skill"), const_cast<char*>("1") };

cvar_t host_killtime = { const_cast<char*>("host_killtime"), const_cast<char*>("0") };

cvar_t sv_stats = { const_cast<char*>("sv_stats"), const_cast<char*>("1") };

cvar_t host_speeds = { const_cast<char*>("host_speeds"), const_cast<char*>("0") };

cvar_t host_name = { const_cast<char*>("hostname"), const_cast<char*>("Half-Life") };
cvar_t pausable = { const_cast<char*>("pausable"), const_cast<char*>("1"), FCVAR_SERVER };

unsigned short* host_basepal = nullptr;
unsigned char* host_colormap = nullptr;

qboolean s_careerAudioPaused;
qboolean gfNoMasterServer;

int host_hunklevel = 0;

int32 startTime;
double cpuPercent;

vec3_t r_playerViewportAngles, r_soundOrigin;

const char* gpszAppName = "Half-Life";

void DI_HandleEvents();
void Info_WriteVars(FileHandle_t fp);
void Host_UpdateSounds();

void Host_InitLocal()
{
	Host_InitCommands();
	
	Cvar_RegisterVariable( &host_killtime );
	Cvar_RegisterVariable( &sys_ticrate );
	Cvar_RegisterVariable( &fps_max );
	Cvar_RegisterVariable( &fps_override );
	Cvar_RegisterVariable( &host_name );
	Cvar_RegisterVariable( &host_limitlocal );
	sys_timescale.value = 1;
	Cvar_RegisterVariable( &host_framerate );
	Cvar_RegisterVariable( &host_speeds );
	Cvar_RegisterVariable( &host_profile );
	
	Cvar_RegisterVariable( &mp_logfile );
	Cvar_RegisterVariable( &mp_logecho );
	Cvar_RegisterVariable( &sv_log_onefile );
	Cvar_RegisterVariable( &sv_log_singleplayer );
	Cvar_RegisterVariable( &sv_logsecret );
	
	Cvar_RegisterVariable( &sv_stats );
	
	Cvar_RegisterVariable( &developer );
	
	Cvar_RegisterVariable( &deathmatch );
	Cvar_RegisterVariable( &coop );
	Cvar_RegisterVariable( &pausable );
	Cvar_RegisterVariable( &skill );
	

	SV_SetMaxclients();
}

void Host_WriteConfiguration(void)
{
	FILE* f;
	kbutton_t* ml;
	kbutton_t* jl;
	qboolean bSetFileToReadOnly;
	char nameBuf[4096];

	if (!host_initialized || cls.state == ca_dedicated)
		return;

#ifdef _WIN32
	Sys_SetRegKeyValue("Software\\Valve\\Steam", "rate", rate.string);
	if (cl_name.string && Q_strcasecmp(cl_name.string, "unnamed") && Q_strcasecmp(cl_name.string, "player") && Q_strlen(cl_name.string))
		Sys_SetRegKeyValue("Software\\Valve\\Steam", "LastGameNameUsed", cl_name.string);
#else
	SetRateRegistrySetting(rate_.string);
#endif
	if (Key_CountBindings() <= 1)
	{
		Con_Printf(const_cast<char*>("skipping config.cfg output, no keys bound\n"));
		return;
	}

	bSetFileToReadOnly = FALSE;
	f = (FILE*)FS_OpenPathID("config.cfg", "w", "GAMECONFIG");
	if (!f)
	{
		if (!developer.value || !FS_FileExists("../goldsrc/dev_build_all.bat"))
		{
			if (FS_GetLocalPath("config.cfg", nameBuf, sizeof(nameBuf)))
			{
				bSetFileToReadOnly = TRUE;
				_chmod(nameBuf, S_IREAD | S_IWRITE);
			}
			f = (FILE*)FS_OpenPathID("config.cfg", "w", "GAMECONFIG");
			if (!f)
			{
				Con_Printf(const_cast<char*>("Couldn't write config.cfg.\n"));
				return;
			}
		}
	}

	FS_FPrintf(f, "// This file is overwritten whenever you change your user settings in the game.\n");
	FS_FPrintf(f, "// Add custom configurations to the file \"userconfig.cfg\".\n\n");
	FS_FPrintf(f, "unbindall\n");

	Key_WriteBindings(f);
	Cvar_WriteVariables(f);
	Info_WriteVars(f);

	ml = ClientDLL_FindKey("in_mlook");
	jl = ClientDLL_FindKey("in_jlook");

	if (ml && (ml->state & 1))
		FS_FPrintf(f, "+mlook\n");

	if (jl && (jl->state & 1))
		FS_FPrintf(f, "+jlook\n");

	FS_FPrintf(f, "exec userconfig.cfg\n");
	FS_Close(f);

	if (bSetFileToReadOnly)
	{
		FS_GetLocalPath("config.cfg", nameBuf, sizeof(nameBuf));
		chmod(nameBuf, S_IREAD);
	}
}

void Host_Error( const char* error, ... )
{
	static bool inerror = false;

	char string[ 1024 ];
	va_list va;

	va_start( va, error );

	if( inerror == false )
	{
		inerror = true;

		SCR_EndLoadingPlaque();

		vsnprintf( string, ARRAYSIZE( string ), error, va );

		
		if( !sv.active && developer.value != 0.0 )
			CL_WriteMessageHistory( 0, 0 );
			

		Con_Printf( const_cast<char*>("Host_Error: %s\n"), string );

		
		if( sv.active )
			Host_ShutdownServer(true);

		if( cls.state != ca_active )
		{
			extern void DbgPrint(FILE*, const char* format, ...);
			extern FILE* m_fMessages;
			DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
			DbgPrint(m_fMessages, "with error %s\r\n", string);
			CL_Disconnect();
			cls.demonum = -1;
			inerror = false;
			longjmp( host_abortserver, 1 );
		}
		

		Sys_Error( "Host_Error: %s\n", string );
	}

	va_end( va );

	Sys_Error( "Host_Error: recursively entered" );
}

void CheckGore()
{
	char szSubKey[128];
	int nBufferLen;
	char szBuffer[128];
	qboolean bReducedGore = false;

	Q_memset(szBuffer, 0, 128);

#if defined(_WIN32)
	_snprintf(szSubKey, sizeof(szSubKey), "Software\\Valve\\%s\\Settings", gpszAppName);

	nBufferLen = 127;
	Q_strcpy(szBuffer, "");

	Sys_GetRegKeyValue(szSubKey, "User Token 2", szBuffer, nBufferLen, szBuffer);

	// Gore reduction active?
	bReducedGore = (Q_strlen(szBuffer) > 0) ? true : false;
	if (!bReducedGore)
	{
		Sys_GetRegKeyValue(szSubKey, "User Token 3", szBuffer, nBufferLen, szBuffer);

		bReducedGore = (Q_strlen(szBuffer) > 0) ? true : false;
	}

	// also check mod specific directories for LV changes
	_snprintf(szSubKey, sizeof(szSubKey), "Software\\Valve\\%s\\%s\\Settings", gpszAppName, com_gamedir);

	nBufferLen = 127;
	Q_strcpy(szBuffer, "");

	Sys_GetRegKeyValue(szSubKey, "User Token 2", szBuffer, nBufferLen, szBuffer);
	if (Q_strlen(szBuffer) > 0)
	{
		bReducedGore = true;
	}

	Sys_GetRegKeyValue(szSubKey, "User Token 3", szBuffer, nBufferLen, szBuffer);
	if (Q_strlen(szBuffer) > 0)
	{
		bReducedGore = true;
	}

#endif

	if( bReducedGore || bLowViolenceBuild )
	{
		Cvar_SetValue( const_cast<char*>("violence_hblood"), 0 );
		Cvar_SetValue( const_cast<char*>("violence_hgibs"), 0 );
		Cvar_SetValue( const_cast<char*>("violence_ablood"), 0 );
		Cvar_SetValue( const_cast<char*>("violence_agibs"), 0 );
	}
	else
	{
		Cvar_SetValue( const_cast<char*>("violence_hblood"), 1 );
		Cvar_SetValue( const_cast<char*>("violence_hgibs"), 1 );
		Cvar_SetValue( const_cast<char*>("violence_ablood"), 1 );
		Cvar_SetValue( const_cast<char*>("violence_agibs"), 1 );
	}
}

void Host_Version()
{
	Q_strcpy( gpszVersionString, "1.0.1.4" );
	Q_strcpy( gpszProductString, "valve" );

	char szFileName[ FILENAME_MAX ];

	strcpy( szFileName, "steam.inf" );

	FileHandle_t hFile = FS_Open( szFileName, "r" );

	if( hFile != FILESYSTEM_INVALID_HANDLE )
	{
		const int iSize = FS_Size( hFile );
		void* pFileData = Mem_Malloc( iSize + 1 );
		FS_Read( pFileData, iSize, 1, hFile );
		FS_Close( hFile );

		char* pBuffer = reinterpret_cast<char*>( pFileData );

		pBuffer[ iSize ] = '\0';

		const int iProductNameLength = Q_strlen( "ProductName=" );
		const int iPatchVerLength = Q_strlen( "PatchVersion=" );

		char szSteamVersionId[ 32 ];

		//Parse out the version and name.
		for( int i = 0; ( pBuffer = COM_Parse( pBuffer ) ) != nullptr && *com_token && i < 2; )
		{
			if( !strnicmp( com_token, "PatchVersion=", iPatchVerLength ) )
			{
				++i;

				Q_strncpy( gpszVersionString, &com_token[ iPatchVerLength ], ARRAYSIZE( gpszVersionString ) );

				if( COM_CheckParm( const_cast<char*>("-steam") ) )
				{
					FS_GetInterfaceVersion( szSteamVersionId, ARRAYSIZE( szSteamVersionId ) - 1 );
					_snprintf( gpszVersionString, ARRAYSIZE( gpszVersionString ), "%s/%s", &com_token[ iPatchVerLength ], szSteamVersionId );
				}
			}
			else if( !strnicmp( com_token, "ProductName=", iProductNameLength ) )
			{
				++i;
				Q_strncpy( gpszProductString, &com_token[ iProductNameLength ], ARRAYSIZE( gpszProductString ) );
			}
		}

		if( pFileData )
			Mem_Free( pFileData );
	}

	if( cls.state != ca_dedicated )
	{
		Con_DPrintf( const_cast<char*>("Protocol version %i\nExe version %s (%s)\n"), PROTOCOL_VERSION, gpszVersionString, gpszProductString );
		Con_DPrintf( const_cast<char*>("Exe build: " __TIME__ " " __DATE__ " (%i)\n"), build_number() );
	}
	else
	{
		Con_Printf( const_cast<char*>("Protocol version %i\nExe version %s (%s)\n"), PROTOCOL_VERSION, gpszVersionString, gpszProductString );
		Con_Printf( const_cast<char*>("Exe build: " __TIME__ " " __DATE__ " (%i)\n"), build_number() );
	}
}

bool Host_Init( quakeparms_t* parms )
{
	srand( time( nullptr ) );

	host_parms = *parms;

	realtime = 0;

	com_argc = parms->argc;
	com_argv = parms->argv;

	Memory_Init( parms->membase, parms->memsize );


	Voice_RegisterCvars();
	Cvar_RegisterVariable( &console );

	
	if( COM_CheckParm( const_cast<char*>("-console") ) || COM_CheckParm( const_cast<char*>("-toconsole") ) || COM_CheckParm( const_cast<char*>("-dev") ) )
		Cvar_DirectSet( &console, const_cast<char*>("1.0") );

	Host_InitLocal();

	
	if( COM_CheckParm( const_cast<char*>("-dev") ) )
		Cvar_SetValue( const_cast<char*>("developer"), 1.0 );

	Cbuf_Init();

	Cmd_Init();

	Cvar_Init();

	Cvar_CmdInit();

	V_Init();

	Chase_Init();

	COM_Init();

	Host_ClearSaveDirectory();
	HPAK_Init();


	W_LoadWadFile( "gfx.wad" );
	W_LoadWadFile( "fonts.wad" );

	Key_Init();
	Con_Init();

	Decal_Init();
	Mod_Init();

	NET_Init();
	Netchan_Init();

	DELTA_Init();

	SV_Init();

	SystemWrapper_Init();
	Host_Version();


	char versionString[ 256 ];
	_snprintf( versionString, ARRAYSIZE( versionString ), "%s,%i,%i", gpszVersionString, PROTOCOL_VERSION, build_number() );

	Cvar_Set( const_cast<char*>("sv_version"), versionString );

	Con_DPrintf( const_cast<char*>("%4.1f Mb heap\n"), parms->memsize / (1024 * 1024.0 ) );

	R_InitTextures();
	HPAK_CheckIntegrity( "custom" );

	Q_memset( &g_module, 0, sizeof( g_module ) );

	if( cls.state != ca_dedicated )
	{
		byte* pPalette = COM_LoadHunkFile( const_cast<char*>("gfx/palette.lmp") );

		if( !pPalette )
			Sys_Error( "Host_Init: Couldn't load gfx/palette.lmp" );

		byte* pSource = pPalette;

		host_basepal = reinterpret_cast<unsigned short*>(Hunk_AllocName(256 * sizeof(PackedColorVec), const_cast<char*>("palette.lmp")));

		for( int i = 0; i < 256; ++i, pSource += 3 )
		{
			host_basepal[ ( 4 * i ) ] = *( pSource + 2 );
			host_basepal[ ( 4 * i ) + 1 ] = *( pSource + 1 );
			host_basepal[ ( 4 * i ) + 2 ] = *pSource;
			host_basepal[ ( 4 * i ) + 3 ] = 0;
		}

#ifdef GLQUAKE
		GL_Init();
#endif
		PM_Init( &g_clmove );

		CL_InitEventSystem();
		ClientDLL_Init();

		VGui_Startup();

		if( !VID_Init( host_basepal ) )
		{
			VGui_Shutdown();
			return false;
		}


		Draw_Init();

		SCR_Init();

		R_Init();

		S_Init();

		CDAudio_Init();

		Voice_Init( "voice_speex", 1 );
		DemoPlayer_Init();

		CL_Init();
	}
	else
	{
		Cvar_RegisterVariable( &suitvolume );
	}

	Cbuf_InsertText( const_cast<char*>("exec valve.rc\n") );

#if defined(GLQUAKE)
	if( cls.state != ca_dedicated )
		GL_Config();
#endif


	Hunk_AllocName( 0, const_cast<char*>("-HOST_HUNKLEVEL-") );
	host_hunklevel = Hunk_LowMark();

	giActive = DLL_ACTIVE;
	scr_skipupdate = false;

	CheckGore();

	host_initialized = true;

	return true;
}

void Host_Shutdown()
{
	static bool isdown = false;

	if( isdown )
	{
		puts( "recursive shutdown" );
	}
	else
	{
		isdown = true;

		if( host_initialized )
			Host_WriteConfiguration();

		SV_ServerShutdown();
		Voice_Deinit();

		host_initialized = false;

		CDAudio_Shutdown();
		VGui_Shutdown();

		if( cls.state != ca_dedicated )
			ClientDLL_Shutdown();

		Cmd_RemoveGameCmds();
		Cmd_Shutdown();
		Cvar_Shutdown();

		HPAK_FlushHostQueue();
		SV_DeallocateDynamicData();

		for( int i = 0; i < svs.maxclientslimit; ++i )
		{
			SV_ClearFrames( &svs.clients[ i ].frames );
		}

		SV_Shutdown();
		SystemWrapper_ShutDown();

		NET_Shutdown();
		S_Shutdown();
		Con_Shutdown();

		ReleaseEntityDlls();

		CL_ShutDownClientStatic();
		CM_FreePAS();

		if( wadpath )
		{
			Mem_Free( wadpath );
			wadpath = nullptr;
		}

		if( cls.state != ca_dedicated )
			Draw_Shutdown();

		Draw_DecalShutdown();

		W_Shutdown();

		Log_Printf( "Server shutdown\n" );
		Log_Close();

		COM_Shutdown();
		CL_Shutdown();
		DELTA_Shutdown();

		Key_Shutdown();

		realtime = 0;

		sv.time = 0;
		cl.time = 0;
	}
}

bool Host_FilterTime( float time )
{
	if( host_framerate.value > 0 )
	{
		if( ( sv.active && svs.maxclients == 1 ) ||
			( cl.maxclients == 1 ) ||
			cls.demoplayback )
		{
			host_frametime = host_framerate.value * sys_timescale.value;
			realtime = host_frametime + realtime;
			return true;
		}
	}

	realtime += time * sys_timescale.value;

	const double flDelta = realtime - oldrealtime;

	if( g_bIsDedicatedServer )
	{
		static int command_line_ticrate = -1;

		if( command_line_ticrate == -1 )
		{
			command_line_ticrate = COM_CheckParm( const_cast<char*>("-sys_ticrate") );
		}

		double flTicRate = sys_ticrate.value;

		if( command_line_ticrate > 0 )
		{
			flTicRate = strtod( com_argv[ command_line_ticrate + 1 ], nullptr );
		}

		if( flTicRate > 0.0 )
		{
			if( ( 1.0 / ( flTicRate + 1.0 ) ) > flDelta )
				return false;
		}
	}
	else
	{
		double flFPSMax;

		if( sv.active || cls.state == ca_disconnected || cls.state == ca_active )
		{
			flFPSMax = 0.5;
			if( fps_max.value >= 0.5 )
				flFPSMax = fps_max.value;
		}
		else
		{
			flFPSMax = 31.0;
		}

		if( !fps_override.value )
		{
			if( flFPSMax > 100.0 )
				flFPSMax = 100.0;
		}

		if( cl.maxclients > 1 )
		{
			if( flFPSMax < 20.0 )
				flFPSMax = 20.0;
		}

		if( gl_vsync.value )
		{
			if( !fps_override.value )
				flFPSMax = 100.0;
		}

		if( !cls.timedemo )
		{
			if( sys_timescale.value / ( flFPSMax + 0.5 ) > flDelta )
				return false;
		}
	}

	host_frametime = flDelta;
	oldrealtime = realtime;

	if( flDelta > 0.25 )
	{
		host_frametime = 0.25;
	}

	return true;
}

void Host_ComputeFPS(double frametime)
{
	rolling_fps = 0.6 * rolling_fps + 0.4 * frametime;
}

void Host_CheckConnectionFailure()
{
	static int frames = 5;

	if (cls.state == ca_disconnected && !scr_connectmsg.string[0] && (giSubState & 4) || !console.value)
	{
		if (frames-- > 0)
			return;

		giActive = 2;
	}

	frames = 5;
}

void Host_UpdateScreen()
{
	if (gfBackground == false)
	{
		SCR_UpdateScreen();
		if (cl_inmovie)
		{
			if (scr_con_current == 0.0)
				VID_WriteBuffer(0);
		}
	}
}

void CL_AddVoiceToDatagram(qboolean bFinal)
{
	if (cls.state != ca_active || !Voice_IsRecording())
		return;

	// Get whatever compressed data there is and and send it.
	char uchVoiceData[2048];
	int nDataLength = Voice_GetCompressedData(uchVoiceData, sizeof(uchVoiceData), bFinal);
	if (!nDataLength)
		return;

	if (cls.datagram.maxsize <= (cls.datagram.cursize + 3 + nDataLength))
		return;

	MSG_WriteByte(&cls.datagram, clc_voicedata);
	MSG_WriteShort(&cls.datagram, nDataLength);
	MSG_WriteBuf(&cls.datagram, nDataLength, uchVoiceData);
}

void CL_VoiceIdle()
{
	Voice_Idle(host_frametime);
}

void Host_Speeds(double *time)
{
	float pass1, pass2, pass3, pass4, pass5;
	double frameTime;
	double fps;


	pass1 = (float)((time[1] - time[0]) * 1000.0);
	pass2 = (float)((time[2] - time[1]) * 1000.0);
	pass3 = (float)((time[3] - time[2]) * 1000.0);
	pass4 = (float)((time[4] - time[3]) * 1000.0);
	pass5 = (float)((time[5] - time[4]) * 1000.0);

	frameTime = (pass5 + pass4 + pass3 + pass2 + pass1) / 1000.0;

	if (frameTime >= 0.0001)
		fps = 1.0 / frameTime;
	else
		fps = 999.0;
	

	if (host_speeds.value != 0.0f)
	{
		int ent_count = 0;
		for (int i = 0; i < sv.num_edicts; i++)
		{
			if (!sv.edicts[i].free)
				ent_count++;
		}
		Con_Printf(const_cast<char*>("%3i fps -- host(%3.0f) sv(%3.0f) cl(%3.0f) gfx(%3.0f) snd(%3.0f) ents(%d)\n"), (int)fps, pass1, pass2, pass3, pass4, pass5, ent_count);
	}

	if (cl_gamegauge.value != 0.0f)
	{
		CL_GGSpeeds(time[3]);
	}
}

void _Host_Frame( float time )
{
	static double host_times[6];

	if( setjmp( host_enddemo )) 
		return;


	DI_HandleEvents();

	if( !Host_FilterTime( time ) )
		return;


	SystemWrapper_RunFrame( host_frametime );

	if( g_modfuncs.m_pfnFrameBegin )
		g_modfuncs.m_pfnFrameBegin();

	
	Host_ComputeFPS(host_frametime);
	R_SetStackBase();

	
	CL_CheckClientState();


	Cbuf_Execute();

	ClientDLL_UpdateClientData();

	if (sv.active)
		CL_Move();

	
	host_times[1] = Sys_FloatTime();

	SV_Frame();

	
	host_times[2] = Sys_FloatTime();

	SV_CheckForRcon();

	
	if (!sv.active)
		CL_Move();

	ClientDLL_Frame(host_frametime);

	
	CL_SetLastUpdate();

	CL_ReadPackets();

	CL_RedoPrediction();

	CL_VoiceIdle();

	
	CL_EmitEntities();

	CL_CheckForResend();

	
	 while (CL_RequestMissingResources())
	 ;

	CL_UpdateSoundFade();

	
	Host_CheckConnectionFailure();

	CL_HTTPUpdate();

	
	Steam_ClientRunFrame();

	ClientDLL_CAM_Think();

	
	CL_MoveSpectatorCamera();

	host_times[3] = Sys_FloatTime();

	Host_UpdateScreen();

	host_times[4] = Sys_FloatTime();

	

	CL_DecayLights();
	Host_UpdateSounds();

	
	host_times[0] = host_times[5];
	host_times[5] = Sys_FloatTime();

	Host_Speeds(host_times);

	host_framecount++;

	CL_AdjustClock();

	if (sv_stats.value == 1.0)
		Host_UpdateStats();

	if (host_killtime.value != 0.0 && sv.time > host_killtime.value)
		Host_Quit_f();

}

int Host_Frame( float time, int iState, int* stateInfo )
{
	if( setjmp( host_abortserver ) )
	{
		return giActive;
	}

	if( giActive != DLL_CLOSE || !g_iQuitCommandIssued )
		giActive = iState;

	*stateInfo = 0;

	double time1, time2;


	if( host_profile.value )
		time1 = Sys_FloatTime();

	_Host_Frame( time );

	if( host_profile.value )
		time2 = Sys_FloatTime();

	if( giStateInfo )
	{
		*stateInfo = giStateInfo;
		giStateInfo = 0;
		Cbuf_Execute();
	}

	if( host_profile.value )
	{
		static double timetotal = 0;
		static int timecount = 0;

		timetotal = time2 - time1 + timetotal;
		++timecount;

		//Print status every 1000 frames.
		if( timecount >= 1000 )
		{
			int iActiveClients = 0;

			for( int i = 0; i < svs.maxclients; ++i )
			{
				if( svs.clients[ i ].active )
					++iActiveClients;
			}

			Con_Printf( const_cast<char*>("host_profile: %2i clients %2i msec\n"),
						iActiveClients,
						static_cast<int>( floor( timetotal * 1000.0 / timecount ) ) );
		
			timecount = 0;
			timetotal = 0;
		}
	}

	return giActive;
}

bool Host_IsServerActive()
{
	return sv.active;
}

void Host_ClearClients(qboolean bFramesOnly)
{
	int i;
	int j;
	client_frame_t* frame;
	netadr_t save;

	host_client = svs.clients;
	for (i = 0; i < svs.maxclients; i++, host_client++)
	{
		if (host_client->frames)
		{
			for (j = 0; j < SV_UPDATE_BACKUP; j++)
			{
				frame = &(host_client->frames[j]);
				SV_ClearPacketEntities(frame);
				frame->senttime = 0;
				frame->ping_time = -1;
			}
		}
		if (host_client->netchan.remote_address.type)
		{
			save = host_client->netchan.remote_address;
			Q_memset(&host_client->netchan, 0, sizeof(netchan_t));
			Netchan_Setup(NS_SERVER, &host_client->netchan, save, host_client - svs.clients, (void*)host_client, SV_GetFragmentSize);
		}
		COM_ClearCustomizationList(&host_client->customdata, 0);
	}

	if (bFramesOnly == FALSE)
	{
		host_client = svs.clients;
		for (i = 0; i < svs.maxclientslimit; i++, host_client++)
			SV_ClearFrames(&host_client->frames);

		Q_memset(svs.clients, 0, sizeof(client_t) * svs.maxclientslimit);
		SV_AllocClientFrames();
	}
}

void Host_ShutdownServer(qboolean crash)
{
	if (!sv.active)
		return;

	SV_ServerShutdown();
	sv.active = FALSE;
	NET_ClearLagData(TRUE, TRUE);

	host_client = svs.clients;
	for (int i = 0; i < svs.maxclients; i++, host_client++)
	{
		if (host_client->active || host_client->connected)
			SV_DropClient(host_client, crash, (char*)"Server shutting down");
	}

	CL_Disconnect();
	SV_ClearEntities();
	SV_ClearCaches();
	FreeAllEntPrivateData();
	Q_memset(&sv, 0, sizeof(server_t));
	CL_ClearClientState();

	SV_ClearClientStates();
	Host_ClearClients(FALSE);

	host_client = svs.clients;
	for (int i = 0; i < svs.maxclientslimit; i++, host_client++)
		SV_ClearFrames(&host_client->frames);

	Q_memset(svs.clients, 0, sizeof(client_t) * svs.maxclientslimit);
	HPAK_FlushHostQueue();
	Steam_Shutdown();
	Log_Printf("Server shutdown\n");
	Log_Close();
}

int Host_MaxClients()
{
	if (sv.active)
		return svs.maxclients;
	else
		return cl.maxclients;
}

bool Host_IsSinglePlayerGame()
{
	return Host_MaxClients() == 1;
}

void Host_GetHostInfo( float* fps, int* nActive, int* unused, int* nMaxPlayers, char* pszMap )
{
	int clients = 0;

	if( rolling_fps > 0.0 )
	{
		*fps = 1.0 / rolling_fps;
	}
	else
	{
		rolling_fps = 0.0;
		*fps = rolling_fps;
	}

	SV_CountPlayers( &clients );
	*nActive = clients;

	if( unused )
		*unused = 0;

	if( pszMap )
	{
		if( sv.name[ 0 ] )
			Q_strcpy( pszMap, sv.name );
		else
			*pszMap = '\0';
	}

	*nMaxPlayers = svs.maxclients;
}

void SV_DropClient( client_t* cl, qboolean crash, char* fmt, ... )
{
	int i;
	unsigned char final[512];
	float connection_time;
	va_list args;
	char string[1024];

	i = 0;

	va_start(args, fmt);
	_vsnprintf(string, sizeof(string), fmt, args);

	if (!crash)
	{
		if (!cl->fakeclient)
		{
			MSG_WriteByte(&cl->netchan.message, svc_disconnect);
			MSG_WriteString(&cl->netchan.message, string);
			final[0] = svc_disconnect;
			Q_strncpy((char*)&final[1], string, min(sizeof(final)-1, (size_t)Q_strlen(string) + 1));
			final[sizeof(final) - 1] = 0;
			i = 1 + min(sizeof(final)-1, (size_t)Q_strlen(string) + 1);
		}

		extern void DbgPrint(FILE*, const char* format, ...);
		extern FILE* m_fMessages;
		DbgPrint(m_fMessages, "disconnected in dropclient\r\n");

		if (cl->edict && cl->spawned)
			gEntityInterface.pfnClientDisconnect(cl->edict);

		Sys_Printf("Dropped %s from server\nReason:  %s\n", cl->name, string);

		if (!cl->fakeclient)
			Netchan_Transmit(&cl->netchan, i, final);
	}

	connection_time = realtime - cl->netchan.connect_time;
	if (connection_time > 60.0)
	{
		++svs.stats.num_sessions;
		svs.stats.cumulative_sessiontime = svs.stats.cumulative_sessiontime + connection_time;
	}

	Netchan_Clear(&cl->netchan);

	Steam_NotifyClientDisconnect(cl);

	cl->active = FALSE;
	cl->connected = FALSE;
	cl->hasusrmsgs = FALSE;
	cl->fakeclient = FALSE;
	cl->spawned = FALSE;
	cl->fully_connected = FALSE;
	cl->name[0] = 0;
	cl->connection_started = realtime;
	cl->proxy = FALSE;
	COM_ClearCustomizationList(&cl->customdata, FALSE);

	cl->edict = NULL;
	Q_memset(cl->userinfo, 0, sizeof(cl->userinfo));
	Q_memset(cl->physinfo, 0, sizeof(cl->physinfo));

	SV_FullClientUpdate(cl, &sv.reliable_datagram);

	NotifyDedicatedServerUI("UpdatePlayers");
}

void Host_CheckDyanmicStructures()
{
	if (!svs.clients)
		return;

	client_t* pClient = svs.clients;

	for (int i = 0; i < svs.maxclientslimit; ++i, pClient++)
	{
		if (pClient->frames)
			SV_ClearFrames(&pClient->frames);
	}
}

void SV_ClearClientStates()
{
	client_t* pClient = svs.clients;

	for (int i = 0; i < svs.maxclients; i++, pClient++)
	{
		COM_ClearCustomizationList(&pClient->customdata, false);
		SV_ClearResourceLists(pClient);
	}
}

void Host_ClearMemory(bool bQuiet)
{
	CM_FreePAS();
	SV_ClearEntities();

	if (!bQuiet)
		Con_DPrintf(const_cast<char*>("Clearing memory\n"));

	D_FlushCaches();
	Mod_ClearAll();

	if (host_hunklevel)
	{
		Host_CheckDyanmicStructures();

		Hunk_FreeToLowMark(host_hunklevel);
	}

	cls.signon = 0;

	SV_ClearCaches();

	Q_memset(&sv, 0, sizeof(sv));

	CL_ClearClientState();

	SV_ClearClientStates();
}

void Host_EndGame(char *message, ...)
{
	int demonum;
	cactive_t state;
	char string[1024];
	va_list va;

	va_start(va, message);
	vsnprintf(string, sizeof(string), message, va);
	va_end(va);

	Con_DPrintf(const_cast<char*>(__FUNCTION__ ": %s\n"), string);
	demonum = cls.demonum;
	if (sv.active)
		Host_ShutdownServer(false);
	cls.demonum = demonum;

	if (cls.state == ca_dedicated)
	{
		Sys_Error(__FUNCTION__ ": %s\n", string);
		return;
	}

	if (demonum != -1)
	{
		CL_Disconnect_f();
		cls.demonum = demonum;
		Host_NextDemo();
		longjmp(host_enddemo, 1);
	}
	extern void DbgPrint(FILE*, const char* format, ...);
	extern FILE* m_fMessages;
	DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
	CL_Disconnect();
	Cbuf_AddText(const_cast<char*>("cd stop\n"));
	Cbuf_Execute();
	longjmp(host_abortserver, 1);
}

int32 Host_GetStartTime()
{
	return startTime;
}

void GetStatsString(char *buf, int bufSize)
{
	long double avgOut = 0.0;
	long double avgIn = 0.0;
	int players = 0;

	for (int i = 0; i < svs.maxclients; i++)
	{
		host_client = &svs.clients[i];
		if (!host_client->active && !host_client->connected && !host_client->spawned || host_client->fakeclient)
			continue;

		players++;
		avgIn = avgIn + host_client->netchan.flow[FLOW_INCOMING].avgkbytespersec;
		avgOut = avgOut + host_client->netchan.flow[FLOW_OUTGOING].avgkbytespersec;
	}

	_snprintf(buf, bufSize - 1, "%5.2f %5.2f %5.2f %7i %5i %7.2f %7i",
		(float)(100.0 * cpuPercent),
		(float)avgIn,
		(float)avgOut,
		(int)floor(Sys_FloatTime() - startTime) / 60,
		g_userid - 1,
		(float)(1.0 / host_frametime),
		players);
	buf[bufSize - 1] = 0;
}

void Host_Stats_f(void)
{
	char stats[512];
	GetStatsString(stats, sizeof(stats));
	Con_Printf(const_cast<char*>("CPU   In    Out   Uptime  Users   FPS    Players\n%s\n"), stats);
}

char *Host_SaveGameDirectory()
{
	static char szDirectory[MAX_OSPATH];

	Q_memset(szDirectory, 0, sizeof(szDirectory));
	snprintf(szDirectory, sizeof(szDirectory), "SAVE/");
	return szDirectory;
}

qboolean Master_IsLanGame(void)
{
	return sv_lan.value != 0.0f;
}

void SV_BroadcastPrintf(const char *fmt, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, fmt);
	_vsnprintf(string, ARRAYSIZE(string) - 1, fmt, argptr);
	va_end(argptr);

	string[ARRAYSIZE(string) - 1] = 0;

	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *cl = &svs.clients[i];
		if ((cl->active || cl->spawned) && !cl->fakeclient)
		{
			MSG_WriteByte(&cl->netchan.message, svc_print);
			MSG_WriteString(&cl->netchan.message, string);
		}
	}
	Con_DPrintf(const_cast<char*>("%s"), string);
}

void SV_ClientPrintf(const char *fmt, ...)
{
	va_list va;
	char string[1024];

	if (!host_client->fakeclient)
	{
		va_start(va, fmt);
		_vsnprintf(string, ARRAYSIZE(string) - 1, fmt, va);
		va_end(va);

		string[ARRAYSIZE(string) - 1] = 0;

		MSG_WriteByte(&host_client->netchan.message, svc_print);
		MSG_WriteString(&host_client->netchan.message, string);
	}
}

int Host_GetMaxClients(void)
{
	return sv.active ? svs.maxclients : cl.maxclients;
}

void SCR_DrawFPS(void)
{
	static float rolling_fps;

	if (cl_showfps.value != 0.0 && host_frametime > 0.0)
	{
		rolling_fps = 0.6 * rolling_fps + host_frametime * 0.4;
		NET_DrawString(2, 2, 0, 1.0, 1.0, 1.0, const_cast<char*>("%d fps"), (int)floor(1.0f / rolling_fps));
	}
}

void Host_UpdateSounds()
{
	vec3_t vSoundForward, vSoundRight, vSoundUp;

	if (gfBackground)
		return;

	if (cls.signon == 2)
	{
		AngleVectors(r_playerViewportAngles, vSoundForward, vSoundRight, vSoundUp);
		S_Update(r_soundOrigin, vSoundForward, vSoundRight, vSoundUp);
	}
	else
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);
	
	S_PrintStats();
}

void Host_ClientCommands(const char* fmt, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, fmt);
	if (!host_client->fakeclient)
	{
		_vsnprintf(string, sizeof(string), fmt, argptr);
		string[sizeof(string) - 1] = 0;

		MSG_WriteByte(&host_client->netchan.message, svc_stufftext);
		MSG_WriteString(&host_client->netchan.message, string);
	}
	va_end(argptr);
}

void Host_EndSection(const char* pszSection)
{
	giActive = DLL_PAUSED;
	giSubState = 1;
	giStateInfo = 1;

	if (!pszSection || !*pszSection)
		Con_Printf(const_cast<char*>(" endsection with no arguments\n"));
	else
	{
		if (!Q_strcasecmp(pszSection, "_oem_end_training"))
			giStateInfo = 1;
		else if (!Q_strcasecmp(pszSection, "_oem_end_logo"))
			giStateInfo = 2;
		else if (!Q_strcasecmp(pszSection, "_oem_end_demo"))
			giStateInfo = 3;
		else
			Con_DPrintf(const_cast<char*>(" endsection with unknown Section keyvalue\n"));
	}
	Cbuf_AddText(const_cast<char*>("\ndisconnect\n"));
}

void Info_WriteVars(FileHandle_t fp)
{
	cvar_t* pcvar;
	char* s;
	char pkey[512];

	static char value[4][512];
	static int valueindex;

	char* o;

	valueindex = (valueindex + 1) % 4;
	s = &cls.userinfo[0];

	if (*s == '\\')
		s++;

	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		pcvar = Cvar_FindVar(pkey);
		if (!pcvar && pkey[0] != '*')
			FS_FPrintf(fp, "setinfo \"%s\" \"%s\"\n", pkey, value[valueindex]);

		if (!*s)
			return;
		s++;
	}
}

void Host_WriteCustomConfig(void)
{
	FILE* f;
	kbutton_t* ml;
	kbutton_t* jl;
	char configname[261];
	_snprintf(configname, 257, "%s", Cmd_Args());
	if (strstr(configname, "..")
		|| !Q_strcasecmp(configname, "config")
		|| !Q_strcasecmp(configname, "autoexec")
		|| !Q_strcasecmp(configname, "listenserver")
		|| !Q_strcasecmp(configname, "server")
		|| !Q_strcasecmp(configname, "userconfig"))
	{
		Con_Printf(const_cast<char*>("skipping writecfg output, invalid filename given\n"));
	}
	else
	{
		if (host_initialized && cls.state != ca_dedicated)
		{
			if (Key_CountBindings() < 2)
				Con_Printf(const_cast<char*>("skipping config.cfg output, no keys bound\n"));
			else
			{
				Q_strcat(configname, const_cast<char*>(".cfg"));
				f = (FILE*)FS_OpenPathID(configname, "w", "GAMECONFIG");
				if (!f)
				{
					Con_Printf(const_cast<char*>("Couldn't write %s.\n"), configname);
					return;
				}

				FS_FPrintf(f, "unbindall\n");
				Key_WriteBindings(f);
				Cvar_WriteVariables(f);
				Info_WriteVars(f);

				ml = ClientDLL_FindKey("in_mlook");
				jl = ClientDLL_FindKey("in_jlook");

				if (ml && ml->state & 1)
					FS_FPrintf(f, "+mlook\n");

				if (jl && jl->state & 1)
					FS_FPrintf(f, "+jlook\n");

				FS_Close(f);
				Con_Printf(const_cast<char*>("%s successfully created!\n"), configname);
			}
		}
	}
}

