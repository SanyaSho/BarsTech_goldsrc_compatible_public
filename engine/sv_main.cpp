#include "quakedef.h"

#include "client.h"
#include "modinfo.h"
#include "pr_edict.h"
#include "sv_main.h"
#include "sv_phys.h"
#include "server.h"
#include "delta.h"
#include "cl_main.h"
#include "host.h"
#include "cmodel.h"
#include "pm_shared/pm_movevars.h"
#include "sv_user.h"
#include "sv_remoteaccess.h"
#include "net_ws.h"
#include "hltv.h"
#include "tmessage.h"
#include "pr_cmds.h"
#include "hashpak.h"
#include "sv_upld.h"
#include "ipratelimitWrapper.h"
#include "pmove.h"
#include "vgui_int.h"
#include "host_cmd.h"
#include "world.h"
#include "r_local.h"
#include "dll_state.h"
#if defined(GLQUAKE)
#include "gl_screen.h"
#else
#include "screen.h"
#endif

typedef struct full_packet_entities_s
{
	int num_entities;
	entity_state_t entities[MAX_PACKET_ENTITIES];
} full_packet_entities_t;

int hashstrings_collisions;

int sv_playermodel;
int g_groupop, g_groupmask;

char *pr_strings;


int sv_lastnum;

UserMsg *sv_gpNewUserMsgs;
UserMsg *sv_gpUserMsgs;

extra_baselines_t g_sv_instance_baselines;

delta_t *g_pplayerdelta;
delta_t *g_pentitydelta;
delta_t *g_pcustomentitydelta;
delta_t *g_pclientdelta;
delta_t *g_pweapondelta;

server_static_t svs;
server_t sv;

playermove_t g_svmove;
globalvars_t gGlobalVariables = {};

qboolean g_bOutOfDateRestart;
cvar_t sv_language = { const_cast<char*>("sv_language"), const_cast<char*>("0") }, 
	laddermode = { const_cast<char*>("laddermode"), const_cast<char*>("0") };

delta_info_t *g_sv_delta;
delta_t *g_peventdelta;

int num_servers;
int player_datacounts[32];
// эквивалент MTU для сетевого движка half-life
char outputbuf[MAX_ROUTEABLE_PACKET];

redirect_t sv_redirected;
netadr_t sv_redirectto;

char *gNullString = const_cast<char*>("");

int giNextUserMsg = 64;
int g_userid = 1;

int gPacketSuppressed;

char localinfo[MAX_LOCALINFO + 1];
char localmodels[MAX_MODELS][5];

ipfilter_t ipfilters[MAX_IPFILTERS];
int numipfilters;
userfilter_t userfilters[MAX_USERFILTERS];
int numuserfilters;

challenge_t g_rg_sv_challenges[MAX_CHALLENGES];
entcount_t ent_datacounts[32];

cvar_t sv_allow_upload = { const_cast<char*>("sv_allowupload"), const_cast<char*>("1"), FCVAR_SERVER };
cvar_t mapcyclefile = { const_cast<char*>("mapcyclefile"), const_cast<char*>("mapcycle.txt") };
cvar_t servercfgfile = { const_cast<char*>("servercfgfile"), const_cast<char*>("server.cfg") };
cvar_t sv_logblocks = { const_cast<char*>("sv_logblocks"), const_cast<char*>("0"), FCVAR_SERVER };
cvar_t max_queries_sec = { const_cast<char*>("max_queries_sec"), const_cast<char*>("3.0"), FCVAR_PROTECTED | FCVAR_SERVER };
cvar_t max_queries_sec_global = { const_cast<char*>("max_queries_sec_global"), const_cast<char*>("30"), FCVAR_PROTECTED | FCVAR_SERVER };
cvar_t max_queries_window = { const_cast<char*>("max_queries_window"), const_cast<char*>("60"), FCVAR_PROTECTED | FCVAR_SERVER };

cvar_t sv_cheats = { const_cast<char*>("sv_cheats"), const_cast<char*>("0"), FCVAR_SERVER };
cvar_t sv_version = { const_cast<char*>("sv_version"), const_cast<char*>("") };

cvar_t sv_lan = { const_cast<char*>("sv_lan"), const_cast<char*>("0") };
cvar_t sv_lan_rate = { const_cast<char*>("sv_lan_rate"), const_cast<char*>("20000.0") };
cvar_t sv_aim = { const_cast<char*>("sv_aim"), const_cast<char*>("1"), FCVAR_SERVER | FCVAR_ARCHIVE };

cvar_t sv_skycolor_r = { const_cast<char*>("sv_skycolor_r"), const_cast<char*>("0") };
cvar_t sv_skycolor_g = { const_cast<char*>("sv_skycolor_g"), const_cast<char*>("0") };
cvar_t sv_skycolor_b = { const_cast<char*>("sv_skycolor_b"), const_cast<char*>("0") };
cvar_t sv_skyvec_x = { const_cast<char*>("sv_skyvec_x"), const_cast<char*>("0") };
cvar_t sv_skyvec_y = { const_cast<char*>("sv_skyvec_y"), const_cast<char*>("0") };
cvar_t sv_skyvec_z = { const_cast<char*>("sv_skyvec_z"), const_cast<char*>("0") };

cvar_t sv_spectatormaxspeed = { const_cast<char*>("sv_spectatormaxspeed"), const_cast<char*>("500") };
cvar_t sv_airaccelerate = { const_cast<char*>("sv_airaccelerate"), const_cast<char*>("10"), FCVAR_SERVER };
cvar_t sv_wateraccelerate = { const_cast<char*>("sv_wateraccelerate"), const_cast<char*>("10"), FCVAR_SERVER };
cvar_t sv_waterfriction = { const_cast<char*>("sv_waterfriction"), const_cast<char*>("1"), FCVAR_SERVER };
cvar_t sv_zmax = { const_cast<char*>("sv_zmax"), const_cast<char*>("4096"), FCVAR_SPONLY };
cvar_t sv_wateramp = { const_cast<char*>("sv_wateramp"), const_cast<char*>("0") };

cvar_t sv_skyname = { const_cast<char*>("sv_skyname"), const_cast<char*>("desert") };
cvar_t motdfile = { const_cast<char*>("motdfile"), const_cast<char*>("motd.txt") };
cvar_t lservercfgfile = { const_cast<char*>("lservercfgfile"), const_cast<char*>("listenserver.cfg") };
cvar_t logsdir = { const_cast<char*>("logsdir"), const_cast<char*>("logs") };
cvar_t bannedcfgfile = { const_cast<char*>("bannedcfgfile"), const_cast<char*>("banned.cfg") };

cvar_t rcon_password = { const_cast<char*>("rcon_password"), const_cast<char*>(""), FCVAR_PRIVILEGED };
cvar_t sv_enableoldqueries = { const_cast<char*>("sv_enableoldqueries"), const_cast<char*>("0") };

cvar_t sv_instancedbaseline = { const_cast<char*>("sv_instancedbaseline"), const_cast<char*>("1") };
cvar_t sv_contact = { const_cast<char*>("sv_contact"), const_cast<char*>(""), FCVAR_SERVER };
cvar_t sv_maxupdaterate = { const_cast<char*>("sv_maxupdaterate"), const_cast<char*>("30.0") };
cvar_t sv_minupdaterate = { const_cast<char*>("sv_minupdaterate"), const_cast<char*>("10.0") };
cvar_t sv_filterban = { const_cast<char*>("sv_filterban"), const_cast<char*>("1") };
cvar_t sv_minrate = { const_cast<char*>("sv_minrate"), const_cast<char*>("0"), FCVAR_SERVER };
cvar_t sv_maxrate = { const_cast<char*>("sv_maxrate"), const_cast<char*>("0"), FCVAR_SERVER };
cvar_t sv_logrelay = { const_cast<char*>("sv_logrelay"), const_cast<char*>("0") };

cvar_t sv_newunit = { const_cast<char*>("sv_newunit"), const_cast<char*>("0") };

cvar_t sv_clienttrace = { const_cast<char*>("sv_clienttrace"), const_cast<char*>("1"), FCVAR_SERVER };
cvar_t sv_timeout = { const_cast<char*>("sv_timeout"), const_cast<char*>("60") };
cvar_t sv_failuretime = { const_cast<char*>("sv_failuretime"), const_cast<char*>("0.5") };
cvar_t sv_password = { const_cast<char*>("sv_password"), const_cast<char*>(""), FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t sv_proxies = { const_cast<char*>("sv_proxies"), const_cast<char*>("1"), FCVAR_SERVER };
cvar_t sv_outofdatetime = { const_cast<char*>("sv_outofdatetime"), const_cast<char*>("1800") };
cvar_t mapchangecfgfile = { const_cast<char*>("mapchangecfgfile"), const_cast<char*>("") };

cvar_t sv_allow_download = { const_cast<char*>("sv_allowdownload"), const_cast<char*>("1") };
cvar_t sv_send_logos = { const_cast<char*>("sv_send_logos"), const_cast<char*>("1") };
cvar_t sv_send_resources = { const_cast<char*>("sv_send_resources"), const_cast<char*>("1") };
cvar_t sv_logbans = { const_cast<char*>("sv_logbans"), const_cast<char*>("0") };
cvar_t sv_max_upload = { const_cast<char*>("sv_uploadmax"), const_cast<char*>("0.5"), FCVAR_SERVER };
cvar_t sv_visiblemaxplayers = { const_cast<char*>("sv_visiblemaxplayers"), const_cast<char*>("-1") };

cvar_t sv_downloadurl = { const_cast<char*>("sv_downloadurl"), const_cast<char*>(""), FCVAR_PROTECTED };
cvar_t sv_allow_dlfile = { const_cast<char*>("sv_allow_dlfile"), const_cast<char*>("1") };

cvar_t sv_rcon_minfailures = { const_cast<char*>("sv_rcon_minfailures"), const_cast<char*>("5") };
cvar_t sv_rcon_maxfailures = { const_cast<char*>("sv_rcon_maxfailures"), const_cast<char*>("10") };
cvar_t sv_rcon_minfailuretime = { const_cast<char*>("sv_rcon_minfailuretime"), const_cast<char*>("30") };
cvar_t sv_rcon_banpenalty = { const_cast<char*>("sv_rcon_banpenalty"), const_cast<char*>("0") };

cvar_t violence_hblood = { const_cast<char*>("violence_hblood"), const_cast<char*>("1") };
cvar_t violence_ablood = { const_cast<char*>("violence_ablood"), const_cast<char*>("1") };
cvar_t violence_hgibs = { const_cast<char*>("violence_hgibs"), const_cast<char*>("1") };
cvar_t violence_agibs = { const_cast<char*>("violence_agibs"), const_cast<char*>("1") };

cvar_t hpk_maxsize = { const_cast<char*>("hpk_maxsize"), const_cast<char*>("4"), FCVAR_ARCHIVE };

int SV_UPDATE_BACKUP = 1 << 3;
int SV_UPDATE_MASK = SV_UPDATE_BACKUP - 1;

decalname_t sv_decalnames[MAX_BASE_DECALS];
int sv_decalnamecount;

int fatbytes;
unsigned char fatpvs[1024];
int fatpasbytes;
unsigned char fatpas[1024];

deltacallback_t g_svdeltacallback;

struct GameToAppIDMapItem_t
{
	AppId_t iAppID;
	const char *pGameDir;
};

const GameToAppIDMapItem_t g_GameToAppIDMap[] =
{
	{ 10, "cstrike" },
	{ 20, "tfc" },
	{ 30, "dod" },
	{ 40, "dmc" },
	{ 50, "gearbox" },
	{ 60, "ricochet" },
	{ HALF_LIFE_APPID, "valve" },
	{ 80, "czero" },
	{ 100, "czeror" },
	{ 130, "bshift" },
	{ 150, "cstrike_beta" }
};

modinfo_t gmodinfo = {};

rcon_failure_t g_rgRconFailures[MAX_RCON_FAILURES_STORAGE];

AppId_t GetGameAppID()
{
	char gd[ FILENAME_MAX ];
	char arg[ FILENAME_MAX ];

	COM_ParseDirectoryFromCmd( "-game", arg, "valve" );
	COM_FileBase( arg, gd );

	for( const auto& data : g_GameToAppIDMap )
	{
		if( !stricmp( data.pGameDir, gd ) )
		{
			return data.iAppID;
		}
	}

	return HALF_LIFE_APPID;
}

bool IsGameSubscribed( const char *game )
{
	AppId_t appId = HALF_LIFE_APPID;

	for( const auto& data : g_GameToAppIDMap )
	{
		if( !stricmp( data.pGameDir, game ) )
		{
			appId = data.iAppID;
		}
	}

	return ISteamApps_BIsSubscribedApp( appId );
}

bool BIsValveGame()
{
	for( const auto& data : g_GameToAppIDMap )
	{
		if( !stricmp( data.pGameDir, com_gamedir ) )
		{
			return false;
		}
	}

	return true;
}

static bool g_bCS_CZ_Flags_Initialized = false;

bool g_bIsCStrike = false;
bool g_bIsCZero = false;
bool g_bIsCZeroRitual = false;
bool g_bIsTerrorStrike = false;
bool g_bIsTFC = false;

void SetCStrikeFlags()
{
	if( !g_bCS_CZ_Flags_Initialized )
	{
		if( !stricmp( com_gamedir, "cstrike" ) || 
			!stricmp( com_gamedir, "cstrike_beta" ) )
		{
			g_bIsCStrike = true;
		}
		else if( !stricmp( com_gamedir, "czero" ) )
		{
			g_bIsCZero = true;
		}
		else if( !stricmp( com_gamedir, "czeror" ) )
		{
			g_bIsCZeroRitual = true;
		}
		else if( !stricmp( com_gamedir, "terror" ) )
		{
			g_bIsTerrorStrike = true;
		}
		else if( !stricmp( com_gamedir, "tfc" ) )
		{
			g_bIsTFC = true;
		}

		g_bCS_CZ_Flags_Initialized = true;
	}
}

qboolean IsCZPlayerModel(uint32 crc, const char* filename)
{
	if (crc == 0x27FB4D2F)
		return stricmp(filename, "models/player/spetsnaz/spetsnaz.mdl") ? 0 : 1;

	if (crc == 0xEC43F76D || crc == 0x270FB2D7)
		return stricmp(filename, "models/player/terror/terror.mdl") ? 0 : 1;

	if (crc == 0x1AAA3360 || crc == 0x35AC6FED)
		return stricmp(filename, "models/player/gign/gign.mdl") ? 0 : 1;

	if (crc == 0x02B95E5F || crc == 0x72DB74E4)
		return stricmp(filename, "models/player/vip/vip.mdl") ? 0 : 1;

	if (crc == 0x1F3CD80B || crc == 0x1B6C4115)
		return stricmp(filename, "models/player/guerilla/guerilla.mdl") ? 0 : 1;

	if (crc == 0x3BCAA016)
		return stricmp(filename, "models/player/militia/militia.mdl") ? 0 : 1;

	if (crc == 0x43E67FF3 || crc == 0xF141AE3F)
		return stricmp(filename, "models/player/sas/sas.mdl") ? 0 : 1;

	if (crc == 0xDA8922A || crc == 0x56DD2D02)
		return stricmp(filename, "models/player/gsg9/gsg9.mdl") ? 0 : 1;

	if (crc == 0xA37D8680 || crc == 0x4986827B)
		return stricmp(filename, "models/player/arctic/arctic.mdl") ? 0 : 1;

	if (crc == 0xC37369F6 || crc == 0x29FE156C)
		return stricmp(filename, "models/player/leet/leet.mdl") ? 0 : 1;

	if (crc == 0xC7F0DBF3 || crc == 0x068168DB)
		return stricmp(filename, "models/player/urban/urban.mdl") ? 0 : 1;

	return 0;
}

delta_t *SV_LookupDelta(char *name)
{
	delta_info_t *p = g_sv_delta;

	while (p)
	{
		if (!Q_strcasecmp(name, p->name))
		{
			return p->delta;
		}
		p = p->next;
	}

	Sys_Error("Couldn't find delta for %s\n", name);

	return NULL;
}

void SV_DownloadingModules(void)
{
	char message[] = "Downloading Security Module from Speakeasy.net ...\n";
	Con_Printf(message);
}

void SV_GatherStatistics(void)
{
	int players = 0;

	if (svs.next_cleartime < realtime)
	{
		SV_CountPlayers(&players);
		svs.next_cleartime = realtime + 3600.0;
		Q_memset(&svs.stats, 0, sizeof(svs.stats));
		svs.stats.maxusers = players;
		svs.stats.minusers = players;
		return;
	}

	if (svs.next_sampletime > realtime)
		return;

	SV_CountPlayers(&players);

	svs.stats.num_samples++;
	svs.next_sampletime = realtime + 60.0;
	svs.stats.cumulative_occupancy += (players * 100.0 / svs.maxclients);

	if (svs.stats.num_samples >= 1)
		svs.stats.occupancy = svs.stats.cumulative_occupancy / svs.stats.num_samples;

	if (svs.stats.minusers > players)
		svs.stats.minusers = players;
	else if (svs.stats.maxusers < players)
		svs.stats.maxusers = players;

	if ((svs.maxclients - 1) <= players)
		svs.stats.at_capacity++;
	if (players <= 1)
		svs.stats.at_empty++;

	if (svs.stats.num_samples >= 1)
	{
		svs.stats.capacity_percent = svs.stats.at_capacity * 100.0 / svs.stats.num_samples;
		svs.stats.empty_percent = svs.stats.at_empty * 100.0 / svs.stats.num_samples;
	}

	int c = 0;
	float v = 0.0f;
	client_t *cl = svs.clients;

	for (int i = svs.maxclients; i > 0; i--, cl++)
	{
		if (cl->active && !cl->fakeclient)
		{
			c++;
			v += cl->latency;
		}
	}

	if (c)
		v = v / c;

	svs.stats.cumulative_latency += v;
	if (svs.stats.num_samples >= 1)
		svs.stats.average_latency = svs.stats.cumulative_latency / svs.stats.num_samples;
	if (svs.stats.num_sessions >= 1)
		svs.stats.average_session_len = svs.stats.cumulative_sessiontime / svs.stats.num_sessions;
}



void SV_DeallocateDynamicData()
{
	if (g_moved_edict)
		Mem_Free(g_moved_edict);
	if (g_moved_from)
		Mem_Free(g_moved_from);
	g_moved_edict = NULL;
	g_moved_from = NULL;
}

void SV_ReallocateDynamicData(void)
{
	if (!sv.max_edicts)
	{
		Con_DPrintf(const_cast<char*>(__FUNCTION__ ": sv.max_edicts == 0\n"));
		return;
	}

	int nSize = sv.max_edicts;

	if (g_moved_edict)
		Con_Printf(const_cast<char*>("Reallocate on moved_edict\n"));
	
	g_moved_edict = (edict_t **)Mem_ZeroMalloc(sizeof(edict_t*) * nSize);

	if (g_moved_from)
		Con_Printf(const_cast<char*>("Reallocate on moved_from\n"));
	
	g_moved_from = (vec3_t *)Mem_ZeroMalloc(sizeof(vec3_t) * nSize);
}

void SV_AllocClientFrames()
{
	for( int i = 0; i < svs.maxclientslimit; ++i )
	{
		if( svs.clients[ i ].frames )
			Con_DPrintf( const_cast<char*>("Allocating over frame pointer?\n") );

		svs.clients[ i ].frames = reinterpret_cast<client_frame_t*>( Mem_ZeroMalloc( sizeof( client_frame_t ) * SV_UPDATE_BACKUP ) );
	}
}

qboolean SV_IsPlayerIndex(int index)
{
	return (index >= 1 && index <= svs.maxclients);
}

void SV_ClearPacketEntities(client_frame_t *frame)
{
	if (frame == NULL)
		return;

	if (frame->entities.entities)
		Mem_Free(frame->entities.entities);
	frame->entities.entities = NULL;
	frame->entities.num_entities = 0;

}

void SV_AllocPacketEntities(client_frame_t *frame, int numents)
{
	if (frame == NULL)
		return;

	if (frame->entities.entities)
		SV_ClearPacketEntities(frame);

	int allocatedslots = numents;
	if (numents < 1)
		allocatedslots = 1;

	frame->entities.num_entities = numents;
	frame->entities.entities = (entity_state_t *)Mem_ZeroMalloc(sizeof(entity_state_t) * allocatedslots * 2);

}

void SV_ClearFrames(client_frame_t** frames)
{
	if (*frames)
	{
		client_frame_t* pFrame = *frames;
		Con_SafePrintf(__FUNCTION__ " clearing frames...\r\n");

		for (int i = 0; i < SV_UPDATE_BACKUP; ++i, pFrame++)
		{
			SV_ClearPacketEntities(pFrame);

			pFrame->senttime = 0;
			pFrame->ping_time = -1;
		}

		Mem_Free(*frames);
		*frames = nullptr;
	}
}

void SV_BroadcastCommand(char *fmt, ...)
{
	va_list argptr;
	char string[1024];
	char data[128];
	sizebuf_t msg;

	if (!sv.active)
		return;

	va_start(argptr, fmt);
	msg.data = (byte *)data;
	msg.buffername = "Broadcast Command";
	msg.cursize = 0;
	msg.maxsize = sizeof(data);
	msg.flags = FSB_ALLOWOVERFLOW;

	_vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&msg, svc_stufftext);
	MSG_WriteString(&msg, string);
	if (msg.flags & FSB_OVERFLOWED)
		Sys_Error(__FUNCTION__ ": Overflowed on %s, %i is max size\n", string, msg.maxsize);

	for (int i = 0; i < svs.maxclients; ++i)
	{
		client_t *cl = &svs.clients[i];
		if (cl->active || cl->connected || (cl->spawned && !cl->fakeclient))
		{
			SZ_Write(&cl->netchan.message, msg.data, msg.cursize);
		}
	}
}

void SV_Serverinfo_f(void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf(const_cast<char*>("Server info settings:\n"));
		Info_Print(Info_Serverinfo());
		return;
	}
	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("usage: serverinfo [ <key> <value> ]\n"));
		return;
	}
	if (Cmd_Argv(1)[0] == '*')
	{
		Con_Printf(const_cast<char*>("Star variables cannot be changed.\n"));
		return;
	}

	Info_SetValueForKey(Info_Serverinfo(), Cmd_Argv(1), Cmd_Argv(2), 2 * MAX_INFO_STRING);

	cvar_t *var = Cvar_FindVar(Cmd_Argv(1));
	if (var)
	{
		Z_Free(var->string);
		var->string = CopyString((char *)Cmd_Argv(2));
		var->value = Q_atof(var->string);
	}

	SV_BroadcastCommand(const_cast<char*>("fullserverinfo \"%s\"\n"), Info_Serverinfo());
}

void SV_Localinfo_f(void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf(const_cast<char*>("Local info settings:\n"));
		Info_Print(localinfo);
		return;
	}
	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("usage: localinfo [ <key> <value> ]\n"));
		return;
	}
	if (Cmd_Argv(1)[0] == '*')
	{
		Con_Printf(const_cast<char*>("Star variables cannot be changed.\n"));
		return;
	}

	Info_SetValueForKey(localinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING * 128);
}

void SV_User_f(void)
{
	if (!sv.active)
	{
		Con_Printf(const_cast<char*>("Can't 'user', not running a server\n"));
		return;
	}
	if (Cmd_Argc() != 2)
	{
		Con_Printf(const_cast<char*>("Usage: user <username / userid>\n"));
		return;
	}

	int uid = Q_atoi(Cmd_Argv(1));
	client_t *cl = svs.clients;

	for (int i = 0; i < svs.maxclients; i++, cl++)
	{
		if (cl->active || cl->spawned || cl->connected)
		{
			if (!cl->fakeclient && cl->name[0])
			{
				if (cl->userid == uid || !Q_strcmp(cl->name, Cmd_Argv(1)))
				{
					Info_Print(cl->userinfo);
					return;
				}
			}
		}
	}

	Con_Printf(const_cast<char*>("User not in server.\n"));
}

char* SV_GetIDString(USERID_t *id)
{
	static char idstr[64];

	idstr[0] = 0;

	if (!id)
	{
		return idstr;
	}

	switch (id->idtype)
	{
	case AUTH_IDTYPE_STEAM:
		if (sv_lan.value != 0.0f)
		{
			Q_strncpy(idstr, "STEAM_ID_LAN", ARRAYSIZE(idstr) - 1);
		}
		else if (!id->m_SteamID)
		{
			Q_strncpy(idstr, "STEAM_ID_PENDING", ARRAYSIZE(idstr) - 1);
		}
		else
		{
			TSteamGlobalUserID steam2ID = Steam_Steam3IDtoSteam2(id->m_SteamID);
			_snprintf(idstr, ARRAYSIZE(idstr) - 1, "STEAM_%u:%u:%u", steam2ID.m_SteamInstanceID, steam2ID.m_SteamLocalUserID.Split.High32bits, steam2ID.m_SteamLocalUserID.Split.Low32bits);
		}
		break;
	case AUTH_IDTYPE_VALVE:
		if (sv_lan.value != 0.0f)
		{
			Q_strncpy(idstr, "VALVE_ID_LAN", ARRAYSIZE(idstr) - 1);
		}
		else if (!id->m_SteamID)
		{
			Q_strncpy(idstr, "VALVE_ID_PENDING", ARRAYSIZE(idstr) - 1);
		}
		else
		{
			TSteamGlobalUserID steam2ID = Steam_Steam3IDtoSteam2(id->m_SteamID);
			_snprintf(idstr, ARRAYSIZE(idstr) - 1, "VALVE_%u:%u:%u", steam2ID.m_SteamInstanceID, steam2ID.m_SteamLocalUserID.Split.High32bits, steam2ID.m_SteamLocalUserID.Split.Low32bits);
		}
		break;
	case AUTH_IDTYPE_LOCAL:
		Q_strncpy(idstr, "HLTV", ARRAYSIZE(idstr) - 1);
		break;
	default:
		Q_strncpy(idstr, "UNKNOWN", ARRAYSIZE(idstr) - 1);
		break;
	}

	return idstr;
}

char *SV_GetClientIDString(client_t *client)
{
	static char idstr[64];

	idstr[0] = 0;

	if (!client)
	{
		return idstr;
	}

	if (client->netchan.remote_address.type == NA_LOOPBACK && client->network_userid.idtype == AUTH_IDTYPE_VALVE)
	{
		_snprintf(idstr, ARRAYSIZE(idstr) - 1, "VALVE_ID_LOOPBACK");
	}
	else
	{
		USERID_t *id = &client->network_userid;
		_snprintf(idstr, ARRAYSIZE(idstr) - 1, "%s", SV_GetIDString(id));
		idstr[ARRAYSIZE(idstr) - 1] = 0;
	}

	return idstr;
}

void SV_Users_f(void)
{
	if (!sv.active)
	{
		Con_Printf(const_cast<char*>("Can't 'users', not running a server\n"));
		return;
	}

	Con_Printf(const_cast<char*>("userid : uniqueid : name\n"));
	Con_Printf(const_cast<char*>("------ : ---------: ----\n"));

	int c = 0;
	client_t *cl = svs.clients;

	for (int i = 0; i < svs.maxclients; i++, cl++)
	{
		if (cl->active || cl->spawned || cl->connected)
		{
			if (!cl->fakeclient && cl->name[0])
			{
				Con_Printf(const_cast<char*>("%6i : %s : %s\n"), cl->userid, SV_GetClientIDString(cl), cl->name);
				c++;
			}
		}
	}

	Con_Printf(const_cast<char*>("%i users\n"), c);
}

