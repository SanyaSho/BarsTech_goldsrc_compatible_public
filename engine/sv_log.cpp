#include <cstdarg>
#include <cstdio>
#include <ctime>

#include "quakedef.h"
#include "buildnum.h"
#include "server.h"
#include "sv_log.h"

const int MAX_DAILY_LOG_FILES = 1000;

struct LOGLIST_T
{
	server_log_t log;
	LOGLIST_T *next;
};

static LOGLIST_T* firstLog = nullptr;

cvar_t sv_log_onefile = { const_cast<char*>("sv_log_onefile"), const_cast<char*>("0") };
cvar_t sv_log_singleplayer = { const_cast<char*>("sv_log_singleplayer"), const_cast<char*>("0") };
cvar_t sv_logsecret = { const_cast<char*>("sv_logsecret"), const_cast<char*>("0") };
cvar_t mp_logecho = { const_cast<char*>("mp_logecho"), const_cast<char*>("1") };
cvar_t mp_logfile = { const_cast<char*>("mp_logfile"), const_cast<char*>("1"), FCVAR_SERVER };

void Log_Printf( const char* fmt, ... )
{
	va_list va;

	va_start( va, fmt );

	if( svs.log.net_log || firstLog || svs.log.active )
	{
		static char string[ 1024 ];

		time_t ltime;
		time( &ltime );
		tm* pTime = localtime( &ltime );

		snprintf(
			string,
			sizeof( string ),
			"L %02i/%02i/%04i - %02i:%02i:%02i: ",
			pTime->tm_mon + 1,
			pTime->tm_mday,
			pTime->tm_year + 1900,
			pTime->tm_hour,
			pTime->tm_min,
			pTime->tm_sec );

		vsnprintf( &string[Q_strlen(string)], sizeof( string ) - Q_strlen(string), fmt, va );

		if( svs.log.net_log )
		{
			
			Netchan_OutOfBandPrint( NS_SERVER, svs.log.net_address, const_cast<char*>("log %s"), string );
			
		}

		for (LOGLIST_T* pLog = firstLog; pLog; pLog = pLog->next)
		{
			if( !sv_logsecret.value )
			{
				Netchan_OutOfBandPrint( NS_SERVER, pLog->log.net_address, const_cast<char*>("log %s"), string );
			}
			else
			{
				Netchan_OutOfBandPrint( NS_SERVER, pLog->log.net_address, const_cast < char*>("%c%s%s"), 83, sv_logsecret.string, string );
			}
		}

		if( svs.log.active && ( svs.maxclients > 1 || sv_log_singleplayer.value != 0.0 ) )
		{
			if( mp_logecho.value != 0.0 )
				Con_Printf( const_cast<char*>("%s"), string );

			if( svs.log.file != FILESYSTEM_INVALID_HANDLE )
			{
				if( mp_logfile.value != 0.0 )
					FS_FPrintf( svs.log.file, "%s", string );
			}
		}
	}

	va_end( va );
}

void Log_PrintServerVars()
{
	if( svs.log.active )
	{
		Log_Printf( const_cast<char*>("Server cvars start\n") );
		for( cvar_t* i = cvar_vars; i; i = i->next )
		{
			if( ( i->flags & FCVAR_SERVER ) )
				Log_Printf( const_cast<char*>("Server cvar \"%s\" = \"%s\"\n"), i->name, i->string );
		}

		Log_Printf( const_cast<char*>("Server cvars end\n") );
	}
}

void Log_Close()
{
	if( svs.log.file )
	{
		Log_Printf( "Log file closed\n" );
		FS_Close( svs.log.file );
	}
	svs.log.file = FILESYSTEM_INVALID_HANDLE;
}

