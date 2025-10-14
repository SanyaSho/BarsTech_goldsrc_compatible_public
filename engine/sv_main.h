#ifndef ENGINE_SV_MAIN_H
#define ENGINE_SV_MAIN_H

#include "quakedef.h"
#include "net_ws.h"
#include "steam/steam_api.h"
#include "server.h"
#include "delta.h"
#include "cl_main.h"

const int MAX_RCON_FAILURES_STORAGE = 32;
const int MAX_RCON_FAILURES = 20;

const AppId_t HALF_LIFE_APPID = 70;

typedef struct rcon_failure_s
{
	qboolean active;
	qboolean shouldreject;
	netadr_t adr;
	int num_failures;
	float last_update;
	float failure_times[MAX_RCON_FAILURES];
} rcon_failure_t;

typedef struct entcount_s
{
	int entdata;
	int playerdata;
} entcount_t;

typedef enum sv_delta_s
{
	sv_packet_nodelta,
	sv_packet_delta,
} sv_delta_t;

struct client_frame_t;

extern cvar_t sv_allow_upload;

extern edict_t* sv_player;

extern cvar_t mapcyclefile;
extern cvar_t servercfgfile;

extern cvar_t max_queries_sec;
extern cvar_t max_queries_sec_global;
extern cvar_t max_queries_window;
extern cvar_t sv_clienttrace;
extern cvar_t sv_max_upload;

extern int g_userid;

extern cvar_t hpk_maxsize;
extern cvar_t sv_password;
extern cvar_t sv_visiblemaxplayers;
extern cvar_t rcon_password;

extern cvar_t sv_newunit;
extern cvar_t motdfile;

extern cvar_t sv_skyname;
extern cvar_t sv_skycolor_r;
extern cvar_t sv_skycolor_g;
extern cvar_t sv_skycolor_b;
extern cvar_t sv_skyvec_x;
extern cvar_t sv_skyvec_y;
extern cvar_t sv_skyvec_z;

int GetGameAppID(void);

qboolean IsGameSubscribed( const char *game );
qboolean BIsValveGame();

extern bool g_bIsCStrike;
extern bool g_bIsCZero;
extern bool g_bIsCZeroRitual;
extern bool g_bIsTerrorStrike;
extern bool g_bIsTFC;

extern delta_t *g_peventdelta;

extern UserMsg* sv_gpNewUserMsgs;
extern UserMsg* sv_gpUserMsgs;

extern char localinfo[MAX_LOCALINFO + 1];

void SetCStrikeFlags();

void SV_DeallocateDynamicData();

void SV_AllocClientFrames();

void SV_ClearFrames( client_frame_t** frames );

void SV_ServerShutdown();

void SV_Init();

void SV_Shutdown();

void SV_SetMaxclients();

void SV_CountPlayers( int* clients );

void SV_KickPlayer( int nPlayerSlot, int nReason );

void SV_ClearEntities();

void SV_ClearCaches();

void SV_PropagateCustomizations();

char* SV_GetIDString(USERID_t *id);
qboolean SV_FilterUser(USERID_t *userid);
void SV_RejectConnection(netadr_t *adr, char *fmt, ...);
char *SV_GetClientIDString(client_t *client);
int SV_CheckForDuplicateSteamID(client_t *client);
void SV_FullClientUpdate(client_t *cl, sizebuf_t *sb);
delta_t *SV_LookupDelta(char *name);
void SV_StartSound(int recipients, edict_t *entity, int channel, const char *sample, int volume, float attenuation, int fFlags, int pitch);
int SV_PointLeafnum(vec_t *p);
qboolean SV_ValidClientMulticast(client_t *client, int soundLeaf, int to);
int SV_ModelIndex(const char *name);
void SV_StartParticle(const vec_t *org, const vec_t *dir, int color, int count);
qboolean IsSafeFileToDownload(const char *filename);
void SV_FlushRedirect(void);
void SV_Frame();
void SV_CheckForRcon(void);
void SV_InitEncoders(void);
void SV_BroadcastCommand(char *fmt, ...);
void SV_SkipUpdates(void);
void SV_Multicast(edict_t* ent, vec_t* origin, int to, qboolean reliable);
int RegUserMsg(const char* pszName, int iSize);
void SV_ExtractFromUserinfo(client_t* cl);
qboolean SV_BuildSoundMsg(edict_t* entity, int channel, const char* sample, int volume, float attenuation, int fFlags, int pitch, const float* origin, sizebuf_t* buffer);
unsigned char* SV_FatPVS(float* org);
unsigned char* SV_FatPAS(float* org);
int SV_CheckVisibility(const edict_t* entity, unsigned char* pset);
int SV_CalcPing(client_t* cl);
void SV_LoadEntities(void);
void SV_ActivateServer(int runPhysics);
int SV_SpawnServer(qboolean bIsDemo, char* server, char* startspot);
void SV_InactivateClients(void);
qboolean SV_IsPlayerIndex(int index);
void Host_Kick_f(void);
void SV_ClearPacketEntities(client_frame_t* frame);
int SV_GetFragmentSize(void* state);
int SV_LookupSoundIndex(const char* sample);
void SV_BuildHashedSoundLookupTable(void);
void SV_WriteVoiceCodec(sizebuf_t* pBuf);
qboolean SV_CompareUserID(USERID_t* id1, USERID_t* id2);
void SV_AddSampleToHashedLookupTable(const char* pszSample, int iSampleIndex);

#endif //ENGINE_SV_MAIN_H
