#ifndef ENGINE_CLIENT_H
#define ENGINE_CLIENT_H

#include "pm_defs.h"
#include "cdll_int.h"
#include "com_model.h"
#include "eventapi.h"
#include "APIProxy.h"
#include "screenfade.h"
#include "sound.h"
#include "net.h"
#include "net_chan.h"
#include "consistency.h"

class CSysModule;

#define	SIGNONS		2			// signon messages to receive before connected


#define	MAX_EFRAGS			640
#define	MAX_DLIGHTS			32
#define	MAX_ELIGHTS			64
#define	MAX_TEMP_ENTITIES	500
#define	MAX_VISEDICTS		512

#define	MAX_DEMOS			32
#define	MAX_DEMONAME		16


//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster


struct lightstyle_t
{
	int		length;
	char	map[ MAX_STYLESTRING ];
};

enum cactive_t
{
	ca_dedicated = 0,	// This is a dedicated server, client code is inactive
	ca_disconnected,	// full screen console with no connection
	ca_connecting,		// netchan_t established, waiting for svc_serverdata
	ca_connected,		// processing data lists, donwloading, etc
	ca_uninitialized,
	ca_active			// everything is in, so frames can be rendered
};

typedef struct cmd_s
{
	usercmd_t cmd;
	float senttime;
	float receivedtime;
	float frame_lerp;
	qboolean processedfuncs;
	qboolean heldback;
	int sendsize;
} cmd_t;

typedef struct downloadtime_s
{
	qboolean bUsed;
	float fTime;
	int nBytesRemaining;
} downloadtime_t;

typedef struct incomingtransfer_s
{
	qboolean doneregistering;
	int percent;
	qboolean downloadrequested;
	downloadtime_t rgStats[8];
	int nCurStat;
	int nTotalSize;
	int nTotalToTransfer;
	int nRemainingToTransfer;
	float fLastStatusUpdate;
	qboolean custom;
} incomingtransfer_t;

typedef struct soundfade_s
{
	int nStartPercent;
	int nClientSoundFadePercent;
	double soundFadeStartTime;
	int soundFadeOutTime;
	int soundFadeHoldTime;
	int soundFadeInTime;
} soundfade_t;

/**
*	the client_static_t structure is persistent through an arbitrary number
*	of server connections
*/
struct client_static_t
{
	// connection information
	cactive_t state;

	// network stuff
	//TODO: implement - Solokiller
	netchan_t netchan;

	sizebuf_t datagram;
	byte datagram_buf[ MAX_DATAGRAM ];

	double connect_time;
	int connect_retry;

	int challenge;

	byte authprotocol;

	int userid;

	char trueaddress[ 32 ];

	float slist_time;

	//TODO: define constants - Solokiller
	int signon;

	char servername[ FILENAME_MAX ];	// name of server from original connect
	char mapstring[ 64 ];

	char spawnparms[ 2048 ];

	// private userinfo for sending to masterless servers
	char userinfo[ MAX_INFO_STRING ];

	float nextcmdtime;
	int lastoutgoingcommand;

	// demo loop control
	int demonum;									// -1 = don't play demos
	char demos[ MAX_DEMOS ][ MAX_DEMONAME ];		// when not playing
													// demo recording info must be here, because record is started before
													// entering a map (and clearing client_state_t)
	qboolean demorecording;
	qboolean demoplayback;
	qboolean timedemo;

	float demostarttime;
	int demostartframe;

	int forcetrack;

	FileHandle_t demofile;
	FileHandle_t demoheader;
	qboolean demowaiting;
	qboolean demoappending;
	char demofilename[ FILENAME_MAX ];
	int demoframecount;

	int td_lastframe;		// to meter out one message a frame
	int td_startframe;		// host_framecount at start
	float td_starttime;		// realtime at second frame of timedemo

	incomingtransfer_t dl;

	float packet_loss;
	double packet_loss_recalc_time;

	int playerbits;

	soundfade_t soundfade;

	char physinfo[ MAX_PHYSINFO_STRING ];

	byte md5_clientdll[ 16 ];

	netadr_t game_stream;
	netadr_t connect_stream;