void Log_Open()
{
	if ( svs.log.active && ( !sv_log_onefile.value || !svs.log.file ) )
	{
		if( !mp_logfile.value )
		{
			Con_Printf( const_cast<char*>("Server logging data to console.\n") );
		}
		else
		{
			Log_Close();

			time_t ltime;
			time( &ltime );
			tm* today = localtime( &ltime );
			char* temp = Cvar_VariableString( const_cast<char*>("logsdir") );

			char szFileBase[ MAX_PATH ];

			if( !temp ||
				Q_strlen( temp ) <= 0 ||
				strstr( temp, ":" ) ||
				strstr( temp, ".." ) )
			{
				snprintf( szFileBase, ARRAYSIZE( szFileBase ), "logs/L%02i%02i", today->tm_mon + 1, today->tm_mday );
			}
			else
			{
				snprintf( szFileBase, ARRAYSIZE( szFileBase ), "%s/L%02i%02i", temp, today->tm_mon + 1, today->tm_mday );
			}

			char szTestFile[ MAX_PATH ];

			int i;

			for( i = 0; i < MAX_DAILY_LOG_FILES; ++i )
			{
				snprintf( szTestFile, ARRAYSIZE( szTestFile ), "%s%03i.log", szFileBase, i );
				
				COM_FixSlashes( szTestFile );
				COM_CreatePath( szTestFile );

				FileHandle_t fp = FS_OpenPathID( szTestFile, "r", "GAMECONFIG" );
				if( !fp )
					break;

				FS_Close( fp );
			}

			if( i < MAX_DAILY_LOG_FILES )
			{
				COM_CreatePath( szTestFile );

				FileHandle_t fp = FS_OpenPathID( szTestFile, "wt", "GAMECONFIG" );

				if( fp )
				{
					Con_Printf( const_cast<char*>("Server logging data to file %s\n"), szTestFile );
					svs.log.file = fp;

					Log_Printf(
						"Log file started (file \"%s\") (game \"%s\") (version \"%i/%s/%d\")\n",
						szTestFile,
						Info_ValueForKey( Info_Serverinfo(), "*gamedir" ),
						PROTOCOL_VERSION,
						gpszVersionString,
						build_number()
					);
					return;
				}
			}

			Con_Printf( const_cast<char*>("Unable to open logfiles under %s\nLogging disabled\n"), szFileBase );
			svs.log.active = false;
		}
	}
}

void SV_SetLogAddress_f(void)
{
	const char *s;
	int nPort;
	char szAdr[MAX_PATH];
	netadr_t adr;

	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("logaddress:  usage\nlogaddress ip port\n"));
		if (svs.log.active)
			Con_Printf(const_cast<char*>("current:  %s\n"), NET_AdrToString(svs.log.net_address));
		return;
	}

	nPort = Q_atoi(Cmd_Argv(2));
	if (!nPort)
	{
		Con_Printf(const_cast<char*>("logaddress:  must specify a valid port\n"));
		return;
	}

	s = Cmd_Argv(1);
	if (!s || *s == '\0')
	{
		Con_Printf(const_cast<char*>("logaddress:  unparseable address\n"));
		return;
	}

	_snprintf(szAdr, sizeof(szAdr), "%s:%i", s, nPort);

	if (!NET_StringToAdr(szAdr, &adr))
	{
		Con_Printf(const_cast<char*>("logaddress:  unable to resolve %s\n"), szAdr);
		return;
	}

	svs.log.net_log = TRUE;
	svs.log.net_address = adr;

	Con_Printf(const_cast<char*>("logaddress:  %s\n"), NET_AdrToString(adr));
}