void SV_CountProxies(int *proxies)
{
	*proxies = 0;
	client_s *cl = svs.clients;

	for (int i = 0; i < svs.maxclients; i++, cl++)
	{
		if (cl->active || cl->spawned || cl->connected)
		{
			if (cl->proxy)
				(*proxies)++;
		}
	}
}

void SV_FindModelNumbers(void)
{
	sv_playermodel = -1;

	for (int i = 0; i < MAX_MODELS; i++)
	{
		if (!sv.model_precache[i])
			break;

		if (!Q_strcasecmp(sv.model_precache[i], "models/player.mdl"))
			sv_playermodel = i;
	}
}

void SV_StartParticle(const vec_t *org, const vec_t *dir, int color, int count)
{
	if (sv.datagram.cursize + 16 <= sv.datagram.maxsize)
	{
		MSG_WriteByte(&sv.datagram, svc_particle);
		MSG_WriteCoord(&sv.datagram, org[0]);
		MSG_WriteCoord(&sv.datagram, org[1]);
		MSG_WriteCoord(&sv.datagram, org[2]);

		for (int i = 0; i < 3; i++)
			MSG_WriteChar(&sv.datagram, clamp((int)(dir[i] * 16.0f), -128, 127));

		MSG_WriteByte(&sv.datagram, count);
		MSG_WriteByte(&sv.datagram, color);
	}
}

int SV_HashString(const char *string, int iBounds)
{
	unsigned int hash = 0;
	const char *cc = string;

	while (*cc)
	{
		hash = tolower(*cc) + 2 * hash;
		++cc;
	}
	return hash % iBounds;
}

void SV_AddSampleToHashedLookupTable(const char *pszSample, int iSampleIndex)
{
	int starting_index = SV_HashString(pszSample, MAX_SOUNDS_HASHLOOKUP_SIZE);
	int index = starting_index;
	while (sv.sound_precache_hashedlookup[index])
	{
		index++;
		hashstrings_collisions++;

		if (index >= MAX_SOUNDS_HASHLOOKUP_SIZE)
			index = 0;

		if (index == starting_index)
			Sys_Error(__FUNCTION__ ": NO FREE SLOTS IN SOUND LOOKUP TABLE");
	}

	sv.sound_precache_hashedlookup[index] = iSampleIndex;
}

void SV_BuildHashedSoundLookupTable(void)
{
	Q_memset(sv.sound_precache_hashedlookup, 0, sizeof(sv.sound_precache_hashedlookup));

	for (int sound_num = 0; sound_num < MAX_SOUNDS; sound_num++)
	{
		if (!sv.sound_precache[sound_num])
			break;

		SV_AddSampleToHashedLookupTable(sv.sound_precache[sound_num], sound_num);
	}

	sv.sound_precache_hashedlookup_built = TRUE;
}

int SV_LookupSoundIndex(const char *sample)
{
	int index;

	if (!sv.sound_precache_hashedlookup_built)
	{
		if (sv.state == ss_loading)
		{
			for (index = 1; index < MAX_SOUNDS && sv.sound_precache[index]; index++) // TODO: why from 1?
			{
				if (!Q_strcasecmp(sample, sv.sound_precache[index]))
					return index;
			}
			return 0;
		}
		SV_BuildHashedSoundLookupTable();
	}

	int starting_index = SV_HashString(sample, 1023);
	index = starting_index;
	while (sv.sound_precache_hashedlookup[index])
	{
		if (!Q_strcasecmp(sample, sv.sound_precache[sv.sound_precache_hashedlookup[index]]))
			return sv.sound_precache_hashedlookup[index];

		index++;
		if (index >= MAX_SOUNDS_HASHLOOKUP_SIZE)
			index = 0;
		if (index == starting_index)
			return 0;
	}
	return 0;
}

qboolean SV_BuildSoundMsg(edict_t *entity, int channel, const char *sample, int volume, float attenuation, int fFlags, int pitch, const float *origin, sizebuf_t *buffer)
{
	int sound_num;
	int field_mask;

	if (volume < 0 || volume > 255)
	{
		Con_Printf(const_cast<char*>("SV_StartSound: volume = %i"), volume);
		volume = (volume < 0) ? 0 : 255;
	}
	if (attenuation < 0.0f || attenuation > 4.0f)
	{
		Con_Printf(const_cast<char*>("SV_StartSound: attenuation = %f"), attenuation);
		attenuation = (attenuation < 0.0f) ? 0.0f : 4.0f;
	}
	if (channel < 0 || channel > 7)
	{
		Con_Printf(const_cast<char*>("SV_StartSound: channel = %i"), channel);
		channel = (channel < 0) ? CHAN_AUTO : CHAN_NETWORKVOICE_BASE;
	}
	if (pitch < 0 || pitch > 255)
	{
		Con_Printf(const_cast<char*>("SV_StartSound: pitch = %i"), pitch);
		pitch = (pitch < 0) ? 0 : 255;
	}

	field_mask = fFlags;

	if (*sample == '!')
	{
		field_mask |= SND_SENTENCE;
		sound_num = Q_atoi(sample + 1);
		if (sound_num >= CVOXFILESENTENCEMAX)
		{
			Con_Printf(const_cast<char*>("SV_StartSound: invalid sentence number: %s"), sample + 1);
			return FALSE;
		}
	}
	else if (*sample == '#')
	{
		field_mask |= SND_SENTENCE;
		sound_num = Q_atoi(sample + 1) + CVOXFILESENTENCEMAX;
	}
	else
	{
		sound_num = SV_LookupSoundIndex(sample);
		if (!sound_num || !sv.sound_precache[sound_num])
		{
			Con_Printf(const_cast<char*>("SV_StartSound: %s not precached (%d)\n"), sample, sound_num);
			return FALSE;
		}
	}

	int ent = NUM_FOR_EDICT(entity);

	if (volume != 255)
		field_mask |= SND_VOLUME;
	if (attenuation != 1)
		field_mask |= SND_ATTENUATION;
	if (pitch != PITCH_NORM)
		field_mask |= SND_PITCH;
	if (sound_num > 255)
		field_mask |= SND_NUMBER;

	MSG_WriteByte(buffer, svc_sound);
	MSG_StartBitWriting(buffer);
	MSG_WriteBits(field_mask, 9);
	if (field_mask & SND_VOLUME)
		MSG_WriteBits(volume, 8);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteBits((uint32)(attenuation * 64.0f), 8);
	MSG_WriteBits(channel, 3);
	MSG_WriteBits(ent, 11);
	MSG_WriteBits(sound_num, (field_mask & SND_NUMBER) ? 16 : 8);
	MSG_WriteBitVec3Coord(origin);
	if (field_mask & SND_PITCH)
		MSG_WriteBits(pitch, 8);
	MSG_EndBitWriting(buffer);

	return TRUE;
}

int SV_PointLeafnum(vec_t *p)
{
	mleaf_t *mleaf = Mod_PointInLeaf(p, sv.worldmodel);
	return mleaf ? (mleaf - sv.worldmodel->leafs) : 0;
}

qboolean SV_ValidClientMulticast(client_t *client, int soundLeaf, int to)
{
	if (Host_IsSinglePlayerGame() || client->proxy)
	{
		return TRUE;
	}

	int destination = to & ~(MSG_FL_ONE);
	if (destination == MSG_FL_BROADCAST)
	{
		return TRUE;
	}

	unsigned char* mask;
	if (destination == MSG_FL_PVS)
	{
		mask = CM_LeafPVS(soundLeaf);
	}
	else
	{
		if (destination == MSG_FL_PAS)
		{
			mask = CM_LeafPAS(soundLeaf);
		}
		else
		{
			Con_Printf(const_cast<char*>("MULTICAST: Error %d!\n"), to);
			return FALSE;
		}
	}

	if (!mask)
	{
		return TRUE;
	}

	int bitNumber = SV_PointLeafnum(client->edict->v.origin);
	if (mask[(bitNumber - 1) >> 3] & (1 << ((bitNumber - 1) & 7)))
	{
		return TRUE;
	}

	return FALSE;
}

void SV_Multicast(edict_t *ent, vec_t *origin, int to, qboolean reliable)
{
	client_t *save = host_client;
	int leafnum = SV_PointLeafnum(origin);
	if (ent && !(host_client && host_client->edict == ent))
	{
		for (int i = 0; i < svs.maxclients; i++)
		{
			if (svs.clients[i].edict == ent)
			{
				host_client = &svs.clients[i];
				break;
			}
		}
	}

	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *client = &svs.clients[i];
		if (!client->active)
			continue;

		if ((to & MSG_FL_ONE) && client == host_client)
			continue;

		if (ent && ent->v.groupinfo != 0 && client->edict->v.groupinfo != 0)
		{
			if (g_groupop)
			{
				if (g_groupop == GROUP_OP_NAND && (ent->v.groupinfo & client->edict->v.groupinfo))
					continue;
			}
			else
			{
				if (!(ent->v.groupinfo & client->edict->v.groupinfo))
					continue;
			}
		}

		if (SV_ValidClientMulticast(client, leafnum, to))
		{
			sizebuf_t *pBuffer = &client->netchan.message;
			if (!reliable)
				pBuffer = &client->datagram;

			if (pBuffer->cursize + sv.multicast.cursize < pBuffer->maxsize)
				SZ_Write(pBuffer, sv.multicast.data, sv.multicast.cursize);
		}
		else
		{
			gPacketSuppressed += sv.multicast.cursize;
		}
	}

	SZ_Clear(&sv.multicast);
	host_client = save;
}

void SV_StartSound(int recipients, edict_t *entity, int channel, const char *sample, int volume, float attenuation, int fFlags, int pitch)
{
	vec3_t origin;

	for (int i = 0; i < 3; i++)
	{
		origin[i] = (entity->v.maxs[i] + entity->v.mins[i]) * 0.5f + entity->v.origin[i];
	}

	if (!SV_BuildSoundMsg(entity, channel, sample, volume, attenuation, fFlags, pitch, origin, &sv.multicast))
	{
		return;
	}

	int flags = 0;
	if (recipients == 1)
	{
		flags = MSG_FL_ONE;
	}

	qboolean sendPAS = channel != CHAN_STATIC && !(fFlags & SND_STOP);
	if (sendPAS)
	{
		SV_Multicast(entity, origin, flags | MSG_FL_PAS, FALSE);
	}
	else
	{
		if (Host_IsSinglePlayerGame())
			SV_Multicast(entity, origin, flags | MSG_FL_BROADCAST, FALSE);
		else
			SV_Multicast(entity, origin, flags | MSG_FL_BROADCAST, TRUE);
	}
}

void SV_WriteMovevarsToClient(sizebuf_t *message)
{
	MSG_WriteByte(message, svc_newmovevars);
	MSG_WriteFloat(message, movevars.gravity);
	MSG_WriteFloat(message, movevars.stopspeed);
	MSG_WriteFloat(message, movevars.maxspeed);
	MSG_WriteFloat(message, movevars.spectatormaxspeed);
	MSG_WriteFloat(message, movevars.accelerate);
	MSG_WriteFloat(message, movevars.airaccelerate);
	MSG_WriteFloat(message, movevars.wateraccelerate);
	MSG_WriteFloat(message, movevars.friction);
	MSG_WriteFloat(message, movevars.edgefriction);
	MSG_WriteFloat(message, movevars.waterfriction);
	MSG_WriteFloat(message, movevars.entgravity);
	MSG_WriteFloat(message, movevars.bounce);
	MSG_WriteFloat(message, movevars.stepsize);
	MSG_WriteFloat(message, movevars.maxvelocity);
	MSG_WriteFloat(message, movevars.zmax);
	MSG_WriteFloat(message, movevars.waveHeight);
	MSG_WriteByte(message, movevars.footsteps != 0);
	MSG_WriteFloat(message, movevars.rollangle);
	MSG_WriteFloat(message, movevars.rollspeed);
	MSG_WriteFloat(message, movevars.skycolor_r);
	MSG_WriteFloat(message, movevars.skycolor_g);
	MSG_WriteFloat(message, movevars.skycolor_b);
	MSG_WriteFloat(message, movevars.skyvec_x);
	MSG_WriteFloat(message, movevars.skyvec_y);
	MSG_WriteFloat(message, movevars.skyvec_z);
	MSG_WriteString(message, movevars.skyName);
}

void SV_WriteDeltaDescriptionsToClient(sizebuf_t *msg)
{
	int i, c;

	delta_description_t nulldesc;
	Q_memset(&nulldesc, 0, sizeof(nulldesc));

	for (delta_info_t *p = g_sv_delta; p != NULL; p = p->next)
	{
		MSG_WriteByte(msg, svc_deltadescription);
		MSG_WriteString(msg, p->name);
		MSG_StartBitWriting(msg);

		c = p->delta->fieldCount;

		MSG_WriteBits(c, 16);

		for (i = 0; i < c; i++)
		{
			DELTA_WriteDelta((byte *)&nulldesc, (byte *)&p->delta->pdd[i], TRUE, (delta_t *)&g_MetaDelta, NULL);
		}

		MSG_EndBitWriting(msg);
	}
}

void SV_SetMoveVars(void)
{
	movevars.entgravity = 1.0f;
	movevars.gravity = sv_gravity.value;
	movevars.stopspeed = sv_stopspeed.value;
	movevars.maxspeed = sv_maxspeed.value;
	movevars.spectatormaxspeed = sv_spectatormaxspeed.value;
	movevars.accelerate = sv_accelerate.value;
	movevars.airaccelerate = sv_airaccelerate.value;
	movevars.wateraccelerate = sv_wateraccelerate.value;
	movevars.friction = sv_friction.value;
	movevars.edgefriction = sv_edgefriction.value;
	movevars.waterfriction = sv_waterfriction.value;
	movevars.bounce = sv_bounce.value;
	movevars.stepsize = sv_stepsize.value;
	movevars.maxvelocity = sv_maxvelocity.value;
	movevars.zmax = sv_zmax.value;
	movevars.waveHeight = sv_wateramp.value;
	movevars.footsteps = sv_footsteps.value;
	movevars.rollangle = sv_rollangle.value;
	movevars.rollspeed = sv_rollspeed.value;
	movevars.skycolor_r = sv_skycolor_r.value;
	movevars.skycolor_g = sv_skycolor_g.value;
	movevars.skycolor_b = sv_skycolor_b.value;
	movevars.skyvec_x = sv_skyvec_x.value;
	movevars.skyvec_y = sv_skyvec_y.value;
	movevars.skyvec_z = sv_skyvec_z.value;

	Q_strncpy(movevars.skyName, sv_skyname.string, sizeof(movevars.skyName) - 1);
	movevars.skyName[sizeof(movevars.skyName) - 1] = 0;
}

void SV_QueryMovevarsChanged(void)
{
	if (movevars.entgravity != 1.0f
		|| sv_maxspeed.value != movevars.maxspeed
		|| sv_gravity.value != movevars.gravity
		|| sv_stopspeed.value != movevars.stopspeed
		|| sv_spectatormaxspeed.value != movevars.spectatormaxspeed
		|| sv_accelerate.value != movevars.accelerate
		|| sv_airaccelerate.value != movevars.airaccelerate
		|| sv_wateraccelerate.value != movevars.wateraccelerate
		|| sv_friction.value != movevars.friction
		|| sv_edgefriction.value != movevars.edgefriction
		|| sv_waterfriction.value != movevars.waterfriction
		|| sv_bounce.value != movevars.bounce
		|| sv_stepsize.value != movevars.stepsize
		|| sv_maxvelocity.value != movevars.maxvelocity
		|| sv_zmax.value != movevars.zmax
		|| sv_wateramp.value != movevars.waveHeight
		|| sv_footsteps.value != movevars.footsteps
		|| sv_rollangle.value != movevars.rollangle
		|| sv_rollspeed.value != movevars.rollspeed
		|| sv_skycolor_r.value != movevars.skycolor_r
		|| sv_skycolor_g.value != movevars.skycolor_g
		|| sv_skycolor_b.value != movevars.skycolor_b
		|| sv_skyvec_x.value != movevars.skyvec_x
		|| sv_skyvec_y.value != movevars.skyvec_y
		|| sv_skyvec_z.value != movevars.skyvec_z
		|| Q_strcmp(sv_skyname.string, movevars.skyName))
	{
		SV_SetMoveVars();

		client_t *cl = svs.clients;
		for (int i = 0; i < svs.maxclients; i++, cl++)
		{
			if (!cl->fakeclient && (cl->active || cl->spawned || cl->connected))
				SV_WriteMovevarsToClient(&cl->netchan.message);
		}
	}
}

extern int build_number(void);
void SV_SendServerinfo(sizebuf_t *msg, client_t *client)
{
	char message[2048];

	if (developer.value != 0.0f || svs.maxclients > 1)
	{
		MSG_WriteByte(msg, svc_print);
		_snprintf(message, ARRAYSIZE(message), "%c\nBUILD %d SERVER (%i CRC)\nServer # %i\n", 2, build_number(), 0, svs.spawncount);
		MSG_WriteString(msg, message);
	}

	MSG_WriteByte(msg, svc_serverinfo);
	MSG_WriteLong(msg, PROTOCOL_VERSION);
	MSG_WriteLong(msg, svs.spawncount);

	int playernum = NUM_FOR_EDICT(client->edict) - 1;
	int mungebuffer = sv.worldmapCRC;

	COM_Munge3((byte *)&mungebuffer, sizeof(mungebuffer), ~playernum);
	MSG_WriteLong(msg, mungebuffer);

	MSG_WriteBuf(msg, sizeof(sv.clientdllmd5), sv.clientdllmd5);
	MSG_WriteByte(msg, svs.maxclients);
	MSG_WriteByte(msg, playernum);

	if (coop.value == 0.0f && deathmatch.value != 0.0f)
		MSG_WriteByte(msg, 1);
	else
		MSG_WriteByte(msg, 0);

	COM_FileBase(com_gamedir, message);
	MSG_WriteString(msg, message);

	MSG_WriteString(msg, Cvar_VariableString(const_cast<char*>("hostname")));

	MSG_WriteString(msg, sv.modelname);

	int len = 0;
	unsigned char *mapcyclelist = COM_LoadFileForMe(mapcyclefile.string, &len);
	if (mapcyclelist && len)
	{
#ifdef _2020_PATCH
		if (len > 8190)
		{
			len = 8192;
			mapcyclelist[8191] = 0;
		}
#endif

		MSG_WriteString(msg, (const char *)mapcyclelist);
		COM_FreeFile(mapcyclelist);
	}
	else
	{
		MSG_WriteString(msg, "mapcycle failure");
	}

	// isVAC2Secure
	MSG_WriteByte(msg, 0);

	MSG_WriteByte(msg, svc_sendextrainfo);
	MSG_WriteString(msg, com_clientfallback);
	MSG_WriteByte(msg, sv_cheats.value);

	SV_WriteDeltaDescriptionsToClient(msg);
	SV_SetMoveVars();
	SV_WriteMovevarsToClient(msg);

	MSG_WriteByte(msg, svc_cdtrack);
	MSG_WriteByte(msg, gGlobalVariables.cdAudioTrack);
	MSG_WriteByte(msg, gGlobalVariables.cdAudioTrack);
	MSG_WriteByte(msg, svc_setview);
	MSG_WriteShort(msg, playernum + 1);

	client->spawned = FALSE;
	client->connected = TRUE;
	client->fully_connected = FALSE;
}

// stopline : 1179
void SV_SendResources(sizebuf_t *msg)
{
	unsigned char nullbuffer[32];
	Q_memset(nullbuffer, 0, sizeof(nullbuffer));

	MSG_WriteByte(msg, svc_resourcerequest);
	MSG_WriteLong(msg, svs.spawncount);
	MSG_WriteLong(msg, 0);

	if (sv_downloadurl.string && sv_downloadurl.string[0] != 0 && Q_strlen(sv_downloadurl.string) < 129)
	{
		MSG_WriteByte(msg, svc_resourcelocation);
		MSG_WriteString(msg, sv_downloadurl.string);
	}

	MSG_WriteByte(msg, svc_resourcelist);
	MSG_StartBitWriting(msg);
	MSG_WriteBits(sv.num_resources, 12);

	resource_t *r = sv.resourcelist;

	for (int i = 0; i < sv.num_resources; i++, r++)
	{
		MSG_WriteBits(r->type, 4);
		MSG_WriteBitString(r->szFileName);
		MSG_WriteBits(r->nIndex, 12);
		MSG_WriteBits(r->nDownloadSize, 24);
		MSG_WriteBits(r->ucFlags & (RES_WASMISSING | RES_FATALIFMISSING), 3);

		if (r->ucFlags & RES_CUSTOM)
		{
			MSG_WriteBitData(r->rgucMD5_hash, sizeof(r->rgucMD5_hash));
		}

		if (!Q_memcmp(r->rguc_reserved, nullbuffer, sizeof(r->rguc_reserved)))
		{
			MSG_WriteBits(0, 1);
		}
		else
		{
			MSG_WriteBits(1, 1);
			MSG_WriteBitData(r->rguc_reserved, sizeof(r->rguc_reserved));
		}
	}

	SV_SendConsistencyList(msg);
	MSG_EndBitWriting(msg);
}

void SV_WriteClientdataToMessage(client_t *client, sizebuf_t *msg)
{
	edict_t *ent = client->edict;
	client_frame_t *frame = &client->frames[SV_UPDATE_MASK & client->netchan.outgoing_sequence];
	int bits = (SV_UPDATE_MASK & host_client->delta_sequence);

	frame->senttime = realtime;
	frame->ping_time = -1.0f;

	if (client->chokecount)
	{
		MSG_WriteByte(msg, svc_choke);
		client->chokecount = 0;
	}

	if (ent->v.fixangle)
	{
		if (ent->v.fixangle == 2)
		{
			MSG_WriteByte(msg, svc_addangle);
			MSG_WriteHiresAngle(msg, ent->v.avelocity[1]);
			ent->v.avelocity[1] = 0;
		}
		else
		{
			MSG_WriteByte(msg, svc_setangle);
			MSG_WriteHiresAngle(msg, ent->v.angles[0]);
			MSG_WriteHiresAngle(msg, ent->v.angles[1]);
			MSG_WriteHiresAngle(msg, ent->v.angles[2]);
		}
		ent->v.fixangle = 0;
	}

	Q_memset(&frame->clientdata, 0, sizeof(frame->clientdata));
	gEntityInterface.pfnUpdateClientData(ent, host_client->lw != 0, &frame->clientdata);

	MSG_WriteByte(msg, svc_clientdata);

	if (client->proxy)
		return;

	MSG_StartBitWriting(msg);

	clientdata_t baseline;
	Q_memset(&baseline, 0, sizeof(baseline));

	clientdata_t *from = &baseline;
	clientdata_t *to = &frame->clientdata;

	if (host_client->delta_sequence == -1)
	{
		MSG_WriteBits(0, 1);
	}
	else
	{
		MSG_WriteBits(1, 1);
		MSG_WriteBits(host_client->delta_sequence, 8);
		from = &host_client->frames[bits].clientdata;
	}

	DELTA_WriteDelta((byte *)from, (byte *)to, TRUE, (delta_t *)g_pclientdelta, NULL);

	if (host_client->lw && gEntityInterface.pfnGetWeaponData(host_client->edict, frame->weapondata))
	{
		weapon_data_t wbaseline;
		Q_memset(&wbaseline, 0, sizeof(wbaseline));

		weapon_data_t *fdata = NULL;
		weapon_data_t *tdata = frame->weapondata;

		for (int i = 0; i < 64; i++, tdata++)
		{
			if (host_client->delta_sequence == -1)
				fdata = &wbaseline;
			else
				fdata = &host_client->frames[bits].weapondata[i];

			if (DELTA_CheckDelta((byte *)fdata, (byte *)tdata, g_pweapondelta))
			{
				MSG_WriteBits(1, 1);
				MSG_WriteBits(i, 6);

				DELTA_WriteDelta((byte *)fdata, (byte *)tdata, TRUE, g_pweapondelta, NULL);
			}
		}
	}

	MSG_WriteBits(0, 1);
	MSG_EndBitWriting(msg);
}

void SV_FullClientUpdate(client_t *cl, sizebuf_t *sb)
{
	char info[MAX_INFO_STRING];
	unsigned char digest[16];
	MD5Context_t ctx;

	Q_strncpy(info, cl->userinfo, sizeof(info) - 1);
	info[sizeof(info) - 1] = '\0';
	Info_RemovePrefixedKeys(info, '_');

	MSG_WriteByte(sb, svc_updateuserinfo);
	MSG_WriteByte(sb, cl - svs.clients);
	MSG_WriteLong(sb, cl->userid);
	MSG_WriteString(sb, info);
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)cl->hashedcdkey, sizeof(cl->hashedcdkey));
	MD5Final(digest, &ctx);
	MSG_WriteBuf(sb, sizeof(digest), digest);
}

void SV_WriteSpawn(sizebuf_t *msg)
{
	int i = 0;
	client_t *client = svs.clients;
	SAVERESTOREDATA currentLevelData;
	char name[256];

	if (sv.loadgame)
	{
		if (host_client->proxy)
		{
			Con_Printf(const_cast<char*>("ERROR! Spectator mode doesn't work with saved game.\n"));
			return;
		}
		sv.paused = FALSE;
	}
	else
	{
		sv.state = ss_loading;
		ReleaseEntityDLLFields(sv_player);

		Q_memset(&sv_player->v, 0, sizeof(sv_player->v));
		InitEntityDLLFields(sv_player);

		sv_player->v.colormap = NUM_FOR_EDICT(sv_player);
		sv_player->v.netname = host_client->name - pr_strings;

		if (host_client->proxy)
			sv_player->v.flags |= FL_PROXY;

		gGlobalVariables.time = sv.time;
		gEntityInterface.pfnClientPutInServer(sv_player);
		sv.state = ss_active;
	}

	SZ_Clear(&host_client->netchan.message);
	SZ_Clear(&host_client->datagram);

	MSG_WriteByte(msg, svc_time);
	MSG_WriteFloat(msg, sv.time);

	host_client->sendinfo = TRUE;

	for (i = 0; i < svs.maxclients; i++, client++)
	{
		if (client == host_client || client->active || client->connected || client->spawned)
			SV_FullClientUpdate(client, msg);
	}

	for (i = 0; i < ARRAYSIZE(sv.lightstyles); i++)
	{
		MSG_WriteByte(msg, svc_lightstyle);
		MSG_WriteByte(msg, i);
		MSG_WriteString(msg, sv.lightstyles[i]);
	}

	if (!host_client->proxy)
	{
		MSG_WriteByte(msg, svc_setangle);
		MSG_WriteHiresAngle(msg, sv_player->v.v_angle[0]);
		MSG_WriteHiresAngle(msg, sv_player->v.v_angle[1]);
		MSG_WriteHiresAngle(msg, 0.0f);
		SV_WriteClientdataToMessage(host_client, msg);
		// добавлена проверка на SWDS. --barspinoff
#ifndef SWDS
		if (sv.loadgame)
		{
			// TODO: add client code
			Q_memset(&currentLevelData, 0, sizeof(currentLevelData));
			gGlobalVariables.pSaveData = &currentLevelData;
			gEntityInterface.pfnParmsChangeLevel();
			MSG_WriteByte(msg, svc_restore);
			snprintf(name, sizeof(name), "%s%s.HL2", Host_SaveGameDirectory(), sv.name);
			COM_FixSlashes(name);
			MSG_WriteString(msg, name);
			MSG_WriteByte(msg, currentLevelData.connectionCount);

			for (i = 0; i < currentLevelData.connectionCount; i++)
				MSG_WriteString(msg, currentLevelData.levelList[i].mapName);

			sv.loadgame = false;
			gGlobalVariables.pSaveData = NULL;
		}
#endif
	}

	MSG_WriteByte(msg, svc_signonnum);
	MSG_WriteByte(msg, 1);

	host_client->connecttime = 0.0;
	host_client->ignorecmdtime = 0.0;
	host_client->cmdtime = 0.0;
	host_client->active = TRUE;
	host_client->spawned = TRUE;
	host_client->connected = TRUE;
	host_client->fully_connected = FALSE;

	NotifyDedicatedServerUI("UpdatePlayers");
}