	qboolean passive;
	qboolean spectator;
	qboolean director;
	qboolean fSecureClient;
	qboolean isVAC2Secure;

	uint64 GameServerSteamID;

	int build_num;
};

struct frame_t
{
	double receivedtime;
	double latency;

	qboolean invalid;
	qboolean choked;

	entity_state_t playerstate[ MAX_CLIENTS ];

	double time;
	clientdata_t clientdata;
	weapon_data_t weapondata[ 64 ];
	packet_entities_t packet_entities;

	unsigned short clientbytes;
	unsigned short playerinfobytes;
	unsigned short packetentitybytes;
	unsigned short tentitybytes;
	unsigned short soundbytes;
	unsigned short eventbytes;
	unsigned short usrbytes;
	unsigned short voicebytes;
	unsigned short msgbytes;
};

struct client_state_t
{
	//TODO: verify contents - Solokiller
	int max_edicts;

	resource_t resourcesonhand;
	resource_t resourcesneeded;
	resource_t resourcelist[ MAX_RESOURCE_LIST ];
	int num_resources;

	qboolean need_force_consistency_response;

	char serverinfo[ 512 ];

	int servercount;

	int validsequence;

	int parsecount;
	int parsecountmod;

	int stats[MAX_CL_STATS];

	int weapons;

	usercmd_t cmd;

	vec3_t viewangles;
	vec3_t punchangle;
	vec3_t crosshairangle;
	vec3_t simorg;
	vec3_t simvel;
	vec3_t simangles;
	vec_t predicted_origins[ 64 ][ 3 ];
	vec3_t prediction_error;

	float idealpitch;

	vec3_t viewheight;

	screenfade_t sf;

	bool paused;

	int onground;
	int moving;
	int waterlevel;
	int usehull;

	float maxspeed;

	int pushmsec;
	int light_level;
	int intermission;

	double mtime[ 2 ];
	double time;
	double oldtime;

	frame_t frames[ 64 ];
	
	cmd_t commands[ 64 ];

	local_state_t predicted_frames[ 64 ];
	int delta_sequence;
	
	int playernum;
	event_t event_precache[ EVENT_MAX_EVENTS ];

	model_t* model_precache[ MAX_MODELS ];
	int model_precache_count;

	sfx_t* sound_precache[ MAX_SOUNDS ];

	
	consistency_t consistency_list[MAX_CONSISTENCY_LIST];
	int num_consistency;
	

	int highentity;

	char levelname[ 40 ];

	int maxclients;

	int gametype;
	int viewentity;

	model_t* worldmodel;

	efrag_t* free_efrags;

	int num_entities;
	int num_statics;

	cl_entity_t viewent;

	int cdtrack;
	int looptrack;

	CRC32_t serverCRC;

	byte clientdllmd5[ 16 ];

	float weaponstarttime;
	int weaponsequence;

	int fPrecaching;

	dlight_t* pLight;
	player_info_t players[ 32 ];

	entity_state_t instanced_baseline[ 64 ];

	int instanced_baseline_number;

	CRC32_t mapCRC;

	event_state_t events;

	char downloadUrl[ 128 ];
};

extern	client_static_t	cls;
extern	client_state_t	cl;

extern playermove_t g_clmove;

extern cvar_t cl_mousegrab;
extern cvar_t m_rawinput;
extern cvar_t rate;

extern cvar_t cl_cmdrate;

extern cl_enginefunc_t cl_enginefuncs;

extern char g_szfullClientName[ 512 ];

extern CSysModule* hClientDLL;

extern	int				cl_numvisedicts;
extern	cl_entity_t		*cl_visedicts[MAX_VISEDICTS];

void CL_ShutDownClientStatic();

void CL_Shutdown();

void CL_Init();

// cl_main

dlight_t* CL_AllocDlight(int key);
void	CL_DecayLights(void);

void CL_Init(void);
void Host_WriteConfiguration(void);

void CL_Disconnect(void);
void CL_Disconnect_f(void);

void CL_RecordHUDCommand(const char* pszCmd);

qboolean CL_GetFragmentSize(void* state);

#endif //ENGINE_CLIENT_H