void SV_AddLogAddress_f(void)
{
	const char *s;
	int nPort;
	char szAdr[MAX_PATH];
	netadr_t adr;
	LOGLIST_T *list;
	qboolean found = FALSE;
	LOGLIST_T *tmp;

	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("logaddress_add:  usage\nlogaddress_add ip port\n"));
		for (list = firstLog; list != NULL; list = list->next)
			Con_Printf(const_cast<char*>("current:  %s\n"), NET_AdrToString(list->log.net_address));
		return;
	}

	nPort = Q_atoi(Cmd_Argv(2));
	if (!nPort)
	{
		Con_Printf(const_cast<char*>("logaddress_add:  must specify a valid port\n"));
		return;
	}

	s = Cmd_Argv(1);
	if (!s || *s == '\0')
	{
		Con_Printf(const_cast<char*>("logaddress_add:  unparseable address\n"));
		return;
	}
	_snprintf(szAdr, sizeof(szAdr), "%s:%i", s, nPort);

	if (!NET_StringToAdr(szAdr, &adr))
	{
		Con_Printf(const_cast<char*>("logaddress_add:  unable to resolve %s\n"), szAdr);
		return;
	}

	if (firstLog)
	{
		for (list = firstLog; list != NULL; list = list->next)
		{
			if (adr.ip == list->log.net_address.ip && adr.port == list->log.net_address.port)
			{
				found = TRUE;
				break;
			}
		}
		if (found)
		{
			Con_Printf(const_cast<char*>("logaddress_add:  address already in list\n"));
			return;
		}
		tmp = (LOGLIST_T *)Mem_Malloc(sizeof(LOGLIST_T));
		if (!tmp)
		{
			Con_Printf(const_cast<char*>("logaddress_add:  error allocating new node\n"));
			return;
		}

		tmp->next = NULL;
		tmp->log.net_address = adr;

		list = firstLog;

		while (list->next)
			list = list->next;

		list->next = tmp;
	}
	else
	{
		firstLog = (LOGLIST_T *)Mem_Malloc(sizeof(LOGLIST_T));
		if (!firstLog)
		{
			Con_Printf(const_cast<char*>("logaddress_add:  error allocating new node\n"));
			return;
		}
		firstLog->next = NULL;
		firstLog->log.net_address = adr;
	}

	Con_Printf(const_cast<char*>("logaddress_add:  %s\n"), NET_AdrToString(adr));

	for (list = firstLog; list != NULL; list = list->next)
		Con_Printf("current:  %s\n", NET_AdrToString(list->log.net_address));
}

void SV_DelLogAddress_f(void)
{
	const char *s;
	int nPort;
	char szAdr[MAX_PATH];
	netadr_t adr;
	LOGLIST_T *list;
	LOGLIST_T *prev;
	qboolean found = FALSE;

	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("logaddress_del:  usage\nlogaddress_del ip port\n"));
		for (list = firstLog; list != NULL; list = list->next)
			Con_Printf(const_cast<char*>("current:  %s\n"), NET_AdrToString(list->log.net_address));
		return;
	}
	nPort = Q_atoi(Cmd_Argv(2));
	if (!nPort)
	{
		Con_Printf(const_cast<char*>("logaddress_del:  must specify a valid port\n"));
		return;
	}

	s = Cmd_Argv(1);
	if (!s || *s == '\0')
	{
		Con_Printf(const_cast<char*>("logaddress_del:  unparseable address\n"));
		return;
	}
	
	_snprintf(szAdr, sizeof(szAdr), "%s:%i", s, nPort);
	if (!NET_StringToAdr(szAdr, &adr))
	{
		Con_Printf(const_cast<char*>("logaddress_del:  unable to resolve %s\n"), szAdr);
		return;
	}
	
	if (!firstLog)
	{
		Con_Printf(const_cast<char*>("logaddress_del:  No addresses added yet\n"));
		return;
	}

	for (list = firstLog, prev = firstLog; list != NULL; list = list->next)
	{
		if (adr.ip == list->log.net_address.ip && adr.port == list->log.net_address.port)
		{
			found = TRUE;
			if (list == prev)
			{
				firstLog = prev->next;
				Mem_Free(prev);
			}
			else
			{
				prev->next = list->next;
				Mem_Free(list);
			}
			break;
		}
		prev = list;
	}
	if (!found)
	{
		Con_Printf(const_cast<char*>("logaddress_del:  Couldn't find address in list\n"));
		return;
	}

	Con_Printf(const_cast<char*>("deleting:  %s\n"), NET_AdrToString(adr));
}

void SV_ServerLog_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf(const_cast<char*>("usage:  log < on | off >\n"));

		if (svs.log.active)
			Con_Printf(const_cast<char*>("currently logging\n"));
		else
			Con_Printf(const_cast<char*>("not currently logging\n"));
		return;
	}

	char *s = Cmd_Argv(1);
	if (Q_stricmp(s, "off"))
	{
		if (Q_stricmp(s, "on"))
			Con_Printf(const_cast<char*>("log:  unknown parameter %s, 'on' and 'off' are valid\n"), s);
		else
		{
			svs.log.active = TRUE;
			Log_Open();
		}
	}
	else if (svs.log.active)
	{
		Log_Close();
		Con_Printf(const_cast<char*>("Server logging disabled.\n"));
		svs.log.active = FALSE;
	}
}
