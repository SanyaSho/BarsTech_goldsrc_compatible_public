#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H

#include "entity_state.h"
#include "eventapi.h"
#include "progs.h"
#include "protocol.h"
#include "pm_defs.h"
#include "pm_info.h"
#include "eiface.h"
#include "mathlib.h"
#include "usercmd.h"
#include "consistency.h"
#include "com_model.h"
#include "qlimits.h"
#include "custom.h"
#include "inst_baseline.h"
#include "net_chan.h"
#include "userid.h"
#include "decals.h"
#include "console.h"

#define MAX_LOCALINFO 32768
#define MAX_IPFILTERS 32768
#define MAX_USERFILTERS 32768
#define MAX_CHALLENGES 1024
#define MAX_ROUTEABLE_PACKET 1400

typedef struct ipfilter_s
{
	unsigned int mask;
	union {
		uint32 u32;
		uint8 octets[4];
	} compare;
	float banEndTime;
	float banTime;
} ipfilter_t;

typedef struct userfilter_s
{
	USERID_t userid;
	float banEndTime;
	float banTime;
} userfilter_t;

typedef struct challenge_s
{
	netadr_t adr;
	int challenge;
	int time;
} challenge_t;

typedef struct deltacallback_s
{
	int *numbase;
	int num;
	qboolean remove;
	qboolean custom;
	qboolean newbl;
	int newblindex;
	qboolean full;
	int offset;
} deltacallback_t;

typedef enum server_state_e
{
	ss_dead = 0,
	ss_loading = 1,
	ss_active = 2,
} server_state_t;

struct server_t
{
	qboolean active;					// false when server is going down
	
	qboolean paused;
	qboolean loadgame;

	double time;
	double oldtime;

	int lastcheck;
	double lastchecktime;
	
	char name[ 64 ];
	
	char oldname[ 64 ];
	char startspot[ 64 ];
	char modelname[ 64 ];

	model_t* worldmodel;
	CRC32_t worldmapCRC;

	byte clientdllmd5[ 16 ];

	resource_t resourcelist[ MAX_RESOURCE_LIST ];
	int num_resources;

	consistency_t consistency_list[ MAX_CONSISTENCY_LIST ];
	int num_consistency;

	char* model_precache[MAX_MODELS];
	model_t* models[MAX_MODELS];
	byte model_precache_flags[MAX_MODELS];
	
	event_t event_precache[ EVENT_MAX_EVENTS ];
	
	char* sound_precache[MAX_SOUNDS];
	short sound_precache_hashedlookup[MAX_SOUNDS_HASHLOOKUP_SIZE];
	bool sound_precache_hashedlookup_built;

	char* generic_precache[ MAX_GENERIC ];
	char generic_precache_names[ MAX_GENERIC ][ 64 ];
	int num_generic_names;

	char* lightstyles[ MAX_LIGHTSTYLES ];
	
	int num_edicts;
	int max_edicts;
	edict_t* edicts;
	
	entity_state_s* baselines;
	extra_baselines_s* instance_baselines;

	server_state_t state;

	sizebuf_t datagram;
	byte datagram_buf[ MAX_DATAGRAM ];

	sizebuf_t reliable_datagram;
	byte reliable_datagram_buf[ MAX_DATAGRAM ];

	sizebuf_t multicast;
	byte multicast_buf[ 1024 ];

	sizebuf_t spectator;
	byte spectator_buf[ 1024 ];

	sizebuf_t signon;
	byte signon_data[ 32768 ];
	
};

struct client_frame_t
{
	// received from client

	// reply
	double senttime;
	float ping_time;
	clientdata_t clientdata;
	weapon_data_t weapondata[ 64 ];
	packet_entities_t entities;
};

typedef struct client_s
{
	qboolean active;						// false = client is free
	qboolean spawned;						// false = don't send datagrams
	qboolean fully_connected;				// true = client has fully connected, set after sendents command is received
	qboolean connected;						// Has been assigned to a client_t, but not in game yet
	qboolean uploading;						// true = client uploading custom resources
	qboolean hasusrmsgs;					// Whether this client has received the list of user messages
	qboolean has_force_unmodified;			// true = mp_consistency is set and at least one file is forced to be consistent

	//===== NETWORK ============

	netchan_t netchan;

	int chokecount;						// amount of choke since last client message
	int delta_sequence;					// -1 = no compression

	qboolean fakeclient;					// Bot
	qboolean proxy;							// HLTV proxy

	usercmd_t lastcmd;					// for filling in big drops and partial predictions

	double connecttime;					// Time at which client connected, this is the time after "spawn" is sent, not initial connection
	double cmdtime;						// Time since connecttime that last usercmd was received
	double ignorecmdtime;				// Time until which usercmds are ignored

	float latency;						// Average latency
	float packet_loss;					// Packet loss suffered by this client

	double localtime;					// of last message
	double nextping;					// next time to recalculate ping for this client
	double svtimebase;					// Server timebase for the client when running movement

	// the datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.
	sizebuf_t datagram;
	byte datagram_buf[ MAX_DATAGRAM ];

	double connection_started;			// or time of disconnect for zombies
	double next_messagetime;			// Earliest time to send another message
	double next_messageinterval;		// Minimum interval between messages

	qboolean send_message;					// set on frames a datagram arived on
	qboolean skip_message;					// Skip sending message next frame

	client_frame_t* frames;				// updates can be deltad from here

	event_state_t events;

	edict_t* edict;						// EDICT_NUM(clientnum+1)
	const edict_t* pViewEntity;			// View entity, equal to edict if not overridden

	int userid;							// identifying number

	USERID_t network_userid;

	char userinfo[ MAX_INFO_STRING ];	// infostring
	qboolean sendinfo;						// at end of frame, send info to all
										// this prevents malicious multiple broadcasts
	float sendinfo_time;				// Time when userinfo was sent

	char hashedcdkey[ 64 ];				// Hashed cd key from user. Really the user's IP address in IPv4 form
	char name[ 32 ];					// for printing to other people
										// extracted from userinfo

	int topcolor;						// top color for model
	int bottomcolor;					// bottom color for model

	int entityId;

	resource_t resourcesonhand;			// Head of resources accounted for list
	resource_t resourcesneeded;			// Head of resources to download list

	FileHandle_t upload;				// Handle of file being uploaded

	qboolean uploaddoneregistering;			// If client files have finished uploading

	customization_t customdata;			// Head of custom client data list

	int crcValue;						// checksum for calcs

	int lw;								// If user is predicting weapons locally (cl_lw)
	int lc;								// If user is lag compensating (cl_lc)

	char physinfo[ MAX_PHYSINFO_STRING ];	//Physics info string

	qboolean m_bLoopback;					// True if client has voice loopback enabled

	uint32 m_VoiceStreams[ 2 ];			// Bit mask for whether client is listening to other client
	double m_lastvoicetime;				// Last time client voice data was processed on server

	int m_sendrescount;					// Count of times resources sent to client
	qboolean m_bSentNewResponse;
} client_t;