void SV_SendUserReg(sizebuf_t *msg)
{
	for (UserMsg *pMsg = sv_gpNewUserMsgs; pMsg; pMsg = pMsg->next)
	{
		MSG_WriteByte(msg, svc_newusermsg);
		MSG_WriteByte(msg, pMsg->iMsg);
		MSG_WriteByte(msg, pMsg->iSize);
		MSG_WriteLong(msg, *(int *)&pMsg->szName[0]);
		MSG_WriteLong(msg, *(int *)&pMsg->szName[4]);
		MSG_WriteLong(msg, *(int *)&pMsg->szName[8]);
		MSG_WriteLong(msg, *(int *)&pMsg->szName[12]);
	}
}

void SV_New_f(void)
{
	int i;
	client_t *client;
	unsigned char data[NET_MAX_PAYLOAD];
	sizebuf_t msg;
	edict_t *ent;
	char szRejectReason[128];
	char szAddress[256];
	char szName[64];

	Q_memset(&msg, 0, sizeof(msg));
	msg.buffername = "New Connection";
	msg.data = data;
	msg.maxsize = sizeof(data);
	msg.cursize = 0;

	// Not valid on the client
	if (cmd_source == src_command)
	{
		return;
	}

	if (host_client->hasusrmsgs && host_client->m_bSentNewResponse)
		return;

	if (!host_client->active && host_client->spawned)
	{
		return;
	}

	ent = host_client->edict;

	host_client->connected = TRUE;
	host_client->connection_started = realtime;
	host_client->m_sendrescount = 0;

	SZ_Clear(&host_client->netchan.message);
	SZ_Clear(&host_client->datagram);

	Netchan_Clear(&host_client->netchan);

	SV_SendServerinfo(&msg, host_client);

	if (sv_gpUserMsgs)
	{
		UserMsg *pTemp = sv_gpNewUserMsgs;
		sv_gpNewUserMsgs = sv_gpUserMsgs;
		SV_SendUserReg(&msg);
		sv_gpNewUserMsgs = pTemp;
	}
	host_client->hasusrmsgs = TRUE;

	// If the client was connected, tell the game dll to disconnect him/her.
	if ((host_client->active || host_client->spawned) && ent)
	{
		extern void DbgPrint(FILE*, const char* format, ...);
		extern FILE* m_fMessages;
		DbgPrint(m_fMessages, "disconnected in sv_new\r\n");
		gEntityInterface.pfnClientDisconnect(ent);
	}

	_snprintf(szName, sizeof(szName), host_client->name);
	_snprintf(szAddress, sizeof(szAddress), NET_AdrToString(host_client->netchan.remote_address));
	_snprintf(szRejectReason, sizeof(szRejectReason), "Connection rejected by game\n");

	// Allow the game dll to reject this client.
	if (!gEntityInterface.pfnClientConnect(ent, szName, szAddress, szRejectReason))
	{
		// Reject the connection and drop the client.
		MSG_WriteByte(&host_client->netchan.message, svc_stufftext);
		MSG_WriteString(&host_client->netchan.message, va(const_cast<char*>("echo %s\n"), szRejectReason));
		SV_DropClient(host_client, FALSE, const_cast<char*>("Server refused connection because:  %s"), szRejectReason);
		return;
	}

	MSG_WriteByte(&msg, svc_stufftext);
	MSG_WriteString(&msg, va(const_cast<char*>("fullserverinfo \"%s\"\n"), Info_Serverinfo()));
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (client == host_client || client->active || client->connected || client->spawned)
			SV_FullClientUpdate(client, &msg);
	}
	Netchan_CreateFragments(TRUE, &host_client->netchan, &msg);
	Netchan_FragSend(&host_client->netchan);
	// добавлена неучтённая команда. --barspinoff
	host_client->m_bSentNewResponse = true;
}

void SV_SendRes_f(void)
{
	unsigned char data[NET_MAX_PAYLOAD];
	sizebuf_t msg;

	Q_memset(&msg, 0, sizeof(msg));
	msg.buffername = "SendResources";
	msg.data = data;
	msg.maxsize = sizeof(data);
	msg.cursize = 0;

	if (cmd_source != src_command && (!host_client->spawned || host_client->active))
	{
		if (svs.maxclients <= 1 || host_client->m_sendrescount <= 1)
		{
			host_client->m_sendrescount++;
			SV_SendResources(&msg);
			Netchan_CreateFragments(true, &host_client->netchan, &msg);
			Netchan_FragSend(&host_client->netchan);
		}
	}
}

void SV_WriteVoiceCodec(sizebuf_t *pBuf)
{
	MSG_WriteByte(pBuf, svc_voiceinit);
	MSG_WriteString(pBuf, "");
	MSG_WriteByte(pBuf, 0);
}

// stopline
void SV_Spawn_f(void)
{
	unsigned char data[NET_MAX_PAYLOAD];
	sizebuf_t msg;

	Q_memset(&msg, 0, sizeof(msg));
	msg.buffername = "Spawning";
	msg.data = data;
	msg.maxsize = sizeof(data);
	msg.cursize = 0;

	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("spawn is not valid\n"));
		return;
	}

	host_client->crcValue = Q_atoi(Cmd_Argv(2));

	COM_UnMunge2((unsigned char *)&host_client->crcValue, 4, (-1 - svs.spawncount) & 0xFF);

	if (cmd_source == src_command)
	{
		Con_Printf(const_cast<char*>("spawn is not valid from the console\n"));
		return;
	}

	// добавлена проверка на спавн. --barspinoff
	if (host_client->spawned)
		return Con_Printf(const_cast<char*>("spawn is not valid when already spawned\n"));

	if (cls.demoplayback || Q_atoi(Cmd_Argv(1)) == svs.spawncount)
	{
		SZ_Write(&msg, sv.signon.data, sv.signon.cursize);
		SV_WriteSpawn(&msg);

		SV_WriteVoiceCodec(&msg);
		Netchan_CreateFragments(TRUE, &host_client->netchan, &msg);
		Netchan_FragSend(&host_client->netchan);
	}
	else
	{
		SV_New_f();
	}
}

void SV_CheckUpdateRate(double *rate)
{
	if (*rate == 0.0)
	{
		*rate = 0.05;
		return;
	}

	if (sv_maxupdaterate.value <= 0.001f && sv_maxupdaterate.value != 0.0f)
		Cvar_Set(const_cast<char*>("sv_maxupdaterate"), const_cast<char*>("30.0"));
	if (sv_minupdaterate.value <= 0.001f && sv_minupdaterate.value != 0.0f)
		Cvar_Set(const_cast<char*>("sv_minupdaterate"), const_cast<char*>("1.0"));

	if (sv_maxupdaterate.value != 0.0f)
	{
		if (*rate < 1.0 / sv_maxupdaterate.value)
			*rate = 1.0 / sv_maxupdaterate.value;
	}
	if (sv_minupdaterate.value != 0.0f)
	{
		if (*rate > 1.0 / sv_minupdaterate.value)
			*rate = 1.0 / sv_minupdaterate.value;
	}
}

void SV_RejectConnection(netadr_t *adr, char *fmt, ...)
{
	va_list argptr;
	char text[1024];
	va_start(argptr, fmt);
	_vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	SZ_Clear(&net_message);
	MSG_WriteLong(&net_message, -1);
	MSG_WriteByte(&net_message, S2C_CONNREJECT);
	MSG_WriteString(&net_message, text);
	NET_SendPacket(NS_SERVER, net_message.cursize, net_message.data, *adr);
	SZ_Clear(&net_message);
}

void SV_RejectConnectionForPassword(netadr_t *adr)
{
	SZ_Clear(&net_message);
	MSG_WriteLong(&net_message, -1);
	MSG_WriteByte(&net_message, S2C_BADPASSWORD);
	MSG_WriteString(&net_message, "BADPASSWORD");
	NET_SendPacket(NS_SERVER, net_message.cursize, net_message.data, *adr);
	SZ_Clear(&net_message);
}

int SV_GetFragmentSize(void *state)
{
	int size = FRAGMENT_S2C_MAX_SIZE;
	client_t *cl = (client_t *)state;

	if (cl->active && cl->spawned && cl->connected && cl->fully_connected)
	{
		size = FRAGMENT_S2C_MIN_SIZE;
		const char *val = Info_ValueForKey(cl->userinfo, "cl_dlmax");
		if (val[0] != 0)
		{
			size = Q_atoi(val);
			size = clamp(size, FRAGMENT_S2C_MIN_SIZE, FRAGMENT_S2C_MAX_SIZE);
		}
	}

	return size;
}

qboolean SV_CompareUserID(USERID_t *id1, USERID_t *id2)
{
	if (id1 == NULL || id2 == NULL)
		return FALSE;

	if (id1->idtype != id2->idtype)
		return FALSE;

	if (id1->idtype != AUTH_IDTYPE_STEAM && id1->idtype != AUTH_IDTYPE_VALVE)
		return FALSE;

	char szID1[64];
	char szID2[64];

	Q_strncpy(szID1, SV_GetIDString(id1), sizeof(szID1)-1);
	szID1[sizeof(szID1)-1] = 0;

	Q_strncpy(szID2, SV_GetIDString(id2), sizeof(szID2)-1);
	szID2[sizeof(szID2)-1] = 0;

	return Q_strcasecmp(szID1, szID2) ? FALSE : TRUE;
}

qboolean SV_FilterUser(USERID_t *userid)
{
	int j = numuserfilters;
	for (int i = numuserfilters - 1; i >= 0; i--)
	{
		userfilter_t *filter = &userfilters[i];
		if (filter->banEndTime == 0.0f || filter->banEndTime > realtime)
		{
			if (SV_CompareUserID(userid, &filter->userid))
				return (qboolean)sv_filterban.value;
		}
		else
		{
			if (i + 1 < j)
				memmove(filter, &filter[1], sizeof(userfilter_t)* (j - i + 1));

			numuserfilters = --j;
		}
	}

	return sv_filterban.value == 0.0f ? TRUE : FALSE;
}

int SV_CheckProtocol(netadr_t *adr, int nProtocol)
{
	if (adr == NULL)
	{
		Sys_Error(__FUNCTION__ ":  Null address\n");
	}

	if (nProtocol == PROTOCOL_VERSION)
	{
		return TRUE;
	}

	if (nProtocol < PROTOCOL_VERSION)
	{
		SV_RejectConnection(
			adr,
			const_cast<char*>("This server is using a newer protocol ( %i ) than your client ( %i ).  You should check for updates to your client.\n"),
			PROTOCOL_VERSION,
			nProtocol);
	}
	else
	{
		const char *contact = "(no email address specified)";
		if (sv_contact.string[0] != 0)
			contact = sv_contact.string;

		SV_RejectConnection(
			adr,
			const_cast<char*>("This server is using an older protocol ( %i ) than your client ( %i ).  If you believe this server is outdated, you can contact the server administrator at %s.\n"),
			PROTOCOL_VERSION,
			nProtocol,
			contact);
	}

	return FALSE;
}

int SV_CheckChallenge(netadr_t *adr, int nChallengeValue)
{
	if (!adr)
	{
		Sys_Error(__FUNCTION__ ":  Null address\n");
		// в псевдокоде завершение выполнения не отображается в то время как в машинном сразу после ошибки идёт retn
		return 0;
	}

	if (NET_IsLocalAddress(*adr))
		return TRUE;

	for (int i = 0; i < MAX_CHALLENGES; i++)
	{
		if (NET_CompareBaseAdr(net_from, g_rg_sv_challenges[i].adr))
		{
			if (nChallengeValue != g_rg_sv_challenges[i].challenge)
			{
				SV_RejectConnection(adr, const_cast<char*>("Bad challenge.\n"));
				return FALSE;
			}
			return TRUE;
		}
	}
	SV_RejectConnection(adr, const_cast<char*>("No challenge for your address.\n"));
	return FALSE;
}

int SV_CheckIPRestrictions(netadr_t *adr, int nAuthProtocol)
{
	if (sv_lan.value || nAuthProtocol != PROTOCOL_STEAM)
	{
		if (nAuthProtocol == PROTOCOL_HASHEDCDKEY)
			return 1;

		return (sv_lan.value && (NET_CompareClassBAdr(*adr, net_local_adr) || NET_IsReservedAdr(*adr)));
	}
	return 1;
}

int SV_CheckIPConnectionReuse(netadr_t *adr)
{
	int count = 0;

	client_t *cl = svs.clients;
	for (int i = 0; i < svs.maxclients; i++, cl++)
	{
		if (cl->connected && !cl->fully_connected && NET_CompareBaseAdr(cl->netchan.remote_address, *adr))
		{
			count++;
		}
	}

	if (count > 5)
	{
		Log_Printf("Too many connect packets from %s\n", NET_AdrToString(*adr));
		return 0;
	}

	return 1;
}

int SV_FinishCertificateCheck(netadr_t *adr, int nAuthProtocol, char *szRawCertificate, char *userinfo)
{
	const char *val;
	
	if (nAuthProtocol != PROTOCOL_HASHEDCDKEY)
	{
		if (Q_strcasecmp(szRawCertificate, "steam"))
		{
			SV_RejectConnection(adr, const_cast<char*>("Expecting STEAM authentication USERID ticket!\n"));
			return 0;
		}

		return 1;
	}

	if (Q_strlen(szRawCertificate) != 32)
	{
		SV_RejectConnection(adr, const_cast<char*>("Invalid CD Key.\n"));
		return 0;
	}

	if (adr->type == NA_LOOPBACK)
	{
		return 1;
	}

	val = Info_ValueForKey(userinfo, "*hltv");

	if (val[0] == 0 || Q_atoi(val) != TYPE_PROXY)
	{
		SV_RejectConnection(adr, const_cast<char*>("Invalid CD Key.\n"));
		return 0;
	}

	return 1;
}

int SV_CheckKeyInfo(netadr_t *adr, char *protinfo, unsigned short *port, int *pAuthProtocol, char *pszRaw, char *cdkey)
{
	const char *s = Info_ValueForKey(protinfo, "prot");
	int nAuthProtocol = Q_atoi(s);

	if (nAuthProtocol <= 0 || nAuthProtocol > PROTOCOL_LASTVALID)
	{
		SV_RejectConnection(adr, const_cast<char*>("Invalid connection.\n"));
		return 0;
	}

	s = Info_ValueForKey(protinfo, "raw");

	if (s[0] == '\0' || (nAuthProtocol == PROTOCOL_HASHEDCDKEY && Q_strlen(s) != 32))
	{
		SV_RejectConnection(adr, const_cast<char*>("Invalid authentication certificate length.\n"));
		return 0;
	}

	Q_strcpy(pszRaw, s);

	if (nAuthProtocol != PROTOCOL_HASHEDCDKEY)
	{
		s = Info_ValueForKey(protinfo, "cdkey");

		if (Q_strlen(s) != 32)
		{
			SV_RejectConnection(adr, const_cast<char*>("Invalid hashed CD key.\n"));
			return 0;
		}
	}

	_snprintf(cdkey, 64, "%s", s);
	*pAuthProtocol = nAuthProtocol;
	*port = Q_atoi("27005");

	return 1;
}

int SV_CheckForDuplicateSteamID(client_t *client)
{
	if (sv_lan.value != 0.0f)
		return -1;

	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *cl = &svs.clients[i];

		if (!cl->connected || cl->fakeclient || cl == client)
			continue;

		if (cl->network_userid.idtype != AUTH_IDTYPE_STEAM && cl->network_userid.idtype != AUTH_IDTYPE_VALVE)
			continue;

		if (SV_CompareUserID(&client->network_userid, &cl->network_userid))
			return i;
	}

	return -1;
}

qboolean SV_CheckForDuplicateNames(char *userinfo, qboolean bIsReconnecting, int nExcludeSlot)
{
	int dupc = 0;
	qboolean changed = FALSE;

	const char *val = Info_ValueForKey(userinfo, "name");

	// добавлен блок проверки на спецсимволы. --barspinoff
	if (!val || val[0] == '\0' || !Q_strcasecmp(val, "console") || strstr(val, "..") != NULL || strstr(val, "\"") != NULL || strstr(val, "\\") != NULL)
	{
		Info_SetValueForKey(userinfo, "name", "unnamed", MAX_INFO_STRING);
		return true;
	}

	char rawname[32], newname[32];
	Q_strncpy(rawname, val, sizeof(rawname) - 1);

	while (true)
	{
		int clientId = 0;
		client_t *client = &svs.clients[0];
		for (; clientId < svs.maxclients; clientId++, client++)
		{
			if (client->connected && !(clientId == nExcludeSlot && bIsReconnecting) && !Q_strcasecmp(client->name, val))
				break;
		}

		// no duplicates for current name
		if (clientId == svs.maxclients)
			return changed;

		_snprintf(newname, sizeof(newname), "(%d)%-0.*s", ++dupc, 28, rawname);

		Info_SetValueForKey(userinfo, "name", newname, MAX_INFO_STRING);
		val = Info_ValueForKey(userinfo, "name");
		changed = TRUE;
	}
}

int SV_CheckUserInfo(netadr_t *adr, char *userinfo, qboolean bIsReconnecting, int nReconnectSlot, char *name)
{
	const char *s;
	char newname[32];
	int proxies;

	if (!NET_IsLocalAddress(*adr))
	{
		const char* password = Info_ValueForKey(userinfo, "password");

		if (sv_password.string[0] != '\0' && Q_strcasecmp(sv_password.string, "none") && Q_strcmp(sv_password.string, password))
		{
			Con_Printf(const_cast<char*>("%s:  password failed\n"), NET_AdrToString(*adr));
			SV_RejectConnectionForPassword(adr);


			return 0;
		}
	}

	int i = Q_strlen(userinfo);
	if (i <= 4 || strstr(userinfo, "\\\\") || userinfo[i - 1] == '\\')
	{
		SV_RejectConnection(adr, const_cast<char*>("Unknown HLTV client type.\n"));

		return 0;
	}

	Info_RemoveKey(userinfo, "password");

	s = Info_ValueForKey(userinfo, "name");

	Q_strncpy(newname, s, sizeof(newname)-1);
	newname[sizeof(newname)-1] = '\0';

	for (char* pChar = newname; *pChar; pChar++)
	{
		if (*pChar == '%'
			|| *pChar == '&'
			)
			*pChar = ' ';
	}

	TrimSpace(newname, name);

	if (!Q_UnicodeValidate(name))
	{
		Q_UnicodeRepair(name);
	}

	for (char* pChar = newname; *pChar == '#'; pChar++)
	{
		*pChar = ' ';
	}

	if (name[0] == '\0' || !Q_strcasecmp(name, "console") || strstr(name, "..") != NULL)
	{
		Info_SetValueForKey(userinfo, "name", "unnamed", MAX_INFO_STRING);
	}
	else
	{
		Info_SetValueForKey(userinfo, "name", name, MAX_INFO_STRING);
	}

	if (SV_CheckForDuplicateNames(userinfo, bIsReconnecting, nReconnectSlot))
	{
		Q_strncpy(name, Info_ValueForKey(userinfo, "name"), 31);
		name[31] = '\0';
	}

	s = Info_ValueForKey(userinfo, "*hltv");
	if (!s[0])
		return 1;

	switch (Q_atoi(s))
	{
	case TYPE_CLIENT:
		return 1;

	case TYPE_PROXY:
		SV_CountProxies(&proxies);
		if (proxies >= sv_proxies.value && !bIsReconnecting)
		{
			SV_RejectConnection(adr, const_cast<char*>("Proxy slots are full.\n"));
			return 0;
		}
		return 1;

	case TYPE_COMMENTATOR:
		SV_RejectConnection(adr, const_cast<char*>("Please connect to HLTV master proxy.\n"));
		return 0;

	default:
		SV_RejectConnection(adr, const_cast<char*>("Unknown HLTV client type.\n"));
		return 0;
	}
}

int SV_FindEmptySlot(netadr_t *adr, int *pslot, client_t ** ppClient)
{
	if (svs.maxclients > 0)
	{
		int slot;
		client_t *client = svs.clients;

		for (slot = 0; slot < svs.maxclients; slot++, client++)
		{
			if (!client->active && !client->spawned && !client->connected)
				break;
		}

		if (slot < svs.maxclients)
		{
			*pslot = slot;
			*ppClient = client;
			return 1;
		}
	}

	SV_RejectConnection(adr, const_cast<char*>("Server is full.\n"));
	return 0;
}

void SV_CheckRate(client_t *cl)
{
	if (sv_maxrate.value > 0.0f)
	{
		if (cl->netchan.rate > sv_maxrate.value)
		{
			if (sv_maxrate.value > MAX_RATE)
				cl->netchan.rate = MAX_RATE;
			else
				cl->netchan.rate = sv_maxrate.value;
		}
	}
	if (sv_minrate.value > 0.0f)
	{
		if (cl->netchan.rate < sv_minrate.value)
		{
			if (sv_minrate.value < MIN_RATE)
				cl->netchan.rate = MIN_RATE;
			else
				cl->netchan.rate = sv_minrate.value;
		}
	}
}

void SV_ExtractFromUserinfo(client_t *cl)
{
	const char *val;
	int i;
	char newname[32];
	char *userinfo = cl->userinfo;

	val = Info_ValueForKey(userinfo, "name");
	Q_strncpy(newname, val, sizeof(newname)-1);
	newname[sizeof(newname)-1] = '\0';

	for (char *p = newname; *p; p++)
	{
		if (*p == '%'
			|| *p == '&'
			)
			*p = ' ';
	}

	// Fix name to not start with '#', so it will not resemble userid
	for (char *p = newname; *p == '#'; p++) *p = ' ';

	TrimSpace(newname, newname);

	if (!Q_UnicodeValidate(newname))
	{
		Q_UnicodeRepair(newname);
	}

	if (newname[0] == '\0' || !Q_strcasecmp(newname, "console"))
	{
		Info_SetValueForKey(userinfo, "name", "unnamed", MAX_INFO_STRING);
	}
	else if (Q_strcmp(val, newname))
	{
		Info_SetValueForKey(userinfo, "name", newname, MAX_INFO_STRING);
	}

	// Check for duplicate names
	SV_CheckForDuplicateNames(userinfo, TRUE, cl - svs.clients);

	gEntityInterface.pfnClientUserInfoChanged(cl->edict, userinfo);

	val = Info_ValueForKey(userinfo, "name");
	Q_strncpy(cl->name, val, sizeof(cl->name) - 1);
	cl->name[sizeof(cl->name) - 1] = '\0';

	ISteamGameServer_BUpdateUserData(cl->network_userid.m_SteamID, cl->name, 0);

	val = Info_ValueForKey(userinfo, "rate");
	if (val[0] != '\0')
	{
		i = Q_atoi(val);
		cl->netchan.rate = clamp(float(i), MIN_RATE, MAX_RATE);
	}

	val = Info_ValueForKey(userinfo, "topcolor");
	if (val[0] != '\0')
		cl->topcolor = Q_atoi(val);
	else
		Con_DPrintf(const_cast<char*>("topcolor unchanged for %s\n"), cl->name);

	val = Info_ValueForKey(userinfo, "bottomcolor");
	if (val[0] != '\0')
		cl->bottomcolor = Q_atoi(val);
	else
		Con_DPrintf(const_cast<char*>("bottomcolor unchanged for %s\n"), cl->name);

	val = Info_ValueForKey(userinfo, "cl_updaterate");
	if (val[0] != '\0')
	{
		i = Q_atoi(val);
		if (i >= 10)
			cl->next_messageinterval = 1.0 / i;
		else
			cl->next_messageinterval = 0.1;
	}

	val = Info_ValueForKey(userinfo, "cl_lw");
	cl->lw = val[0] != '\0' ? Q_atoi(val) != 0 : 0;

	val = Info_ValueForKey(userinfo, "cl_lc");
	cl->lc = val[0] != '\0' ? Q_atoi(val) != 0 : 0;

	val = Info_ValueForKey(userinfo, "*hltv");
	cl->proxy = val[0] != '\0' ? Q_atoi(val) == TYPE_PROXY : 0;

	SV_CheckUpdateRate(&cl->next_messageinterval);
	SV_CheckRate(cl);
}

