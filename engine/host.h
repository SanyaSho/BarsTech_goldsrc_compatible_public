#ifndef ENGINE_HOST_H
#define ENGINE_HOST_H

#include <csetjmp>

#include <tier0/platform.h>

#include "server.h"
#include "GameUI/CareerDefs.h"

extern jmp_buf host_abortserver;

extern cvar_t developer;
extern cvar_t sys_timescale;
extern cvar_t host_framerate;
extern cvar_t deathmatch, coop;

extern double host_frametime;
extern int host_framecount;

extern client_t* host_client;

extern cvar_t host_limitlocal;
extern cvar_t skill;
extern cvar_t pausable;

extern cvar_t console;

extern unsigned short* host_basepal;
extern unsigned char* host_colormap;

extern CareerStateType g_careerState;
extern qboolean s_careerAudioPaused;

extern qboolean g_bUsingInGameAdvertisements;
extern qboolean gfNoMasterServer;

extern vec3_t r_playerViewportAngles, r_soundOrigin;

extern int32 startTime;
extern double cpuPercent;

int Host_Frame( float time, int iState, int* stateInfo );

bool Host_IsServerActive();

bool Host_IsSinglePlayerGame();

void Host_GetHostInfo( float* fps, int* nActive, int* unused, int* nMaxPlayers, char* pszMap );

void SV_DropClient( client_t* cl, qboolean crash, char* fmt, ... );

void SV_ClearResourceLists( client_t* cl );

void Host_CheckDyanmicStructures();

void SV_ClearClientStates();

void Host_ClearMemory( bool bQuiet );

void Host_ShutdownServer(qboolean crash);

int Host_MaxClients();

void Host_EndGame(char *message, ...);

int32 Host_GetStartTime();
void GetStatsString(char *buf, int bufSize);
void Host_Stats_f();
char *Host_SaveGameDirectory();
qboolean Master_IsLanGame(void);
void SV_BroadcastPrintf(const char *fmt, ...);
void SV_ClientPrintf(const char *fmt, ...);
int Host_GetMaxClients(void);
void SCR_DrawFPS(void);
void Host_ClientCommands(const char* fmt, ...);
void Host_EndSection(const char* pszSection);
void Host_WriteCustomConfig(void);

#endif //ENGINE_HOST_H
