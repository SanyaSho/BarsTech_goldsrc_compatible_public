#ifndef ENGINE_CL_PARSEFN_H
#define ENGINE_CL_PARSEFN_H

#include "event_args.h"
#include "progs.h"

typedef struct event_hook_s
{
	event_hook_s *next;
	char *name;
	void(*pfnEvent)(event_args_s *);
} event_hook_t;

extern event_hook_t *g_pEventHooks;

void CL_InitEventSystem();

void CL_HookEvent( char* name, void( *pfnEvent )( event_args_t* ) );

void CL_QueueEvent( int flags, int index, float delay, event_args_t* pargs );

void CL_ResetEvent( event_info_t* ei );

event_hook_t *CL_FindEventHook(char *name);

void CL_DescribeEvent(int slot, char *eventname);

void CL_FireEvents();

void CL_FlushEventSystem();
void CL_ParseFileTxferFailed();
void CL_Send_CvarValue();
void CL_Send_CvarValue2();
void CL_Exec();

#endif //ENGINE_CL_PARSEFN_H