void SV_ConnectClient(void)
{
	client_t *client;
	netadr_t adr;
	int nClientSlot = 0;
	char userinfo[1024];
	char protinfo[1024];
	char cdkey[64] = {};
	const char *s;
	char name[32];
	char szRawCertificate[512];
	int nAuthProtocol;
	unsigned short port;
	qboolean reconnect;
	qboolean bIsSecure;

	client = NULL;
	Q_memcpy(&adr, &net_from, sizeof(adr));
	nAuthProtocol = -1;
	reconnect = FALSE;
	port = Q_atoi(PORT_CLIENT);
	if (Cmd_Argc() < 5)
	{
		SV_RejectConnection(&adr, const_cast<char*>("Insufficient connection info\n"));
		return;
	}

	if (!SV_CheckProtocol(&adr, Q_atoi(Cmd_Argv(1))))
		return;

	if (!SV_CheckChallenge(&adr, Q_atoi(Cmd_Argv(2))))
		return;

	Q_memset(szRawCertificate, 0, sizeof(szRawCertificate));
	s = Cmd_Argv(3);
	if (!Info_IsValid(s))
	{
		SV_RejectConnection(&adr, const_cast<char*>("Invalid protinfo in connect command\n"));
		return;
	}

	Q_strncpy(protinfo, s, sizeof(protinfo)-1);
	protinfo[sizeof(protinfo)-1] = 0;
	if (!SV_CheckKeyInfo(&adr, protinfo, &port, &nAuthProtocol, szRawCertificate, cdkey))
		return;

	FILE* f = fopen("wauth0.log", "wb");
	fprintf(f, "proto %d\r\n", nAuthProtocol);
	fclose(f);

	if (!SV_CheckIPRestrictions(&adr, nAuthProtocol))
	{
		SV_RejectConnection(&adr, const_cast<char*>("LAN servers are restricted to local clients (class C).\n"));
		return;
	}

	s = Cmd_Argv(4);
	if (Q_strlen(s) > MAX_INFO_STRING || !Info_IsValid(s))
	{
		SV_RejectConnection(&adr, const_cast<char*>("Invalid userinfo in connect command\n"));
		return;
	}
	Q_strncpy(userinfo, s, sizeof(userinfo)-1);
	userinfo[sizeof(userinfo)-1] = 0;
	if (Master_IsLanGame() || nAuthProtocol == PROTOCOL_HASHEDCDKEY || nAuthProtocol == PROTOCOL_STEAM)
	{
		for (nClientSlot = 0; nClientSlot < svs.maxclients; nClientSlot++)
		{
			client = &svs.clients[nClientSlot];
			if (NET_CompareAdr(adr, client->netchan.remote_address))
			{
				reconnect = TRUE;
				break;
			}
		}
	}

	if (!SV_CheckUserInfo(&adr, userinfo, reconnect, nClientSlot, name))
		return;

	if (!SV_FinishCertificateCheck(&adr, nAuthProtocol, szRawCertificate, userinfo))
		return;

	if (reconnect)
	{
		Steam_NotifyClientDisconnect(client);

		extern void DbgPrint(FILE*, const char* format, ...);
		extern FILE* m_fMessages;
		DbgPrint(m_fMessages, "disconnected in connectclient\r\n");
		if ((client->active || client->spawned) && client->edict)
			gEntityInterface.pfnClientDisconnect(client->edict);

		Con_Printf(const_cast<char*>("%s:reconnect\n"), NET_AdrToString(adr));
	}
	else
	{
		if (!SV_FindEmptySlot(&adr, &nClientSlot, &client))
			return;
	}

	if (!SV_CheckIPConnectionReuse(&adr))
		return;

	int len = net_message.cursize - msg_readcount;
	f = fopen("wauth2.log", "wb");
	fprintf(f, "proto %d\r\n", nAuthProtocol);
	fclose(f);

	host_client = client;
	client->userid = g_userid++;
	if (nAuthProtocol == PROTOCOL_STEAM)
	{
		char szSteamAuthBuf[1024];
		int len = net_message.cursize - msg_readcount;
		FILE* f = fopen("wauth.log", "wb");
		fwrite(net_message.data, net_message.cursize, 1, f);
		fclose(f);
		if (net_message.cursize - msg_readcount <= 0 || len >= sizeof(szSteamAuthBuf))
		{
			SV_RejectConnection(&adr, const_cast<char*>("STEAM certificate length error! %i/%i\n"), net_message.cursize - msg_readcount, sizeof(szSteamAuthBuf));
			return;
		}
		Q_memcpy(szSteamAuthBuf, &net_message.data[msg_readcount], len);
		client->network_userid.clientip = *(uint32 *)&adr.ip[0];
		if (adr.type == NA_LOOPBACK)
		{
			if (sv_lan.value <= 0.0f)
				client->network_userid.clientip = *(uint32 *)&adr.ip[0];
			else
				client->network_userid.clientip = 0x7F000001; //127.0.0.1
		}

		client->netchan.remote_address.port = adr.port ? adr.port : port;
		if (!Steam_NotifyClientConnect(client, szSteamAuthBuf, len))
		{
			if (sv_lan.value == 0.0f)
			{
				SV_RejectConnection(&adr, const_cast<char*>("STEAM validation rejected\n"));
				return;
			}
			host_client->network_userid.idtype = AUTH_IDTYPE_STEAM;
			host_client->network_userid.m_SteamID = 0;
		}
	}
	else
	{
		if (nAuthProtocol != PROTOCOL_HASHEDCDKEY)
		{
			SV_RejectConnection(&adr, const_cast<char*>("Invalid authentication type\n"));
			return;
		}

		const char* val = Info_ValueForKey(userinfo, "*hltv");
		if (!Q_strlen(val))
		{
			SV_RejectConnection(&adr, const_cast<char*>("Invalid validation type\n"));
			return;
		}
		if (Q_atoi(val) != TYPE_PROXY)
		{
			SV_RejectConnection(&adr, const_cast<char*>("Invalid validation type\n"));
			return;
		}
		host_client->network_userid.idtype = AUTH_IDTYPE_LOCAL;
		host_client->network_userid.m_SteamID = 0;
		host_client->network_userid.clientip = *(uint32 *)&adr.ip[0];
		Steam_NotifyBotConnect(client);
	}

	SV_ClearResourceLists(host_client);
	SV_ClearFrames(&host_client->frames);

	host_client->frames = (client_frame_t *)Mem_ZeroMalloc(sizeof(client_frame_t) * SV_UPDATE_BACKUP);
	host_client->resourcesneeded.pPrev = &host_client->resourcesneeded;
	host_client->resourcesneeded.pNext = &host_client->resourcesneeded;
	host_client->resourcesonhand.pPrev = &host_client->resourcesonhand;
	host_client->resourcesonhand.pNext = &host_client->resourcesonhand;
	host_client->edict = EDICT_NUM(nClientSlot + 1);

	if (g_modfuncs.m_pfnConnectClient)
		g_modfuncs.m_pfnConnectClient(nClientSlot);

	Netchan_Setup(NS_SERVER, &host_client->netchan, adr, client - svs.clients, client, SV_GetFragmentSize);
	host_client->next_messageinterval = 5.0;
	host_client->next_messagetime = realtime + 0.05;
	host_client->delta_sequence = -1;
	Q_memset(&host_client->lastcmd, 0, sizeof(usercmd_t));
	host_client->nextping = -1.0;
	if (host_client->netchan.remote_address.type == NA_LOOPBACK)
	{
		Con_DPrintf(const_cast<char*>("Local connection.\n"));
	}
	else
	{
		Con_DPrintf(const_cast<char*>("Client %s connected\nAdr: %s\n"), name, NET_AdrToString(host_client->netchan.remote_address));
	}


	// возможный баг: на этом месте может быть пересчёт
	// контрольной суммы md5 и запись дайджеста в host_client.
	// --barspinoff
	Q_strncpy(host_client->hashedcdkey, cdkey, 32);
	host_client->hashedcdkey[32] = '\0';

	host_client->active = FALSE;
	host_client->spawned = FALSE;
	host_client->connected = TRUE;
	host_client->uploading = FALSE;
	host_client->fully_connected = FALSE;

	bIsSecure = Steam_GSBSecure();
	Netchan_OutOfBandPrint(NS_SERVER, adr, const_cast<char*>("%c %i \"%s\" %i %i"), S2C_CONNECTION, host_client->userid, NET_AdrToString(host_client->netchan.remote_address), bIsSecure, build_number());

	Log_Printf("\"%s<%i><%s><>\" connected, address \"%s\"\n", name, host_client->userid, SV_GetClientIDString(host_client), NET_AdrToString(host_client->netchan.remote_address));

	Q_strncpy(host_client->userinfo, userinfo, MAX_INFO_STRING);

	host_client->userinfo[MAX_INFO_STRING - 1] = 0;

	SV_ExtractFromUserinfo(host_client);
	Info_SetValueForStarKey(host_client->userinfo, "*sid", va(const_cast<char*>("%lld"), host_client->network_userid.m_SteamID), MAX_INFO_STRING);

	host_client->datagram.flags = FSB_ALLOWOVERFLOW;
	host_client->datagram.data = (byte *)host_client->datagram_buf;
	host_client->datagram.maxsize = sizeof(host_client->datagram_buf);
	host_client->datagram.buffername = host_client->name;
	host_client->sendinfo_time = 0.0f;
}

void SVC_Ping(void)
{
	char data[6];
	data[0] = data[1] = data[2] = data[3] = 0xFF;
	data[4] = A2A_ACK;
	data[5] = 0x00;
	NET_SendPacket(NS_SERVER, sizeof(data), data, net_from);
}

int SV_GetChallenge(const netadr_t& adr)
{
	int i;
	int oldest = 0;
	int oldestTime = INT_MAX;

	for (i = 0; i < MAX_CHALLENGES; i++)
	{
		if (NET_CompareBaseAdr(adr, g_rg_sv_challenges[i].adr))
			break;
		if (g_rg_sv_challenges[i].time < oldestTime)
		{
			oldest = i;
			oldestTime = g_rg_sv_challenges[i].time;
		}
	}

	if (i == MAX_CHALLENGES)
	{
		i = oldest;
		// generate new challenge number
		g_rg_sv_challenges[i].challenge = (RandomLong(0, 36863) << 16) | (RandomLong(0, 65535));
		g_rg_sv_challenges[i].adr = adr;
		g_rg_sv_challenges[i].time = (int)realtime;
	}

	return g_rg_sv_challenges[i].challenge;
}

void SVC_GetChallenge(void)
{
	char data[1024];
	qboolean steam = (Cmd_Argc() == 2 && !Q_strcasecmp(Cmd_Argv(1), "steam"));
	int challenge = SV_GetChallenge(net_from);

	if (steam)
#ifdef _WIN32
		_snprintf(data, sizeof(data), "%c%c%c%c%c00000000 %u %i %I64i %d\n", 255, 255, 255, 255, S2C_CHALLENGE, challenge, PROTOCOL_STEAM, Steam_GSGetSteamID(), Steam_GSBSecure());
#else
		_snprintf(data, sizeof(data), "%c%c%c%c%c00000000 %u %i %llim %d\n", 255, 255, 255, 255, S2C_CHALLENGE, challenge, PROTOCOL_STEAM, Steam_GSGetSteamID(), Steam_GSBSecure());
#endif
	else
	{
		Con_DPrintf(const_cast<char*>("Server requiring authentication\n"));
		_snprintf(data, sizeof(data), "%c%c%c%c%c00000000 %u %i\n", 255, 255, 255, 255, S2C_CHALLENGE, challenge, PROTOCOL_HASHEDCDKEY);
	}

	NET_SendPacket(NS_SERVER, Q_strlen(data) + 1, data, net_from);
}

void SVC_ServiceChallenge(void)
{
	char data[128];
	const char *type;
	int challenge;

	if (Cmd_Argc() != 2)
		return;

	type = Cmd_Argv(1);
	if (!type)
		return;

	if (!type[0] || Q_strcasecmp(type, "rcon"))
		return;

	challenge = SV_GetChallenge(net_from);

	_snprintf(data, sizeof(data), "%c%c%c%cchallenge %s %u\n", 255, 255, 255, 255, type, challenge);

	NET_SendPacket(NS_SERVER, Q_strlen(data) + 1, data, net_from);
}

int SV_GetFakeClientCount(void)
{
	int i;
	int fakeclients = 0;

	for (i = 0; i < svs.maxclients; i++)
	{
		client_t *client = &svs.clients[i];

		if (client->fakeclient)
			fakeclients++;
	}
	return fakeclients;
}

qboolean SV_GetModInfo(char *pszInfo, char *pszDL, int *version, int *size, qboolean *svonly, qboolean *cldll, char *pszHLVersion)
{
	if (gmodinfo.bIsMod)
	{
		Q_strcpy(pszInfo, gmodinfo.szInfo);
		Q_strcpy(pszDL, gmodinfo.szDL);
		Q_strcpy(pszHLVersion, gmodinfo.szHLVersion);

		*version = gmodinfo.version;
		*size = gmodinfo.size;
		*svonly = gmodinfo.svonly;
		*cldll = gmodinfo.cldll;
	}
	else
	{
		Q_strcpy(pszInfo, "");
		Q_strcpy(pszDL, "");
		Q_strcpy(pszHLVersion, "");

		*version = 1;
		*size = 0;
		*svonly = TRUE;
		*cldll = FALSE;
	}
	return gmodinfo.bIsMod;
}

qboolean RequireValidChallenge(netadr_t* adr)
{
	return sv_enableoldqueries.value == 0.0f;
}

qboolean ValidInfoChallenge(netadr_t *adr, const char *nugget)
{
	if (nugget && sv.active && svs.maxclients > 1)
	{
		if (RequireValidChallenge(adr))
			return Q_strcasecmp(nugget, A2S_KEY_STRING) == 0;
		return TRUE;
	}
	return FALSE;
}

int GetChallengeNr(netadr_t *adr)
{
	int oldest = 0;
	int oldestTime = 0x7FFFFFFF;
	int i;

	for (i = 0; i < MAX_CHALLENGES; i++)
	{
		if (NET_CompareBaseAdr(*adr, g_rg_sv_challenges[i].adr))
		{
			// добавлено изменение времени вопроса.
			// --barspinoff
			g_rg_sv_challenges[i].time = realtime;
			break;
		}

		if (g_rg_sv_challenges[i].time < oldestTime)
		{
			oldestTime = g_rg_sv_challenges[i].time;
			oldest = i;
		}
	}
	if (i == MAX_CHALLENGES)
	{
		g_rg_sv_challenges[oldest].challenge = RandomLong(0, 65535) | (RandomLong(0, 36863) << 16);
		g_rg_sv_challenges[oldest].adr = net_from;
		g_rg_sv_challenges[oldest].time = realtime;
		i = oldest;
	}
	return g_rg_sv_challenges[i].challenge;
}

qboolean CheckChallengeNr(netadr_t *adr, int nChallengeValue)
{
	int i;
	if (nChallengeValue == -1 || adr == NULL)
		return FALSE;

	for (i = 0; i < MAX_CHALLENGES; i++)
	{
		if (NET_CompareBaseAdr(*adr, g_rg_sv_challenges[i].adr))
		{
			if (g_rg_sv_challenges[i].challenge == nChallengeValue)
			{
				if (g_rg_sv_challenges[i].time + 3600.0f >= realtime)
					return TRUE;
			}
			return FALSE;
		}
	}
	return FALSE;
}

void ReplyServerChallenge(netadr_t *adr)
{
	char buffer[16];
	sizebuf_t buf;

	buf.buffername = "SVC_Challenge";
	buf.data = (byte *)buffer;
	buf.maxsize = sizeof(buffer);
	buf.cursize = 0;
	buf.flags = FSB::FSB_ALLOWOVERFLOW;

	MSG_WriteLong(&buf, 0xffffffff);
	MSG_WriteByte(&buf, S2C_CHALLENGE);
	MSG_WriteLong(&buf, GetChallengeNr(adr));
	NET_SendPacket(NS_SERVER, buf.cursize, (char *)buf.data, *adr);
}

qboolean ValidChallenge(netadr_t *adr, int challengeNr)
{
	if (!sv.active)
		return FALSE;

	if (svs.maxclients <= 1)
		return FALSE;

	if (!RequireValidChallenge(adr) || CheckChallengeNr(adr, challengeNr))
		return TRUE;

	ReplyServerChallenge(adr);
	return FALSE;
}

void SVC_InfoString(void)
{
	int i;
	int count = 0;
	int proxy = 0;
	sizebuf_t buf;
	unsigned char data[MAX_ROUTEABLE_PACKET];
	char address[256];
	char gd[260];
	char info[2048];
	int iHasPW = 0;
	char szOS[2];

	if (noip && noipx)
		return;

	if (!sv.active)
		return;

	buf.buffername = "SVC_InfoString";
	buf.data = data;
	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.flags = FSB_ALLOWOVERFLOW;

	for (i = 0; i < svs.maxclients; i++)
	{
		client_t *client = &svs.clients[i];
		if (client->active || client->spawned || client->connected)
		{
			if (client->proxy)
				proxy++;
			else
				count++;
		}
	}

	address[0] = 0;

	if (noip && !noipx)
		Q_strncpy(address, NET_AdrToString(net_local_ipx_adr), 255);
	else
		Q_strncpy(address, NET_AdrToString(net_local_adr), 255);

	Q_strcpy(szOS, "w");

	address[255] = 0;

	info[0] = 0;

	if (*sv_password.string)
		iHasPW = Q_strcasecmp(sv_password.string, "none") != 0;

	Info_SetValueForKey(info, "protocol", va(const_cast<char*>("%i"), PROTOCOL_VERSION), sizeof(info));
	Info_SetValueForKey(info, "address", address, sizeof(info));
	Info_SetValueForKey(info, "players", va(const_cast<char*>("%i"), count), sizeof(info));
	Info_SetValueForKey(info, "proxytarget", va(const_cast<char*>("%i"), proxy), sizeof(info));
	Info_SetValueForKey(info, "lan", va(const_cast<char*>("%i"), Master_IsLanGame() != FALSE), sizeof(info));

	int maxplayers = (int)sv_visiblemaxplayers.value;
	if (maxplayers < 0)
		maxplayers = svs.maxclients;

	Info_SetValueForKey(info, "max", va(const_cast<char*>("%i"), maxplayers), sizeof(info));
	Info_SetValueForKey(info, "bots", va(const_cast<char*>("%i"), SV_GetFakeClientCount()), sizeof(info));

	COM_FileBase(com_gamedir, gd);
	Info_SetValueForKey(info, "gamedir", gd, sizeof(info));

	Info_SetValueForKey(info, "description", gEntityInterface.pfnGetGameDescription(), ARRAYSIZE(info));
	Info_SetValueForKey(info, "hostname", Cvar_VariableString(const_cast<char*>("hostname")), ARRAYSIZE(info));
	Info_SetValueForKey(info, "map", sv.name, ARRAYSIZE(info));

	const char *type;
	if (cls.state)
		type = "l";
	else
		type = "d";
	Info_SetValueForKey(info, "type", type, sizeof(info));
	Info_SetValueForKey(info, "password", va(const_cast<char*>("%i"), iHasPW), sizeof(info));
	Info_SetValueForKey(info, "os", szOS, sizeof(info));
	Info_SetValueForKey(info, "secure", Steam_GSBSecure() ? "0" : "1", sizeof(info));

	if (gmodinfo.bIsMod)
	{
		Info_SetValueForKey(info, "mod", va(const_cast<char*>("%i"), 1), sizeof(info));
		Info_SetValueForKey(info, "modversion", va(const_cast<char*>("%i"), gmodinfo.version), sizeof(info));
		Info_SetValueForKey(info, "svonly", va(const_cast<char*>("%i"), gmodinfo.svonly), sizeof(info));
		Info_SetValueForKey(info, "cldll", va(const_cast<char*>("%i"), gmodinfo.cldll), sizeof(info));
	}

	MSG_WriteLong(&buf, 0xffffffff);
	MSG_WriteString(&buf, "infostringresponse");
	MSG_WriteString(&buf, info);
	NET_SendPacket(NS_SERVER, buf.cursize, (char *)buf.data, net_from);
}

void SVC_Info(qboolean bDetailed)
{
	int i;
	int count = 0;
	sizebuf_t buf;
	unsigned char data[MAX_ROUTEABLE_PACKET];
	char szModURL_Info[512];
	char szModURL_DL[512];
	int mod_version;
	int mod_size;
	char gd[260];
	qboolean cldll;
	qboolean svonly;
	char szHLVersion[32];

	if (!sv.active)
		return;

	buf.buffername = "SVC_Info";
	buf.data = data;
	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.flags = FSB_ALLOWOVERFLOW;

	for (i = 0; i < svs.maxclients; i++)
	{
		client_t *client = &svs.clients[i];
		if (client->active || client->spawned || client->connected)
			count++;
	}

	MSG_WriteLong(&buf, 0xffffffff);
	MSG_WriteByte(&buf, bDetailed ? S2A_INFO_DETAILED : S2A_INFO);

	if (noip)
	{
		if (!noipx)
			MSG_WriteString(&buf, NET_AdrToString(net_local_ipx_adr));
		else
			MSG_WriteString(&buf, "LOOPBACK");
	}
	else
		MSG_WriteString(&buf, NET_AdrToString(net_local_adr));

	MSG_WriteString(&buf, Cvar_VariableString(const_cast<char*>("hostname")));
	MSG_WriteString(&buf, sv.name);
	COM_FileBase(com_gamedir, gd);
	MSG_WriteString(&buf, gd);

	MSG_WriteString(&buf, gEntityInterface.pfnGetGameDescription());

	MSG_WriteByte(&buf, count);
	int maxplayers = (int)sv_visiblemaxplayers.value;
	if (maxplayers < 0)
		maxplayers = svs.maxclients;

	MSG_WriteByte(&buf, maxplayers);
	MSG_WriteByte(&buf, PROTOCOL_VERSION);

	if (bDetailed)
	{
		MSG_WriteByte(&buf, cls.state != ca_dedicated ? A2A_PRINT : M2A_SERVERS);

		MSG_WriteByte(&buf, M2A_MASTERSERVERS);

		if (Q_strlen(sv_password.string) > 0 && Q_strcasecmp(sv_password.string, "none"))
			MSG_WriteByte(&buf, 1);
		else
			MSG_WriteByte(&buf, 0);

		if (SV_GetModInfo(szModURL_Info, szModURL_DL, &mod_version, &mod_size, &svonly, &cldll, szHLVersion))
		{
			MSG_WriteByte(&buf, 1);
			MSG_WriteString(&buf, szModURL_Info);
			MSG_WriteString(&buf, szModURL_DL);
			MSG_WriteString(&buf, "");
			MSG_WriteLong(&buf, mod_version);
			MSG_WriteLong(&buf, mod_size);
			MSG_WriteByte(&buf, svonly != FALSE);
			MSG_WriteByte(&buf, cldll != FALSE);
		}
		else
			MSG_WriteByte(&buf, 0);

		MSG_WriteByte(&buf, Steam_GSBSecure() != FALSE);
		MSG_WriteByte(&buf, SV_GetFakeClientCount());
	}
	NET_SendPacket(NS_SERVER, buf.cursize, (char *)buf.data, net_from);
}

void SVC_PlayerInfo(void)
{
	int i;
	int count = 0;
	client_t *client;
	sizebuf_t buf;
	unsigned char data[2048];

	if (!sv.active)
		return;

	if (svs.maxclients <= 1)
		return;

	buf.buffername = "SVC_PlayerInfo";
	buf.data = data;
	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.flags = FSB_ALLOWOVERFLOW;

	MSG_WriteLong(&buf, 0xffffffff);
	MSG_WriteByte(&buf, S2A_PLAYERS);

	for (i = 0; i < svs.maxclients; i++)
	{
		client = &svs.clients[i];
		if (client->active)
			count++;
	}

	MSG_WriteByte(&buf, count);

	count = 0;
	for (i = 0; i < svs.maxclients; i++)
	{
		client = &svs.clients[i];

		if (client->active)
		{
			MSG_WriteByte(&buf, ++count);
			MSG_WriteString(&buf, client->name);
			MSG_WriteLong(&buf, client->edict->v.frags);
			MSG_WriteFloat(&buf, (float)(realtime - client->netchan.connect_time));
		}
	}
	NET_SendPacket(NS_SERVER, buf.cursize, (char *)buf.data, net_from);
}

void SVC_RuleInfo(void)
{
	int nNumRules;
	cvar_t *var;
	sizebuf_t buf;
	unsigned char data[8192];

	if (!sv.active)
		return;

	if (svs.maxclients <= 1)
		return;

	buf.buffername = "SVC_RuleInfo";
	buf.data = data;
	buf.maxsize = sizeof(data);
	buf.cursize = 0;
	buf.flags = FSB_ALLOWOVERFLOW;

	nNumRules = Cvar_CountServerVariables();
	if (!nNumRules)
		return;

	MSG_WriteLong(&buf, 0xffffffff);
	MSG_WriteByte(&buf, S2A_RULES);
	MSG_WriteShort(&buf, nNumRules);

	var = cvar_vars;
	while (var != NULL)
	{
		if (var->flags & FCVAR_SERVER)
		{
			MSG_WriteString(&buf, var->name);
			if (var->flags & FCVAR_PROTECTED)
			{
				if (Q_strlen(var->string) > 0 && Q_strcasecmp(var->string, "none"))
					MSG_WriteString(&buf, "1");
				else
					MSG_WriteString(&buf, "0");
			}
			else
				MSG_WriteString(&buf, var->string);
		}
		var = var->next;
	}
	NET_SendPacket(NS_SERVER, buf.cursize, (char *)buf.data, net_from);
}

int SVC_GameDllQuery(const char *s)
{
	int len;
	// изменён размер стековой переменной со стандартных
	// 2048 байт на 4096. возвращать?
	// --barspinoff
	unsigned char data[4096];
	int valid;

	if (!sv.active || svs.maxclients <= 1)
		return 0;

	Q_memset(data, 0, sizeof(data));
	// откуда 2044 появилось я не ебу, пусть так и остаётся
	//len = 2044 - sizeof(data);
	// формула = размер массива - 4
	// 4 байта в данном случае - заголовок
	// --barspinoff.
	len = sizeof(data) - 4;
	valid = gEntityInterface.pfnConnectionlessPacket(&net_from, s, (char *)&data[4], &len);
	if (len && len <= sizeof(data) - 4)
	{
		*(uint32 *)data = 0xFFFFFFFF; //connectionless packet
		NET_SendPacket(NS_SERVER, len + 4, data, net_from);
	}
	return valid;
}

void SV_FlushRedirect(void)
{
	unsigned char *data;
	sizebuf_t buf;

	if (sv_redirected == RD_PACKET)
	{
		int allocLen = Q_strlen(outputbuf) + 10;
		allocLen &= ~3;
		data = (unsigned char *)alloca(allocLen);

		buf.buffername = "Redirected Text";
		buf.data = data;
		buf.maxsize = Q_strlen(outputbuf) + 7;
		buf.cursize = 0;
		buf.flags = FSB_ALLOWOVERFLOW;

		MSG_WriteLong(&buf, -1);
		MSG_WriteByte(&buf, A2A_PRINT);
		MSG_WriteString(&buf, outputbuf);
		MSG_WriteByte(&buf, 0);
		NET_SendPacket(NS_SERVER, buf.cursize, buf.data, sv_redirectto);
		outputbuf[0] = 0;
	}
	else
	{
		if (sv_redirected == RD_CLIENT)
		{
			MSG_WriteByte(&host_client->netchan.message, svc_print);
			MSG_WriteString(&host_client->netchan.message, outputbuf);
		}
		outputbuf[0] = 0;
	}
}

void SV_EndRedirect(void)
{
	SV_FlushRedirect();
	sv_redirected = (redirect_t)RD_NONE;
}

void SV_BeginRedirect(redirect_t rd, netadr_t *addr)
{
	Q_memcpy(&sv_redirectto, addr, sizeof(sv_redirectto));
	sv_redirected = rd;
	outputbuf[0] = 0;
}

void SV_ResetRcon_f(void)
{
	Q_memset(g_rgRconFailures, 0, sizeof(g_rgRconFailures));
}

