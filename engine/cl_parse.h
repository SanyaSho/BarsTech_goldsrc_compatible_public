#ifndef ENGINE_CL_PARSE_H
#define ENGINE_CL_PARSE_H

#include "cdll_int.h"

extern int CL_UPDATE_BACKUP;
extern int CL_UPDATE_MASK;
extern double g_flLatency;
extern double parsecounttime;

typedef struct svc_func_s
{
	char opcode;
	char *pszname;
	void (*pfnParse)(void);
} svc_func_t;

extern cvar_t cl_allow_download;
extern cvar_t cl_download_ingame;
extern cvar_t cl_weaponlistfix;
extern cvar_t cl_showmessages;
extern cvar_t cl_filterstuffcmd;
extern cvar_t cl_allow_upload;
extern cvar_t cl_clockreset;

extern svc_func_t cl_parsefuncs[SVC_LASTMSG + 2];

void CL_ShutDownUsrMessages();

pfnUserMsgHook HookServerMsg( const char* pszName, pfnUserMsgHook pfn );

void CL_RemoveFromResourceList( resource_t* pResource );
void CL_DumpMessageLoad_f();
void CL_ClientQueueEvent(int flags, int index, int client_index, float delay, event_args_t* pargs);

cl_entity_t *CL_EntityNum(int num);

event_info_t *CL_FindEmptyEvent();

event_info_t *CL_FindUnreliableEvent();

qboolean CL_CheckGameDirectory(char *gamedir);

void CL_ProcessFile(qboolean successfully_received, const char *filename);

void CL_ParseServerMessage(qboolean normal_message = true);

qboolean ValidStuffText(const char* cmd);

void DispatchUserMsg(int iMsg);

int DispatchDirectUserMsg(const char* pszName, int iSize, void* pBuf);

void CL_AdjustClock();

#endif //ENGINE_CL_PARSE_H
