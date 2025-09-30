#include <cstdarg>

#include "winheaders.h"

#include "quakedef.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "client.h"
#include "cdll_exp.h"
#include "const.h"
#include "screen.h"

#if defined(GLQUAKE)
#include "gl_draw.h"
#include "gl_rmain.h"
#include "gl_screen.h"
#else
#include "draw.h"
#include "r_local.h"
#endif

#include "vid.h"
#include "r_studio.h"
#include "sound.h"
#include "vgui_int.h"
#include "vgui2/text_draw.h"

int hudGetScreenInfo( SCREENINFO* pscrinfo )
{
	RecEnghudGetScreenInfo( pscrinfo );

	if( pscrinfo && pscrinfo->iSize == sizeof( SCREENINFO ) )
	{
		pscrinfo->iWidth = vid.width;
		pscrinfo->iHeight = vid.height;
#if defined(GLQUAKE)
		pscrinfo->iFlags = SCRINFO_SCREENFLASH;
#else
		pscrinfo->iFlags = vid_stretched ? SCRINFO_STRETCHED : 0;
#endif
		pscrinfo->iCharHeight = VGUI2_MessageFontInfo( pscrinfo->charWidths, VGUI2_GetCreditsFont() );
		return sizeof( SCREENINFO );
	}

	return 0;
}

cvar_t* hudRegisterVariable( char* szName, char* szValue, int flags )
{
	RecEnghudRegisterVariable( szName, szValue, flags );

	cvar_t* cv = reinterpret_cast<cvar_t*>( Z_Malloc( sizeof( cvar_t ) ) );
	cv->name = szName;
	cv->string = szValue;
	cv->flags = flags | FCVAR_CLIENTDLL;

	Cvar_RegisterVariable( cv );

	return cv;
}

float hudGetCvarFloat( char* szName )
{
	if( szName )
	{
		RecEnghudGetCvarFloat( szName );
		cvar_t* cv = Cvar_FindVar( szName );

		if( cv )
			return cv->value;
	}

	return NULL;
}

char* hudGetCvarString(char* szName)
{
	if (szName)
	{
		RecEnghudGetCvarString(szName);

		cvar_t* cv = Cvar_FindVar(szName);

		if (cv)
			return cv->string;
	}

	return NULL;
}

int hudAddCommand( char* cmd_name, void( *function )( ) )
{
	RecEnghudAddCommand( cmd_name, function );
	Cmd_AddHUDCommand( cmd_name, function );
	return true;
}

int hudHookUserMsg( char* szMsgName, pfnUserMsgHook pfn )
{
	RecEnghudHookUserMsg(szMsgName, pfn);

	return HookServerMsg(szMsgName, pfn) != NULL;
}

int hudServerCmd( char* pszCmdString )
{
	char buf[ 2048 ];

	RecEnghudServerCmd( pszCmdString );

	snprintf( buf, ARRAYSIZE( buf ), "cmd %s", pszCmdString );
	Cmd_TokenizeString( buf );
	Cmd_ForwardToServer();

	return false;
}

int hudServerCmdUnreliable( char* pszCmdString )
{
	char buf[ 2048 ];

	RecEnghudServerCmdUnreliable( pszCmdString );

	snprintf( buf, ARRAYSIZE( buf ), "cmd %s", pszCmdString );
	Cmd_TokenizeString( buf );

	return Cmd_ForwardToServerUnreliable();
}

int hudClientCmd( char* pszCmdString )
{
	RecEnghudClientCmd( pszCmdString );

	if( pszCmdString )
	{
		Cbuf_AddText( pszCmdString );
		Cbuf_AddText( const_cast<char*>("\n") );
		return true;
	}

	return false;
}

void hudGetPlayerInfo( int ent_num, hud_player_info_t* pinfo )
{
	RecEnghudGetPlayerInfo( ent_num, pinfo );

	if( cl.maxclients >= ent_num && ent_num > 0 && 
		cl.players[ ent_num - 1 ].name[ 0 ] )
	{
		//Adjust to 0 based
		--ent_num;

		pinfo->name = cl.players[ ent_num ].name;
		pinfo->ping = cl.players[ ent_num ].ping;
		pinfo->spectator = cl.players[ ent_num ].spectator;
		pinfo->packetloss = cl.players[ ent_num ].packet_loss;
		pinfo->thisplayer = ent_num == cl.playernum;
		pinfo->topcolor = cl.players[ ent_num ].topcolor;
		pinfo->model = cl.players[ ent_num ].model;
		pinfo->bottomcolor = cl.players[ ent_num ].bottomcolor;

		if( g_bIsCStrike || g_bIsCZero )
		{
			pinfo->m_nSteamID = cl.players[ ent_num ].m_nSteamID;
		}
	}
	else
	{
		pinfo->name = NULL;
		pinfo->thisplayer = false;
	}
}

void hudPlaySoundByName( char* szSound, float volume )
{
	RecEnghudPlaySoundByName( szSound, volume );

	volume = clamp( volume, 0.0f, 1.0f );

	sfx_t* sfx = S_PrecacheSound( szSound );

	if( sfx )
		S_StartDynamicSound( cl.viewentity, CHAN_ITEM, sfx, r_origin, volume, 1.0, 0, PITCH_NORM );
	else
		Con_DPrintf( const_cast<char*>("invalid sound %s\n"), szSound );
}