void SV_AddFailedRcon(netadr_t *adr)
{
	int i;
	int best = 0;
	float best_update = -99999.0f;
	float time;
	qboolean found = FALSE;
	rcon_failure_t *r;
	int failed;

	if (sv_rcon_minfailures.value < 1.0f)
	{
		Cvar_SetValue(const_cast<char*>("sv_rcon_minfailures"), 1.0);
	}
	else if (sv_rcon_minfailures.value > 20.0f)
	{
		Cvar_SetValue(const_cast<char*>("sv_rcon_minfailures"), 20.0);
	}
	if (sv_rcon_maxfailures.value < 1.0f)
	{
		Cvar_SetValue(const_cast<char*>("sv_rcon_maxfailures"), 1.0);
	}
	else if (sv_rcon_maxfailures.value > 20.0f)
	{
		Cvar_SetValue(const_cast<char*>("sv_rcon_maxfailures"), 20.0);
	}
	if (sv_rcon_maxfailures.value < sv_rcon_minfailures.value)
	{
		float temp = sv_rcon_maxfailures.value;
		Cvar_SetValue(const_cast<char*>("sv_rcon_maxfailures"), sv_rcon_minfailures.value);
		Cvar_SetValue(const_cast<char*>("sv_rcon_minfailures"), temp);
	}
	if (sv_rcon_minfailuretime.value < 1.0f)
	{
		Cvar_SetValue(const_cast<char*>("sv_rcon_minfailuretime"), 1.0);
	}

	for (i = 0; i < MAX_RCON_FAILURES_STORAGE; i++)
	{
		r = &g_rgRconFailures[i];
		if (!r->active)
		{
			break;
		}
		if (NET_CompareAdr(r->adr, *adr))
		{
			found = TRUE;
			break;
		}
		time = (float)(realtime - r->last_update);
		if (time >= best_update)
		{
			best = i;
			best_update = time;
		}
	}

	// If no match found, take the oldest entry for usage
	if (i >= MAX_RCON_FAILURES_STORAGE)
	{
		r = &g_rgRconFailures[best];
	}

	// Prepare new or stale entry
	if (!found)
	{
		r->shouldreject = FALSE;
		r->num_failures = 0;
		Q_memcpy(&r->adr, adr, sizeof(netadr_t));
	}
	else if (r->shouldreject)
	{
		return;
	}

	r->active = TRUE;
	r->last_update = (float)realtime;

	if (r->num_failures >= sv_rcon_maxfailures.value)
	{
		// чтение за пределами массива.
		// --barspinoff
		for (i = 0; i < sv_rcon_maxfailures.value - 1; i++)
		{
			r->failure_times[i] = r->failure_times[i + 1];
		}
		r->num_failures--;
	}

	r->failure_times[r->num_failures] = (float)realtime;
	r->num_failures++;

	failed = 0;
	for (i = 0; i < r->num_failures; i++)
	{
		if (realtime - r->failure_times[i] <= sv_rcon_minfailuretime.value)
			failed++;
	}

	if (failed >= sv_rcon_minfailures.value)
	{
		Con_Printf(const_cast<char*>("User %s will be banned for rcon hacking\n"), NET_AdrToString(*adr));
		r->shouldreject = TRUE;
	}
}

qboolean SV_CheckRconFailure(netadr_t *adr)
{
	for (int i = 0; i < MAX_RCON_FAILURES_STORAGE; i++)
	{
		rcon_failure_t *r = &g_rgRconFailures[i];
		if (NET_CompareAdr(*adr, r->adr))
		{
			if (r->shouldreject)
				return TRUE;
		}
	}

	return FALSE;
}

int SV_Rcon_Validate(void)
{
	if (Cmd_Argc() < 3 || Q_strlen(rcon_password.string) == 0)
		return 1;

	if (sv_rcon_banpenalty.value < 0.0f)
		Cvar_SetValue(const_cast<char*>("sv_rcon_banpenalty"), 0.0);

	if (SV_CheckRconFailure(&net_from))
	{
		Con_Printf(const_cast<char*>("Banning %s for rcon hacking attempts\n"), NET_AdrToString(net_from));
		Cbuf_AddText(va(const_cast<char*>("addip %i %s\n"), (int)sv_rcon_banpenalty.value, NET_BaseAdrToString(net_from)));
		return 3;
	}

	if (!SV_CheckChallenge(&net_from, Q_atoi(Cmd_Argv(1))))
		return 2;

	if (Q_strcmp(Cmd_Argv(2), rcon_password.string))
	{
		SV_AddFailedRcon(&net_from);
		return 1;
	}
	return 0;
}

void SV_Rcon(netadr_t *net_from_)
{
	char remaining[512];
	char rcon_buff[1024];

	int invalid = SV_Rcon_Validate();
	int len = net_message.cursize - Q_strlen("rcon");
	if (len <= 0 || len >= sizeof(remaining))
		return;

	Q_memcpy(remaining, &net_message.data[Q_strlen("rcon")], len);
	remaining[len] = 0;

	
	if (invalid)
	{
		Con_Printf(const_cast<char*>("Bad Rcon from %s:\n%s\n"), NET_AdrToString(*net_from_), remaining);
		Log_Printf("Bad Rcon: \"%s\" from \"%s\"\n", remaining, NET_AdrToString(*net_from_));
	}
	else
	{
		Con_Printf(const_cast<char*>("Rcon from %s:\n%s\n"), NET_AdrToString(*net_from_), remaining);
		Log_Printf("Rcon: \"%s\" from \"%s\"\n", remaining, NET_AdrToString(*net_from_));
	}
	

	SV_BeginRedirect((redirect_t)RD_PACKET, net_from_);

	if (invalid)
	{
		if (invalid == 2)
			Con_Printf(const_cast<char*>("Bad rcon_password.\n"));
		else if (Q_strlen(rcon_password.string) == 0)
			Con_Printf(const_cast<char*>("Bad rcon_password.\nNo password set for this server.\n"));
		else
			Con_Printf(const_cast<char*>("Bad rcon_password.\n"));

		SV_EndRedirect();
		return;
	}
	char *data = COM_Parse(COM_Parse(COM_Parse(remaining)));
	if (!data)
	{
		Con_Printf(const_cast<char*>("Empty rcon\n"));
		return;
	}

	Q_strncpy(rcon_buff, data, sizeof(rcon_buff)-1);
	rcon_buff[sizeof(rcon_buff)-1] = 0;
	Cmd_ExecuteString(rcon_buff, src_command);
	SV_EndRedirect();
}

void SV_ConnectionlessPacket(void)
{
	char *args;
	const char *c;

	MSG_BeginReading();
	MSG_ReadLong();
	args = MSG_ReadStringLine();
	Cmd_TokenizeString(args);
	c = Cmd_Argv(0);

	if (!Q_strcmp(c, "ping") || (c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n')))
	{
		SVC_Ping();
	}
	else if (c[0] == A2A_ACK && (c[1] == 0 || c[1] == '\n'))
	{
		Con_Printf(const_cast<char*>("A2A_ACK from %s\n"), NET_AdrToString(net_from));
	}
	else if (c[0] == A2A_GETCHALLENGE || c[0] == A2S_INFO || c[0] == A2S_PLAYER || c[0] == A2S_RULES ||
		c[0] == S2A_LOGSTRING || c[0] == M2S_REQUESTRESTART || c[0] == M2A_CHALLENGE)
		return;

	else if (!Q_strcasecmp(c, "log"))
	{
		if (sv_logrelay.value != 0.0f && Q_strlen(args) > 4)
		{
			const char *s = &args[Q_strlen("log ")];
			if (s && s[0])
			{
				Con_Printf(const_cast<char*>("%s\n"), s);
			}
		}
	}
	else if (!Q_strcmp(c, "getchallenge"))
	{
		SVC_GetChallenge();
	}
	else if (!Q_strcasecmp(c, "challenge"))
	{
		SVC_ServiceChallenge();
	}
	else if (!Q_strcmp(c, "connect"))
	{
		SV_ConnectClient();
	}
	else if (!Q_strcmp(c, "pstat"))
	{
		if (g_modfuncs.m_pfnPlayerStatus)
			g_modfuncs.m_pfnPlayerStatus(net_message.data, net_message.cursize);
	}
	else if (!Q_strcmp(c, "rcon"))
	{
		SV_Rcon(&net_from);
	}
	else
		SVC_GameDllQuery(args);
}

void SV_ProcessFile(client_t *cl, char *filename)
{
	customization_t *pList;
	resource_t *resource;
	unsigned char md5[16];
	qboolean bFound;

	if (filename[0] != '!')
	{
		Con_Printf(const_cast<char*>("Ignoring non-customization file upload of %s\n"), filename);
		return;
	}

	COM_HexConvert(filename + 4, 32, md5);
	resource = cl->resourcesneeded.pNext;
	bFound = FALSE;
	while (resource != &cl->resourcesneeded)
	{
		if (!Q_memcmp(resource->rgucMD5_hash, md5, 16))
		{
			bFound = TRUE;
			break;
		}

		resource = resource->pNext;
	}

	if (!bFound)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ":  Unrequested decal\n"));
		return;
	}

	if (resource->nDownloadSize != cl->netchan.tempbuffersize)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ":  Downloaded %i bytes for purported %i byte file\n"), cl->netchan.tempbuffersize, resource->nDownloadSize);
		return;
	}

	int iCustomFlags = 0;

	HPAK_AddLump(TRUE, const_cast<char*>("custom.hpk"), resource, cl->netchan.tempbuffer, NULL);
	resource->ucFlags &= ~RES_WASMISSING;
	SV_MoveToOnHandList(resource);
	pList = cl->customdata.pNext;
	while (pList) {
		if (!Q_memcmp(pList->resource.rgucMD5_hash, resource->rgucMD5_hash, 16))
		{
			Con_DPrintf(const_cast<char*>("Duplicate resource received and ignored.\n"));
			return;
		}
		pList = pList->pNext;
	}

	iCustomFlags |= (FCUST_FROMHPAK | FCUST_WIPEDATA | RES_CUSTOM);
	if (!COM_CreateCustomization(&cl->customdata, resource, -1, iCustomFlags, NULL, NULL))
		Con_Printf(const_cast<char*>("Error parsing custom decal from %s\n"), cl->name);
}

qboolean SV_FilterPacket(void)
{
	for (int i = numipfilters - 1; i >= 0; i--)
	{
		ipfilter_t* curFilter = &ipfilters[i];
		if (curFilter->compare.u32 == 0xFFFFFFFF || curFilter->banEndTime == 0.0f || curFilter->banEndTime > realtime)
		{
			if ((*(uint32*)net_from.ip & curFilter->mask) == curFilter->compare.u32)
				return (int)sv_filterban.value;
		}
		else
		{
			if (i < numipfilters - 1)
				memmove(curFilter, &curFilter[1], sizeof(ipfilter_t) * (numipfilters - i - 1));

			--numipfilters;
		}
	}
	return sv_filterban.value == 0.0f;
}

void SV_SendBan(void)
{
	char szMessage[64];
	_snprintf(szMessage, sizeof(szMessage), "You have been banned from this server.\n");

	SZ_Clear(&net_message);

	MSG_WriteLong(&net_message, -1);
	MSG_WriteByte(&net_message, A2A_PRINT);
	MSG_WriteString(&net_message, szMessage);
	NET_SendPacket(NS_SERVER, net_message.cursize, net_message.data, net_from);

	SZ_Clear(&net_message);
}

void SV_ReadPackets(void)
{
	while (NET_GetPacket(NS_SERVER))
	{
		if (SV_FilterPacket())
		{
			SV_SendBan();
			continue;
		}

		if (*(uint32 *)net_message.data == 0xFFFFFFFF)
		{
			// Connectionless packet
			if (CheckIP(net_from))
			{
				Steam_HandleIncomingPacket(net_message.data, net_message.cursize, ntohl(*(u_long *)&net_from.ip[0]), htons(net_from.port));
				SV_ConnectionlessPacket();
			}
			else if (sv_logblocks.value != 0.0)
			{
				Log_Printf("Traffic from %s was blocked for exceeding rate limits\n", NET_AdrToString(net_from));
			}

			continue;
		}

		for (int i = 0; i < svs.maxclients; i++)
		{
			client_t *cl = &svs.clients[i];
			if (!cl->connected && !cl->active && !cl->spawned)
			{
				continue;
			}

			if (NET_CompareAdr(net_from, cl->netchan.remote_address) != TRUE)
			{
				continue;
			}

			if (Netchan_Process(&cl->netchan))
			{
				if (svs.maxclients == 1 || !cl->active || !cl->spawned || !cl->fully_connected)
				{
					cl->send_message = TRUE;
				}

				SV_ExecuteClientMessage(cl);
				gGlobalVariables.frametime = host_frametime;
			}

			if (Netchan_IncomingReady(&cl->netchan))
			{
				if (Netchan_CopyNormalFragments(&cl->netchan))
				{
					MSG_BeginReading();
					SV_ExecuteClientMessage(cl);
				}
				if (Netchan_CopyFileFragments(&cl->netchan))
				{
					host_client = cl;
					SV_ProcessFile(cl, cl->netchan.incomingfilename);
				}
			}
		}
	}
}

void SV_CheckTimeouts(void)
{
	int i;
	client_t *cl;
	float droptime;

	droptime = realtime - sv_timeout.value;

	for (i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++)
	{
		if (cl->fakeclient)
			continue;
		if (!cl->connected && !cl->active && !cl->spawned)
			continue;
		if (cl->netchan.last_received < droptime)
		{
			SV_BroadcastPrintf("%s timed out\n", cl->name);
			SV_DropClient(cl, FALSE, const_cast<char*>("Timed out"));
		}
	}
}

int SV_CalcPing(client_t *cl)
{
	float ping;
	int i;
	int count;
	int back;
	client_frame_t *frame;
	int idx;

	if (cl->fakeclient)
	{
		return 0;
	}

	if (SV_UPDATE_BACKUP <= 31)
	{
		back = SV_UPDATE_BACKUP / 2;
		if (back <= 0)
		{
			return 0;
		}
	}
	else
	{
		back = 16;
	}

	ping = 0.0f;
	count = 0;
	for (i = 0; i < back; i++)
	{
		idx = cl->netchan.incoming_acknowledged + ~i;
		frame = &cl->frames[SV_UPDATE_MASK & idx];

		if (frame->ping_time > 0.0f)
		{
			ping += frame->ping_time;
			count++;
		}
	}

	if (count)
	{
		ping /= count;
		if (ping > 0.0f)
		{
			return ping * 1000.0f;
		}
	}
	return 0;
}

void SV_EmitEvents(client_t *cl, packet_entities_t *pack, sizebuf_t *msg)
{
	int i;
	int ev;
	event_state_t *es;
	event_info_t *info;
	entity_state_t *state;
	int ev_count = 0;
	int etofind;
	int c;
	event_args_t nullargs;

	Q_memset(&nullargs, 0, sizeof(event_args_t));

	es = &cl->events;

	for (ev = 0; ev < MAX_EVENT_QUEUE; ev++)
	{
		info = &es->ei[ev];

		if (info->index != 0)
		{
			ev_count++;
		}
	}

	if (ev_count == 0)
	{
		return;
	}

	if (ev_count > 31)
	{
		ev_count = 31;
	}

	for (ev = 0; ev < MAX_EVENT_QUEUE; ev++)
	{
		info = &es->ei[ev];

		if (info->index == 0)
		{
			continue;
		}

		etofind = info->entity_index;

		for (i = 0; i < pack->num_entities; i++)
		{
			state = &pack->entities[i];

			if (state->number == etofind)
			{
				break;
			}
		}

		if (i < pack->num_entities)
		{
			info->packet_index = i;
			info->args.ducking = 0;

			if (!(info->args.flags & FEVENT_ORIGIN))
			{
				info->args.origin[0] = 0.0f;
				info->args.origin[1] = 0.0f;
				info->args.origin[2] = 0.0f;
			}
			if (!(info->args.flags & FEVENT_ANGLES))
			{
				info->args.angles[0] = 0.0f;
				info->args.angles[1] = 0.0f;
				info->args.angles[2] = 0.0f;
			}
		}
		else
		{
			info->args.entindex = etofind;
			info->packet_index = pack->num_entities;
		}
	}

	MSG_WriteByte(msg, svc_event);
	MSG_StartBitWriting(msg);
	MSG_WriteBits(ev_count, 5);

	c = 0;

	for (ev = 0; ev < MAX_EVENT_QUEUE; ev++)
	{
		info = &es->ei[ev];

		if (info->index == 0)
		{
			info->packet_index = -1;
			info->entity_index = -1;
			continue;
		}

		if (c < ev_count)
		{
			MSG_WriteBits(info->index, 10);

			if (info->packet_index != -1)
			{
				MSG_WriteBits(1, 1);
				MSG_WriteBits(info->packet_index, 11);
				if (Q_memcmp(&nullargs, &info->args, sizeof(event_args_t)))
				{
					MSG_WriteBits(1, 1);
					DELTA_WriteDelta((byte *)&nullargs, (byte *)&info->args, TRUE, g_peventdelta, NULL);
				}
				else
				{
					MSG_WriteBits(0, 1);
				}
			}
			else
			{
				MSG_WriteBits(0, 1);
			}

			if (info->fire_time == 0.0f)
			{
				MSG_WriteBits(0, 1);
			}
			else
			{
				MSG_WriteBits(1, 1);
				MSG_WriteBits(info->fire_time * 100.0f, 16);
			}
		}

		info->index = 0;
		info->packet_index = -1;
		info->entity_index = -1;

		c++;
	}
	MSG_EndBitWriting(msg);
}

void SV_AddToFatPVS(vec_t *org, mnode_t *node)
{
	while (node->contents >= 0)
	{
		mplane_t *plane = node->plane;
		float d = plane->normal[2] * org[2] + plane->normal[1] * org[1] + plane->normal[0] * org[0] - plane->dist;
		if (d <= 8.0f)
		{
			if (d >= -8.0f)
			{
				SV_AddToFatPVS(org, node->children[0]);
				node = node->children[1];
			}
			else
			{
				node = node->children[1];
			}
		}
		else
		{
			node = node->children[0];
		}
	}
	if (node->contents != CONTENTS_SOLID)
	{
		unsigned char *pvs = Mod_LeafPVS((mleaf_t *)node, sv.worldmodel);
		for (int i = 0; i < fatbytes; i++)
			fatpvs[i] |= pvs[i];
	}
}

unsigned char* SV_FatPVS(float *org)
{
	fatbytes = (sv.worldmodel->numleafs + 31) >> 3;
	Q_memset(fatpvs, 0, fatbytes);
	SV_AddToFatPVS(org, sv.worldmodel->nodes);
	return fatpvs;
}

void SV_AddToFatPAS(vec_t *org, mnode_t *node)
{
	int i;
	unsigned char *pas;
	mplane_t *plane;
	float d;

	while (node->contents >= 0)
	{
		plane = node->plane;
		d = org[0] * plane->normal[0] +
			org[1] * plane->normal[1] +
			org[2] * plane->normal[2] - plane->dist;

		if (d > 8.0f)
		{
			node = node->children[0];
		}
		else if (d < -8.0f)
		{
			node = node->children[1];
		}
		else
		{
			SV_AddToFatPAS(org, node->children[0]);
			node = node->children[1];
		}
	}

	if (node->contents == CONTENTS_SOLID)
	{
		return;
	}

	int leafnum = (mleaf_t *)node - sv.worldmodel->leafs;
	pas = CM_LeafPAS(leafnum);

	for (i = 0; i < fatpasbytes; ++i)
	{
		fatpas[i] |= pas[i];
	}
}

unsigned char* SV_FatPAS(float *org)
{
	fatpasbytes = (sv.worldmodel->numleafs + 31) >> 3;
	Q_memset(fatpas, 0, fatpasbytes);
	SV_AddToFatPAS(org, sv.worldmodel->nodes);
	return fatpas;
}

void TRACE_DELTA(char *fmt, ...)
{
}

void SV_SetCallback(int num, qboolean remove, qboolean custom, int *numbase, qboolean full, int offset)
{
	g_svdeltacallback.num = num;
	g_svdeltacallback.remove = remove;
	g_svdeltacallback.custom = custom;
	g_svdeltacallback.numbase = numbase;
	g_svdeltacallback.full = full;
	g_svdeltacallback.newbl = FALSE;
	g_svdeltacallback.newblindex = 0;
	g_svdeltacallback.offset = offset;
}

void SV_SetNewInfo(int newblindex)
{
	g_svdeltacallback.newbl = TRUE;
	g_svdeltacallback.newblindex = newblindex;
}

void SV_WriteDeltaHeader(int num, qboolean remove, qboolean custom, int *numbase, qboolean newbl, int newblindex, qboolean full, int offset)
{
	int delta;

	delta = num - *numbase;
	extern void DbgPrint(FILE*, const char* format, ...);
	extern FILE* m_fMessages;
	DbgPrint(m_fMessages, "writing delta header... <%s#%d>\r\n", __FILE__, __LINE__);
	if (full)
	{
		DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, delta == 1, __FILE__, __LINE__);
		if (delta == 1)
		{
			MSG_WriteBits(1, 1);
		}
		else
		{
			MSG_WriteBits(0, 1);
		}
	}
	else
	{
		DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, (remove != 0) ? 1 : 0, __FILE__, __LINE__);
		MSG_WriteBits((remove != 0) ? 1 : 0, 1);
	}

	if (!full || delta != 1)
	{
		if (delta <= 0 || delta > 63)
		{
			DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, 1, __FILE__, __LINE__);
			MSG_WriteBits(1, 1);
			DbgPrint(m_fMessages, "wrote %d bits -> %d - numbase <%s#%d>\r\n", 11, num, __FILE__, __LINE__);
			MSG_WriteBits(num, 11);
		}
		else
		{
			DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, 0, __FILE__, __LINE__);
			MSG_WriteBits(0, 1);
			DbgPrint(m_fMessages, "wrote %d bits -> %d - numbase = %d <%s#%d>\r\n", 6, delta, delta + *numbase, __FILE__, __LINE__);
			MSG_WriteBits(delta, 6);
		}
	}

	*numbase = num;
	if (!remove)
	{
		DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, custom != 0, __FILE__, __LINE__);
		MSG_WriteBits(custom != 0, 1);
		if (sv.instance_baselines->number)
		{
			if (newbl)
			{
				DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, 1, __FILE__, __LINE__);
				MSG_WriteBits(1, 1);
				DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 6, newblindex, __FILE__, __LINE__);
				MSG_WriteBits(newblindex, 6);
			}
			else
			{
				DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, 0, __FILE__, __LINE__);
				MSG_WriteBits(0, 1);
			}
		}
		if (full && !newbl)
		{
			if (offset)
			{
				DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, 1, __FILE__, __LINE__);
				MSG_WriteBits(1, 1);
				DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 6, offset, __FILE__, __LINE__);
				MSG_WriteBits(offset, 6);
			}
			else
			{
				DbgPrint(m_fMessages, "wrote %d bits -> %d... <%s#%d>\r\n", 1, 0, __FILE__, __LINE__);
				MSG_WriteBits(0, 1);
			}
		}
	}
}

void SV_InvokeCallback(void)
{
	SV_WriteDeltaHeader(
		g_svdeltacallback.num,
		g_svdeltacallback.remove,
		g_svdeltacallback.custom,
		g_svdeltacallback.numbase,
		g_svdeltacallback.newbl,
		g_svdeltacallback.newblindex,
		g_svdeltacallback.full,
		g_svdeltacallback.offset
		);
}

int SV_FindBestBaseline(int index, entity_state_t ** baseline, entity_state_t *to, int num, qboolean custom)
{
	int bestbitnumber;
	delta_t* delta;

	if (custom)
	{
		delta = g_pcustomentitydelta;
	}
	else
	{
		if (SV_IsPlayerIndex(num))
			delta = g_pplayerdelta;
		else
			delta = g_pentitydelta;
	}

	bestbitnumber = DELTA_TestDelta((byte *)*baseline, (byte *)&to[index], delta);
	bestbitnumber -= 6;

	int i = 0;
	int bitnumber = 0;
	int bestfound = index;

	for (i = index - 1; bestbitnumber > 0 && i >= 0 && (index - i) <= 64; i--)
	{
		if (to[index].entityType == to[i].entityType)
		{
			bitnumber = DELTA_TestDelta((byte *)&to[i], (byte *)&to[index], delta);

			if (bitnumber < bestbitnumber)
			{
				bestbitnumber = bitnumber;
				bestfound = i;
			}
		}
	}

	if (index != bestfound)
		*baseline = &to[bestfound];

	return index - bestfound;
}

int SV_CreatePacketEntities(sv_delta_t type, client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	packet_entities_t *from;
	int oldindex;
	int newindex;
	int oldnum;
	int newnum;
	int oldmax;
	int numbase;

	numbase = 0;
	int idbg = msg->cursize;
	if (type == sv_packet_delta)
	{
		client_frame_t *fromframe = &client->frames[SV_UPDATE_MASK & client->delta_sequence];
		from = &fromframe->entities;
		oldmax = from->num_entities;
		MSG_WriteByte(msg, svc_deltapacketentities);
		MSG_WriteShort(msg, to->num_entities);
		MSG_WriteByte(msg, client->delta_sequence);
	}
	else
	{
		oldmax = 0;
		from = NULL;
		MSG_WriteByte(msg, svc_packetentities);
		MSG_WriteShort(msg, to->num_entities);
	}

	newnum = 0; //index in to->entities
	oldnum = 0; //index in from->entities
	MSG_StartBitWriting(msg);
	while (1)
	{
		if (newnum < to->num_entities)
		{
			newindex = to->entities[newnum].number;
		}
		else
		{
			if (oldnum >= oldmax)
				break;

			if (newnum < to->num_entities)
				newindex = to->entities[newnum].number;
			else
				newindex = 9999;
		}

		if (oldnum < oldmax)
			oldindex = from->entities[oldnum].number;
		else
			oldindex = 9999;

		if (newindex == oldindex)
		{
			entity_state_t *baseline_ = &to->entities[newnum];
			qboolean custom = baseline_->entityType & ENTITY_BEAM ? TRUE : FALSE;
			SV_SetCallback(newindex, FALSE, custom, &numbase, FALSE, 0);
			DELTA_WriteDelta((uint8 *)&from->entities[oldnum], (uint8 *)baseline_, FALSE, custom ? g_pcustomentitydelta : (SV_IsPlayerIndex(newindex) ? g_pplayerdelta : g_pentitydelta), &SV_InvokeCallback);
			++oldnum;
			++newnum;
			continue;
		}

		if (newindex >= oldindex)
		{
			if (newindex > oldindex)
			{
				SV_WriteDeltaHeader(oldindex, TRUE, FALSE, &numbase, FALSE, 0, FALSE, 0);
				++oldnum;
			}
			continue;
		}

		edict_t *ent = EDICT_NUM(newindex);
		qboolean custom = to->entities[newnum].entityType & ENTITY_BEAM ? TRUE : FALSE;
		SV_SetCallback(
			newindex,
			FALSE,
			custom,
			&numbase,
			from == NULL,
			0);

		entity_state_t *baseline_ = &sv.baselines[newindex];
		if (sv_instancedbaseline.value != 0.0f && sv.instance_baselines->number != 0 && newindex > sv_lastnum)
		{
			for (int i = 0; i < sv.instance_baselines->number; i++)
			{
				if (sv.instance_baselines->classname[i] == ent->v.classname)
				{
					SV_SetNewInfo(i);
					baseline_ = &sv.instance_baselines->baseline[i];
					break;
				}
			}
		}
		else
		{
			if (!from)
			{
				int offset = SV_FindBestBaseline(newnum, &baseline_, to->entities, newindex, custom);
				if (offset)
					SV_SetCallback(newindex, FALSE, custom, &numbase, TRUE, offset);

			}
		}


		delta_t* delta = custom ? g_pcustomentitydelta : (SV_IsPlayerIndex(newindex) ? g_pplayerdelta : g_pentitydelta);

		DELTA_WriteDelta(
			(uint8 *)baseline_,
			(uint8 *)&to->entities[newnum],
			TRUE,
			delta,
			&SV_InvokeCallback
			);

		++newnum;

	}

	MSG_WriteBits(0, 16);
	MSG_EndBitWriting(msg);

	return msg->cursize;
}

