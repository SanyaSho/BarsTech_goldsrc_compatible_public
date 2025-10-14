#include "quakedef.h"
#include "client.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_spectator.h"
#include "cl_tent.h"
#include "com_custom.h"

#if defined(GLQUAKE)
#include "gl_rmain.h"
#include "gl_screen.h"
#include "vid.h"
#include "glquake.h"
#else
#include "r_shared.h"
#include "r_local.h"
#include "screen.h"
#endif

#include "hashpak.h"
#include "host.h"
#include "pmove.h"
#include "tmessage.h"
#include "net_ws.h"
#include "vgui_int.h"
#include "cl_draw.h"
#include "sv_steam3.h"
#include "sound.h"
#include "DemoPlayerWrapper.h"
#include "cl_demo.h"
#include "r_trans.h"
#if defined(GLQUAKE)
#include "gl_model.h"
#else
#include "model.h"
#endif
#include "download.h"
#include "r_studio.h"
#include "host_cmd.h"
#include "eiface.h"
#include "cl_spectator.h"
#include "net_api_int.h"
#include "cl_pred.h"
#include "sv_main.h"
#include "cl_pmove.h"
#include "cl_parsefn.h"
#include "cl_ents.h"
#include "cl_extrap.h"
#include "modinfo.h"
#include "voice.h"
#include "pr_cmds.h"
#include "common/Sequence.h"

// Only send this many requests before timing out.
#define CL_CONNECTION_RETRIES		4   
// If we get more than 250 messages in the incoming buffer queue, dump any above this #
#define MAX_INCOMING_MESSAGES		250
// Largest # of commands to send in a packet
#define MAX_TOTAL_CMDS_OLD			16
#define MAX_TOTAL_CMDS				62

#define NEWLIM_SERVER_BUILDNUM		5971

#define MAX_CMD_BUFFER				2048
// In release, send commands at least this many times per second
#define MIN_CMD_RATE				10.0
// If the network connection hasn't been active in this many seconds, display some warning text.
#define CONNECTION_PROBLEM_TIME		15.0

#define CLIENT_RECONNECT_TIME		30.0

#define CL_PACKETLOSS_RECALC		1.0


client_static_t cls;
client_state_t	cl;

cl_entity_t *cl_visedicts[MAX_VISEDICTS];
int cl_numvisedicts;

cl_entity_t cl_static_entities[32];

playermove_t g_clmove;

cl_entity_t* cl_entities = nullptr;

int g_bRedirectedToProxy;
int cl_playerindex;

entity_state_t	cl_baselines[MAX_EDICTS];
efrag_t cl_efrags[MAX_EFRAGS];
dlight_t cl_dlights[MAX_DLIGHTS];
dlight_t cl_elights[MAX_ELIGHTS];
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];

float g_LastScreenUpdateTime = 0;

cvar_t fs_lazy_precache = { const_cast<char*>("fs_lazy_precache"), const_cast<char*>("0") };
cvar_t fs_precache_timings = { const_cast<char*>("fs_precache_timings"), const_cast<char*>("0") };
cvar_t fs_startup_timings = { const_cast<char*>("fs_startup_timings"), const_cast<char*>("0") };