void hudPlaySoundByNameAtPitch( char* szSound, float volume, int pitch )
{
	RecEnghudPlaySoundByName( szSound, volume );

	volume = clamp( volume, 0.0f, 1.0f );

	sfx_t* sfx = S_PrecacheSound( szSound );

	if( sfx )
	{
		pitch = max( 0, pitch );

		S_StartDynamicSound( cl.viewentity, CHAN_ITEM, sfx, r_origin, volume, 1.0, 0, pitch );
	}
	else
	{
		Con_DPrintf( const_cast<char*>("invalid sound %s\n"), szSound );
	}
}

void hudPlaySoundVoiceByName( char* szSound, float volume, int pitch )
{
	RecEnghudPlaySoundVoiceByName( szSound, volume );

	volume = clamp( volume, 0.0f, 1.0f );

	sfx_t* sfx = S_PrecacheSound( szSound );

	if( sfx )
	{
		pitch = max( 0, pitch );

		S_StartDynamicSound( cl.viewentity, CHAN_BOT, sfx, r_origin, volume, 1.0, 0, pitch );
	}
	else
	{
		Con_DPrintf( const_cast<char*>("invalid sound %s\n"), szSound );
	}
}

void hudPlaySoundByIndex(int iSound, float volume)
{
	RecEnghudPlaySoundByIndex(iSound, volume);

	volume = clamp(volume, 0.0f, 1.0f);

	if (iSound >= 0 && iSound < MAX_SOUNDS)
	{
		S_StartDynamicSound(cl.viewentity, CHAN_ITEM, cl.sound_precache[iSound], r_origin, volume, 1.0, 0, PITCH_NORM);
	}
	else
	{
		Con_DPrintf(const_cast<char*>("invalid sound %i\n"), iSound);
	}
}

void hudPlaySoundByNameAtLocation( char* szSound, float volume, float* origin )
{
	RecEnghudPlaySoundByNameAtLocation( szSound, volume, origin );

	volume = clamp( volume, 0.0f, 1.0f );

	sfx_t* sfx = CL_LookupSound( szSound );

	if( sfx )
		S_StartDynamicSound( cl.viewentity, CHAN_AUTO, sfx, origin, volume, 1.0, 0, PITCH_NORM );
}

void hudDrawConsoleStringLen( const char* string, int* width, int* height )
{
	RecEnghudDrawConsoleStringLen( string, width, height );

	*width = Draw_StringLen( string, VGUI2_GetConsoleFont() );
	*height = VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
}

void hudConsolePrint( const char* string )
{
	RecEnghudConsolePrint( string );

	Con_Printf( const_cast<char*>("%s"), string );
}

void hudCenterPrint( const char* string )
{
	RecEnghudConsolePrint( string );

	SCR_CenterPrint( string );
}

void hudGetClientOrigin(vec3_t origin)
{
	cl_entity_t* pViewEnt = &cl_entities[cl.viewentity];

	if (pViewEnt->index - 1 == cl.playernum)
	{
		VectorCopy(cl.simorg, origin);
	}
	else
	{
		VectorCopy(pViewEnt->origin, origin);
	}
}

void hudCvar_SetValue( char* var_name, float value )
{
	Cvar_SetValue( var_name, value );
}

char* hudCmd_Argv( int arg )
{
	return Cmd_Argv( arg );
}

void hudCon_Printf( char* fmt, ... )
{
	char buffer[ 1024 ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( buffer, ARRAYSIZE( buffer ), fmt, va );
	va_end( va );

	Con_Printf( const_cast<char*>("%s"), buffer );
}

void hudCon_DPrintf( char* fmt, ... )
{
	char buffer[ 1024 ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( buffer, ARRAYSIZE( buffer ), fmt, va );
	va_end( va );

	Con_DPrintf( const_cast<char*>("%s"), buffer );
}

void hudCon_NPrintf( int idx, char* fmt, ... )
{
	char szBuf[ CON_MAX_NOTIFY_STRING ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( szBuf, ARRAYSIZE( szBuf ), fmt, va );
	va_end( va );

	Con_NPrintf( idx, const_cast<char*>("%s"), szBuf );
}

void hudCon_NXPrintf( con_nprint_t* info, char* fmt, ... )
{
	char szBuf[ CON_MAX_NOTIFY_STRING ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( szBuf, ARRAYSIZE( szBuf ), fmt, va );
	va_end( va );

	Con_NXPrintf( info, "%s", szBuf );
}

void hudKey_Event( int key, int down )
{
	Key_Event( key, down != 0 );
}

void* hudVGuiWrap_GetPanel()
{
	return VGuiWrap_GetPanel();
}

byte* hudCOM_LoadFile( char* path, int usehunk, int* pLength )
{
	return COM_LoadFile( path, usehunk, pLength );
}

void* hudVguiWrap2_GetCareerUI()
{
	return VguiWrap2_GetCareerUI();
}

void hudCvar_Set( char* var_name, char* value )
{
	Cvar_Set( var_name, value );
}

int hudVGuiWrap2_IsInCareerMatch()
{
	return VGuiWrap2_IsInCareerMatch();
}

int hudGetGameAppID()
{
	return GetGameAppID();
}

int hudFilteredClientCmd(char* pszCmdString)
{
	RecEnghudFilteredClientCmd(pszCmdString);

	if (!pszCmdString || ValidStuffText(pszCmdString) == false)
		return 0;

	Cbuf_AddFilteredText(pszCmdString);
	Cbuf_AddFilteredText(const_cast<char*>("\n"));

	return 1;
}