void SV_EmitPacketEntities(client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	SV_CreatePacketEntities(client->delta_sequence == -1 ? sv_packet_nodelta : sv_packet_delta, client, to, msg);
}

qboolean SV_ShouldUpdatePing(client_t *client)
{
	if (client->proxy)
	{
		if (realtime < client->nextping)
			return FALSE;

		client->nextping = realtime + 2.0;
		return TRUE;
	}

	//useless call
	//SV_CalcPing(client);

	return client->lastcmd.buttons & IN_SCORE;
}

qboolean SV_HasEventsInQueue(client_t *client)
{
	int i;
	event_state_t *es;
	event_info_t *ei;

	es = &client->events;

	for (i = 0; i < MAX_EVENT_QUEUE; i++)
	{
		ei = &es->ei[i];

		if (ei->index)
			return TRUE;
	}
	return FALSE;
}

void SV_GetNetInfo(client_t *client, int *ping, int *packet_loss)
{
	static uint16 lastping[32];
	static uint8 lastloss[32];

	int i = client - svs.clients;
	if (realtime >= client->nextping)
	{
		client->nextping = realtime + 2.0;
		lastping[i] = SV_CalcPing(client);
		lastloss[i] = client->packet_loss;
	}

	*ping = lastping[i];
	*packet_loss = lastloss[i];
}

int SV_CheckVisibility(const edict_t *entity, unsigned char *pset)
{
	int leaf;

	if (!pset)
		return 1;

	if (entity->headnode < 0)
	{
		for (int i = 0; i < entity->num_leafs; i++)
		{
			leaf = entity->leafnums[i];
			if (pset[leaf >> 3] & (1 << (leaf & 7)))
				return 1;
		}
		return 0;
	}
	else
	{
		for (int i = 0; i < 48; i++)
		{
			leaf = entity->leafnums[i];
			if (leaf == -1)
				break;

			if (pset[leaf >> 3] & (1 << (leaf & 7)))
				return 1;
		}

		if (CM_HeadnodeVisible(&sv.worldmodel->nodes[entity->headnode], pset, &leaf))
		{
			((edict_t * )entity)->leafnums[entity->num_leafs] = leaf;
			((edict_t*)entity)->num_leafs = (entity->num_leafs + 1) % 48;
			return 2;
		}

		return 0;
	}
}

void SV_EmitPings(client_t *client, sizebuf_t *msg)
{
	int ping;
	int packet_loss;

	MSG_WriteByte(msg, svc_pings);
	MSG_StartBitWriting(msg);
	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *cl = &svs.clients[i];
		if (!cl->active)
			continue;

		SV_GetNetInfo(cl, &ping, &packet_loss);
		MSG_WriteBits(1, 1);
		MSG_WriteBits(i, 5);
		MSG_WriteBits(ping, 12);
		MSG_WriteBits(packet_loss, 7);
	}

	MSG_WriteBits(0, 1);
	MSG_EndBitWriting(msg);
}

void SV_WriteEntitiesToClient(client_t *client, sizebuf_t *msg)
{
	client_frame_t *frame = &client->frames[SV_UPDATE_MASK & client->netchan.outgoing_sequence];

	unsigned char *pvs = NULL;
	unsigned char *pas = NULL;
	gEntityInterface.pfnSetupVisibility((edict_t*)client->pViewEntity, client->edict, &pvs, &pas);
	unsigned char *pSet = pvs;

	packet_entities_t *pack = &frame->entities;

	// Con_SafePrintf(__FUNCTION__ " accesing frame %d for %d packet entities\r\n", SV_UPDATE_MASK & client->netchan.outgoing_sequence, frame->entities.num_entities);

	SV_ClearPacketEntities(frame);
	full_packet_entities_t fullpack;
	fullpack.num_entities = 0;
	full_packet_entities_t* curPack = &fullpack;

	qboolean sendping = SV_ShouldUpdatePing(client);
	int flags = client->lw != 0;

	int e;
	for (e = 1; e <= svs.maxclients; e++)
	{
		client_t *cl = &svs.clients[e - 1];
		if ((!cl->active && !cl->spawned) || cl->proxy)
			continue;

		qboolean add = gEntityInterface.pfnAddToFullPack(&curPack->entities[curPack->num_entities], e, &sv.edicts[e], host_client->edict, flags, TRUE, pSet);
		if (add)
			++curPack->num_entities;
	}

	for (; e < sv.num_edicts; e++)
	{
		if (curPack->num_entities >= MAX_PACKET_ENTITIES)
		{
			Con_DPrintf(const_cast<char*>("Too many entities in visible packet list.\n"));
			break;
		}

		edict_t* ent = &sv.edicts[e];

		qboolean add = gEntityInterface.pfnAddToFullPack(&curPack->entities[curPack->num_entities], e, ent, host_client->edict, flags, FALSE, pSet);
		if (add)
			++curPack->num_entities;
	}

	SV_AllocPacketEntities(frame, fullpack.num_entities);
	if (pack->num_entities)
		Q_memcpy(pack->entities, fullpack.entities, sizeof(entity_state_t) * pack->num_entities);

	SV_EmitPacketEntities(client, pack, msg);
	SV_EmitEvents(client, pack, msg);
	if (sendping)
		SV_EmitPings(client, msg);
}

void SV_CleanupEnts(void)
{
	for (int e = 1; e < sv.num_edicts; e++)
	{
		edict_t *ent = &sv.edicts[e];
		ent->v.effects &= ~(EF_NOINTERP | EF_MUZZLEFLASH);
	}
}

qboolean SV_SendClientDatagram(client_t *client)
{
	unsigned char buf[MAX_DATAGRAM];
	sizebuf_t msg;

	msg.buffername = "Client Datagram";
	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;
	msg.flags = FSB_ALLOWOVERFLOW;

	MSG_WriteByte(&msg, svc_time);
	MSG_WriteFloat(&msg, sv.time);

	SV_WriteClientdataToMessage(client, &msg);
	SV_WriteEntitiesToClient(client, &msg);

	if (client->datagram.flags & FSB_OVERFLOWED)
	{
		Con_Printf(const_cast<char*>("WARNING: datagram overflowed for %s\n"), client->name);
	}
	else
	{
		SZ_Write(&msg, client->datagram.data, client->datagram.cursize);
	}

	SZ_Clear(&client->datagram);

	if (msg.flags & FSB_OVERFLOWED)
	{
		Con_Printf(const_cast<char*>("WARNING: msg overflowed for %s\n"), client->name);
		SZ_Clear(&msg);
	}

	Netchan_Transmit(&client->netchan, msg.cursize, buf);

	return TRUE;
}

void SV_UpdateToReliableMessages(void)
{
	int i;
	client_t *client;

	// Prepare setinfo changes and send new user messages
	for (i = 0; i < svs.maxclients; i++)
	{
		client = &svs.clients[i];

		if (!client->edict)
			continue;

		host_client = client;

		if (client->sendinfo && client->sendinfo_time <= realtime)
		{
			client->sendinfo = FALSE;
			client->sendinfo_time = realtime + 1.0;
			SV_ExtractFromUserinfo(client);
			SV_FullClientUpdate(host_client, &sv.reliable_datagram);
		}

		if (!client->fakeclient && (client->active || client->connected))
		{
			if (sv_gpNewUserMsgs != NULL)
			{
				SV_SendUserReg(&client->netchan.message);
			}
		}
	}

	// Link new user messages to sent chain
	if (sv_gpNewUserMsgs != NULL)
	{
		UserMsg *pMsg = sv_gpUserMsgs;
		if (pMsg != NULL)
		{
			while (pMsg->next)
			{
				pMsg = pMsg->next;
			}
			pMsg->next = sv_gpNewUserMsgs;
		}
		else
		{
			sv_gpUserMsgs = sv_gpNewUserMsgs;
		}
		sv_gpNewUserMsgs = NULL;
	}

	if (sv.datagram.flags & FSB_OVERFLOWED)
	{
		Con_DPrintf(const_cast<char*>("sv.datagram overflowed!\n"));
		SZ_Clear(&sv.datagram);
	}
	if (sv.spectator.flags & FSB_OVERFLOWED)
	{
		Con_DPrintf(const_cast<char*>("sv.spectator overflowed!\n"));
		SZ_Clear(&sv.spectator);
	}

	// Send broadcast data
	for (i = 0; i < svs.maxclients; i++)
	{
		client = &svs.clients[i];

		if (!(!client->fakeclient && client->active))
			continue;

		if (sv.reliable_datagram.cursize + client->netchan.message.cursize < client->netchan.message.maxsize)
		{
			SZ_Write(&client->netchan.message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
		}
		else
		{
			Netchan_CreateFragments(TRUE, &client->netchan, &sv.reliable_datagram);
		}

		if (sv.datagram.cursize + client->datagram.cursize < client->datagram.maxsize)
		{
			SZ_Write(&client->datagram, sv.datagram.data, sv.datagram.cursize);
		}
		else
		{
			Con_DPrintf(const_cast<char*>("Warning:  Ignoring unreliable datagram for %s, would overflow\n"), client->name);
		}

		if (client->proxy)
		{
			if (sv.spectator.cursize + client->datagram.cursize < client->datagram.maxsize)
			{
				SZ_Write(&client->datagram, sv.spectator.data, sv.spectator.cursize);
			}
		}
	}

	SZ_Clear(&sv.reliable_datagram);
	SZ_Clear(&sv.datagram);
	SZ_Clear(&sv.spectator);
}

void SV_SkipUpdates(void)
{
	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *client = &svs.clients[i];
		if (!client->active && !client->connected && !client->spawned)
			continue;

		if (!host_client->fakeclient)
			client->skip_message = TRUE;
	}
}

void SV_SendClientMessages(void)
{
	SV_UpdateToReliableMessages();

	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *cl = &svs.clients[i];
		host_client = cl;

		if ((!cl->active && !cl->connected && !cl->spawned) || cl->fakeclient)
			continue;

		if (cl->skip_message)
		{
			cl->skip_message = FALSE;
			continue;
		}

		if (host_limitlocal.value == 0.0f && cl->netchan.remote_address.type == NA_LOOPBACK)
			cl->send_message = TRUE;

		if (cl->active && cl->spawned && cl->fully_connected && host_frametime + realtime >= cl->next_messagetime)
			cl->send_message = TRUE;

		if (cl->netchan.message.flags & FSB_OVERFLOWED)
		{
			SZ_Clear(&cl->netchan.message);
			SZ_Clear(&cl->datagram);
			SV_BroadcastPrintf("%s overflowed\n", cl->name);
			Con_Printf(const_cast<char*>("WARNING: reliable overflow for %s\n"), cl->name);
			SV_DropClient(cl, FALSE, const_cast<char*>("Reliable channel overflowed"));
			cl->send_message = TRUE;
			cl->netchan.cleartime = 0;
		}
		else if (cl->send_message)
		{
			if (sv_failuretime.value < realtime - cl->netchan.last_received)
				cl->send_message = FALSE;
		}

		if (cl->send_message)
		{
			if (!Netchan_CanPacket(&cl->netchan))
			{
				++cl->chokecount;
				continue;
			}

			host_client->send_message = FALSE;
			cl->next_messagetime = host_frametime + cl->next_messageinterval + realtime;
			if (cl->active && cl->spawned && cl->fully_connected)
				SV_SendClientDatagram(cl);
			else
				Netchan_Transmit(&cl->netchan, 0, NULL);
		}
	}
	SV_CleanupEnts();
}

int SV_ModelIndex(const char *name)
{
	if (!name || !name[0])
		return 0;

	for (int i = 0; i < MAX_MODELS; i++)
	{
		if (!sv.model_precache[i])
			break;

		if (!Q_strcasecmp(sv.model_precache[i], name))
			return i;
	};

	Sys_Error(__FUNCTION__ ": model %s not precached", name);
	return 0;
}

void SV_AddResource(resourcetype_t type, const char *name, int size, unsigned char flags, int index)
{
	resource_t *r;
	if (sv.num_resources >= MAX_RESOURCE_LIST)
	{
		Sys_Error(__FUNCTION__ ": Too many resources on server.");
	}

	r = &sv.resourcelist[sv.num_resources++];

	Q_strncpy(r->szFileName, name, 63);
	r->szFileName[63] = 0;

	r->type = type;
	r->ucFlags = flags;
	r->nDownloadSize = size;
	r->nIndex = index;
}

size_t SV_CountResourceByType(resourcetype_t type, resource_t **pResourceList, size_t nListMax, size_t *nWidthFileNameMax)
{
	if (type < t_sound || type >= t_end)
		return 0;

	if (pResourceList && nListMax <= 0)
		return 0;

	resource_t *r;
	r = &sv.resourcelist[0];

	size_t nCount = 0;
	for (int i = 0; i < sv.num_resources; i++, r++)
	{
		if (r->type != type)
			continue;

		if (pResourceList)
			pResourceList[nCount] = r;

		if (nWidthFileNameMax)
			*nWidthFileNameMax = max(*nWidthFileNameMax, (size_t)Q_strlen(r->szFileName));

		if (++nCount >= nListMax && nListMax > 0)
			break;
	}

	return nCount;
}

void SV_CreateGenericResources(void)
{
	char filename[MAX_PATH];
	char *buffer;
	char *data;

	COM_StripExtension(sv.modelname, filename);
	COM_DefaultExtension(filename, const_cast<char*>(".res"));
	COM_FixSlashes(filename);

	buffer = (char *)COM_LoadFile(filename, 5, NULL);
	if (buffer == NULL)
		return;

	// skip bytes BOM signature
	if ((byte)buffer[0] == 0xEFu && (byte)buffer[1] == 0xBBu && (byte)buffer[2] == 0xBFu)
		data = &buffer[3];
	else
		data = buffer;

	Con_DPrintf(const_cast<char*>("Precaching from %s\n"), filename);
	Con_DPrintf(const_cast<char*>("----------------------------------\n"));
	sv.num_generic_names = 0;

	while (1)
	{
		data = COM_Parse(data);
		if (Q_strlen(com_token) <= 0)
			break;

		if (strstr(com_token, ".."))
			Con_Printf(const_cast<char*>("Can't precache resource with invalid relative path %s\n"), com_token);
		else if (strstr(com_token, ":"))
			Con_Printf(const_cast<char*>("Can't precache resource with absolute path %s\n"), com_token);
		else if (strstr(com_token, "\\"))
			Con_Printf(const_cast<char*>("Can't precache resource with invalid relative path %s\n"), com_token);
		else if (strstr(com_token, ".cfg"))
			Con_Printf(const_cast<char*>("Can't precache .cfg files:  %s\n"), com_token);
		else if (strstr(com_token, ".lst"))
			Con_Printf(const_cast<char*>("Can't precache .lst files:  %s\n"), com_token);
		else if (strstr(com_token, ".exe"))
			Con_Printf(const_cast<char*>("Can't precache .exe files:  %s\n"), com_token);
		else if (strstr(com_token, ".vbs"))
			Con_Printf(const_cast<char*>("Can't precache .vbs files:  %s\n"), com_token);
		else if (strstr(com_token, ".com"))
			Con_Printf(const_cast<char*>("Can't precache .com files:  %s\n"), com_token);
		else if (strstr(com_token, ".bat"))
			Con_Printf(const_cast<char*>("Can't precache .bat files:  %s\n"), com_token);
		else if (strstr(com_token, ".dll"))
			Con_Printf(const_cast<char*>("Can't precache .dll files:  %s\n"), com_token);
		else
		{
			Q_strncpy(sv.generic_precache_names[sv.num_generic_names], com_token, sizeof(sv.generic_precache_names[sv.num_generic_names]) - 1);
			sv.generic_precache_names[sv.num_generic_names][sizeof(sv.generic_precache_names[sv.num_generic_names]) - 1] = 0;
			PF_precache_generic_I(sv.generic_precache_names[sv.num_generic_names]);
			Con_DPrintf(const_cast<char*>("  %s\n"), sv.generic_precache_names[sv.num_generic_names++]);
		}
	}
	Con_DPrintf(const_cast<char*>("----------------------------------\n"));
	COM_FreeFile(buffer);
}

void SV_CreateResourceList(void)
{
	char ** s;
	int ffirstsent = 0;
	int i;
	int nSize;
	event_t *ep = NULL;

	sv.num_resources = 0;

	extern void DbgPrint(FILE*, const char* format, ...);
	extern FILE* m_fMessages;

	DbgPrint(m_fMessages, "now %d of %d resources in resourcelist\r\n", sv.num_resources, MAX_RESOURCE_LIST);

	for (i = 1, s = &sv.generic_precache[1]; *s != NULL; i++, s++)
	{
		if (svs.maxclients > 1)
			nSize = FS_FileSize(*s);
		else
			nSize = 0;


		DbgPrint(m_fMessages, "appending t_generic %s with size %d\r\n", *s, nSize);
		SV_AddResource(t_generic, *s, nSize, RES_FATALIFMISSING, i);
	}

	for (i = 1, s = &sv.sound_precache[1]; *s != NULL; i++, s++)
	{
		if (**s == '!')
		{
			if (!ffirstsent)
			{
				ffirstsent = 1;

				DbgPrint(m_fMessages, "appending special t_sound '!'\r\n");
				SV_AddResource(t_sound, "!", 0, RES_FATALIFMISSING, i);
			}
		}
		else
		{
			nSize = 0;
			if (svs.maxclients > 1)
				nSize = FS_FileSize(va(const_cast<char*>("sound/%s"), *s));

			DbgPrint(m_fMessages, "appending t_sound %s with size %d\r\n", *s, nSize);
			SV_AddResource(t_sound, *s, nSize, 0, i);
		}
	}
	for (i = 1, s = &sv.model_precache[1]; *s != NULL; i++, s++)
	{
		if (svs.maxclients > 1 && **s != '*')
			nSize = FS_FileSize(*s);
		else
			nSize = 0;


		DbgPrint(m_fMessages, "appending t_model %s with size %d\r\n", *s, nSize);
		SV_AddResource(t_model, *s, nSize, sv.model_precache_flags[i], i);
	}
	for (i = 0; i < sv_decalnamecount; i++)
	{
		DbgPrint(m_fMessages, "appending t_decal %s with size %d\r\n", sv_decalnames[i].name, Draw_DecalSize(i));
		SV_AddResource(t_decal, sv_decalnames[i].name, Draw_DecalSize(i), 0, i);
	}

	for (i = 1; i < MAX_EVENTS; i++)
	{
		ep = &sv.event_precache[i];
		if (!ep->filename)
			break;


		DbgPrint(m_fMessages, "appending t_eventscript %s with size %d\r\n", ep->filename, ep->filesize);
		SV_AddResource(t_eventscript, (char *)ep->filename, ep->filesize, RES_FATALIFMISSING, i);
	}
}

void SV_ClearCaches(void)
{
	int i;
	event_t *ep;
	for (i = 1; i < MAX_EVENTS; i++)
	{
		ep = &sv.event_precache[i];
		if (ep->filename == NULL)
			break;

		ep->filename = NULL;
		if (ep->pszScript)
			Mem_Free((void *)ep->pszScript);
		ep->pszScript = NULL;
	}
}

void SV_PropagateCustomizations(void)
{
	client_t *pHost;
	customization_t *pCust;
	resource_t *pResource;
	int i;

	// For each active player
	for (i = 0, pHost = svs.clients; i < svs.maxclients; i++, pHost++)
	{
		if (!pHost->active && !pHost->spawned || pHost->fakeclient)
			continue;

		// Send each customization to current player
		pCust = pHost->customdata.pNext;
		while (pCust != NULL)
		{
			if (pCust->bInUse)
			{
				pResource = &pCust->resource;
				MSG_WriteByte(&host_client->netchan.message, svc_customization);
				MSG_WriteByte(&host_client->netchan.message, i);
				MSG_WriteByte(&host_client->netchan.message, pResource->type);
				MSG_WriteString(&host_client->netchan.message, pResource->szFileName);
				MSG_WriteShort(&host_client->netchan.message, pResource->nIndex);
				MSG_WriteLong(&host_client->netchan.message, pResource->nDownloadSize);
				MSG_WriteByte(&host_client->netchan.message, pResource->ucFlags);
				if (pResource->ucFlags & RES_CUSTOM)
				{
					SZ_Write(&host_client->netchan.message, pResource->rgucMD5_hash, sizeof(pResource->rgucMD5_hash));
				}
			}

			pCust = pCust->pNext;
		}
	}
}

void SV_CreateBaseline(void)
{
	edict_t *svent;
	int entnum;
	qboolean player;
	entity_state_t nullstate;
	delta_t *pDelta;

	sv.instance_baselines = &g_sv_instance_baselines;
	Q_memset(&nullstate, 0, sizeof(entity_state_t));
	SV_FindModelNumbers();

	for (entnum = 0; entnum < sv.num_edicts; entnum++)
	{
		svent = &sv.edicts[entnum];
		if (!svent->free)
		{
			if (svs.maxclients >= entnum || svent->v.modelindex)
			{
				player = SV_IsPlayerIndex(entnum);
				sv.baselines[entnum].number = entnum;

				// set entity type
				if (svent->v.flags & FL_CUSTOMENTITY)
					sv.baselines[entnum].entityType = ENTITY_BEAM;
				else
					sv.baselines[entnum].entityType = ENTITY_NORMAL;

				// retline
				gEntityInterface.pfnCreateBaseline(player, entnum, &sv.baselines[entnum], svent, sv_playermodel, player_mins[0], player_maxs[0]);

				sv_lastnum = entnum;
			}
		}
	}
	gEntityInterface.pfnCreateInstancedBaselines();
	MSG_WriteByte(&sv.signon, svc_spawnbaseline);
	MSG_StartBitWriting(&sv.signon);
	for (entnum = 0; entnum < sv.num_edicts; entnum++)
	{
		svent = &sv.edicts[entnum];
		if (!svent->free && (svs.maxclients >= entnum || svent->v.modelindex))
		{
			MSG_WriteBits(entnum, 11);
			MSG_WriteBits(sv.baselines[entnum].entityType, 2);

			if (sv.baselines[entnum].entityType & ENTITY_NORMAL)
			{
				pDelta = g_pplayerdelta;
				if (!SV_IsPlayerIndex(entnum))
					pDelta = g_pentitydelta;
			}
			else
				pDelta = g_pcustomentitydelta;

			DELTA_WriteDelta((byte *)&nullstate, (byte *)&(sv.baselines[entnum]), TRUE, pDelta, NULL);
		}
	}

	MSG_WriteBits(0xFFFF, 16);
	MSG_WriteBits(sv.instance_baselines->number, 6);
	for (entnum = 0; entnum < sv.instance_baselines->number; entnum++)
		DELTA_WriteDelta((byte *)&nullstate, (byte *)&(sv.instance_baselines->baseline[entnum]), TRUE, g_pentitydelta, NULL);

	MSG_EndBitWriting(&sv.signon);
}

void SV_BuildReconnect(sizebuf_t *msg)
{
	MSG_WriteByte(msg, svc_stufftext);
	MSG_WriteString(msg, "reconnect\n");
}

void SV_ReconnectAllClients(void)
{
	int i;
	char message[34];
	_snprintf(message, sizeof(message), "Server updating Security Module.\n");

	for (i = 0; i < svs.maxclients; i++)
	{
		client_t *client = &svs.clients[i];

		if ((client->active || client->connected) && !client->fakeclient)
		{
			Netchan_Clear(&client->netchan);

			MSG_WriteByte(&client->netchan.message, svc_print);
			MSG_WriteString(&client->netchan.message, message);

			MSG_WriteByte(&client->netchan.message, svc_stufftext);
			MSG_WriteString(&client->netchan.message, "retry\n");

			SV_DropClient(client, FALSE, message);
		}
	}
}

void SV_ActivateServer(int runPhysics)
{
	int i;
	unsigned char data[NET_MAX_PAYLOAD];
	sizebuf_t msg;
	client_t *cl;
	UserMsg *pTemp;
	char szCommand[256];

	Q_memset(&msg, 0, sizeof(sizebuf_t));
	msg.buffername = "Activate Server";
	msg.data = data;
	msg.maxsize = sizeof(data);
	msg.cursize = 0;

	SetCStrikeFlags();
	Cvar_Set(const_cast<char*>("sv_newunit"), const_cast<char*>("0"));

	ContinueLoadingProgressBar("Server", 8, 0.0f);

	gEntityInterface.pfnServerActivate(sv.edicts, sv.num_edicts, svs.maxclients);

	Steam_Activate();

	ContinueLoadingProgressBar("Server", 9, 0.0f);

	SV_CreateGenericResources();

	sv.active = TRUE;
	sv.state = ss_active;

	ContinueLoadingProgressBar("Server", 10, 0.0f);

	if (!runPhysics)
	{
		host_frametime = 0.001;
		SV_Physics();
	}
	else
	{
		if (svs.maxclients <= 1)
		{
			host_frametime = 0.1;
			SV_Physics();
			SV_Physics();
		}
		else
		{
			host_frametime = 0.8;
			for (i = 0; i < 16; i++)
				SV_Physics();
		}
	}
	SV_CreateBaseline();
	SV_CreateResourceList();
	sv.num_consistency = SV_TransferConsistencyInfo();
	for (i = 0, cl = svs.clients; i < svs.maxclients; cl++, i++)
	{
		if (!cl->fakeclient && (cl->active || cl->connected))
		{
			Netchan_Clear(&cl->netchan);
			if (svs.maxclients > 1)
			{
				SV_BuildReconnect(&cl->netchan.message);
				Netchan_Transmit(&cl->netchan, 0, NULL);
			}
			else
				SV_SendServerinfo(&msg, cl);

			if (sv_gpUserMsgs)
			{
				pTemp = sv_gpNewUserMsgs;
				sv_gpNewUserMsgs = sv_gpUserMsgs;
				SV_SendUserReg(&msg);
				sv_gpNewUserMsgs = pTemp;
			}
			cl->hasusrmsgs = TRUE;
			Netchan_CreateFragments(TRUE, &cl->netchan, &msg);
			Netchan_FragSend(&cl->netchan);
			SZ_Clear(&msg);
		}
	}
	HPAK_FlushHostQueue();
	if (svs.maxclients <= 1)
		Con_DPrintf(const_cast<char*>("Game Started\n"));
	else
		Con_DPrintf(const_cast<char*>("%i player server started\n"), svs.maxclients);
	Log_Printf("Started map \"%s\" (CRC \"%i\")\n", sv.name, sv.worldmapCRC);

	if (mapchangecfgfile.string && *mapchangecfgfile.string)
	{
		AlertMessage(at_console, const_cast<char*>("Executing map change config file\n"));
		_snprintf(szCommand, sizeof(szCommand), "exec %s\n", mapchangecfgfile.string);
		Cbuf_AddText(szCommand);
	}
}

void SV_ServerShutdown(void)
{
	Steam_NotifyOfLevelChange();
	gGlobalVariables.time = sv.time;

	if (svs.dll_initialized)
	{
		if (sv.active)
			gEntityInterface.pfnServerDeactivate();
	}
}

