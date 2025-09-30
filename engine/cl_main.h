#ifndef ENGINE_CL_MAIN_H
#define ENGINE_CL_MAIN_H

#include "client.h"
#include "server.h"
#include "cl_entity.h"
#include "dlight.h"

#define MAX_USERPACKET_SIZE	192
#define MAX_USERMESSAGES	256
#define MAX_PACKETACCUM		64

#define CMD_COUNT	32   // Must be power of two
#define CMD_MASK	( CMD_COUNT - 1 )

typedef struct _UserMsg
{
	int iMsg;
	int iSize;
	char szName[16];
	struct _UserMsg *next;
	pfnUserMsgHook pfn;
} UserMsg;

typedef struct oldcmd_s
{
	int command;
	int starting_offset;
	int frame_number;
} oldcmd_t;

struct server_cache_t
{
	int inuse;
	netadr_t adr;
	netadr_t spec_adr;
	char name[256];
	char spec_name[256];
	char map[256];
	char desc[256];
	int players;
	int maxplayers;
	int bots;
	int has_spectator_address;
	char os;
	char type;
	qboolean is_private;
	qboolean is_vac;
};

#define MAX_LOCAL_SERVERS 16

const int MAX_STARTUP_TIMINGS = 32;

extern cl_entity_t* cl_entities;

extern efrag_t cl_efrags[ MAX_EFRAGS ];
extern dlight_t cl_dlights[ MAX_DLIGHTS ];
extern dlight_t cl_elights[ MAX_ELIGHTS ];
extern lightstyle_t cl_lightstyle[ MAX_LIGHTSTYLES ];
extern float g_LastScreenUpdateTime;
extern int g_bRedirectedToProxy;

extern cvar_t cl_lw;
extern cvar_t fs_perf_warnings;
extern cvar_t fs_lazy_precache;
extern cvar_t fs_precache_timings;
extern cvar_t fs_startup_timings;
extern cvar_t cl_gaitestimation;
extern cvar_t cl_himodels;
extern cvar_t rate;
extern cvar_t cl_name;
extern cvar_t cl_needinstanced;
extern cvar_t cl_showfps;
extern cvar_t cl_updaterate;
extern cvar_t cl_solid_players;
extern cvar_t cl_showevents;
extern cvar_t cl_idealpitchscale;
extern cvar_t cl_shownet;
extern cvar_t password;

extern cvar_t cl_fixtimerate;

extern float g_flTime;
extern int g_iFrames;
extern int last_data[MAX_PACKETACCUM];
extern int currentcmd;
extern oldcmd_t oldcmd[32];
extern int msg_buckets[MAX_PACKETACCUM];
extern int total_data[MAX_PACKETACCUM];

extern qboolean cl_inmovie;

extern UserMsg *gClientUserMsgs;

int ClientDLL_AddEntity(int type, cl_entity_s *ent);

void SetupStartupTimings();
void AddStartupTiming( const char* name );
void PrintStartupTimings();

dlight_t* CL_AllocDlight( int key );

dlight_t* CL_AllocElight( int key );

model_t* CL_GetModelByIndex( int index );

void CL_GetPlayerHulls();

BOOL UserIsConnectedOnLoopback();

void SetPal( int i );

void GetPos( vec3_t origin, vec3_t angles );

const char* CL_CleanFileName( const char* filename );

void CL_ClearCaches();

void CL_ClearClientState();

void CL_ClearState( bool bQuiet );

void CL_CreateResourceList();

void CL_RegisterCustomization(resource_t* resource);

void CL_ProcessFile(qboolean successfully_received, const char* filename);
void CL_SendResourceListBlock();
void CL_HudMessage(const char *pMessage);
void CL_DeallocateDynamicData();
void CL_ReallocateDynamicData(int clients);
void CL_WriteMessageHistory(int starting_count, int cmd);
void CL_PlayerDropped(int nPlayerNumber);
cl_entity_t* CL_EntityNum(int num);
void AddNewUserMsg();
void DispatchUserMsg(int iMsg);
int DispatchDirectUserMsg(const char* pszName, int iSize, void* pBuf);
void CL_Parse_Sound();
void CL_UpdateUserinfo(void);
void CL_ClearResourceLists();
qboolean CL_CheckCRCs(char* pszMap);
void CL_ParseResourceLocation();
customization_t* CL_PlayerHasCustomization(int nPlayerNum, resourcetype_t type);
void CL_RemoveCustomization(int nPlayerNum, customization_t* pRemove);
void CL_ParseCustomization();
void CL_CreateCustomizationList();
qboolean CL_CheckGameDirectory(char* gamedir);

void CL_SignonReply(void);
void CL_ParseResourceList();
void CL_AddToResourceList(resource_t *pResource, resource_t *pList);
void CL_StartResourceDownloading(char *pszMessage, qboolean bCustom);
qboolean CL_PrecacheResources();
void CL_RegisterResources(sizebuf_t *msg);
void CL_MoveToOnHandList(resource_t *pResource);

void CL_TouchLight(dlight_t *dl);
float	CL_LerpPoint(void);
void CL_CheckClientState();
qboolean CL_RequestMissingResources();
void CL_Move();
void CL_CheckForResend();
void CL_UpdateSoundFade();
void CL_VoiceIdle();

void CL_StopPlayback();
int DispatchDirectUserMsg(const char* pszName, int iSize, void* pBuf);
void CL_Stop_f();
void CL_HudMessage(const char* pMessage);
void CL_AddVoiceToDatagram(qboolean bFinal);
void CL_ReadPackets();
int CL_GetServerCount();
model_t* CL_GetWorldModel();

#endif //ENGINE_CL_MAIN_H