cvar_t cl_mousegrab = { const_cast<char*>("cl_mousegrab"), const_cast<char*>("1"), FCVAR_ARCHIVE };
cvar_t m_rawinput = { const_cast<char*>("m_rawinput"), const_cast<char*>("1"), FCVAR_ARCHIVE };
cvar_t rate = { const_cast<char*>("rate"), const_cast<char*>("30000"), FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_PRIVILEGED };
cvar_t fs_perf_warnings = { const_cast<char*>("fs_perf_warnings"), const_cast<char*>("0") };
cvar_t cl_lw = { const_cast<char*>("cl_lw"), const_cast<char*>("1"), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t cl_lc = { const_cast<char*>("cl_lc"), const_cast<char*>("1"), FCVAR_ARCHIVE | FCVAR_USERINFO };

cvar_t cl_updaterate = { const_cast<char*>("cl_updaterate"), const_cast<char*>("60"), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t cl_cmdrate = { const_cast<char*>("cl_cmdrate"), const_cast<char*>("60"), FCVAR_ARCHIVE | FCVAR_USERINFO };
//cvar_t rate = { "rate", "24123", FCVAR_ARCHIVE | FCVAR_USERINFO };

cvar_t cl_name = { const_cast<char*>("name"), const_cast<char*>("unnamed"), FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE };

cvar_t cl_himodels = { const_cast<char*>("cl_himodels"), const_cast<char*>("0") };
cvar_t cl_gaitestimation = { const_cast<char*>("cl_gaitestimation"), const_cast<char*>("1") };

cvar_t cl_needinstanced = { const_cast<char*>("cl_needinstanced"), const_cast<char*>("0") };

cvar_t cl_fixtimerate = { const_cast<char*>("cl_fixtimerate"), const_cast<char*>("7.5") };

cvar_t cl_logofile = { const_cast<char*>("cl_logofile"), const_cast<char*>("lambda"), FCVAR_ARCHIVE };
cvar_t cl_logocolor = { const_cast<char*>("cl_logocolor"), const_cast<char*>("orange"), FCVAR_ARCHIVE };
cvar_t cl_solid_players = { const_cast<char*>("cl_solid_players"), const_cast<char*>("1") };
cvar_t cl_showfps = { const_cast<char*>("cl_showfps"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t cl_slisttimeout = { const_cast<char*>("cl_slist"), const_cast<char*>("10.0") };
cvar_t rcon_port = { const_cast<char*>("rcon_port"), const_cast<char*>("0"), FCVAR_PRIVILEGED };
cvar_t rcon_address = { const_cast<char*>("rcon_address"), const_cast<char*>(""), FCVAR_PRIVILEGED };
cvar_t cl_shownet = { const_cast<char*>("cl_shownet"), const_cast<char*>("0") };
cvar_t cl_cmdbackup = { const_cast<char*>("cl_cmdbackup"), const_cast<char*>("") };
cvar_t cl_timeout = { const_cast<char*>("cl_timeout"), const_cast<char*>("60"), FCVAR_ARCHIVE };
cvar_t cl_resend = { const_cast<char*>("cl_resend"), const_cast<char*>("6.0") };
cvar_t cl_idealpitchscale = { const_cast<char*>("cl_idealpitchscale"), const_cast<char*>("0.8"), FCVAR_ARCHIVE };
cvar_t cl_showevents = { const_cast<char*>("cl_showevents"), const_cast<char*>("0") };
cvar_t cl_dlmax = { const_cast<char*>("cl_dlmax"), const_cast<char*>("512"), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t bottomcolor = { const_cast<char*>("bottomcolor"), const_cast<char*>("0"), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t topcolor = { const_cast<char*>("topcolor"), const_cast<char*>("0"), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t cl_model = { const_cast<char*>("model"), const_cast<char*>(""), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t skin = { const_cast<char*>("skin"), const_cast<char*>(""), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t team = { const_cast<char*>("team"), const_cast<char*>(""), FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t password = { const_cast<char*>("password"), const_cast<char*>(""), FCVAR_USERINFO };

cvar_t cl_nodelta = { const_cast<char*>("cl_nodelta"), const_cast<char*>("0") };

static int g_iCurrentTiming = 0;

startup_timing_t g_StartupTimings[MAX_STARTUP_TIMINGS];

netadr_t g_GameServerAddress, g_rconaddress;

double frametime_remainder;
UserMsg *gClientUserMsgs;

float g_flTime;
int g_iFrames;

int currentcmd;
oldcmd_t oldcmd[CMD_COUNT];

int total_data[MAX_PACKETACCUM];
byte g_ucModuleC_MD5[16];
int last_data[MAX_PACKETACCUM];
int msg_buckets[MAX_PACKETACCUM];

static server_cache_t cached_servers[MAX_LOCAL_SERVERS];
static int num_servers;
qboolean cl_inmovie;
char g_lastrconcommand[1024];

void CL_SendConnectPacket(void);
void Sequence_Init();

void AddStartupTiming(const char* name)
{
	g_iCurrentTiming++;
	g_StartupTimings[g_iCurrentTiming].name = name;
	g_StartupTimings[g_iCurrentTiming].time = Sys_FloatTime();
}

void SetupStartupTimings()
{
	g_iCurrentTiming = 0;
	g_StartupTimings[ g_iCurrentTiming ].name = "Startup";
	g_StartupTimings[ g_iCurrentTiming ].time = Sys_FloatTime();
}

void PrintStartupTimings()
{
	Con_Printf( const_cast<char*>("Startup timings (%.2f total)\n"), g_StartupTimings[ g_iCurrentTiming ].time - g_StartupTimings[ 0 ].time );
	Con_Printf( const_cast<char*>("    0.00    Startup\n") );

	//Print the relative time between each system startup
	for( int i = 1; i < g_iCurrentTiming; ++i )
	{
		Con_Printf( const_cast<char*>("    %.2f    %s\n"),
					g_StartupTimings[ i ].time - g_StartupTimings[ i - 1 ].time,
					g_StartupTimings[ i ].name
		);
	}
}

void CL_CheckClientState()
{
	WORD port;
	DWORD ip;

	if ((cls.state == ca_connected || cls.state == ca_uninitialized) && cls.signon == SIGNONS)
	{
		cls.state = ca_active;
		if (fs_startup_timings.value)
			PrintStartupTimings();

		ip = *(DWORD*)&cls.netchan.remote_address.ip[0];
		port = cls.netchan.remote_address.port;

		if (!port)
		{
			port = net_local_adr.port;
			ip = *(DWORD*)&net_local_adr.ip[0];
		}

		VGuiWrap2_NotifyOfServerConnect(com_gamedir, ip, Q_ntohs(port));
	}
}

void CL_UpdateSoundFade()
{
	cls.soundfade.nClientSoundFadePercent = 0;

	if (realtime - cls.soundfade.soundFadeStartTime >= cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeHoldTime + cls.soundfade.soundFadeInTime)
		return;

	if (cls.soundfade.soundFadeOutTime && realtime - cls.soundfade.soundFadeStartTime < cls.soundfade.soundFadeOutTime)
		cls.soundfade.nClientSoundFadePercent = (float)cls.soundfade.nStartPercent * ((realtime - cls.soundfade.soundFadeStartTime) / cls.soundfade.soundFadeOutTime);
	else
	{
		if (realtime - cls.soundfade.soundFadeStartTime >= cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeHoldTime)
			cls.soundfade.nClientSoundFadePercent = (float)cls.soundfade.nStartPercent * (1.f - (((realtime - cls.soundfade.soundFadeStartTime) - (cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeHoldTime)) / cls.soundfade.soundFadeOutTime));
		else
			cls.soundfade.nClientSoundFadePercent = cls.soundfade.nStartPercent;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Broadcast pings to any servers that we can see on our LAN
//-----------------------------------------------------------------------------
void CL_PingServers_f(void)
{
	netadr_t	adr;
	int			i;

	// send a broadcast packet
	Con_Printf(const_cast<char*>("Searching for local servers...\n"));

	cls.slist_time = realtime;

	NET_Config(true);		// allow remote	

	Q_memset(&adr, 0, sizeof(adr));

	for (i = 0; i < 10; i++)
	{
		adr.port = BigShort((unsigned short)(Q_atoi(PORT_SERVER) + i));

		if (!noip)
		{
			adr.type = NA_BROADCAST;
			Netchan_OutOfBandPrint(NS_CLIENT, adr, const_cast<char*>("%c%s"), A2S_INFO, A2S_KEY_STRING);
		}

#ifdef _WIN32
		if (!noipx)
		{
			adr.type = NA_BROADCAST_IPX;
			Netchan_OutOfBandPrint(NS_CLIENT, adr, const_cast<char*>("infostring"));
		}
#endif
	}
}

/*
==================
CL_Slist_f (void)

Populates the client's server_cache_t structrue
Replaces Slist command
==================
*/
void CL_Slist_f(void)
{
	num_servers = 0;
	memset(cached_servers, 0, MAX_LOCAL_SERVERS * sizeof(server_cache_t));

	// send out info packets
	// FIXME:  Record realtime when sent and determine ping times for responsive servers.
	CL_PingServers_f();
}

void CL_PrintCachedServer(int slot)
{
	server_cache_t *p;
	char *type, *os;

	if (slot < 0 || slot >= MAX_LOCAL_SERVERS)
		return;

	p = &cached_servers[slot];

	if (!p->inuse)
		return;

	Con_Printf(const_cast<char*>("%d "), slot + 1);

	Con_Printf(const_cast<char*>("Name: %s\n"), p->name);
	Con_Printf(const_cast<char*>("Address: %s\n"), NET_AdrToString(p->adr));
	Con_Printf(const_cast<char*>("Map: %s\n"), p->map);
	Con_Printf(const_cast<char*>("Description: %s\n"), p->desc);
	Con_Printf(const_cast<char*>("Players: %d/%d   Bots: %d\n"), p->players, p->maxplayers, p->bots);

	switch (p->type)
	{
	case 'l':
		type = const_cast<char*>("Listen");
		break;
	case 'd':
		type = const_cast<char*>("Dedicated");
		break;
	case 'p':
		type = const_cast<char*>("Proxy");
		break;
	default:
		type = const_cast<char*>("Unknown");
		break;
	}

	switch (p->os)
	{
	case 'w':
		os = const_cast<char*>("Windows");
		break;
	case 'l':
		os = const_cast<char*>("Linux");
		break;
	default:
		os = const_cast<char*>("macOS");
		break;
	}

	Con_Printf(const_cast<char*>("%s - %s - %s - %s\n"), type, os, p->is_private ? "Private" : "Public", p->is_vac ? "Secure" : "Insecure");

	if (p->has_spectator_address)
		Con_Printf(const_cast<char*>("Spectator Address: %s\n"), NET_AdrToString(p->spec_adr));
}

/*
=================
CL_AddToServerCache

Adds the address, name to the server cache
=================
*/
void CL_AddToServerCache(server_cache_t *server)
{
	int		i;

	// Too many servers.
	if (num_servers >= MAX_LOCAL_SERVERS)
	{
		return;
	}

	if (cl_slisttimeout.value > 0.0)
	{
		if ((realtime - cls.slist_time) >= cl_slisttimeout.value)
		{
			return;
		}
	}

	// Already cached by address
	for (i = 0; i<num_servers; i++)
	{
		if (NET_CompareAdr(cached_servers[i].adr, server->adr))
		{
			return;
		}
	}

	// Display it.
	cached_servers[num_servers] = *server;
	cached_servers[num_servers].inuse = 1;
	num_servers++;
	Con_Printf(const_cast<char*>("------------------\n"));
	CL_PrintCachedServer(num_servers - 1);
}

void CL_ListCachedServers_f()
{
	server_cache_t *p;

	if (!num_servers)
		return Con_Printf(const_cast<char*>("No local servers in list.\nTry 'slist' to search again.\n"));

	for (int i = 0; i<num_servers; i++)
	{
		p = &cached_servers[i];
		if (!p->inuse)
			continue;

		Con_Printf(const_cast<char*>("------------------\n"));
		CL_PrintCachedServer(i);
	}
}

void CL_ParseServerList()
{
	char szAddress[128];
	byte cIP[4];

	for (int i = 0; i < sizeof(szAddress); i++)
		szAddress[i] = 0;

	int nServerNum = 1;

	MSG_ReadByte();

	int nNumAddresses = (net_message.cursize - 6) / 6;

	for (int i = 0; i < nNumAddresses; i++)
	{
		Q_memset(szAddress, 0, sizeof(szAddress));
		
		for (int j = 0; j < 4; j++)
			cIP[i] = MSG_ReadByte();

		_snprintf(szAddress, sizeof(szAddress), "%i.%i.%i.%i", cIP[0], cIP[1], cIP[2], cIP[3]);

		int port = MSG_ReadShort();
		port = BigShort(port);

		if (nServerNum <= 100)
			Con_Printf(const_cast<char*>("%4i:  %s:%i\n"), nServerNum, szAddress, port);
	}

	Con_Printf(const_cast<char*>("%i total servers\n"), nServerNum);
}

void CL_ParseBatchModList()
{
	char szRequest[128];
	char szName[256];

	char* pszModName;

	int nCount = 0;

	MSG_ReadByte();

	while (true)
	{
		pszModName = MSG_ReadString();

		if (!pszModName || !pszModName[0] || !Q_strcasecmp(pszModName, "end-of-list") || !Q_strcasecmp(pszModName, "more-in-list"))
			break;

		Q_strncpy(szName, pszModName, sizeof(szName) - 1);
		szName[sizeof(szName) - 1] = 0;

		char* pszServers = MSG_ReadString();
		int nServers = Q_atoi(pszServers);

		char* pszPlayers = MSG_ReadString();
		int nPlayers = Q_atoi(pszPlayers);

		nCount++;

		if (!pszServers || !pszPlayers)
			continue;

		Con_Printf(const_cast<char*>("%3i %5i %s\n"), nServers, nPlayers, szName);
	}

	Con_Printf(const_cast<char*>("%i servers\n"), nCount);

	if (pszModName && pszModName[0] && !Q_strcasecmp(pszModName, "more-in-list"))
	{
		_snprintf(szRequest, 128, "%c\r\n%s\r\n", A2M_GETACTIVEMODS, szName);
		Con_DPrintf(const_cast<char*>("Requesting next batch ( >%s ) of mods from %s\n"), szName, NET_AdrToString(net_from));
		NET_SendPacket(NS_CLIENT, Q_strlen(szRequest) + 1, szRequest, net_from);

		return;
	}

	Con_Printf(const_cast<char*>("Done.\n"));
	
}

void CL_ParseBatchUserList()
{
	char szAddress[128];
	byte cIP[4];
	byte c[20];
	static int count;

	for (int i = 0; i < sizeof(szAddress); i++)
		szAddress[i] = 0;

	MSG_ReadByte();

	int unique = MSG_ReadLong();
	int hash = MSG_ReadLong();

	int nNumAddresses = (net_message.cursize - msg_readcount) / 6;

	for (int i = 0; i < nNumAddresses; i++)
	{
		Q_memset(szAddress, 0, 128);

		for (int j = 0; j < 4; j++)
			cIP[j] = MSG_ReadByte();

		_snprintf(szAddress, 128, "%i.%i.%i.%i", cIP[0], cIP[1], cIP[2], cIP[3]);

		int iIPPort = BigShort(MSG_ReadShort());

		Con_Printf(const_cast<char*>("%4i:  %s:%i\n"), count, szAddress, iIPPort);
		count++;
	}

	if (hash && unique)
	{
		Q_strcpy((char*)c, "users");

		*(int*)&c[Q_strlen("users") + 1] = unique;
		*(int*)&c[Q_strlen("users") + 5] = hash;

		Con_DPrintf(const_cast<char*>("Requesting next batch ( %i ) user list from %s\n"), hash, NET_AdrToString(net_from));

		NET_SendPacket(NS_CLIENT, Q_strlen("users") + 9, c, net_from);

		return;
	}

	Con_Printf(const_cast<char*>("%i users\n"), count);

	count = 0;

	Con_Printf(const_cast<char*>("Done.\n"));
}

qboolean CL_GetFragmentSize(void* state)
{
	int retval = 128;

	int state_value = ((client_static_t*)state)->state;

	if (state_value == ca_active)
	{
		if (cl_dlmax.value < 16)
			retval = 256;
		else if (cl_dlmax.value <= 1024)
			retval = cl_dlmax.value;
		else
			retval = 1024;
	}
	return retval;
}

/*
================
CL_ConnectClient

Received connection initiation response from server, set up connection
================
*/
void CL_ConnectClient(void)
{
	ContinueLoadingProgressBar("ClientConnect", 2, 0.0);

	// Already connected?
	if (cls.state == ca_connected)
	{
		if (!cls.demoplayback)
			Con_DPrintf(const_cast<char*>("Duplicate connect ack. received.  Ignored.\n"));
		return;
	}

	cls.userid = 0;
	cls.trueaddress[0] = 0;
	if (Cmd_Argc() > 2)
	{
		cls.userid = Q_atoi(Cmd_Argv(1));
		Q_strncpy(cls.trueaddress, Cmd_Argv(2), sizeof(cls.trueaddress) - 1);
		cls.trueaddress[sizeof(cls.trueaddress) - 1] = 0;
		if (Cmd_Argc() > 4)
		{
			cls.build_num = Q_atoi(Cmd_Argv(4));
		}
	}

	// Initiate the network channel
	Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, -1, &cls, CL_GetFragmentSize);

	// Clear remaining lagged packets to prevent problems
	NET_ClearLagData(true, false);

	if (fs_startup_timings.value)
		AddStartupTiming("connection accepted by server");

	// Report connection success.
	if (Q_strcasecmp("loopback", NET_AdrToString(net_from)))
	{
		Con_Printf(const_cast<char*>("Connection accepted by %s\n"), NET_AdrToString(net_from));
	}
	else
	{
		Con_DPrintf(const_cast<char*>("Connection accepted.\n"));
	}

	// Mark client as connected
	cls.state = ca_connected;

	// Signon process will commence now that server ok'd connection.
	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, "new");

	// We can send a cmd right away
	cls.nextcmdtime = realtime;

	// Not in the demo loop now
	cls.demonum = -1;

	// Need all the signon messages before playing ( or drawing first frame )
	cls.signon = 0;

	// Bump connection time to now so we don't resend a connection
	// Request	
	cls.connect_time = realtime;

	// Haven't gotten a valid frame update yet
	cl.validsequence = 0;

	// We'll request a full delta from the baseline
	cl.delta_sequence = -1;

	// We don't have a backed up cmd history yet
	cls.lastoutgoingcommand = -1;
}

/*
=======================
CL_ClientDllQuery

=======================
*/
int CL_ClientDllQuery(const char *s)
{
	int len;
	int maxsize = 2048;
	byte	data[2048];
	int valid;

	if (!Host_IsServerActive())
		return 0;

	if (Host_IsSinglePlayerGame())   // ignore in single player
		return 0;

	Q_memset(data, 0, maxsize);

	// Allow game .dll to parse string
	len = maxsize - sizeof(int);
	valid = ClientDLL_ConnectionlessPacket(&net_from, s, (char *)(data + sizeof(int)), &len);
	if (len && (len <= (maxsize - 4)))
	{
		// Always prepend connectionless / oob signal to this packet
		*(int *)&data[0] = -1;
		NET_SendPacket(NS_CLIENT, len + 4, data, net_from);
	}

	return valid;
}

void CL_SendRcon(netadr_t *to, unsigned int challenge)
{
	char message[1152];

	if (!g_lastrconcommand[0])
		return Con_Printf(const_cast<char*>("Rcon:  Empty rcon string\n"));

	if (!NET_CompareAdr(*to, g_rconaddress))
		return Con_Printf(const_cast<char*>("Rcon:  Challenge from spoofed/invalid IP address\n"));

	_snprintf(message, sizeof(message), "rcon %u \"%s\" %s", challenge, rcon_password.string, g_lastrconcommand);
	Netchan_OutOfBandPrint(NS_CLIENT, *to, const_cast<char*>("%s"), message);
}

void CL_ServiceChallenge()
{
	char *cmd;

	if (Cmd_Argc() != 3)
		return;

	cmd = (char*)Cmd_Argv(1);

	if (!cmd || !cmd[0] || Q_strcasecmp(cmd, "rcon"))
		return;

	CL_SendRcon(&net_from, Q_atoi(Cmd_Argv(2)));
}

void CL_CreateResponse(netadr_t* to, const char* varname, const char* value)
{
	char response[1024];

	if (value != NULL)
		_snprintf(response, sizeof(response), "%c%c%c%c\"%s\"=\"%s\"\n", 255, 255, 255, 255, varname, value);
	else
		_snprintf(response, sizeof(response), "%c%c%c%c\"%s\"\n", 255, 255, 255, 255, varname);

	NET_SendPacket(NS_CLIENT, Q_strlen(response) + 1, response, *to);
}

void CL_ServiceGet()
{
	char* var;
	cvar_t* cv;
	char* s2;

	if (net_from.type != NA_IP)
		return;

	// check localhost
	if (net_from.ip[0] != 127 || net_from.ip[1] != 0 || net_from.ip[2] != 0 || net_from.ip[3] != 1)
		return;

	if (Cmd_Argc() != 2)
		return;

	var = Cmd_Argv(1);

	if (var == NULL || !var[0])
		return;

	if (!Q_strcasecmp("clientaddress", var))
	{
		s2 = cls.trueaddress;
		CL_CreateResponse(&net_from, var, va((char*)"%s", s2));
	}
	else if (!Q_strcasecmp("serveraddress", var))
	{
		s2 = NET_AdrToString(cls.netchan.remote_address);
		CL_CreateResponse(&net_from, var, va((char*)"%s", s2));
	}
	else if (!Q_strcasecmp("userid", var))
	{
		CL_CreateResponse(&net_from, var, va((char*)"%i", cls.userid));
	}
	else
	{
		cv = Cvar_FindVar(var);
		if (cv != NULL)
			CL_CreateResponse(&net_from, var, va((char*)"%s", cv->string));
		else
			CL_CreateResponse(&net_from, var, "unknown variable");
	}
}

void CL_Listen(char *address)
{
	if (!address)
		return;

	Net_StringToAdr(address, &cls.connect_stream);
	cls.connect_stream.type = NA_IP;

	if (cls.connect_stream.ip[0] < 224 || cls.connect_stream.ip[0] > 239)
		return Con_Printf(const_cast<char*>("%s is not a class D IP.\n"), address);

	if (cls.demoplayback)
		CL_StopPlayback();

	CL_Disconnect();
	Q_memset(msg_buckets, 0, sizeof(msg_buckets));
	Q_memset(total_data, 0, sizeof(total_data));
	cls.connect_retry = 0;
	gfExtendedError = false;
	NET_Config(true);
	Netchan_Setup(NS_MULTICAST, &cls.netchan, cls.connect_stream, -1, &cls, CL_GetFragmentSize);

	NET_FlushSocket(NS_MULTICAST);
	cls.passive = true;

	if (!cls.connect_stream.port)
		cls.connect_stream.port = BigShort(Q_atoi(PORT_MATCHMAKING));

	NET_JoinGroup(cls.netchan.sock, cls.connect_stream);

	Con_Printf(const_cast<char*>("Joining multicast group %s ...\n"), NET_AdrToString(cls.connect_stream));

	NET_ClearLagData(true, false);
	cls.state = ca_connecting;

	cl.delta_sequence = -1;
	cl.validsequence = 0;
	cls.demonum = -1;
	cls.signon = 0;
	cls.connect_time = cls.nextcmdtime = realtime;
	cls.lastoutgoingcommand = -1;
}

void CL_Listen_f()
{
	char *s;

	if (Cmd_Argc() <= 1)
		return Con_Printf(const_cast<char*>("usage: listen <IP>\n"));
	
	s = (char*)Cmd_Args();
	if (s)
	{
		cls.challenge = 0;
		CL_Listen(s);
	}
}

void CL_Commentator_f()
{
	const char *val;
	int iArg;

	if (Cmd_Argc() <= 1)
	{
		val = Info_ValueForKey(cls.userinfo, "*hltv");
		if (Q_strlen(val) && Q_atoi(val) == 3)
			Con_Printf(const_cast<char*>("HLTV commentator mode enabled.\n"));
		else
			Con_Printf(const_cast<char*>("HLTV commentator mode disabled.\n"));
	}
	else
	{
		iArg = Q_atoi(Cmd_Args());
		if (iArg == 0)
			return Info_SetValueForStarKey(cls.userinfo, "*hltv", va(const_cast<char*>("%i"), 0), sizeof(cls.userinfo));
		if (iArg == 1)
			return Info_SetValueForStarKey(cls.userinfo, "*hltv", va(const_cast<char*>("%i"), 3), sizeof(cls.userinfo));
	}
}

qboolean IsFromConnectedServer(netadr_t adr)
{
	if (NET_CompareAdr(adr, g_GameServerAddress))
		return true;

	if (NET_CompareAdr(adr, g_rconaddress))
		return true;

	return false;
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket(void)
{
	char	*s;
	char    *cmd;
	char	*args;
	unsigned char c = 0;
	int proto, infoByte;
	char buffer[260];
	server_cache_t szCommand;

	MSG_BeginReading();
	// skip the -1 marker
	MSG_ReadLong();

	s = MSG_ReadStringLine();

	args = s;

	Cmd_TokenizeString(s);

	cmd = (char*)Cmd_Argv(0);
	if (cmd)
	{
		c = (unsigned char)cmd[0];
	}

	if (!Q_strcasecmp(cmd, "challenge") && IsFromConnectedServer(net_from))
	{
		CL_ServiceChallenge();
		return;
	}

	switch (c)
	{
	case S2C_CONNECTION:
		if (IsFromConnectedServer(net_from))
			CL_ConnectClient();
		break;
	case S2C_LISTEN:
		// Response from getchallenge we sent to the server we are connecting to
		// Blow it off if we are not connected.
		if (cls.state != ca_connecting)
			break;

		if (Cmd_Argc() != 4)
			return;

		cls.challenge = Q_atoi(Cmd_Argv(1));
		cls.authprotocol = Q_atoi(Cmd_Argv(2));

		if (cls.authprotocol == (unsigned char)-1)
		{
			cls.authprotocol = PROTOCOL_HASHEDCDKEY;
		}
		else if (cls.authprotocol != PROTOCOL_HASHEDCDKEY)
		{
			Con_Printf(const_cast<char*>("Invalid multicast authentication type (%i).\n"), cls.authprotocol);
			CL_Disconnect_f();
			break;
		}
			
		CL_Listen((char*)Cmd_Argv(3));
		break;
	case S2C_CHALLENGE:
		if (!IsFromConnectedServer(net_from))
			return;

		// Response from getchallenge we sent to the server we are connecting to
		// Blow it off if we are not connected.
		if (cls.state != ca_connecting)
			break;

		if (Cmd_Argc() != 3 && Cmd_Argc() != 5)
			return;

		cls.challenge = ( unsigned int )Q_atoi( Cmd_Argv(1) );
		cls.authprotocol = ( int )Q_atoi( Cmd_Argv(2) ) ;

		if (cls.authprotocol == (unsigned char)-1)
		{
			cls.authprotocol = PROTOCOL_HASHEDCDKEY;
		}

		if (cls.authprotocol == PROTOCOL_STEAM)
		{
			if (Cmd_Argc() == 5)
			{
				char *isSecure, *steamID = (char*)Cmd_Argv(3);
				
				if (!steamID)
				{
					Con_Printf(const_cast<char*>("STEAM keysize is bogus (1)\n"));
					CL_Disconnect_f();
					break;
				}

				isSecure = (char*)Cmd_Argv(4);

				if (!isSecure)
				{
					Con_Printf(const_cast<char*>("STEAM keysize is bogus (2)\n"));
					CL_Disconnect_f();
					break;
				}

				cls.isVAC2Secure = Q_atoi(isSecure);
				cls.GameServerSteamID = _atoi64(steamID);
				CL_SendConnectPacket();
			}
		}
		else
		{
			//Con_Printf( "Using Normal\n");
			CL_SendConnectPacket();
		}
		break;
	case A2A_PRINT:
		if (!IsFromConnectedServer(net_from))
			return;

		MSG_BeginReading ();
		MSG_ReadLong ();		// skip the -1 marker
		MSG_ReadByte();

		Con_Printf ( const_cast<char*>("%s\n"), MSG_ReadString() );
		break;
	case S2C_BADPASSWORD:
		if (!IsFromConnectedServer(net_from))
			return;

		if (cls.state != ca_connecting)  // Spoofed?
			return;

		s++;

		if (!Q_strncasecmp(s, "BADPASSWORD", Q_strlen("BADPASSWORD")))
			s += Q_strlen("BADPASSWORD");

		Con_Printf(s);

		// Force bad pw dialog to come up now.
		COM_ExplainDisconnection(false, const_cast<char*>("#GameUI_ServerConnectionFailedBadPassword"));
		Con_Printf(const_cast<char*>("Invalid server password.\n"));
		CL_Disconnect();
		break;
	case S2C_REDIRECT:
		if (!IsFromConnectedServer(net_from))
			return;

		if (cls.state != ca_connecting)  // Spoofed?
			return;

		s++;

		if (strchr(s, '\n') || strchr(s, ';'))
		{
			COM_ExplainDisconnection(true, const_cast<char*>("Invalid command separator in redirect server name, disconnecting.\n"));
			CL_Disconnect();
			break;
		}

		_snprintf(buffer, sizeof(buffer), "connect %s\n", s);
		Cbuf_AddText(buffer);
		Con_Printf(const_cast<char*>("Redirecting connection to %s.\n"), s);
		g_bRedirectedToProxy = true;
		break;
	case S2C_CONNREJECT:
		if (!IsFromConnectedServer(net_from))
			return;

		if (!COM_CheckParm(const_cast<char*>("-steam")) && cls.state != ca_connecting)  // Spoofed?
			return;

		s++;

		// Force failure dialog to come up now.
		COM_ExplainDisconnection(true, s);
		CL_Disconnect();
		break;
	case S2A_INFO_STATUS:
		memset(&szCommand, 0, sizeof(szCommand));
		szCommand.adr = net_from;

		Q_strncpy_s(szCommand.name, &s[2], sizeof(szCommand.name));
		Q_strncpy_s(szCommand.map, MSG_ReadString(), sizeof(szCommand.map));
		MSG_SkipString();
		Q_strncpy_s(szCommand.desc, MSG_ReadString(), sizeof(szCommand.desc));
		MSG_ReadShort();

		szCommand.players = MSG_ReadByte();
		szCommand.maxplayers = MSG_ReadByte();
		szCommand.bots = MSG_ReadByte();
		szCommand.type = MSG_ReadByte();
		szCommand.os = MSG_ReadByte();
		szCommand.is_private = MSG_ReadByte() != 0;
		szCommand.is_vac = MSG_ReadByte() != 0;
		MSG_SkipString();

		infoByte = MSG_ReadByte();
		if (!msg_badread)
		{
			if (infoByte & S2A_EXTRA_DATA_HAS_GAME_PORT)
				szCommand.adr.port = Q_ntohs(MSG_ReadShort());

			if (infoByte & S2A_EXTRA_DATA_HAS_STEAMID)
			{
				MSG_ReadLong();
				MSG_ReadLong();
			}

			if (infoByte & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA)
			{
				szCommand.spec_adr = net_from;
				szCommand.spec_adr.port = Q_ntohs(MSG_ReadShort());
				Q_strncpy_s(szCommand.spec_name, MSG_ReadString(), sizeof(szCommand.spec_name));
				szCommand.has_spectator_address = 1;
			}

			if (infoByte & S2A_EXTRA_DATA_HAS_GAMETAG_DATA)
				MSG_SkipString();

			if (infoByte & S2A_EXTRA_DATA_GAMEID)
			{
				MSG_ReadLong();
				MSG_ReadLong();
			}
		}
		CL_AddToServerCache(&szCommand);
		break;
	default:
		if (!CL_ClientDllQuery(s))
			Con_Printf(const_cast<char*>("Unknown command:\n%c\n"), c);
		break;
	}
}

qboolean CL_PrecacheResources()
{
	resource_t* next;

	SetLoadingProgressBarStatusText("#GameUI_PrecachingResources");
	ContinueLoadingProgressBar("ClientConnect", 7, 0.0);

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("begin CL_PrecacheResources()");

	for (resource_t *pResource = cl.resourcesonhand.pNext; pResource != NULL && pResource != &cl.resourcesonhand; pResource = next)
	{
		next = pResource->pNext;

		if (pResource->ucFlags & RES_PRECACHED)
			continue;

		switch (pResource->type)
		{
		case t_sound:
			if (pResource->ucFlags & RES_WASMISSING)
			{
				cl.sound_precache[pResource->nIndex] = NULL;
				pResource->ucFlags |= RES_PRECACHED;
			}
			else
			{
				S_BeginPrecaching();
				cl.sound_precache[pResource->nIndex] = S_PrecacheSound(pResource->szFileName);
				S_EndPrecaching();

				if (cl.sound_precache[pResource->nIndex] || !(pResource->ucFlags & RES_FATALIFMISSING))
					pResource->ucFlags |= RES_PRECACHED;
				else
				{
					COM_ExplainDisconnection(true, const_cast<char*>("Cannot continue without sound %s, disconnecting."), pResource->szFileName);
					CL_Disconnect();
					return FALSE;
				}
			}
			break;
		case t_skin:
		case t_generic:
			pResource->ucFlags |= RES_PRECACHED;
			break;
		case t_model:
			if (pResource->nIndex >= cl.model_precache_count)
				cl.model_precache_count = min(pResource->nIndex + 1, MAX_MODELS);
			
			if (pResource->szFileName[0] == '*')
			{
				pResource->ucFlags |= RES_PRECACHED;
			}
			else
			{
				if (fs_lazy_precache.value == 0.0)
					cl.model_precache[pResource->nIndex] = Mod_ForName(pResource->szFileName, false, true);
				else if (!Q_strncasecmp(pResource->szFileName, "maps", 4))
					cl.model_precache[pResource->nIndex] = Mod_ForName(pResource->szFileName, false, true);
				else
					cl.model_precache[pResource->nIndex] = Mod_FindName(true, pResource->szFileName);

				if (!cl.model_precache[pResource->nIndex])
				{
					if (pResource->ucFlags != 0)
						Con_Printf(const_cast<char*>("Model %s not found and not available from server\n"), pResource->szFileName);
					
					if (pResource->ucFlags & RES_FATALIFMISSING)
					{
						COM_ExplainDisconnection(true, const_cast<char*>("Cannot continue without model %s, disconnecting."), pResource->szFileName);
						CL_Disconnect();
						return FALSE;
					}
				}

				if (!Q_strcasecmp(pResource->szFileName, "models/player.mdl"))
					cl_playerindex = pResource->nIndex;

				pResource->ucFlags |= RES_PRECACHED;
			}
			break;
		case t_decal:
			if (!(pResource->ucFlags & RES_CUSTOM))
				Draw_DecalSetName(pResource->nIndex, pResource->szFileName);
			pResource->ucFlags |= RES_PRECACHED;
			break;
		case t_eventscript:
			cl.event_precache[pResource->nIndex].filename = Mem_Strdup(pResource->szFileName);
			cl.event_precache[pResource->nIndex].pszScript = (char *)COM_LoadFile(pResource->szFileName, 5, 0);
			if (!cl.event_precache[pResource->nIndex].pszScript)
			{
				Con_Printf(const_cast<char*>("Script %s not found and not available from server\n"), pResource->szFileName);
				if (pResource->ucFlags & RES_FATALIFMISSING)
				{
					COM_ExplainDisconnection(true, const_cast<char*>("Cannot continue without script %s, disconnecting."), pResource->szFileName);
					CL_Disconnect();
					return FALSE;
				}
			}
			
			pResource->ucFlags |= RES_PRECACHED;
			break;
		default:
			Con_DPrintf(const_cast<char*>("Unknown resource type\n"));
			pResource->ucFlags |= RES_PRECACHED;
			break;
		}
	}

	if (fs_startup_timings.value == 0.0)
		return true;

	AddStartupTiming("end  CL_PrecacheResources()");

	return true;
}

void CL_MoveToOnHandList(resource_t *pResource)
{
	if (pResource)
	{
		// перемещение ресурса из двухсвязного списка "необходимых" в "имеющиеся"
		CL_RemoveFromResourceList(pResource);
		CL_AddToResourceList(pResource, &cl.resourcesonhand);
	}
	else
	{
		Con_DPrintf(const_cast<char*>("Null resource passed to CL_MoveToOnHandList\n"));
	}
}

const char* CL_CleanFileName( const char* filename )
{
	if( filename && filename[0] == '!')
		return "customization";

	return filename;
}

void CL_RegisterCustomization(resource_t *resource)
{
	customization_t *pcustom;

	for (pcustom = cl.players[resource->playernum].customdata.pNext; pcustom != NULL; pcustom = pcustom->pNext)
	{
		if (!Q_memcmp(pcustom->resource.rgucMD5_hash, resource->rgucMD5_hash, sizeof(resource->rgucMD5_hash)))
			return Con_DPrintf(const_cast<char*>("Duplicate resource received and ignored.\n"));
	}

	if (COM_CreateCustomization(&cl.players[resource->playernum].customdata, resource, resource->playernum, FCUST_FROMHPAK, 0, 0) == false)
		Con_Printf(const_cast<char*>("Unable to create custom decal for player %i\n"), resource->playernum);
}

void CL_ProcessFile(qboolean successfully_received, const char *filename)
{
	byte data[20480];
	sizebuf_t msg;
	const char *pfilename;
	int tempbuffersize, playernum;
	byte hash[16];
	qboolean bCondPassed;

	if (successfully_received)
	{
		if (filename && filename[0] != '!')
			Con_Printf(const_cast<char*>("Processing %s\n"), CL_CleanFileName(filename));
	}
	else
		Con_Printf(const_cast<char*>("Error: server failed to transmit file '%s'\n"), CL_CleanFileName(filename));

	if (!Q_strncasecmp(filename, "sound/", Q_strlen("sound/")))
		pfilename = &filename[Q_strlen("sound/")];
	else
		pfilename = filename;

	for (resource_t *p = cl.resourcesneeded.pNext; p != &cl.resourcesneeded; p = p->pNext)
	{
		if (!Q_strncasecmp(filename, "!MD5", 4))
		{
			COM_HexConvert(&pfilename[4], 32, hash);
			bCondPassed = Q_memcmp(p->rgucMD5_hash, hash, sizeof(hash));
		}
		else
			bCondPassed = p->type == t_generic ? Q_strcasecmp(p->szFileName, filename) : Q_strcasecmp(p->szFileName, pfilename);
		
		if (bCondPassed)
			continue;

		if (successfully_received)
			p->ucFlags &= ~RES_WASMISSING;

		if (filename[0] == '!' && cls.netchan.tempbuffer != NULL)
		{
			if (p->nDownloadSize == cls.netchan.tempbuffersize)
			{
				if (p->ucFlags & RES_CUSTOM)
				{
					HPAK_AddLump(true, const_cast<char*>("custom.hpk"), p, cls.netchan.tempbuffer, 0);
					CL_RegisterCustomization(p);
				}
			}
			else
				Con_Printf(const_cast<char*>(__FUNCTION__ ":  Downloaded %i bytes for purported %i byte file, ignoring download\n"), cls.netchan.tempbuffersize, p->nDownloadSize);
		}

		if (cls.netchan.tempbuffer != NULL)
			Mem_Free(cls.netchan.tempbuffer);

		cls.netchan.tempbuffer = NULL;
		cls.netchan.tempbuffersize = 0;
		CL_MoveToOnHandList(p);
		break;
	}

	if (cls.state != ca_disconnected)
	{
		if (cl.resourcesneeded.pNext == &cl.resourcesneeded)
		{
			Q_memset(&msg, 0, sizeof(msg));
			msg.buffername = "Resource Registration";
			msg.data = data;
			msg.cursize = 0;
			msg.maxsize = sizeof(data);
			if (CL_PrecacheResources())
				CL_RegisterResources(&msg);
			if (msg.cursize > 0)
			{
				Netchan_CreateFragments(false, &cls.netchan, &msg);
				Netchan_FragSend(&cls.netchan);
			}
		}

		if (cls.netchan.tempbuffer != NULL)
		{
			Con_Printf(const_cast<char*>(__FUNCTION__ ": Received a decal %s, but didn't find it in resources needed list!\n"), pfilename);
			Mem_Free(cls.netchan.tempbuffer);
		}
		cls.netchan.tempbuffer = 0;
		cls.netchan.tempbuffersize = 0;
	}
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage()
{
	if (cls.demoplayback)
		return CL_ReadDemoMessage();

	if (cls.passive)
		return NET_GetPacket(NS_MULTICAST);

	return NET_GetPacket(NS_CLIENT);
}

/*
=================
CL_ReadPackets

Updates the local time and reads/handles messages on client net connection.
=================
*/
void CL_ReadPackets()
{
	int msg_count = 0;
	char buf[128];

	if (cls.state == ca_dedicated)
		return;

	cl.oldtime = cl.time;
	cl.time += host_frametime;

	bool bReceived = false;

	while (CL_GetMessage())
	{
		bReceived = true;

		msg_count++;

		// demo code
		if (!cls.demoplayback)
		{
			if (msg_count > MAX_INCOMING_MESSAGES)
			{
				// Ignore rest of messages this frame, we won't be able to uncompress the delta
				//  if there is one!!!
				continue;
			}
		}

		//
		// remote command packet
		//
		if (*(int *)net_message.data == -1)
		{
			CL_ConnectionlessPacket();
			continue;
		}
		
		//
		// packet from server, verify source IP address.
		//
		if (!cls.passive && !cls.demoplayback &&
			!NET_CompareAdr(net_from, cls.netchan.remote_address))
		{
			continue;
		}

		if (cls.passive && cls.state == ca_connecting)
		{
			cls.state = ca_connected;
		}
		

		// Sequenced message before fully connected, throw away.
		if (cls.state == ca_disconnected ||
			cls.state == ca_connecting)
			continue;		// dump sequenced message from server if not connected yet

		if (!cls.demoplayback && net_message.cursize < 8)
		{
			Con_Printf(const_cast<char*>("CL_ReadPackets:  undersized packet %i bytes from %s\n"), net_message.cursize, NET_AdrToString(net_from));
			continue;
		}

		// Do we have the bandwidth to process this packet and are we
		//  up to date on the reliable message stream.
		if (!cls.demoplayback && !Netchan_Process(&cls.netchan))
			continue;		// wasn't accepted for some reason

		if (cls.demoplayback)
		{
			MSG_BeginReading();
		}

		// Parse out the commands.
		CL_ParseServerMessage();
	}

	if (bReceived && msg_count > MAX_INCOMING_MESSAGES && !cls.demoplayback)
	{
		Con_DPrintf(const_cast<char*>("Ignored %i messages\n"), msg_count - MAX_INCOMING_MESSAGES);
	}

	CL_SetSolidEntities();

	// Check for fragmentation/reassembly related packets.
	if (cls.state != ca_dedicated &&
		cls.state != ca_disconnected &&
		Netchan_IncomingReady(&cls.netchan))
	{

		// Process the incoming buffer(s)
		if (Netchan_CopyNormalFragments(&cls.netchan))
		{
			MSG_BeginReading();
			CL_ParseServerMessage(false);
		}

		if (Netchan_CopyFileFragments(&cls.netchan))
		{
			if (gDownloadFile[0])
			{
				if (gDownloadFile[0] == '!')
				{
					SetSecondaryProgressBarText("#GameUI_SecurityModule");
				}
				else
				{
					_snprintf(buf, sizeof(buf), "File '%s'", gDownloadFile);
					SetSecondaryProgressBarText(buf);
				}
				SetSecondaryProgressBar(100.0f);
			}
			// Remove from resource request stuff.
			CL_ProcessFile(true, cls.netchan.incomingfilename);
		}
	}

	if ((cls.state >= ca_connected) &&
		(!cls.demoplayback) &&
		((realtime - cls.netchan.last_received) > cl_timeout.value))
	{
		COM_ExplainDisconnection(true, const_cast<char*>("#GameUI_ServerConnectionTimeout"));
		CL_Disconnect();
		return;
	}

	if (cl_shownet.value)
		Con_Printf(const_cast<char*>("\n"));

	Netchan_UpdateProgress(&cls.netchan);

	if (scr_downloading.value > 0.0 && gDownloadFile[0])
	{
		if (gDownloadFile[0] == '!')
			_snprintf(buf, sizeof(buf), "#GameUI_DownloadingSecurityModule");
		else
			_snprintf(buf, sizeof(buf), "Downloading file '%s'", gDownloadFile);
		SetSecondaryProgressBar(scr_downloading.value / 100.0f);
		SetSecondaryProgressBarText(buf);
		SetLoadingProgressBarStatusText("#GameUI_VerifyingAndDownloading");
	}

	Net_APICheckTimeouts();
}

resource_t* CL_AddResource(resourcetype_t type, char* name, int size, qboolean bFatalIfMissing, int index)
{
	resource_t* pResource;

	pResource = &cl.resourcelist[cl.num_resources];

	if (cl.num_resources >= MAX_RESOURCE_LIST)
		Sys_Error("Too many resources on client.");

	cl.num_resources++;

	pResource->type = type;
	
	Q_strncpy(pResource->szFileName, name, sizeof(pResource->szFileName) - 1);
	pResource->szFileName[sizeof(pResource->szFileName) - 1] = 0;

	pResource->nDownloadSize = size;
	pResource->ucFlags |= bFatalIfMissing != false ? RES_FATALIFMISSING : 0;
	pResource->nIndex = index;

	return pResource;
}

void CL_CreateResourceList()
{
	if (cls.state == ca_dedicated)
		return;

	HPAK_FlushHostQueue();

	cl.num_resources = 0;

	char szFileName[MAX_PATH];
	_snprintf(szFileName, sizeof(szFileName), "tempdecal.wad");

	byte rgucMD5_hash[16];
	Q_memset(rgucMD5_hash, 0, sizeof(rgucMD5_hash));


	FileHandle_t hFile = FS_OpenPathID(szFileName, "rb", "GAMECONFIG");

	if (FILESYSTEM_INVALID_HANDLE != hFile)
	{
		unsigned int uiSize = FS_Size(hFile);

		MD5_Hash_File(rgucMD5_hash, szFileName, false, false, NULL);

		if (uiSize)
		{
			resource_t* pLastResource = CL_AddResource(t_decal, szFileName, uiSize, false, 0);

			if (pLastResource != NULL)
			{
				pLastResource->ucFlags |= RES_CUSTOM;

				Q_memcpy(pLastResource->rgucMD5_hash, rgucMD5_hash, sizeof(rgucMD5_hash));

				HPAK_AddLump(false, const_cast<char*>("custom.hpk"), pLastResource, nullptr, hFile);
			}
		}

		FS_Close(hFile);
	}
}

void CL_ClearCaches()
{
	for( int i = 1; i < ARRAYSIZE( cl.event_precache ) && cl.event_precache[ i ].pszScript; ++i )
	{
		Mem_Free( const_cast<char*>( cl.event_precache[ i ].pszScript ) );
		Mem_Free( const_cast<char*>( cl.event_precache[ i ].filename ) );

		Q_memset( &cl.event_precache[ i ], 0, sizeof( cl.event_precache[ i ] ) );
	}
}

void CL_ClearClientState()
{
	for( int i = 0; i < CL_UPDATE_BACKUP; ++i )
	{
		if( cl.frames[ i ].packet_entities.entities )
		{
			Mem_Free( cl.frames[ i ].packet_entities.entities );
		}

		cl.frames[ i ].packet_entities.entities = NULL;
		cl.frames[ i ].packet_entities.num_entities = 0;
	}

	CL_ClearResourceLists();

	for( int i = 0; i < MAX_CLIENTS; ++i )
	{
		COM_ClearCustomizationList( &cl.players[ i ].customdata, false );
	}

	CL_ClearCaches();

	Q_memset( &cl, 0, sizeof( cl ) );

	cl.resourcesneeded.pPrev = &cl.resourcesneeded;
	cl.resourcesneeded.pNext = &cl.resourcesneeded;
	cl.resourcesonhand.pPrev = &cl.resourcesonhand;
	cl.resourcesonhand.pNext = &cl.resourcesonhand;

	CL_CreateResourceList();
}

void CL_ClearState( qboolean bQuiet )
{
	if( !Host_IsServerActive() )
		Host_ClearMemory( bQuiet );

	CL_ClearClientState();

	SZ_Clear( &cls.netchan.message );

	// clear other arrays
	Q_memset( cl_efrags, 0, sizeof( cl_efrags ) );
	Q_memset( cl_dlights, 0, sizeof( cl_dlights ) );
	Q_memset( cl_elights, 0, sizeof( cl_elights ) );
	Q_memset( cl_lightstyle, 0, sizeof( cl_lightstyle ) );

	CL_TempEntInit();

	//
	// allocate the efrags and chain together into a free list
	//
	cl.free_efrags = cl_efrags;

	int i;
	for( i = 0; i < MAX_EFRAGS - 1; ++i )
	{
		cl.free_efrags[ i ].entnext = &cl.free_efrags[ i + 1 ];
	}

	cl.free_efrags[ i ].entnext = NULL;
}

void CL_Disconnect(void)
{
	cls.connect_time = -99999.0;
	cls.connect_retry = 0;
	g_careerState = CAREER_NONE;

	SetCareerAudioState(0);

	SPR_Shutdown_NoModelFree();

	S_StopAllSounds(true);

	if (cls.demoplayback)
	{
		CL_StopPlayback();
	}
	else if (cls.state == ca_active || cls.state == ca_connected || cls.state == ca_uninitialized || cls.state == ca_connecting)
	{
		if (cls.demorecording)
			CL_Stop_f();

		FS_LogLevelLoadStarted("ExitGame");

		if (cls.passive)
		{			
			NET_LeaveGroup(cls.netchan.sock, cls.connect_stream);
			NET_LeaveGroup(cls.netchan.sock, cls.game_stream);
		}
		else if (cls.netchan.remote_address.type != NA_UNUSED)
		{
			char final[20];

			final[0] = clc_stringcmd;
			Q_strcpy(&final[1], "dropclient\n");

			Netchan_Transmit(&cls.netchan, 13, (byte*)final);
			Netchan_Transmit(&cls.netchan, 13, (byte*)final);
			Netchan_Transmit(&cls.netchan, 13, (byte*)final);
		}

		DWORD curip;

		curip = *(DWORD*)&g_GameServerAddress.ip[0];

		cls.state = ca_disconnected;

		if (g_GameServerAddress.type == NA_LOOPBACK)
			curip = *(DWORD*)&net_local_adr.ip[0];

		Steam_GSTerminateGameConnection(curip, g_GameServerAddress.port);

		Q_memset(&g_GameServerAddress, 0, sizeof(g_GameServerAddress));

		if (Host_IsServerActive())
			Host_ShutdownServer(false);

		CloseSecurityModule();
	}

	cls.timedemo = false;
	cls.demoplayback = false;
	cls.signon = 0;

	CL_ClearState(true);

	Netchan_Clear(&cls.netchan);

	CL_DeallocateDynamicData();

	scr_downloading.value = -1.0f;
	sys_timescale.value = 1.0f;
	g_LastScreenUpdateTime = 0.0f;

	VGuiWrap2_NotifyOfServerDisconnect();

	StopLoadingProgressBar();
}

void CL_Disconnect_f(void)
{
	CL_Disconnect();

	if (Host_IsServerActive())
		Host_ShutdownServer(false);
}

void CL_ShutDownClientStatic()
{
	for (int i = 0; i < CL_UPDATE_BACKUP; i++)
	{
		if (cl.frames[i].packet_entities.entities)
			Mem_Free(cl.frames[i].packet_entities.entities);

		cl.frames[i].packet_entities.entities = NULL;
	}

	Q_memset(cl.frames, 0, sizeof(frame_t) * CL_UPDATE_BACKUP);
}

void CL_Retry_f()
{
	char szCommand[MAX_PATH];

	if (!cls.servername[0])
		return Con_Printf(const_cast<char*>("Can't retry, no previous connection\n"));

	if (strchr(cls.servername, '\n') || strchr(cls.servername, ':'))
		return Con_Printf(const_cast<char*>("Invalid command separator in server name, refusing retry\n"));
	
	if (cls.passive)
		snprintf(szCommand, sizeof(szCommand), "listen %s\n", cls.servername);
	else
		snprintf(szCommand, sizeof(szCommand), "connect %s\n", cls.servername);

	Cbuf_AddText(szCommand);
	Con_Printf(const_cast<char*>(const_cast<char*>("Commencing connection retry to %s\n")), cls.servername);
}

// HL1 CD Key
#define GUID_LEN 13

/*
=======================
CL_GetCDKeyHash()

Connections will now use a hashed cd key value
A LAN server will know not to allows more then xxx users with the same CD Key
=======================
*/
char *CL_GetCDKeyHash(void)
{
	char szKeyBuffer[256]; // Keys are about 13 chars long.	
	static char szHashedKeyBuffer[256];
	int nKeyLength;
	qboolean bDedicated = false;
	MD5Context_t ctx;
	unsigned char digest[16]; // The MD5 Hash

	Sys_GetCDKey(szKeyBuffer, &nKeyLength, &bDedicated);

	if (bDedicated)
	{
		Con_Printf(const_cast<char*>("Key has no meaning on dedicated server...\n"));
		return const_cast<char*>("");
	}

	if (nKeyLength <= 0 ||
		nKeyLength >= 256)
	{
		Con_Printf(const_cast<char*>("Bogus key length on CD Key...\n"));
		return const_cast<char*>("");
	}

	szKeyBuffer[nKeyLength] = 0;

	// Now get the md5 hash of the key
	Q_memset(&ctx, 0, sizeof(ctx));
	Q_memset(digest, 0, sizeof(digest));

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)szKeyBuffer, nKeyLength);
	MD5Final(digest, &ctx);
	Q_memset(szHashedKeyBuffer, 0, sizeof(szHashedKeyBuffer));
	Q_strncpy(szHashedKeyBuffer, MD5_Print(digest), sizeof(szHashedKeyBuffer) - 1);
	szHashedKeyBuffer[sizeof(szHashedKeyBuffer) - 1] = 0;

	return szHashedKeyBuffer;
}

void CL_ConnectTest()
{
}

//-----------------------------------------------------------------------------
// Purpose: called by CL_Connect and CL_CheckResend
// If we are in ca_connecting state and we have gotten a challenge
//   response before the timeout, send another "connect" request.
// Output : void CL_SendConnectPacket
//-----------------------------------------------------------------------------
void CL_SendConnectPacket(void)
{
	netadr_t adr{};
	char	data[2048];
	char szServerName[MAX_OSPATH];
	int len;
	unsigned char buffer[1024];
	char protinfo[1024];
	char rgchSteam3LoginCookie[1024];
	byte authprotocol;
	int steamproto;
	int steampacket;
	int length;

	Q_memset(protinfo, 0, sizeof(protinfo));
	ContinueLoadingProgressBar("ClientConnect", 1, 0.0);
	if (fs_startup_timings.value)
		SetupStartupTimings();

	Q_strncpy(szServerName, cls.servername, MAX_OSPATH);
	szServerName[MAX_OSPATH - 1] = 0;

	// Deal with local connection.
	if (!stricmp(cls.servername, "local"))
	{
		_snprintf(szServerName, sizeof(szServerName), "%s", "localhost");
	}

	if (!NET_StringToAdr(szServerName, &adr))
	{
		Con_Printf(const_cast<char*>("Bad server address\n"));
		cls.connect_time = -99999.0;
		cls.connect_retry = 0;
		cls.state = ca_disconnected;
		Q_memset(&g_GameServerAddress, 0, sizeof(g_GameServerAddress));
		return;
	}

	if (adr.port == (unsigned short)0)
	{
		adr.port = BigShort((unsigned short)atoi(PORT_SERVER));
	}

	switch (cls.authprotocol)
	{
	default:
		cls.authprotocol = PROTOCOL_HASHEDCDKEY;
		// Fall through, bogus protocol type, use CD key hash.
	case PROTOCOL_HASHEDCDKEY:
		len = strlen(CL_GetCDKeyHash());
		Q_strncpy((char*)buffer, CL_GetCDKeyHash(), sizeof(buffer)-1);
		buffer[sizeof(buffer)-1] - 0;
		Info_SetValueForKey(protinfo, "prot", va(const_cast<char*>("%i"), cls.authprotocol), sizeof(protinfo));
		Info_SetValueForKey(protinfo, "raw", (char*)buffer, sizeof(protinfo));
		steamproto = 0;
		break;
	case PROTOCOL_STEAM:
		Info_SetValueForKey(protinfo, "prot", va(const_cast<char*>("%i"), cls.authprotocol), sizeof(protinfo));
		steamproto = 1;
		Info_SetValueForKey(protinfo, "unique", va(const_cast<char*>("%i"), -1), sizeof(protinfo));
		Info_SetValueForKey(protinfo, "raw", "steam", sizeof(protinfo));
		break;
	}

	if (cls.authprotocol != PROTOCOL_HASHEDCDKEY)
		Info_SetValueForKey(protinfo, "cdkey", CL_GetCDKeyHash(), sizeof(protinfo));

	Q_memset(rgchSteam3LoginCookie, 0, 1024);

	if (steamproto)
	{
		steampacket = Steam_GSInitiateGameConnection(rgchSteam3LoginCookie, sizeof(rgchSteam3LoginCookie), cls.GameServerSteamID, 
			adr.type == NA_LOOPBACK ? *(uint32*)&net_local_adr.ip[0] : *(uint32*)&adr.ip[0], adr.port, cls.isVAC2Secure);
	}

	// Mark time of this attempt for retransmit requests
	cls.connect_time = realtime;

	_snprintf(data, sizeof(data), "%c%c%c%cconnect %i %i \"%s\" \"%s\"\n", 255, 255, 255, 255,
		PROTOCOL_VERSION, cls.challenge, protinfo, cls.userinfo);  // Send protocol and challenge value

	length = Q_strlen(data);

	if (steamproto)
	{
		if ((length + steampacket) > sizeof(data))
		{
			Con_Printf(const_cast<char*>("CL_SendConnectPacket:  Potential buffer overrun (%i/%i>%i) copying steamkeybuffer into connection packet\n"), length, steampacket, sizeof(data));
			CL_Disconnect();
			return;
		}

		Q_memcpy(&data[length], rgchSteam3LoginCookie, steampacket);
		length += steampacket;
	}

	g_GameServerAddress = adr;

	NET_SendPacket(NS_CLIENT, length, data, adr);
}

void CL_CheckForResend()
{
	netadr_t adr;
	char szServerName[MAX_PATH];
	float cvar_new;
	char data[2048];

	CL_ConnectTest();

	if (cls.state == ca_disconnected && Host_IsServerActive())
	{
		cls.state = ca_connecting;
		gfExtendedError = false;
		Q_strncpy(cls.servername, "localhost", sizeof(cls.servername) - 1);
		cls.servername[sizeof(cls.servername) - 1] = 0;
		cls.authprotocol = PROTOCOL_STEAM;
		CL_SendConnectPacket();
	}

	if (cls.state == ca_connecting)
	{
		if (cls.passive)
		{
			if (realtime - cls.connect_time > CLIENT_RECONNECT_TIME)
			{
				if (cls.challenge)
				{
					Con_Printf(const_cast<char*>("Multicast failed. Falling back to unicast...\n"));
					cls.passive = false;
					NET_LeaveGroup(cls.netchan.sock, cls.connect_stream);
					CL_SendConnectPacket();
				}
				else
				{
					COM_ExplainDisconnection(true, const_cast<char*>("Joining multicast group failed after %.0f seconds."), CLIENT_RECONNECT_TIME);
					cls.connect_time = -99999.0;
					cls.connect_retry = 0;
					cls.state = ca_disconnected;
					Q_memset(&g_GameServerAddress, 0, sizeof(g_GameServerAddress));
					cls.passive = false;
					NET_LeaveGroup(cls.netchan.sock, cls.connect_stream);
				}
			}

			return;
		}

		cvar_new = clamp(cl_resend.value, 1.5f, 20.f);
		if (cl_resend.value != cvar_new)
			Cvar_SetValue(const_cast<char*>("cl_resend"), cvar_new);

		if (cl_resend.value > realtime - cls.connect_time)
			return;

		Q_strncpy(szServerName, cls.servername, sizeof(szServerName));
		szServerName[sizeof(szServerName)-1] = 0;

		if (!Q_strcasecmp(cls.servername, "local"))
			_snprintf(szServerName, sizeof(szServerName), "%s", "localhost");

		if (!NET_StringToAdr(szServerName, &adr))
		{
			Con_Printf(const_cast<char*>("Bad server address\n"));
			cls.state = ca_disconnected;
			Q_memset(&g_GameServerAddress, 0, sizeof(g_GameServerAddress));
			return;
		}


		if (!adr.port)
			adr.port = BigShort(Q_atoi(PORT_SERVER));

		if (cls.connect_retry >= CL_CONNECTION_RETRIES)
		{
			COM_ExplainDisconnection(true, const_cast<char*>("#GameUI_CouldNotContactGameServer"));
			cls.connect_time = -99999.0;
			cls.connect_retry = 0;
			cls.state = ca_disconnected;
			Q_memset(&g_GameServerAddress, 0, sizeof(g_GameServerAddress));
			CL_Disconnect();
			return;
		}

		cls.connect_time = realtime;
		if (Q_strcasecmp(szServerName, "localhost"))
		{
			if (cls.connect_retry)
			{
				if (cls.connect_retry == 2)
				{
					SetLoadingProgressBarStatusText("#GameUI_RetryingConnectionToServer2");
				}
				else if (cls.connect_retry == 3)
				{
					SetLoadingProgressBarStatusText("#GameUI_RetryingConnectionToServer3");
				}
				else
				{
					SetLoadingProgressBarStatusText("#GameUI_RetryingConnectionToServer");
				}
				Con_Printf(const_cast<char*>("Retrying %s...\n"), szServerName);
			}
			else
			{
				Con_Printf(const_cast<char*>("Connecting to %s...\n"), szServerName);
			}
		}

		cls.connect_retry++;
		
		snprintf(data, sizeof(data), "%c%c%c%cgetchallenge steam\n", 255, 255, 255, 255);
		g_GameServerAddress = adr;
		NET_SendPacket(NS_CLIENT, Q_strlen(data), data, adr);
	}
}

void CL_Connect_f()
{
	char name[MAX_PATH];
	CareerStateType career_old;
	int cached_num;

	if (Cmd_Argc() <= 1)
		return Con_Printf(const_cast<char*>("usage: connect <server>\n"));

	SetCStrikeFlags();

	if ((g_bIsCStrike || g_bIsCZero) && cmd_source != src_command)
		return Con_Printf(const_cast<char*>("Connect only works from the console.\n"));

	if (!Cmd_Args())
		return;
	
	Q_strncpy(name, Cmd_Args(), sizeof(name));
	name[sizeof(name) - 1] = 0;

	if (cls.demoplayback)
		CL_StopPlayback();

	career_old = g_careerState;
	CL_Disconnect();
	if (career_old == CAREER_LOADING)
		g_careerState = CAREER_LOADING;

	StartLoadingProgressBar("Connecting", 12);
	SetLoadingProgressBarStatusText("#GameUI_EstablishingConnection");

	cached_num = Q_atoi(name);

	if (!Q_strstr(name, ".") && cached_num > 0 && cached_num <= num_servers)
	{
		Q_strncpy(name, NET_AdrToString(cached_servers[cached_num - 1].adr), sizeof(name));
		name[sizeof(name) - 1] = 0;
	}

	Q_memset(msg_buckets, 0, sizeof(msg_buckets));
	Q_memset(total_data, 0, sizeof(total_data));

	Q_strncpy(cls.servername, name, sizeof(name) - 1);
	cls.servername[sizeof(name) - 1] = 0;

	cls.state = ca_connecting;
	cls.connect_time = -99999.0;
	cls.connect_retry = 0;
	cls.passive = false;
	cls.spectator = false;
	cls.isVAC2Secure = false;
	cls.GameServerSteamID = 0;
	cls.build_num = 0;

	Q_memset(&g_GameServerAddress, 0, sizeof(g_GameServerAddress));

	cls.challenge = 0;
	gfExtendedError = false;
	g_LastScreenUpdateTime = 0.0;

	if (Q_strncasecmp(cls.servername, "local", 5))
		NET_Config(true);
}

//-----------------------------------------------------------------------------
// Purpose: A svc_signonnum has been received, perform a client side setup
// Output : void CL_SignonReply
//-----------------------------------------------------------------------------
void CL_SignonReply(void)
{
	Con_DPrintf (const_cast<char*>("CL_SignonReply: %i\n"), cls.signon);

	switch (cls.signon)
	{
	case 1:
	{
			  Cache_Report();
			  MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
			  MSG_WriteString(&cls.netchan.message, "sendents");
	}
		break;

	case SIGNONS:
	{
			  // Tell client .dll about the transition
			  char mapname[256];
			  int len;

			  SCR_EndLoadingPlaque();
			  sscanf(cl.levelname, "maps/%s.bsp", mapname);

			  len = strlen(mapname);
			  if (len > 4 && !strcmp(&mapname[len - 4], ".bsp"))
				  mapname[len - 4] = 0;

			  FS_LogLevelLoadFinished(mapname);
			  VGuiWrap2_LoadingFinished("level", mapname);

			  if (g_bRedirectedToProxy)
			  {
				  if (cls.spectator == false)
				  {
					  Con_Printf(const_cast<char*>("Redirected to invalid server\n"));
					  CL_Disconnect_f();
				  }
			  }
			  g_bRedirectedToProxy = 0;

			  if (cls.demoplayback)
				  Sequence_OnLevelLoad(mapname);
	}

		break;
	}
}

void CL_PrintEntities_f()
{
	for (int i = 0; i < cl.num_entities; i++)
	{
		Con_Printf(const_cast<char*>("%3i:"), i);

		if (cl_entities[i].model != NULL)
		{
			Con_Printf(const_cast<char*>("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n"), cl_entities[i].model->name, cl_entities[i].curstate.frame,
				cl_entities[i].origin[0], cl_entities[i].origin[1], cl_entities[i].origin[2], 
				cl_entities[i].angles[0], cl_entities[i].angles[1], cl_entities[i].angles[2]);
		}
	}
}

void CL_TakeSnapshot_f()
{
	FileHandle_t f;
	char base[64]; 
	char filename[64]; 

	if (cl.num_entities && cl_entities->model != NULL)
		COM_FileBase(cl_entities->model->name, base);
	else
		Q_strcpy(base, "Snapshot");

	for (int i = 0; i < 1000; i++)
	{
		snprintf(filename, sizeof(filename), "%s%04d.bmp", base, i);
		f = FS_OpenPathID(filename, "r", "GAMECONFIG");

		if (!f)
			return VID_TakeSnapshot(filename);
		
		FS_Close(f);
	}
	Con_Printf(const_cast<char*>("Unable to take a screenshot.\n"));
}

void CL_StartMovie_f()
{
	char *pfile;

	if (cmd_source == src_command)
	{
		if (Cmd_Argc() == 3)
		{
			if (cls.demoplayback)
			{
				pfile = (char*)Cmd_Argv(1);
				if (pfile[0] == '/' || strstr(pfile, ":") || strstr(pfile, "..") || strstr(pfile, "\\"))
				{
					Con_Printf(const_cast<char*>("Couldn't open %s (contains illegal characters).\n"), pfile);
				}
				else
				{
					cl_inmovie = true;
					host_framerate.value = 1.f / Q_atof(Cmd_Argv(2));
					VID_WriteBuffer(Cmd_Argv(1));
					Con_Printf(const_cast<char*>("Started recording movie...\n"));
				}
			}
			else
			{
				Con_Printf(const_cast<char*>("startmovie only allowed during demo playback.\n"));
			}
		}
		else
		{
			Con_Printf(const_cast<char*>("startmovie <filename> <fps>\n"));
		}
	}
}

void CL_EndMovie_f()
{
	if (cl_inmovie)
	{
		cl_inmovie = false;
		host_framerate.value = 0.0;
		Con_Printf(const_cast<char*>("Stopped recording movie...\n"));
	}
	else
	{
		Con_Printf(const_cast<char*>("No movie started.\n"));
	}
}

/*
=====================
CL_Rcon_f

Send the rest of the command line over as
an unconnected command.
=====================
*/
void CL_Rcon_f(void)
{
	char	message[1024];   // Command message
	char    szParam[256];
	int		i;
	netadr_t to;    // Command destination
	unsigned short nPort;    // Outgoing port.

	if (!rcon_password.string)
	{
		Con_Printf(const_cast<char*>("You must set 'rcon_password' before\n"
			"issuing an rcon command.\n"));
		return;
	}

	if (!Cmd_Args())
		return Con_Printf(const_cast<char*>("Rcon:  Empty rcon string\n"));


	// If we are connected to a server, send rcon to it, otherwise
	//  send rcon commaand to whereever we specify in rcon_address, if possible.
	if (cls.state < ca_connected)
	{
		NET_Config(true);		// allow remote	
		if (!Q_strlen(rcon_address.string))
		{
			Con_Printf(const_cast<char*>("You must either be connected,\n"
				"or set the 'rcon_address' cvar\n"
				"to issue rcon commands\n"));

			return;
		}
		if (!NET_StringToAdr(rcon_address.string, &to))
		{
			Con_Printf(const_cast<char*>("Unable to resolve rcon address %s\n"), rcon_address.string);
			return;
		}
	}
	else if (Q_strlen(rcon_address.string))
	{
		if (!NET_StringToAdr(rcon_address.string, &to))
			return Con_Printf(const_cast<char*>("Unable to resolve rcon address %s\n"), rcon_address.string);
	}
	else
		to = cls.netchan.remote_address;

	nPort = (unsigned short)rcon_port.value;  // ?? BigShort
	if (!nPort)
	{
		if (cls.state <= ca_connecting)
			nPort = Q_atoi(PORT_SERVER);
		else
			nPort = BigShort(cls.netchan.remote_address.port);
	}

	to.port = BigShort(nPort);

	Q_strncpy(g_lastrconcommand, Cmd_Args(), sizeof(g_lastrconcommand) - 1);
	g_lastrconcommand[sizeof(g_lastrconcommand) - 1] = 0;
	g_rconaddress = to;
	Netchan_OutOfBandPrint(NS_CLIENT, to, const_cast<char*>("challenge rcon\n"));
}

void SetPal(int i)
{
	//Nothing
}

dlight_t* CL_AllocDlight( int key )
{
	int		i;
	dlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i = 0; i<MAX_DLIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				Q_memset(dl, 0, sizeof(*dl));
				dl->key = key;
				r_dlightchanged |= 1 << i;
				r_dlightactive |= 1 << i;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i = 0; i<MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			Q_memset(dl, 0, sizeof(*dl));
			dl->key = key;
			r_dlightchanged |= 1 << i;
			r_dlightactive |= 1 << i;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	Q_memset(dl, 0, sizeof(*dl));
	dl->key = key;
	r_dlightchanged |= 1;
	r_dlightactive |= 1;
	return dl;

}

dlight_t* CL_AllocElight( int key )
{
	int		i;
	dlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_elights;
		for (i = 0; i<MAX_ELIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				Q_memset(dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_elights;
	for (i = 0; i<MAX_ELIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			Q_memset(dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_elights[0];
	Q_memset(dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

void CL_DecayLights(void)
{
	int			i;
	dlight_t* dl;
	float		time;

	if (cls.signon != SIGNONS)
		return;

	time = cl.time - cl.oldtime;

	dl = cl_dlights;

	r_dlightchanged = 0;
	r_dlightactive = 0;

	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (!dl->radius)
			continue;

		if (dl->die < cl.time)
		{
			r_dlightchanged |= (1 << i);
			dl->radius = 0;
			continue;
		}
		
		if (dl->decay)
		{
			r_dlightchanged |= (1 << i);

			dl->radius -= time * dl->decay;
			if (dl->radius < 0)
				dl->radius = 0;
		}

		if (dl->radius != 0)
		{
			r_dlightactive |= (1 << i);
		}
	}

	dl = cl_elights;
	for (i = 0; i < MAX_ELIGHTS; i++, dl++)
	{
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
		{
			dl->radius = 0;
			continue;
		}

		dl->radius -= time * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

/*
=================
CL_ComputePacketLoss

=================
*/
void CL_ComputePacketLoss(void)
{
	int i;
	int frm;
	frame_t *frame;
	int count = 0;
	int lost = 0;

	if (realtime < cls.packet_loss_recalc_time)
		return;

	cls.packet_loss_recalc_time = realtime + CL_PACKETLOSS_RECALC;

	// Compuate packet loss
	// Fill in frame data
	for (i = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP + 1
		; i <= cls.netchan.incoming_sequence
		; i++)
	{
		frm = i;
		frame = &cl.frames[frm &CL_UPDATE_MASK];

		if (frame->receivedtime == -1)
		{
			lost++;
		}
		count++;
	}

	if (count <= 0)
	{
		cls.packet_loss = 0.0;
	}
	else
	{
		cls.packet_loss = (100.0 * (float)lost) / (float)count;
	}
}

int CL_DriftInterpolationAmount(int goal)
{
	float fgoal, maxmove, diff;

	fgoal = (float)goal / 1000.f;

	if (fgoal != g_flInterpolationAmount)
	{
		maxmove = host_frametime * 0.05f;

		diff = fgoal - g_flInterpolationAmount;

		if (diff > 0.f)
		{
			if (diff > maxmove)
				diff = maxmove;
		}
		else
		{
			if (-diff > maxmove)
				diff = -maxmove;
		}

		g_flInterpolationAmount += diff;
	}

	// ТЦК та МСЕК за гроші пропонують варіанти
	float msec = g_flInterpolationAmount * 1000.f;

	return clamp((int)msec, 0, 100);
}

void CL_ComputeClientInterpolationAmount(usercmd_t *cmd)
{
	int extime, intime, temp;
	float fInterp;

	if (cl_updaterate.value < 10.0f)
	{
		Cvar_Set(const_cast<char*>("cl_updaterate"), const_cast<char*>("20"));
		Con_Printf(const_cast<char*>("cl_updaterate minimum is %f, resetting to default (20)\n"), 10.0f);
	}
	if (cl_updaterate.value > 102.0f)
	{
		Cvar_Set(const_cast<char*>("cl_updaterate"), const_cast<char*>("102"));
		Con_Printf(const_cast<char*>("cl_updaterate maximum is %f, resetting to maximum (102)\n"), 102.0f);
	}

	extime = 50;
	intime = !cls.spectator ? 100 : 200;

	if (cl_updaterate.value > 0.f)
	{
		extime = 1000.f / cl_updaterate.value;
		if (extime < 1)
			extime = 1;
	}

	temp = 1000.f * ex_interp.value;

	if (extime > temp + 1)
	{
		temp = extime;
		Con_Printf(const_cast<char*>("ex_interp forced up to %i msec\n"), extime);
		Cvar_Set(const_cast<char*>("ex_interp"), va(const_cast<char*>("%f"), (float)temp / 1000.f));
	}
	else
	{
		if (intime < temp - 1)
		{
			temp = intime;
			Con_Printf(const_cast<char*>("ex_interp forced down to %i msec\n"), intime);
			Cvar_Set(const_cast<char*>("ex_interp"), va(const_cast<char*>("%f"), (float)temp / 1000.f));
		}
	}

	if (extime > temp)
		temp = extime;
	if (intime < temp)
		temp = intime;

	cmd->lerp_msec = CL_DriftInterpolationAmount(temp);
}

BOOL UserIsConnectedOnLoopback()
{
	return cls.netchan.remote_address.type == NA_LOOPBACK;
}

void CL_Move()
{
	byte data[MAX_CMD_BUFFER];
	usercmd_t cmdbaseline;
	sizebuf_t buf;
	con_nprint_t np;
	usercmd_t *p_cmdbaseline;
	int				i;
	cmd_t			*cmd;
	double			dmsec;
	int				msec;
	int				newcmds;
	int				numbackup;
	int				numcmds;
	int				cmdnumber;
	int				szbefore;
	int				rgmunge;
	int				checksumposition;
	int				active;

	// Not ready for commands yet.
	if (cls.state == ca_dedicated ||
		cls.state == ca_disconnected ||
		cls.state == ca_connecting)
		return;

	if (cls.state == ca_connected && !cls.passive)
		return Netchan_Transmit(&cls.netchan, 0, NULL);

	CL_ComputePacketLoss();

	buf.buffername = "Client Move";
	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.flags = 0;
	buf.data = data;

	// Message we are constructing.
	i = cls.netchan.outgoing_sequence & CL_UPDATE_MASK;

	cmd = &cl.commands[i];

	cmd->senttime = realtime;
	cmd->receivedtime = -1;
	cmd->processedfuncs = false;
	// Assume we'll transmit this command this frame ( rather than queueing it for next frame )
	cmd->heldback = false;
	cmd->sendsize = 0;

	Q_memset(&cmd->cmd, 0, sizeof(usercmd_t));

	active = cls.signon == SIGNONS;

	CL_SetSolidEntities();
	CL_PushPMStates();
	CL_SetSolidPlayers(cl.playernum);
	ClientDLL_CreateMove(host_frametime, &cmd->cmd, active);

	// Determine number of backup commands to send along
	numbackup = cl_cmdbackup.value;
	if (numbackup < 0)
	{
		numbackup = 0;
	}
	else if (numbackup > MAX_BACKUP_COMMANDS)
	{
		numbackup = MAX_BACKUP_COMMANDS;
	}

	CL_PopPMStates();
	CL_ComputeClientInterpolationAmount(&cmd->cmd);

	cmd->cmd.lightlevel = cl.light_level;

	// Write a clc_move message
	MSG_WriteByte(&buf, clc_move);
	// сохранение позиции для последующего заполнения двух байт (размера и хэша)
	szbefore = buf.cursize + 1;
	MSG_WriteByte(&buf, 0);
	MSG_WriteByte(&buf, 0);

	int packet_loss = clamp((int)cls.packet_loss, 0, 100);

	// tell the server if we want loopback on.
	if (Voice_GetLoopback())
		packet_loss |= (1 << 7);

	MSG_WriteByte(&buf, packet_loss);

	dmsec = host_frametime * 1000.f;

	msec = (int)dmsec;

	frametime_remainder += dmsec - (float)msec;

	if (frametime_remainder > 1.0)
	{
		frametime_remainder -= 1.0;
		msec++;
	}

	msec = (int)min((double)msec, 255.0);

	cmd->cmd.msec = (byte)msec;

	cl.cmd = cmd->cmd;

	if (cls.passive || cls.demoplayback)
		return;

	CL_PredictMove(false);

	if (cl_cmdrate.value < MIN_CMD_RATE)
		Cvar_Set(const_cast<char*>("cl_cmdrate"), const_cast<char*>("10"));

	if (cls.spectator && cls.state == ca_active && cl.delta_sequence == cl.validsequence && (cls.demorecording == false || cls.demowaiting == false) && cls.nextcmdtime + 1.0 > realtime)
		return;

	if (msec > 0 &&
		( (cl.maxclients == 1) ||
		((cls.netchan.remote_address.type == NA_LOOPBACK) && !host_limitlocal.value) ||
		((realtime >= cls.nextcmdtime) && Netchan_CanPacket(&cls.netchan)) ) )
	{
		if (cl_cmdrate.value > 0)
		{
			cls.nextcmdtime = realtime + (1.0f / cl_cmdrate.value);
		}
		else
		{
			// Always able to send right away
			cls.nextcmdtime = realtime;
		}

		if (cls.lastoutgoingcommand == -1)
			cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;

		if (cls.spectator)
		{
			SZ_Clear(&buf);
		}
		else
		{
			MSG_WriteByte(&buf, numbackup);

			// How many real commands have queued up
			newcmds = (cls.netchan.outgoing_sequence - cls.lastoutgoingcommand);

			// Put an upper/lower bound on this
			if (cls.build_num < NEWLIM_SERVER_BUILDNUM)
				newcmds = min(newcmds, MAX_TOTAL_CMDS_OLD);
			else
				newcmds = min(newcmds, MAX_TOTAL_CMDS - numbackup);

			MSG_WriteByte(&buf, newcmds);

			Q_memset(&cmdbaseline, 0, sizeof(usercmd_t));
			p_cmdbaseline = &cmdbaseline;

			numcmds = newcmds + numbackup;

			for (i = numcmds - 1; i >= 0; i--, p_cmdbaseline = &cl.commands[cmdnumber].cmd)
			{
				cmdnumber = (cls.netchan.outgoing_sequence - i) & CL_UPDATE_MASK;
			
				MSG_WriteUsercmd(&buf, &cl.commands[cmdnumber].cmd, p_cmdbaseline);
			}

			checksumposition = buf.cursize - szbefore - 1;

			buf.data[szbefore - 1] = min(checksumposition, 255);

			buf.data[szbefore] = COM_BlockSequenceCRCByte(&buf.data[szbefore + 1], buf.cursize + ~szbefore, cls.netchan.outgoing_sequence);

			rgmunge = min(buf.cursize + ~szbefore, 255);

			COM_Munge(&buf.data[szbefore + 1], rgmunge, cls.netchan.outgoing_sequence);
		}

		// Request delta compression of entities
		if ((cls.state == ca_active) &&
			(cls.signon == SIGNONS) &&
			((cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged) >= CL_UPDATE_MASK) &&
			((realtime - cls.netchan.last_received) > CONNECTION_PROBLEM_TIME) )
		{
			con_nprint_t np;
			np.time_to_live = 1.0;
			np.index = 0;
			np.color[0] = 1.0;
			np.color[1] = 0.2;
			np.color[2] = 0.0;
			Con_NXPrintf(&np, "WARNING:  Connection Problem");
			cl.validsequence = 0;
		}

		// Determine if we need to ask for a new set of delta's.
		if (cl.validsequence &&
			!cl_nodelta.value &&
			(cls.state == ca_active) &&
			!(cls.demorecording && cls.demowaiting))
		{
			cl.delta_sequence = cl.validsequence;

			MSG_WriteByte(&buf, clc_delta);
			MSG_WriteByte(&buf, cl.validsequence);
		}
		else
		{
			// Tell server to send us an uncompressed update
			cl.delta_sequence = -1;
		}

		// Remember outgoing command that we are sending
		cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;

		// Update size counter for netgraph
		cl.commands[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].sendsize = buf.cursize;

		CL_AddVoiceToDatagram(false);

		// Composite the rest of the datagram..
		if (cls.datagram.cursize <= buf.maxsize - buf.cursize)
		{
			Q_memcpy(&buf.data[buf.cursize], cls.datagram.data, cls.datagram.cursize);
			buf.cursize += cls.datagram.cursize;
		}
		cls.datagram.cursize = 0;

		//COM_Log( "cl.log", "Sending command number %i(%i) to server\n", cls.netchan.outgoing_sequence, cls.netchan.outgoing_sequence & CL_UPDATE_MASK );
		// Send the message
		Netchan_Transmit(&cls.netchan, buf.cursize, buf.data);
	}
	else
	{
		// Mark command as held back so we'll send it next time
		cl.commands[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].heldback = true;
		// Increment sequence number so we can detect that we've held back packets.
		cls.netchan.outgoing_sequence++;
	}

	// Update download/upload slider.
	Netchan_UpdateProgress(&cls.netchan);
}

void CL_BeginUpload_f()
{
	resource_t custResource;
	MD5Context_t ctx;
	byte binmd5[16], genmd5[16];
	int size;
	char *pszFileName;
	byte *buf;

	buf = NULL;
	size = 0;

	pszFileName = (PCHAR)Cmd_Argv(1);

	if (!pszFileName || !pszFileName[0])
		return Con_Printf(const_cast<char*>("Upload without filename\n"));

	if (!cl_allow_upload.value)
		return Con_Printf(const_cast<char*>("Ingoring decal upload ( cl_allow_upload is 0 )\n"));
	
	// 36 -> hex md5 + /!MD5/
	if (Q_strlen(pszFileName) != 36 || Q_strncasecmp(pszFileName, "!MD5", 4))
		return Con_Printf(const_cast<char*>("Ingoring upload of non-customization\n"));

	ContinueLoadingProgressBar("ClientConnect", 10, 0.0);
	Q_memset(&custResource, 0, sizeof(custResource));
	COM_HexConvert(&pszFileName[4], sizeof(binmd5) * 2, binmd5);

	if (HPAK_ResourceForHash(const_cast<char*>("custom.hpk"), binmd5, &custResource))
	{
		if (Q_memcmp(binmd5, custResource.rgucMD5_hash, 16))
		{
			Con_DPrintf(const_cast<char*>("Bogus data retrieved from custom.hpk, attempting to delete entry\n"));
			return HPAK_RemoveLump(const_cast<char*>("custom.hpk"), &custResource);
		}
		if (HPAK_GetDataPointer(const_cast<char*>("custom.hpk"), &custResource, &buf, &size))
		{
			Q_memset(&ctx, 0, sizeof(ctx));
			MD5Init(&ctx);
			MD5Update(&ctx, buf, size);
			MD5Final(genmd5, &ctx);
			if (Q_memcmp(custResource.rgucMD5_hash, genmd5, 16))
			{
				Con_Printf(const_cast<char*>("HPAK_AddLump called with bogus lump, md5 mismatch\n"));
				Con_Printf(const_cast<char*>("Purported:  %s\n"), MD5_Print(custResource.rgucMD5_hash));
				Con_Printf(const_cast<char*>("Actual   :  %s\n"), MD5_Print(genmd5));
				Con_Printf(const_cast<char*>("Removing conflicting lump\n"));
				return HPAK_RemoveLump(const_cast<char*>("custom.hpk"), &custResource);
			}
		}
	}

	if (buf && size)
	{
		Netchan_CreateFileFragmentsFromBuffer(false, &cls.netchan, pszFileName, buf, size);
		Netchan_FragSend(&cls.netchan);
		Mem_Free(buf);
	}
	else
		Con_Printf(const_cast<char*>("Ingoring customization upload, couldn't find decal locally\n"));
}

void CL_SendResourceListBlock()
{
	byte buf[NET_MAX_PAYLOAD];
	sizebuf_t msg;
	int nStartIndex, arg;

	ContinueLoadingProgressBar("ClientConnect", 6, 0.0);

	Q_memset(&msg, 0, sizeof(msg));

	msg.buffername = "Client to Server Resource Block";
	msg.data = buf;
	msg.cursize = 0;
	msg.maxsize = sizeof(buf);

	if (cls.state == ca_dedicated || cls.state == ca_disconnected)
		return Con_Printf(const_cast<char*>("custom resource list not valid -- not connected\n"));

	arg = MSG_ReadLong();

	if (cls.demoplayback || cl.servercount == arg)
	{
		nStartIndex = MSG_ReadLong(); // always 0

		if (nStartIndex >= 0 && nStartIndex <= cl.num_resources)
		{
			MSG_WriteByte(&msg, clc_resourcelist);
			MSG_WriteShort(&msg, cl.num_resources); // should be cl.num_resources - nStartIndex

			for (int i = nStartIndex; i < cl.num_resources; i++)
			{
				MSG_WriteString(&msg, cl.resourcelist[i].szFileName);
				MSG_WriteByte(&msg, cl.resourcelist[i].type);
				MSG_WriteShort(&msg, cl.resourcelist[i].nIndex);
				MSG_WriteLong(&msg, cl.resourcelist[i].nDownloadSize);
				MSG_WriteByte(&msg, cl.resourcelist[i].ucFlags);

				if (cl.resourcelist[i].ucFlags & RES_CUSTOM)
					SZ_Write(&msg, cl.resourcelist[i].rgucMD5_hash, sizeof(cl.resourcelist[i].rgucMD5_hash));
			}

			if (msg.cursize > 0)
			{
				Netchan_CreateFragments(false, &cls.netchan, &msg);
				Netchan_FragSend(&cls.netchan);
			}
		}
		else
			Con_Printf(const_cast<char*>("custom resource list request out of range\n"));
	}
	else
		Con_Printf(const_cast<char*>("CL_SendResourceListBlock_f from different level\n"));
}

void CL_HudMessage(const char *pMessage)
{
	DispatchDirectUserMsg("HudText", Q_strlen((char*)pMessage), (void *)pMessage);
}

void CL_FullServerinfo_f()
{
	if (Cmd_Argc() != 2)
		return Con_Printf(const_cast<char*>("usage: fullserverinfo <complete info string>\n"));

	Q_strncpy(cl.serverinfo, Cmd_Argv(1), sizeof(cl.serverinfo) - 1);
	cl.serverinfo[sizeof(cl.serverinfo) - 1] = 0;
}

void CL_Shutdown()
{
	CL_FlushEventSystem();
	TextMessageShutdown();
	Net_APIShutDown();

	if (cls.demoheader != NULL)
	{
		FS_Close(cls.demoheader);
		cls.demoheader = 0;
	}

	if (FS_FileExists("demoheader.dmf"))
		FS_RemoveFile("demoheader.dmf", NULL);
}

void CL_GameDir_f()
{
	if (Cmd_Argc() != 2)
		return Con_Printf(const_cast<char*>("gamedir is %s\n"), com_gamedir);

	CL_Disconnect_f();

	Con_Printf(const_cast<char*>("Setting gamedir to %s\n"), Cmd_Argv(1));
	CL_CheckGameDirectory((char*)Cmd_Argv(1));
}

void CL_GetPlayerHulls()
{
	for( int i = 0; i < MAX_MAP_HULLS; i++ )
	{
		if( !ClientDLL_GetHullBounds( i, player_mins[ i ], player_maxs[ i ] ) )
			break;
	}
}

void CL_Rate_f()
{
	double fNewRate;

	if (Cmd_Argc() != 2)
		return Con_Printf(const_cast<char*>("Usage:  cl_rate <num>\nClient to server transmission rate (bytes/sec)\nCurrent:  %.5f\n"), cls.netchan.rate);
	

	fNewRate = Q_atof(Cmd_Argv(1));

	
	if (fNewRate == 0.0)
		fNewRate = DEFAULT_RATE;
	else if (fNewRate < MIN_RATE || fNewRate > MAX_RATE)
		return Con_Printf(const_cast<char*>("cl_rate:  Maximum %f, Minimum %f\n"), MAX_RATE, MIN_RATE);

	cls.netchan.rate = fNewRate;
}

void GetPos( vec3_t* origin, vec3_t* angles )
{	
	VectorCopy(r_refdef.vieworg, *origin);
	VectorCopy(r_refdef.viewangles, *angles);

	if( Cmd_Argc() == 2 )
	{
		if( Q_atoi( (char*)Cmd_Argv( 1 ) ) == 2 && cls.state == ca_active )
		{
			VectorCopy(cl.frames[ cl.parsecountmod ].playerstate[ cl.playernum ].origin, *origin);
			VectorCopy(cl.frames[ cl.parsecountmod ].playerstate[ cl.playernum ].angles, *angles);
		}
	}
}

void CL_SpecPos_f()
{
	vec3_t origin;
	vec3_t angles;

	GetPos(&origin, &angles);
	Con_Printf(const_cast<char*>("spec_pos %.1f %.1f %.1f %.1f %.1f\n"), origin[0], origin[1], origin[2], angles[0], angles[1]);
}

void CL_Init()
{
	const char *pCLName = Steam_GetCommunityName();
	if (!pCLName)
		pCLName = "unknown";

	Info_SetValueForKey(cls.userinfo, "name", pCLName, sizeof(cls.userinfo));
	Info_SetValueForKey(cls.userinfo, "topcolor", "0", sizeof(cls.userinfo));
	Info_SetValueForKey(cls.userinfo, "bottomcolor", "0", sizeof(cls.userinfo));
	Info_SetValueForKey(cls.userinfo, "rate", "2500", sizeof(cls.userinfo));
	Info_SetValueForKey(cls.userinfo, "cl_updaterate", "20", sizeof(cls.userinfo));
	Info_SetValueForKey(cls.userinfo, "cl_lw", "1", sizeof(cls.userinfo));
	Info_SetValueForKey(cls.userinfo, "cl_lc", "1", sizeof(cls.userinfo));

	Net_APIInit();
	CL_InitTEnts();
	CL_InitExtrap();
	TextMessageInit();
	Sequence_Init();

	Cvar_RegisterVariable(&cl_weaponlistfix);
	Cvar_RegisterVariable(&cl_fixtimerate);
	Cvar_RegisterVariable(&cl_showmessages);
	Cvar_RegisterVariable(&cl_name);
	Cvar_RegisterVariable(&password);
	Cvar_RegisterVariable(&team);
	Cvar_RegisterVariable(&cl_model);
	Cvar_RegisterVariable(&skin);
	Cvar_RegisterVariable(&topcolor);
	Cvar_RegisterVariable(&bottomcolor);
	Cvar_RegisterVariable(&rate);
	Cvar_RegisterVariable(&cl_updaterate);
	Cvar_RegisterVariable(&cl_lw);
	Cvar_RegisterVariable(&cl_lc);
	Cvar_RegisterVariable(&cl_dlmax);
	Cvar_RegisterVariable(&fs_startup_timings);
	Cvar_RegisterVariable(&fs_lazy_precache);
	Cvar_RegisterVariable(&fs_precache_timings);
	Cvar_RegisterVariable(&fs_perf_warnings);
	Cvar_RegisterVariable(&cl_clockreset);
	Cvar_RegisterVariable(&cl_showevents);
	Cvar_RegisterVariable(&cl_himodels);
	Cvar_RegisterVariable(&cl_gaitestimation);
	Cvar_RegisterVariable(&cl_idealpitchscale);
	Cvar_RegisterVariable(&cl_resend);
	Cvar_RegisterVariable(&cl_timeout);
	Cvar_RegisterVariable(&cl_cmdbackup);
	Cvar_RegisterVariable(&cl_shownet);
	Cvar_RegisterVariable(&rcon_address);
	Cvar_RegisterVariable(&rcon_port);
	Cvar_RegisterVariable(&cl_solid_players);
	Cvar_RegisterVariable(&cl_slisttimeout);
	Cvar_RegisterVariable(&cl_download_ingame);
	Cvar_RegisterVariable(&cl_allow_download);
	Cvar_RegisterVariable(&cl_allow_upload);
	Cvar_RegisterVariable(&cl_gamegauge);
	Cvar_RegisterVariable(&cl_cmdrate);
	Cvar_RegisterVariable(&cl_showfps);
	Cvar_RegisterVariable(&cl_needinstanced);
	Cvar_RegisterVariable(&dev_overview);
	Cvar_RegisterVariable(&cl_logofile);
	Cvar_RegisterVariable(&cl_logocolor);
	Cvar_RegisterVariable(&cl_mousegrab);
	Cvar_RegisterVariable(&m_rawinput);
	Cvar_RegisterVariable(&cl_filterstuffcmd);
	// HL25 cvar
	Cvar_RegisterVariable(&cl_fixmodelinterpolationartifacts);

	if (COM_CheckParm(const_cast<char*>("-nomousegrab")))
		Cvar_Set(const_cast<char*>("cl_mousegrab"), const_cast<char*>("0"));

	ClientDLL_HudInit();

	Cmd_AddCommand(const_cast<char*>("gamedir"), CL_GameDir_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("connect"), CL_Connect_f, CMD_PRIVILEGED);
	Cmd_AddCommand(const_cast<char*>("fullserverinfo"), CL_FullServerinfo_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("retry"), CL_Retry_f, CMD_PRIVILEGED);
	Cmd_AddCommand(const_cast<char*>("disconnect"), CL_Disconnect_f);
	Cmd_AddCommand(const_cast<char*>("record"), CL_Record_f);
	Cmd_AddCommand(const_cast<char*>("stop"), CL_Stop_f);
	Cmd_AddCommand(const_cast<char*>("playdemo"), CL_PlayDemo_f);
	Cmd_AddCommand(const_cast<char*>("viewdemo"), CL_ViewDemo_f);
	Cmd_AddCommand(const_cast<char*>("timedemo"), CL_TimeDemo_f);
	Cmd_AddCommand(const_cast<char*>("gg"), CL_GameGauge_f);
	Cmd_AddCommand(const_cast<char*>("listdemo"), CL_ListDemo_f);
	Cmd_AddCommand(const_cast<char*>("appenddemo"), CL_AppendDemo_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("removedemo"), CL_RemoveDemo_f, CMD_UNSAFE);
	Cmd_AddCommand(const_cast<char*>("swapdemo"), CL_SwapDemo_f);
	Cmd_AddCommand(const_cast<char*>("setdemoinfo"), CL_SetDemoInfo_f);
	Cmd_AddCommand(const_cast<char*>("listen"), CL_Listen_f);
	Cmd_AddCommand(const_cast<char*>("commentator"), CL_Commentator_f);
	Cmd_AddCommand(const_cast<char*>("waveplaylen"), CL_WavePlayLen_f);
	Cmd_AddCommand(const_cast<char*>("snapshot"), CL_TakeSnapshot_f);
	Cmd_AddCommand(const_cast<char*>("startmovie"), CL_StartMovie_f);
	Cmd_AddCommand(const_cast<char*>("endmovie"), CL_EndMovie_f);
	Cmd_AddCommand(const_cast<char*>("entities"), CL_PrintEntities_f);
	Cmd_AddCommandWithFlags(const_cast<char*>("rcon"), CL_Rcon_f, CMD_PRIVILEGED);
	Cmd_AddCommand(const_cast<char*>("cl_messages"), CL_DumpMessageLoad_f);
	Cmd_AddCommand(const_cast<char*>("pingservers"), CL_PingServers_f);
	Cmd_AddCommand(const_cast<char*>("slist"), CL_Slist_f);
	Cmd_AddCommand(const_cast<char*>("list"), CL_ListCachedServers_f);
	Cmd_AddCommand(const_cast<char*>("upload"), CL_BeginUpload_f);

	CL_InitPrediction();

	Cmd_AddCommand(const_cast<char*>("httpstop"), CL_HTTPCancel_f);

	Cmd_AddCommand(const_cast<char*>("spec_pos"), CL_SpecPos_f);


	cls.datagram.data = cls.datagram_buf;
	cls.datagram.cursize = 0;
	cls.datagram.buffername = "cls.datagram";
	cls.isVAC2Secure = false;
	cls.GameServerSteamID = 0;
	cls.datagram.maxsize = sizeof(cls.datagram_buf);
	cls.build_num = 0;
	cls.datagram.flags = 0;

	Q_memset(&cl, 0, sizeof(client_state_t));

	cl.resourcesneeded.pNext = &cl.resourcesneeded;
	cl.resourcesonhand.pPrev = &cl.resourcesonhand;
	cl.resourcesneeded.pPrev = &cl.resourcesneeded;
	cl.resourcesonhand.pNext = &cl.resourcesonhand;

	DELTA_RegisterDescription(const_cast<char*>("clientdata_t"));
	DELTA_RegisterDescription(const_cast<char*>("entity_state_t"));
	DELTA_RegisterDescription(const_cast<char*>("entity_state_player_t"));
	DELTA_RegisterDescription(const_cast<char*>("custom_entity_state_t"));
	DELTA_RegisterDescription(const_cast<char*>("usercmd_t"));
	DELTA_RegisterDescription(const_cast<char*>("weapon_data_t"));
	DELTA_RegisterDescription(const_cast<char*>("event_t"));
}

void CL_SendKeepaliveToServer()
{
	sizebuf_t buf;
	byte szData[32];

	if (cls.state == ca_disconnected)
		return;
	
	buf.cursize = 0;
	buf.buffername = "Client Keepalive";
	buf.maxsize = sizeof szData;
	buf.flags = 0;
	buf.data = szData;

	for (int i = 0; i < sizeof(szData); i++)
		MSG_WriteByte(&buf, clc_nop);

	Netchan_Transmit(&cls.netchan, buf.cursize, buf.data);
}

void CL_KeepConnectionActive()
{
	static float flLastKeepaliveTime = 0.0;
	float flCurrentTime;

	flCurrentTime = Sys_FloatTime();
	if (flCurrentTime < flLastKeepaliveTime)
		flLastKeepaliveTime = flCurrentTime;

	if ((flCurrentTime - flLastKeepaliveTime) > 5.0f)
	{
		CL_SendKeepaliveToServer();
		flLastKeepaliveTime = flCurrentTime;
	}
}

model_t* CL_GetModelByIndex( int index )
{
	float start;

	if (index >= MAX_MODELS)
		return NULL;

	if (cl.model_precache[index] == NULL)
		return NULL;

	if (cl.model_precache[index]->needload != NL_NEEDS_LOADED && cl.model_precache[index]->needload != NL_UNREFERENCED)
		return cl.model_precache[index];

	if (fs_precache_timings.value == 0.0)
	{
		Mod_LoadModel(cl.model_precache[index], false, false);
	}
	else
	{
		start = Sys_FloatTime();
		Mod_LoadModel(cl.model_precache[index], false, false);
		Con_DPrintf(const_cast<char*>("fs_precache_timings: loaded model %s in time %.3f sec\n"), cl.model_precache[index]->name, Sys_FloatTime() - start);
	}

	return cl.model_precache[index];
}

model_t* CL_GetWorldModel()
{
	return cl.worldmodel;
}

int CL_GetServerCount()
{
	return cl.servercount;
}