int SV_SpawnServer(qboolean bIsDemo, char *server, char *startspot)
{
	client_t *cl;
	edict_t *ent;
	int i;
	char *pszhost;
	char oldname[64];

	if (sv.active)
	{
		cl = svs.clients;
		for (i = 0; i < svs.maxclients; i++, cl++)
		{
			if (cl->active || cl->spawned || cl->connected)
			{
				ent = cl->edict;
				if (ent == NULL || ent->free)
					continue;

				extern void DbgPrint(FILE*, const char* format, ...);
				extern FILE* m_fMessages;
				DbgPrint(m_fMessages, "disconnected in spawnserver\r\n");
				if (ent->pvPrivateData)
					gEntityInterface.pfnClientDisconnect(ent);
				else
					Con_Printf(const_cast<char*>("Skipping reconnect on %s, no pvPrivateData\n"), cl->name);
			}
		}
	}
	if (g_bOutOfDateRestart)
	{
		g_bOutOfDateRestart = FALSE;
		Cmd_ExecuteString(const_cast<char*>("quit\n"), src_command);
	}

	Log_Open();
	Log_Printf("Loading map \"%s\"\n", server);
	Log_PrintServerVars();
	NET_Config((qboolean)(svs.maxclients > 1));

	pszhost = Cvar_VariableString(const_cast<char*>("hostname"));
	if (pszhost != NULL && *pszhost == '\0')
	{
		if (gEntityInterface.pfnGetGameDescription != NULL)
			Cvar_Set(const_cast<char*>("hostname"), const_cast<char*>(gEntityInterface.pfnGetGameDescription()));
		else
			Cvar_Set(const_cast<char*>("hostname"), const_cast<char*>("Half-Life"));
	}

	scr_centertime_off = 0.0f;
	if (startspot)
		Con_DPrintf(const_cast<char*>("Spawn Server %s: [%s]\n"), server, startspot);
	else
		Con_DPrintf(const_cast<char*>("Spawn Server %s\n"), server);

	g_LastScreenUpdateTime = 0.0f;
	svs.spawncount = ++gHostSpawnCount;

	if (coop.value != 0.0f)
		Cvar_SetValue(const_cast<char*>("deathmatch"), 0.0f);

	current_skill = (int)(skill.value + 0.5f);
	if (current_skill < 0)
		current_skill = 0;
	else if (current_skill > 3)
		current_skill = 3;

	Cvar_SetValue(const_cast<char*>("skill"), current_skill);
	ContinueLoadingProgressBar("Server", 2, 0.0f);

	HPAK_CheckSize(const_cast<char*>("custom"));
	oldname[0] = 0;
	Q_strncpy(oldname, sv.name, sizeof(oldname)-1);
	oldname[sizeof(oldname)-1] = 0;
	Host_ClearMemory(FALSE);

	cl = svs.clients;
	for (i = 0; i < svs.maxclientslimit; i++, cl++)
		SV_ClearFrames(&cl->frames);

	SV_UPDATE_BACKUP = (svs.maxclients == 1) ? SINGLEPLAYER_BACKUP : MULTIPLAYER_BACKUP;
	SV_UPDATE_MASK = (SV_UPDATE_BACKUP - 1);

	SV_AllocClientFrames();
	Q_memset(&sv, 0, sizeof(server_t));

	Q_strncpy(sv.oldname, oldname, sizeof(oldname)-1);
	sv.oldname[sizeof(oldname)-1] = 0;
	Q_strncpy(sv.name, server, sizeof(sv.name) - 1);
	sv.name[sizeof(sv.name) - 1] = 0;

	if (startspot)
	{
		Q_strncpy(sv.startspot, startspot, sizeof(sv.startspot) - 1);
		sv.startspot[sizeof(sv.startspot) - 1] = 0;
	}
	else
		sv.startspot[0] = 0;

	pr_strings = gNullString;
	gGlobalVariables.pStringBase = gNullString;

	if (svs.maxclients == 1)
		Cvar_SetValue(const_cast<char*>("sv_clienttrace"), 1.0);

	sv.max_edicts = COM_EntsForPlayerSlots(svs.maxclients);

	SV_DeallocateDynamicData();
	SV_ReallocateDynamicData();

	gGlobalVariables.maxEntities = sv.max_edicts;

	sv.edicts = (edict_t *)Hunk_AllocName(sizeof(edict_t) * sv.max_edicts, const_cast<char*>("edicts"));
	sv.baselines = (entity_state_s *)Hunk_AllocName(sizeof(entity_state_t) * sv.max_edicts, const_cast<char*>("baselines"));

	sv.datagram.buffername = "Server Datagram";
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.cursize = 0;

	sv.reliable_datagram.buffername = "Server Reliable Datagram";
	sv.reliable_datagram.data = sv.reliable_datagram_buf;
	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.cursize = 0;

	sv.spectator.buffername = "Server Spectator Buffer";
	sv.spectator.data = sv.spectator_buf;
	sv.spectator.maxsize = sizeof(sv.spectator_buf);

	sv.multicast.buffername = "Server Multicast Buffer";
	sv.multicast.data = sv.multicast_buf;
	sv.multicast.maxsize = sizeof(sv.multicast_buf);

	sv.signon.buffername = "Server Signon Buffer";
	sv.signon.data = sv.signon_data;
	sv.signon.maxsize = sizeof(sv.signon_data);
	sv.signon.cursize = 0;

	sv.num_edicts = svs.maxclients + 1;

	for (i = 0; i < svs.maxclients; i++)
		svs.clients[i].edict = &sv.edicts[i + 1];

	gGlobalVariables.maxClients = svs.maxclients;
	sv.time = 1.0f;
	sv.state = ss_loading;
	sv.paused = FALSE;
	gGlobalVariables.time = 1.0f;

	R_ForceCVars((qboolean)(svs.maxclients > 1));
	ContinueLoadingProgressBar("Server", 3, 0.0f);

	_snprintf(sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName(sv.modelname, FALSE, FALSE);

	if (!sv.worldmodel)
	{
		Con_Printf(const_cast<char*>("Couldn't spawn server %s\n"), sv.modelname);
		sv.active = FALSE;
		return 0;
	}

	Sequence_OnLevelLoad(server);
	ContinueLoadingProgressBar("Server", 4, 0.0);
	if (gmodinfo.clientcrccheck)
	{
		char szDllName[64];
		_snprintf(szDllName, sizeof(szDllName), "cl_dlls//client.dll");
		COM_FixSlashes(szDllName);
		if (!MD5_Hash_File(sv.clientdllmd5, szDllName, FALSE, FALSE, NULL))
		{
			Con_Printf(const_cast<char*>("Couldn't CRC client side dll:  %s\n"), szDllName);
			sv.active = FALSE;
			return 0;
		}
	}
	ContinueLoadingProgressBar("Server", 6, 0.0);

	if (svs.maxclients <= 1)
		sv.worldmapCRC = 0;
	else
	{
		CRC32_Init(&sv.worldmapCRC);
		if (!CRC_MapFile(&sv.worldmapCRC, sv.modelname))
		{
			Con_Printf(const_cast<char*>("Couldn't CRC server map:  %s\n"), sv.modelname);
			sv.active = FALSE;
			return 0;
		}
		CM_CalcPAS(sv.worldmodel);
	}

	sv.models[1] = sv.worldmodel;
	SV_ClearWorld();
	sv.model_precache_flags[1] |= RES_FATALIFMISSING;
	sv.model_precache[1] = sv.modelname;

	sv.sound_precache[0] = pr_strings;
	sv.model_precache[0] = pr_strings;
	sv.generic_precache[0] = pr_strings;

	for (i = 1; i < sv.worldmodel->numsubmodels; i++)
	{
		sv.model_precache[i + 1] = localmodels[i];
		sv.models[i + 1] = Mod_ForName(localmodels[i], FALSE, FALSE);
		sv.model_precache_flags[i + 1] |= RES_FATALIFMISSING;
	}

	Q_memset(&sv.edicts->v, 0, sizeof(entvars_t));

	sv.edicts->free = FALSE;
	sv.edicts->v.modelindex = 1;
	sv.edicts->v.model = (size_t)sv.worldmodel - (size_t)pr_strings;
	sv.edicts->v.solid = SOLID_BSP;
	sv.edicts->v.movetype = MOVETYPE_PUSH;

	if (coop.value == 0.0f)
		gGlobalVariables.deathmatch = deathmatch.value;
	else
		gGlobalVariables.coop = coop.value;

	gGlobalVariables.serverflags = svs.serverflags;
	gGlobalVariables.mapname = (size_t)sv.name - (size_t)pr_strings;
	gGlobalVariables.startspot = (size_t)sv.startspot - (size_t)pr_strings;
	SV_SetMoveVars();

	return 1;
}

void SV_LoadEntities(void)
{
	ED_LoadFromFile(sv.worldmodel->entities);
}

void SV_ClearEntities(void)
{
	int i;
	edict_t *pEdict;

	for (i = 0; i < sv.num_edicts; i++)
	{
		pEdict = &sv.edicts[i];

		if (!pEdict->free)
			ReleaseEntityDLLFields(pEdict);
	}
}

int RegUserMsg(const char *pszName, int iSize)
{
	if (giNextUserMsg > 255)
	{
		Con_Printf(const_cast<char*>("Not enough room to register message %s\n"), pszName);
		return 0;
	}

	if (!pszName)
		return 0;

	if (Q_strlen(pszName) > 11)
	{
		Con_Printf(const_cast<char*>("Message name too long: %s\n"), pszName);
		return 0;
	}

	if (iSize > MAX_USERPACKET_SIZE)
		return 0;

	UserMsg *pUserMsgs = sv_gpUserMsgs;
	while (pUserMsgs)
	{
		if (!Q_strcmp(pszName, pUserMsgs->szName))
			return pUserMsgs->iMsg;

		pUserMsgs = pUserMsgs->next;
	}

	UserMsg *pNewMsg = (UserMsg *)Mem_ZeroMalloc(sizeof(UserMsg));
	pNewMsg->iMsg = giNextUserMsg++;
	pNewMsg->iSize = iSize;
	Q_strcpy(pNewMsg->szName, pszName);
	pNewMsg->next = sv_gpNewUserMsgs;
	sv_gpNewUserMsgs = pNewMsg;

	return pNewMsg->iMsg;
}

USERID_t *SV_StringToUserID(const char *str)
{
	static USERID_t id;
	Q_memset(&id, 0, sizeof(id));

	if (!str || !*str)
		return &id;

	char szTemp[128];
	const char *pszUserID = str + 6;
	if (Q_strncasecmp(str, "STEAM_", 6))
	{
		Q_strncpy(szTemp, pszUserID, sizeof(szTemp)-1);
		id.idtype = AUTH_IDTYPE_VALVE;
	}
	else
	{
		Q_strncpy(szTemp, pszUserID, sizeof(szTemp)-1);
		id.idtype = AUTH_IDTYPE_STEAM;
	}
	szTemp[127] = 0;
	id.m_SteamID = Steam_StringToSteamID(szTemp);

	return &id;
}

void SV_BanId_f(void)
{
	char szreason[256];
	char idstring[64];
	const char *pszCmdGiver;
	qboolean bKick;
	USERID_t *id = NULL;
	int i = 0;
	client_t *save;

	if (Cmd_Argc() < 3 || Cmd_Argc() > 8)
	{
		Con_Printf(const_cast<char*>("Usage:  banid <minutes> <uniqueid or #userid> { kick }\n"));
		Con_Printf(const_cast<char*>("Use 0 minutes for permanent\n"));
		return;
	}

	float banTime = Q_atof(Cmd_Argv(1));
	if (banTime < 0.01f)
		banTime = 0.0f;

	Q_strncpy(idstring, Cmd_Argv(2), sizeof(idstring));
	idstring[63] = 0;

	if (Cmd_Argc() > 3 && !Q_strcasecmp(Cmd_Argv(Cmd_Argc() - 1), "kick"))
		bKick = TRUE;
	else
		bKick = FALSE;

	if (idstring[0] == '#')
	{
		int search;
		if (Q_strlen(idstring) == 1)
		{
			if (Cmd_Argc() < 3)
			{
				Con_Printf(const_cast<char*>("Insufficient arguments to banid\n"));
				return;
			}
			search = Q_atoi(Cmd_Argv(3));
		}
		else
			search = Q_atoi(&idstring[1]);

		for (i = 0; i < svs.maxclients; i++)
		{
			client_t *cl = &svs.clients[i];
			if ((!cl->active && !cl->connected && !cl->spawned) || cl->fakeclient)
				continue;

			if (cl->userid == search)
			{
				id = &cl->network_userid;
				break;
			}
		}
		if (id == NULL)
		{
			Con_Printf(const_cast<char*>(__FUNCTION__ ":  Couldn't find #userid %u\n"), search);
			return;
		}
	}
	else
	{
		if (!Q_strncasecmp(idstring, "STEAM_", 6) || !Q_strncasecmp(idstring, "VALVE_", 6))
		{
			_snprintf(idstring, sizeof(idstring)-1, "%s:%s:%s", Cmd_Argv(2), Cmd_Argv(4), Cmd_Argv(6));
			idstring[63] = 0;
		}

		for (i = 0; i < svs.maxclients; i++)
		{
			client_t *cl = &svs.clients[i];
			if (!cl->active && !cl->connected && !cl->spawned || cl->fakeclient)
				continue;

			if (!Q_strcasecmp(SV_GetClientIDString(cl), idstring))
			{
				id = &cl->network_userid;
				break;
			}
		}
		if (id == NULL)
			id = SV_StringToUserID(idstring);

		if (id == NULL)
		{
			Con_Printf(const_cast<char*>(__FUNCTION__ ":  Couldn't resolve uniqueid %s.\n"), idstring);
			Con_Printf(const_cast<char*>("Usage:  banid <minutes> <uniqueid or #userid> { kick }\n"));
			Con_Printf(const_cast<char*>("Use 0 minutes for permanent\n"));
			return;
		}
	}

	for (i = 0; i < numuserfilters; i++)
	{
		if (SV_CompareUserID(&userfilters[i].userid, id))
			break;
	}

	if (i >= numuserfilters)
	{
		if (numuserfilters >= MAX_USERFILTERS)
		{
			Con_Printf(const_cast<char*>(__FUNCTION__ ":  User filter list is full\n"));
			return;
		}
		numuserfilters++;
	}

	userfilters[i].banTime = banTime;
	userfilters[i].banEndTime = (banTime == 0.0f) ? 0.0f : banTime * 60.0f + realtime;
	userfilters[i].userid = *id;

	if (banTime == 0.0f)
		sprintf(szreason, "permanently");
	else
		_snprintf(szreason, sizeof(szreason), "for %.2f minutes", banTime);

	if (cmd_source == src_command)
		pszCmdGiver = (cls.state != ca_dedicated) ? cl_name.string : "Console";
	else
		pszCmdGiver = host_client->name;

	if (!bKick)
	{
		if (sv_logbans.value != 0.0f)
			Log_Printf("Ban: \"<><%s><>\" was banned \"%s\" by \"%s\"\n", SV_GetIDString(id), szreason, pszCmdGiver);

		return;
	}

	save = host_client;
	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *cl = &svs.clients[i];
		if (!cl->active && !cl->connected && !cl->spawned || cl->fakeclient)
			continue;

		if (SV_CompareUserID(&cl->network_userid, id))
		{
			host_client = cl;
			if (sv_logbans.value != 0.0f)
			{
				Log_Printf(
					"Ban: \"%s<%i><%s><>\" was kicked and banned \"%s\" by \"%s\"\n",
					host_client->name, host_client->userid, SV_GetClientIDString(host_client), szreason, pszCmdGiver
					);
			}
			SV_ClientPrintf("You have been kicked and banned %s by the server op.\n", szreason);
			SV_DropClient(host_client, FALSE, const_cast <char*>("Kicked and banned"));
			break;
		}
	}
	host_client = save;
}

void Host_Kick_f(void)
{
	const char *p;
	char idstring[64];
	int argsStartNum;

	qboolean isSteam = FALSE;
	qboolean found = FALSE;

	if (Cmd_Argc() <= 1)
	{
		Con_Printf(const_cast<char*>("usage:  kick < name > | < # userid >\n"));
		return;
	}

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else
	{
		if (host_client->netchan.remote_address.type != NA_LOOPBACK)
		{
			SV_ClientPrintf("You can't 'kick' because you are not a server operator\n");
			return;
		}
	}

	client_t *save = host_client;
	p = Cmd_Argv(1);
	if (p && *p == '#')
	{
		int searchid;
		if (Cmd_Argc() <= 2 || p[1])
		{
			p++;
			argsStartNum = 2;
			searchid = Q_atoi(p);
		}
		else
		{
			searchid = Q_atoi(Cmd_Argv(2));
			p = Cmd_Argv(2);
			argsStartNum = 3;
		}

		Q_strncpy(idstring, p, 63);
		idstring[63] = 0;

		if (!Q_strncasecmp(idstring, "STEAM_", 6) || !Q_strncasecmp(idstring, "VALVE_", 6))
		{
			_snprintf(idstring, 63, "%s:%s:%s", p, Cmd_Argv(argsStartNum + 1), Cmd_Argv(argsStartNum + 3));

			argsStartNum += 4;
			idstring[63] = 0;
			isSteam = 1;
		}

		for (int i = 0; i < svs.maxclients; i++)
		{
			host_client = &svs.clients[i];
			if (!host_client->active && !host_client->connected)
				continue;

			if (searchid && host_client->userid == searchid)
			{
				found = TRUE;
				break;
			}

			if (Q_strcasecmp(SV_GetClientIDString(host_client), idstring) == 0)
			{
				found = TRUE;
				break;
			}
		}
	}
	else
	{
		for (int i = 0; i < svs.maxclients; i++)
		{
			host_client = &svs.clients[i];
			if (!host_client->active && !host_client->connected)
				continue;

			if (!Q_strcasecmp(host_client->name, Cmd_Argv(1)))
			{
				found = TRUE;
				break;
			}

		}
		argsStartNum = 2;
	}

	if (found)
	{
		const char *who;
		if (cmd_source == src_command)
			who = (cls.state != ca_dedicated) ? cl_name.string : "Console";
		else
			who = save->name;

		if (host_client->netchan.remote_address.type == NA_LOOPBACK)
		{
			Con_Printf(const_cast<char*>("The local player cannot be kicked!\n"));
			return;
		}

		if (Cmd_Argc() > argsStartNum)
		{
			unsigned int dataLen = 0;
			for (int i = 1; i < argsStartNum; i++)
			{
				dataLen += Q_strlen(Cmd_Argv(i)) + 1;
			}

			if (isSteam)
				dataLen -= 4;

			p = Cmd_Args();
			if (dataLen <= Q_strlen(p))
			{
				const char *message = dataLen + p;
				if (message)
				{
					SV_ClientPrintf("Kicked by %s: %s\n", who, message);
					Log_Printf(
						"Kick: \"%s<%i><%s><>\" was kicked by \"%s\" (message \"%s\")\n",
						host_client->name, host_client->userid, SV_GetClientIDString(host_client), who, message
						);
					SV_DropClient(host_client, 0, va(const_cast<char*>("Kicked :%s"), message));
					host_client = save;
					return;
				}
			}
		}

		SV_ClientPrintf("Kicked by %s\n", who);
		Log_Printf("Kick: \"%s<%i><%s><>\" was kicked by \"%s\"\n", host_client->name, host_client->userid, SV_GetClientIDString(host_client), who);
		SV_DropClient(host_client, FALSE, const_cast<char*>("Kicked"));
	}
	host_client = save;
}

void SV_RemoveId_f(void)
{
	char idstring[64];

	if (Cmd_Argc() != 2 && Cmd_Argc() != 6)
	{
		Con_Printf(const_cast<char*>("Usage:  removeid <uniqueid | #slotnumber>\n"));
		return;
	}

	Q_strncpy(idstring, Cmd_Argv(1), sizeof(idstring)-1);
	idstring[sizeof(idstring)-1] = 0;

	if (!idstring[0])
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ":  Id string is empty!\n"));
		return;
	}

	if (idstring[0] == '#')
	{
		int slot = Q_atoi(&idstring[1]);
		if (slot <= 0 || slot > numuserfilters)
		{
			Con_Printf(const_cast<char*>(__FUNCTION__ ":  invalid slot #%i\n"), slot);
			return;
		}
		slot--;

		USERID_t id = userfilters[slot].userid;

		if (slot + 1 < numuserfilters)
			Q_memcpy(&userfilters[slot], &userfilters[slot + 1], (numuserfilters - (slot + 1)) * sizeof(userfilter_t));

		numuserfilters--;
		Con_Printf(const_cast<char*>("UserID filter removed for %s, id %s\n"), idstring, SV_GetIDString(&id));
	}
	else
	{
		if (!Q_strncasecmp(idstring, "STEAM_", 6) || !Q_strncasecmp(idstring, "VALVE_", 6))
		{
			_snprintf(idstring, sizeof(idstring)-1, "%s:%s:%s", Cmd_Argv(1), Cmd_Argv(3), Cmd_Argv(5));
			idstring[63] = 0;
		}

		for (int i = 0; i < numuserfilters; i++)
		{
			if (!Q_strcasecmp(SV_GetIDString(&userfilters[i].userid), idstring))
			{
				if (i + 1 < numuserfilters)
					Q_memcpy(&userfilters[i], &userfilters[i + 1], (numuserfilters - (i + 1)) * sizeof(userfilter_t));

				numuserfilters--;
				Con_Printf(const_cast<char*>("UserID filter removed for %s\n"), idstring);
				return;
			}
		}

		Con_Printf(const_cast<char*>("removeid: couldn't find %s\n"), idstring);
	}
}

void SV_WriteId_f(void)
{
	FileHandle_t f;

	if (bannedcfgfile.string[0] == '/' ||
		strstr(bannedcfgfile.string, ":") ||
		strstr(bannedcfgfile.string, "..") ||
		strstr(bannedcfgfile.string, "\\"))
	{
		Con_Printf(const_cast<char*>("Couldn't open %s (contains illegal characters).\n"), bannedcfgfile.string);
		return;
	}

	char name[MAX_PATH];
	Q_strncpy(name, bannedcfgfile.string, MAX_PATH - 1);
	COM_DefaultExtension(name, const_cast<char*>(".cfg"));

	char *pszFileExt = COM_FileExtension(name);
	if (Q_strcasecmp(pszFileExt, "cfg") != 0)
	{
		Con_Printf(const_cast<char*>("Couldn't open %s (wrong file extension, must be .cfg).\n"), name);
		return;
	}

	Con_Printf(const_cast<char*>("Writing %s.\n"), name);

	f = FS_Open(name, "wt");
	if (!f)
	{
		Con_Printf(const_cast<char*>("Couldn't open %s\n"), name);
		return;
	}

	for (int i = 0; i < numuserfilters; i++)
	{
		if (userfilters[i].banTime != 0.0f)
			continue;

		FS_FPrintf(f, "banid 0.0 %s\n", SV_GetIDString(&userfilters[i].userid));
	}

	FS_Close(f);
}

void SV_ListId_f(void)
{
	if (numuserfilters <= 0)
	{
		Con_Printf(const_cast<char*>("UserID filter list: empty\n"));
		return;
	}

	Con_Printf(const_cast<char*>("UserID filter list: %i entries\n"), numuserfilters);
	for (int i = 0; i < numuserfilters; i++)
	{
		if (userfilters[i].banTime == 0.0f)
		{
			Con_Printf(const_cast<char*>("%i %s : permanent\n"), i + 1, SV_GetIDString(&userfilters[i].userid));
		}
		else
		{
			Con_Printf(const_cast<char*>("%i %s : %.3f min\n"), i + 1, SV_GetIDString(&userfilters[i].userid), userfilters[i].banTime);
		}
	}
}

qboolean StringToFilter(const char *s, ipfilter_t *f)
{
	char num[128];
	unsigned char b[4] = { 0, 0, 0, 0 };
	unsigned char m[4] = { 0, 0, 0, 0 };

	const char* cc = s;
	int i = 0;
	while (1)
	{
		if (*cc < '0' || *cc > '9')
			break;

		int j = 0;
		while (*cc >= '0' && *cc <= '9')
			num[j++] = *(cc++);

		num[j] = 0;
		b[i] = Q_atoi(num);
		if (b[i])
			m[i] = -1;

		if (*cc)
		{
			++cc;
			++i;
			if (i < 4)
				continue;
		}
		f->mask = *(uint32 *)m;
		f->compare.u32 = *(uint32 *)b;
		return TRUE;
	}
	Con_Printf(const_cast<char*>("Bad filter address: %s\n"), cc);
	return FALSE;
}

void SV_AddIP_f(void)
{
	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("Usage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent\n"));
		return;
	}

	float banTime = Q_atof(Cmd_Argv(1));
	ipfilter_t tempFilter;
	if (!StringToFilter(Cmd_Argv(2), &tempFilter))
	{
		Con_Printf(const_cast<char*>("Invalid IP address!\nUsage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent\n"));
		return;
	}

	int i = 0;
	for (; i < numipfilters; i++)
	{
		if (ipfilters[i].mask == tempFilter.mask && ipfilters[i].compare.u32 == tempFilter.compare.u32)
		{
			ipfilters[i].banTime = banTime;
			ipfilters[i].banEndTime = (banTime == 0.0f) ? 0.0f : banTime * 60.0f + realtime;
			return;
		}
	}

	if (numipfilters >= MAX_IPFILTERS)
	{
		Con_Printf(const_cast<char*>("IP filter list is full\n"));
		return;
	}

	++numipfilters;
	// checkline
	if (banTime < 0.01f)
		banTime = 0.0f;

	ipfilters[i].banTime = banTime;
	ipfilters[i].compare = tempFilter.compare;
	ipfilters[i].banEndTime = (banTime == 0.0f) ? 0.0f : banTime * 60.0f + realtime;
	ipfilters[i].mask = tempFilter.mask;

	for (int i = 0; i < svs.maxclients; i++)
	{
		host_client = &svs.clients[i];
		if (!host_client->connected || !host_client->active || !host_client->spawned || host_client->fakeclient)
			continue;

		Q_memcpy(&net_from, &host_client->netchan.remote_address, sizeof(net_from));
		if (SV_FilterPacket())
		{
			SV_ClientPrintf("The server operator has added you to banned list\n");
			SV_DropClient(host_client, 0, const_cast<char*>("Added to banned list"));
		}
	}
}

void SV_RemoveIP_f(void)
{
	ipfilter_t f;

	if (!StringToFilter(Cmd_Argv(1), &f))
		return;

	for (int i = 0; i < numipfilters; i++)
	{
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare.u32 == f.compare.u32)
		{
			if (i + 1 < numipfilters)
				memmove(&ipfilters[i], &ipfilters[i + 1], (numipfilters - (i + 1)) * sizeof(ipfilter_t));
			numipfilters--;
			ipfilters[numipfilters].banTime = 0.0f;
			ipfilters[numipfilters].banEndTime = 0.0f;
			ipfilters[numipfilters].compare.u32 = 0;
			ipfilters[numipfilters].mask = 0;
			Con_Printf(const_cast<char*>("IP filter removed.\n"));
			return;
		}
	}
	Con_Printf(const_cast<char*>("removeip: couldn't find %s.\n"), Cmd_Argv(1));
}