/**
*	log messages are used so that fraglog processes can get stats
*/
struct server_log_t
{
	/**
	*	Is the log file active?
	*/
	qboolean active;

	/**
	*	Are we logging to a remote address?
	*/
	qboolean net_log;

	/**
	*	Remote address to log to
	*/
	netadr_t net_address;

	/**
	*	Handle to the log file
	*/
	void* file;
};

struct server_stats_t
{
	/**
	*	Total number of samples taken over server lifetime
	*/
	int num_samples;

	/**
	*	Number of samples where server was filled to capacity (numclients == maxclients)
	*/
	int at_capacity;

	/**
	*	Number of samples where server was empty (numclients <= 1, singleplayer counts as empty)
	*/
	int at_empty;

	/**
	*	Percentage of time that server was at capacity
	*/
	float capacity_percent;

	/**
	*	Percentage of time that server was empty
	*/
	float empty_percent;

	/**
	*	Lowest number of players on server at any time
	*/
	int minusers;

	/**
	*	Highest number of players on server at any time
	*/
	int maxusers;

	/**
	*	Cumulative occupancy level over time
	*/
	float cumulative_occupancy;

	/**
	*	Average occupancy
	*/
	float occupancy;

	/**
	*	Number of client sessions (clients that joined and left, and were on server for more than a minute)
	*/
	int num_sessions;

	/**
	*	Total amount of time spent on server by all clients with recorded session
	*/
	float cumulative_sessiontime;

	/**
	*	Average length of a single client session
	*/
	float average_session_len;

	/**
	*	Cumulation of average latency for all clients per sample
	*/
	float cumulative_latency;

	/**
	*	Average latency for all clients over server lifetime
	*/
	float average_latency;
};

typedef struct server_static_s
{
	/**
	*	Whether the server dll has been loaded and initialized
	*/
	qboolean dll_initialized;

	/**
	*	Array of maxclientslimit clients
	*/
	client_t* clients;

	/**
	*	Maximum number of players on this server as defined by host
	*/
	int maxclients;

	/**
	*	Maximum number of players supported on this server as dictated by memory limits
	*/
	int maxclientslimit;

	/**
	*	number of servers spawned since start,
	*	used to check late spawns
	*/
	int spawncount;

	int serverflags;

	server_log_t log;

	/**
	*	Next time to clear stats
	*/
	double next_cleartime;

	/**
	*	Next time to gather stat samples
	*/
	double next_sampletime;

	/**
	*	Server statistics
	*/
	server_stats_t stats;

	qboolean isSecure;
} server_static_t;

struct startup_timing_t
{
	const char *name;
	float time;
};

//============================================================================

enum
{
	MSG_FL_NONE = 0,		// No flags
	MSG_FL_BROADCAST = 1 << 0,	// Broadcast?
	MSG_FL_PVS = 1 << 1,	// Send to PVS
	MSG_FL_PAS = 1 << 2,	// Send to PAS
	MSG_FL_ONE = 1 << 7,	// Send to single client
};

#define GROUP_OP_AND	0
#define GROUP_OP_NAND	1

extern int g_groupmask;
extern int g_groupop;

extern	server_static_t	svs;	//! persistent server info
extern	server_t		sv;		//! local server

extern cvar_t sv_cheats;

extern cvar_t sv_lan;
extern cvar_t sv_lan_rate;
extern cvar_t sv_aim;

extern cvar_t violence_hblood;
extern cvar_t violence_ablood;
extern cvar_t violence_hgibs;
extern cvar_t violence_agibs;

extern playermove_t g_svmove;

extern DLL_FUNCTIONS gEntityInterface;

extern decalname_t sv_decalnames[MAX_BASE_DECALS];
extern int sv_decalnamecount;

#endif //ENGINE_SERVER_H