void SV_ListIP_f(void)
{
	if (numipfilters <= 0)
	{
		Con_Printf(const_cast<char*>("IP filter list: empty\n"));
		return;
	}

	Con_Printf(const_cast<char*>("IP filter list:\n"));

	for (int i = 0; i < numipfilters; i++)
	{
		uint8* b = ipfilters[i].compare.octets;

		if (ipfilters[i].banTime == 0.0f)
			Con_Printf(const_cast<char*>("%3i.%3i.%3i.%3i : permanent\n"), b[0], b[1], b[2], b[3]);
		else
			Con_Printf(const_cast<char*>("%3i.%3i.%3i.%3i : %.3f min\n"), b[0], b[1], b[2], b[3], ipfilters[i].banTime);

	}
}

void SV_WriteIP_f(void)
{
	char name[MAX_PATH];
	FileHandle_t f;
	_snprintf(name, MAX_PATH, "listip.cfg");
	Con_Printf(const_cast<char*>("Writing %s.\n"), name);

	f = FS_Open(name, "wb");
	if (!f)
	{
		Con_Printf(const_cast<char*>("Couldn't open %s\n"), name);
		return;
	}

	for (int i = 0; i < numipfilters; i++)
	{
		if (ipfilters[i].banTime != 0.0f)
			continue;

		uint8 *b = ipfilters[i].compare.octets;
		FS_FPrintf(f, "addip 0.0 %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}
	FS_Close(f);
}

void SV_KickPlayer(int nPlayerSlot, int nReason)
{
	if (nPlayerSlot < 0 || nPlayerSlot >= svs.maxclients)
		return;

	client_t * client = &svs.clients[nPlayerSlot];
	if (!client->connected || !svs.isSecure)
		return;

	USERID_t id;
	Q_memcpy(&id, &client->network_userid, sizeof(id));

	Log_Printf("Secure: \"%s<%i><%s><>\" was detected cheating and dropped from the server.\n", client->name, client->userid, SV_GetIDString(&id), nReason);

	char rgchT[1024];
	rgchT[0] = svc_print;
	sprintf(
		&rgchT[1],
		"\n********************************************\nYou have been automatically disconnected\nfrom this secure server because an illegal\ncheat was detected on your computer.\nRepeat violators may be permanently banned\nfrom all secure servers.\n\nFor help cleaning your system of cheats, visit:\nhttp://www.counter-strike.net/cheat.html\n********************************************\n\n"
		);
	Netchan_Transmit(&svs.clients[nPlayerSlot].netchan, Q_strlen(rgchT) + 1, (byte *)rgchT);

	sprintf(rgchT, "%s was automatically disconnected\nfrom this secure server.\n", client->name);
	for (int i = 0; i < svs.maxclients; i++)
	{
		if (!svs.clients[i].active && !svs.clients[i].spawned || svs.clients[i].fakeclient)
			continue;

		MSG_WriteByte(&svs.clients[i].netchan.message, svc_centerprint);
		MSG_WriteString(&svs.clients[i].netchan.message, rgchT);
	}

	SV_DropClient(&svs.clients[nPlayerSlot], FALSE, const_cast<char*>("Automatically dropped by cheat detector"));
}

void SV_InactivateClients(void)
{
	int i;
	client_t *cl;
	for (i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++)
	{
		if (!cl->active && !cl->connected && !cl->spawned)
			continue;

		if (cl->fakeclient)
			SV_DropClient(cl, FALSE, const_cast<char*>("Dropping fakeclient on level change"));
		else
		{
			cl->active = FALSE;
			cl->connected = TRUE;
			cl->spawned = FALSE;
			cl->fully_connected = FALSE;
			// добавлены неиспользуемые команды. --barspinoff
			// куда ты их добавил? /2023/
			cl->m_bSentNewResponse = FALSE;
			cl->hasusrmsgs = FALSE;

			SZ_Clear(&cl->netchan.message);
			SZ_Clear(&cl->datagram);

			COM_ClearCustomizationList(&cl->customdata, FALSE);
			Q_memset(cl->physinfo, 0, MAX_PHYSINFO_STRING);
		}
	}
}

void SV_FailDownload(const char *filename)
{
	if (filename && *filename)
	{
		MSG_WriteByte(&host_client->netchan.message, svc_filetxferfailed);
		MSG_WriteString(&host_client->netchan.message, filename);
	}
}

qboolean IsSafeFileToDownload( const char *tfilename )
{
	if ( !tfilename )
	{
		return FALSE;
	}

	char filename[MAX_PATH] = { 0 };
	Q_strncpy( filename, tfilename, sizeof( filename ) );
	Q_strlwr( filename );

	// Disallow access to toplevel directories
	if ( Q_strstr( filename, ":" ) || Q_strstr( filename, ".." ) || Q_strstr( filename, "~" ) )
	{
		return FALSE;
	}

	if ( filename[0] == '/' || filename[0] == '\\' )
	{
		return FALSE;
	}

#define BADEXT( f ) Q_stristr( filename, f )

	// Check if we're trying to download any bad files
	if ( strchr( filename, '.' ) != strrchr( filename, '.' ) || BADEXT( ".cfg" ) || BADEXT( ".rc" ) || BADEXT( ".lst" ) || BADEXT( ".exe" ) || BADEXT( ".vbs" ) || BADEXT( ".com" ) || BADEXT( ".bat" ) || BADEXT( ".dll" ) || BADEXT( ".ini" ) || BADEXT( ".log" ) || BADEXT( "halflife.wad" ) || BADEXT( "pak0.pak" ) || BADEXT( "xeno.wad" ) || BADEXT( ".so" ) || BADEXT( ".dylib" ) || BADEXT( ".sys" ) || BADEXT( ".asi" ) || BADEXT( ".mix" ) || BADEXT( ".flt" ) )
	{
		return FALSE;
	}

#undef BADEXT

	return TRUE;
}

void SV_BeginFileDownload_f(void)
{
	const char *name;
	char szModuleC[13] = "!ModuleC.dll";

	unsigned char md5[16];
	resource_t custResource;
	unsigned char *pbuf = NULL;
	int size = 0;

	if (Cmd_Argc() < 2 || cmd_source == src_command)
	{
		return;
	}

	name = Cmd_Argv(1);
	if (!name || !name[0] || (!Q_strncmp(name, szModuleC, 12) && svs.isSecure))
	{
		return;
	}

	if (!IsSafeFileToDownload(name) || sv_allow_download.value == 0.0f)
	{
		SV_FailDownload(name);
		return;
	}

	// Regular downloads
	if (name[0] != '!')
	{
		if (host_client->fully_connected ||
			sv_send_resources.value == 0.0f ||
			sv_downloadurl.string != NULL && sv_downloadurl.string[0] != 0 && Q_strlen(sv_downloadurl.string) <= 128 && sv_allow_dlfile.value == 0.0f ||
			Netchan_CreateFileFragments(TRUE, &host_client->netchan, name) == 0)
		{
			SV_FailDownload(name);
			return;
		}
		Netchan_FragSend(&host_client->netchan);
		return;
	}

	// Customizations
	if (Q_strlen(name) != 36 || Q_strncasecmp(name, "!MD5", 4) != 0 || sv_send_logos.value == 0.0f)
	{
		SV_FailDownload(name);
		return;
	}

	Q_memset(&custResource, 0, sizeof(custResource));
	COM_HexConvert(name + 4, 32, md5);
	if (HPAK_ResourceForHash(const_cast<char*>("custom.hpk"), md5, &custResource))
	{
		HPAK_GetDataPointer(const_cast<char*>("custom.hpk"), &custResource, &pbuf, &size);
		if (pbuf && size)
		{
			Netchan_CreateFileFragmentsFromBuffer(TRUE, &host_client->netchan, name, pbuf, size);
			Netchan_FragSend(&host_client->netchan);
			Mem_Free((void *)pbuf);
		}
	}
}

void SV_SetMaxclients()
{
	for( int i = 0; i < svs.maxclientslimit; ++i )
	{
		SV_ClearFrames( &svs.clients[ i ].frames );
	}

	svs.maxclients = 1;

	const int iCmdMaxPlayers = COM_CheckParm( const_cast<char*>("-maxplayers") );

	if( iCmdMaxPlayers )
	{
		svs.maxclients = Q_atoi( (char*)com_argv[ iCmdMaxPlayers + 1 ] );
	}

	cls.state = g_bIsDedicatedServer ? ca_dedicated : ca_disconnected;

	if( svs.maxclients <= 0 )
	{
		svs.maxclients = MP_MIN_CLIENTS;
	}
	else if( svs.maxclients > MAX_CLIENTS )
	{
		svs.maxclients = MAX_CLIENTS;
	}

	svs.maxclientslimit = MAX_CLIENTS;

	//If we're a listen server and we're low on memory, reduce maximum player limit.
	if( !g_bIsDedicatedServer && host_parms.memsize < LISTENSERVER_SAFE_MINIMUM_MEMORY )
		svs.maxclientslimit = 4;

	//Increase the number of updates available for multiplayer.
	SV_UPDATE_BACKUP = 8;

	if( svs.maxclients != 1 )
		SV_UPDATE_BACKUP = 64;

	SV_UPDATE_MASK = SV_UPDATE_BACKUP - 1;

	svs.clients = reinterpret_cast<client_t*>( Hunk_AllocName( sizeof( client_t ) * svs.maxclientslimit, const_cast<char*>("clients") ) );

	for( int i = 0; i < svs.maxclientslimit; ++i )
	{
		auto& client = svs.clients[ i ];

		Q_memset( &client, 0, sizeof( client ) );
		client.resourcesneeded.pPrev = &client.resourcesneeded;
		client.resourcesneeded.pNext = &client.resourcesneeded;
		client.resourcesonhand.pPrev = &client.resourcesonhand;
		client.resourcesonhand.pNext = &client.resourcesonhand;
	}

	Cvar_SetValue( const_cast<char*>("deathmatch"), svs.maxclients > 1 ? 1 : 0 );

	SV_AllocClientFrames();

	int maxclients = svs.maxclientslimit;

	if( maxclients > svs.maxclients )
		maxclients = svs.maxclients;

	svs.maxclients = maxclients;
}

void SV_HandleRconPacket(void)
{
	MSG_BeginReading();
	MSG_ReadLong();
	char* s = MSG_ReadStringLine();
	Cmd_TokenizeString(s);
	const char* c = Cmd_Argv(0);
	if (!Q_strcmp(c, "getchallenge"))
	{
		SVC_GetChallenge();
	}
	else if (!Q_strcasecmp(c, "challenge"))
	{
		SVC_ServiceChallenge();
	}
	else if (!Q_strcmp(c, "rcon"))
	{
		SV_Rcon(&net_from);
	}
}

void SV_CheckCmdTimes(void)
{
	static double lastreset;

	if (Host_IsSinglePlayerGame())
		return;

	if (realtime - lastreset < 1.0)
		return;

	lastreset = realtime;
	for (int i = svs.maxclients - 1; i >= 0; i--)
	{
		client_t* cl = &svs.clients[i];
		if (!cl->connected || !cl->active)
			continue;

		if (cl->connecttime == 0.0)
			cl->connecttime = realtime;

		float dif = cl->connecttime + cl->cmdtime - realtime;
		if (dif > clockwindow.value)
		{
			cl->ignorecmdtime = clockwindow.value + realtime;
			cl->cmdtime = realtime - cl->connecttime;
		}

		if (dif < -clockwindow.value)
			cl->cmdtime = realtime - cl->connecttime;
	}
}

void SV_CheckForRcon(void)
{
	if (sv.active || cls.state != ca_dedicated || giActive == DLL_CLOSE || !host_initialized)
		return;

	while (NET_GetPacket(NS_SERVER))
	{
		if (SV_FilterPacket())
		{
			SV_SendBan();
		}
		else
		{
			if (*(uint32 *)net_message.data == 0xFFFFFFFF)
				SV_HandleRconPacket();
		}
	}
}

qboolean SV_IsSimulating(void)
{
	if (sv.paused)
		return FALSE;

	if (svs.maxclients > 1)
		return TRUE;

	if (!key_dest && (cls.state == ca_active || cls.state == ca_dedicated))
		return TRUE;

	return FALSE;
}

void SV_CheckMapDifferences(void)
{
	static double lastcheck;

	if (realtime - lastcheck < 5.0)
		return;

	lastcheck = realtime;
	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t *cl = &svs.clients[i];
		if (!cl->active || !cl->crcValue)
			continue;

		if (cl->netchan.remote_address.type == NA_LOOPBACK)
			continue;

		if (cl->crcValue != sv.worldmapCRC)
			cl->netchan.message.flags |= FSB_OVERFLOWED;
	}
}

void SV_Frame()
{
	if (!sv.active)
		return;

	gGlobalVariables.frametime = host_frametime;
	sv.oldtime = sv.time;
	SV_CheckCmdTimes();
	SV_ReadPackets();
	if (SV_IsSimulating())
	{
		SV_Physics();
		sv.time += host_frametime;
	}
	SV_QueryMovevarsChanged();
	SV_RequestMissingResourcesFromClients();
	SV_CheckTimeouts();
	SV_SendClientMessages();
	SV_CheckMapDifferences();
	SV_GatherStatistics();
	Steam_RunFrame();
}

void SV_Drop_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
	}
	else
	{
		SV_EndRedirect();
		SV_BroadcastPrintf(const_cast<char*>("%s dropped\n"), host_client->name);
		SV_DropClient(host_client, FALSE, const_cast<char*>("Client sent 'drop'"));
	}
}

void SV_RegisterDelta(char *name, char *loadfile)
{
	delta_t *pdesc = NULL;
	if (!DELTA_Load(name, &pdesc, loadfile))
		Sys_Error("Error parsing %s!!!\n", loadfile);

	delta_info_t *p = (delta_info_t *)Mem_ZeroMalloc(sizeof(delta_info_t));
	p->loadfile = Mem_Strdup(loadfile);
	p->name = Mem_Strdup(name);
	p->delta = pdesc;
	p->next = g_sv_delta;
	g_sv_delta = p;
}

void SV_InitDeltas(void)
{
	Con_DPrintf(const_cast<char*>("Initializing deltas\n"));
	SV_RegisterDelta(const_cast<char*>("clientdata_t"), const_cast<char*>("delta.lst"));
	SV_RegisterDelta(const_cast<char*>("entity_state_t"), const_cast<char*>("delta.lst"));
	SV_RegisterDelta(const_cast<char*>("entity_state_player_t"), const_cast<char*>("delta.lst"));
	SV_RegisterDelta(const_cast<char*>("custom_entity_state_t"), const_cast<char*>("delta.lst"));
	SV_RegisterDelta(const_cast<char*>("usercmd_t"), const_cast<char*>("delta.lst"));
	SV_RegisterDelta(const_cast<char*>("weapon_data_t"), const_cast<char*>("delta.lst"));
	SV_RegisterDelta(const_cast<char*>("event_t"), const_cast<char*>("delta.lst"));

	g_pplayerdelta = SV_LookupDelta(const_cast<char*>("entity_state_player_t"));
	if (!g_pplayerdelta)
		Sys_Error("No entity_state_player_t encoder on server!\n");

	g_pentitydelta = SV_LookupDelta(const_cast<char*>("entity_state_t"));
	if (!g_pentitydelta)
		Sys_Error("No entity_state_t encoder on server!\n");

	g_pcustomentitydelta = SV_LookupDelta(const_cast<char*>("custom_entity_state_t"));
	if (!g_pcustomentitydelta)
		Sys_Error("No custom_entity_state_t encoder on server!\n");

	g_pclientdelta = SV_LookupDelta(const_cast<char*>("clientdata_t"));
	if (!g_pclientdelta)
		Sys_Error("No clientdata_t encoder on server!\n");

	g_pweapondelta = SV_LookupDelta(const_cast<char*>("weapon_data_t"));
	if (!g_pweapondelta)
		Sys_Error("No weapon_data_t encoder on server!\n");

	g_peventdelta = SV_LookupDelta(const_cast<char*>("event_t"));
	if (!g_peventdelta)
		Sys_Error("No event_t encoder on server!\n");
}

void SV_InitEncoders(void)
{
	delta_t *pdesc;
	delta_info_t *p;
	for (p = g_sv_delta; p != NULL; p = p->next)
	{
		pdesc = p->delta;
		if (Q_strlen(pdesc->conditionalencodename) > 0)
			pdesc->conditionalencode = DELTA_LookupEncoder(pdesc->conditionalencodename);
	}
}

void SV_Init(void)
{
	Cmd_AddCommand(const_cast<char*>("banid"), SV_BanId_f);
	Cmd_AddCommand(const_cast<char*>("removeid"), SV_RemoveId_f);
	Cmd_AddCommand(const_cast<char*>("listid"), SV_ListId_f);
	Cmd_AddCommand(const_cast<char*>("writeid"), SV_WriteId_f);
	Cmd_AddCommand(const_cast<char*>("resetrcon"), SV_ResetRcon_f);
	Cmd_AddCommand(const_cast<char*>("logaddress"), SV_SetLogAddress_f);
	Cmd_AddCommand(const_cast<char*>("logaddress_add"), SV_AddLogAddress_f);
	Cmd_AddCommand(const_cast<char*>("logaddress_del"), SV_DelLogAddress_f);
	Cmd_AddCommand(const_cast<char*>("log"), SV_ServerLog_f);
	Cmd_AddCommand(const_cast<char*>("serverinfo"), SV_Serverinfo_f);
	Cmd_AddCommand(const_cast<char*>("localinfo"), SV_Localinfo_f);
	Cmd_AddCommand(const_cast<char*>("showinfo"), SV_ShowServerinfo_f);
	Cmd_AddCommand(const_cast<char*>("user"), SV_User_f);
	Cmd_AddCommand(const_cast<char*>("users"), SV_Users_f);
	Cmd_AddCommand(const_cast<char*>("dlfile"), SV_BeginFileDownload_f);
	Cmd_AddCommand(const_cast<char*>("addip"), SV_AddIP_f);
	Cmd_AddCommand(const_cast<char*>("removeip"), SV_RemoveIP_f);
	Cmd_AddCommand(const_cast<char*>("listip"), SV_ListIP_f);
	Cmd_AddCommand(const_cast<char*>("writeip"), SV_WriteIP_f);
	Cmd_AddCommand(const_cast<char*>("dropclient"), SV_Drop_f);
	Cmd_AddCommand(const_cast<char*>("spawn"), SV_Spawn_f);
	Cmd_AddCommand(const_cast<char*>("new"), SV_New_f);
	Cmd_AddCommand(const_cast<char*>("sendres"), SV_SendRes_f);
	Cmd_AddCommand(const_cast<char*>("sendents"), SV_SendEnts_f);
	Cmd_AddCommand(const_cast<char*>("fullupdate"), SV_FullUpdate_f);

	Cvar_RegisterVariable(&sv_failuretime);
	Cvar_RegisterVariable(&sv_voiceenable);
	Cvar_RegisterVariable(&rcon_password);
	Cvar_RegisterVariable(&sv_enableoldqueries);
	Cvar_RegisterVariable(&mp_consistency);
	Cvar_RegisterVariable(&sv_instancedbaseline);
	Cvar_RegisterVariable(&sv_contact);
	Cvar_RegisterVariable(&sv_unlag);
	Cvar_RegisterVariable(&sv_maxunlag);
	Cvar_RegisterVariable(&sv_unlagpush);
	Cvar_RegisterVariable(&sv_unlagsamples);
	Cvar_RegisterVariable(&sv_filterban);
	Cvar_RegisterVariable(&sv_maxupdaterate);
	Cvar_RegisterVariable(&sv_minupdaterate);
	Cvar_RegisterVariable(&sv_logrelay);
	Cvar_RegisterVariable(&sv_lan);
	Cvar_DirectSet(&sv_lan, const_cast<char*>(PF_IsDedicatedServer() ? "0" : "1"));
	Cvar_RegisterVariable(&sv_lan_rate);
	Cvar_RegisterVariable(&sv_proxies);
	Cvar_RegisterVariable(&sv_outofdatetime);
	Cvar_RegisterVariable(&sv_visiblemaxplayers);
	Cvar_RegisterVariable(&sv_password);
	Cvar_RegisterVariable(&sv_aim);
	Cvar_RegisterVariable(&violence_hblood);
	Cvar_RegisterVariable(&violence_ablood);
	Cvar_RegisterVariable(&violence_hgibs);
	Cvar_RegisterVariable(&violence_agibs);
	Cvar_RegisterVariable(&sv_newunit);
	Cvar_RegisterVariable(&sv_gravity);
	Cvar_RegisterVariable(&sv_friction);
	Cvar_RegisterVariable(&sv_edgefriction);
	Cvar_RegisterVariable(&sv_stopspeed);
	Cvar_RegisterVariable(&sv_maxspeed);
	Cvar_RegisterVariable(&sv_footsteps);
	Cvar_RegisterVariable(&sv_accelerate);
	Cvar_RegisterVariable(&sv_stepsize);
	Cvar_RegisterVariable(&sv_bounce);
	Cvar_RegisterVariable(&sv_airaccelerate);
	Cvar_RegisterVariable(&sv_wateraccelerate);
	Cvar_RegisterVariable(&sv_waterfriction);
	Cvar_RegisterVariable(&sv_skycolor_r);
	Cvar_RegisterVariable(&sv_skycolor_g);
	Cvar_RegisterVariable(&sv_skycolor_b);
	Cvar_RegisterVariable(&sv_skyvec_x);
	Cvar_RegisterVariable(&sv_skyvec_y);
	Cvar_RegisterVariable(&sv_skyvec_z);
	Cvar_RegisterVariable(&sv_timeout);
	Cvar_RegisterVariable(&sv_clienttrace);
	Cvar_RegisterVariable(&sv_zmax);
	Cvar_RegisterVariable(&sv_wateramp);
	Cvar_RegisterVariable(&sv_skyname);
	Cvar_RegisterVariable(&sv_maxvelocity);
	Cvar_RegisterVariable(&sv_cheats);
	if (COM_CheckParm(const_cast<char*>("-dev")))
		Cvar_SetValue(const_cast<char*>("sv_cheats"), 1.0);
	Cvar_RegisterVariable(&sv_spectatormaxspeed);
	Cvar_RegisterVariable(&sv_allow_download);
	Cvar_RegisterVariable(&sv_allow_upload);
	Cvar_RegisterVariable(&sv_max_upload);
	Cvar_RegisterVariable(&sv_send_logos);
	Cvar_RegisterVariable(&sv_send_resources);
	Cvar_RegisterVariable(&sv_logbans);
	Cvar_RegisterVariable(&hpk_maxsize);
	Cvar_RegisterVariable(&mapcyclefile);
	Cvar_RegisterVariable(&motdfile);
	Cvar_RegisterVariable(&servercfgfile);
	Cvar_RegisterVariable(&mapchangecfgfile);
	Cvar_RegisterVariable(&lservercfgfile);
	Cvar_RegisterVariable(&logsdir);
	Cvar_RegisterVariable(&bannedcfgfile);
	Cvar_RegisterVariable(&sv_rcon_minfailures);
	Cvar_RegisterVariable(&sv_rcon_maxfailures);
	Cvar_RegisterVariable(&sv_rcon_minfailuretime);
	Cvar_RegisterVariable(&sv_rcon_banpenalty);
	Cvar_RegisterVariable(&sv_minrate);
	Cvar_RegisterVariable(&sv_maxrate);
	Cvar_RegisterVariable(&max_queries_sec);
	Cvar_RegisterVariable(&max_queries_sec_global);
	Cvar_RegisterVariable(&max_queries_window);
	Cvar_RegisterVariable(&sv_logblocks);
	Cvar_RegisterVariable(&sv_downloadurl);
	Cvar_RegisterVariable(&sv_version);
	Cvar_RegisterVariable(&sv_allow_dlfile);

	for (int i = 0; i < MAX_MODELS; i++)
	{
		_snprintf(localmodels[i], sizeof(localmodels[i]), "*%i", i);
	}

	svs.isSecure = FALSE;

	for (int i = 0; i < svs.maxclientslimit; i++)
	{
		client_t *cl = &svs.clients[i];
		SV_ClearFrames(&cl->frames);
		Q_memset(cl, 0, sizeof(client_t));
		cl->resourcesonhand.pPrev = &cl->resourcesonhand;
		cl->resourcesonhand.pNext = &cl->resourcesonhand;
		cl->resourcesneeded.pPrev = &cl->resourcesneeded;
		cl->resourcesneeded.pNext = &cl->resourcesneeded;
	}

	PM_Init(&g_svmove);
	SV_AllocClientFrames();
	SV_InitDeltas();
}

void SV_Shutdown(void)
{
	delta_info_t *p = g_sv_delta;
	while (p)
	{
		delta_info_t *n = p->next;
		if (p->delta)
			DELTA_FreeDescription(&p->delta);

		Mem_Free(p->name);
		Mem_Free(p->loadfile);
		Mem_Free(p);
		p = n;
	}

	g_sv_delta = NULL;
}

void SV_ResetModInfo()
{
	Q_memset( &gmodinfo, 0, sizeof( gmodinfo ) );

	gmodinfo.version = 1;
	gmodinfo.svonly = true;
	gmodinfo.num_edicts = MAX_EDICTS;

	char szDllListFile[ FILENAME_MAX ];
	snprintf( szDllListFile, sizeof( szDllListFile ), "%s", "liblist.gam" );

	FileHandle_t hLibListFile = FS_Open( szDllListFile, "rb" );

	if( hLibListFile != FILESYSTEM_INVALID_HANDLE )
	{
		char szKey[ 64 ];
		char szValue[ 256 ];

		const int iSize = FS_Size( hLibListFile );

		if( iSize > ( 512 * 512 ) || !iSize )
			Sys_Error( "Game listing file size is bogus [%s: size %i]", "liblist.gam", iSize );

		byte* pFileData = reinterpret_cast<byte*>( Mem_Malloc( iSize + 1 ) );

		if( !pFileData )
			Sys_Error( "Could not allocate space for game listing file of %i bytes", iSize + 1 );

		const int iRead = FS_Read( pFileData, iSize, 1, hLibListFile );

		if( iRead != iSize )
			Sys_Error( "Error reading in game listing file, expected %i bytes, read %i", iSize, iRead );

		pFileData[ iSize ] = '\0';

		char* pBuffer = ( char* ) pFileData;

		com_ignorecolons = true;

		while( 1 )
		{
			pBuffer = COM_Parse( pBuffer );

			if( Q_strlen( com_token ) <= 0 )
				break;

			Q_strncpy( szKey, com_token, sizeof( szKey ) - 1 );
			szKey[ sizeof( szKey ) - 1 ] = '\0';

			pBuffer = COM_Parse( pBuffer );

			Q_strncpy( szValue, com_token, sizeof( szValue ) - 1 );
			szValue[ sizeof( szValue ) - 1 ] = '\0';

			if( Q_strcasecmp( szKey, "gamedll" ) )
				DLL_SetModKey( &gmodinfo, szKey, szValue );
		}

		com_ignorecolons = false;
		Mem_Free( pFileData );
		FS_Close( hLibListFile );
	}
}

void SV_CountPlayers( int* clients )
{
	*clients = 0;

	for( int i = 0; i < svs.maxclients; ++i )
	{
		auto& client = svs.clients[ i ];

		if( client.active || client.spawned || client.connected )
		{
			++*clients;
		}
	}
}
