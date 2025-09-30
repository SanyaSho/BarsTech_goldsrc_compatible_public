#include "quakedef.h"
#include "client.h"
#include "cl_parse.h"
#include "cl_parsefn.h"
#include "cl_main.h"

event_hook_t *g_pEventHooks;

event_hook_t* CL_FindEventHook(char* name)
{
	event_hook_t* hook;

	if (!name || !g_pEventHooks)
		return NULL;

	hook = g_pEventHooks;
	do
	{
		if (hook->name != NULL && !Q_strcasecmp(name, hook->name))
			return hook;
	} while ((hook = hook->next) != NULL);

	return NULL;
}

void CL_HookEvent( char* name, void( *pfnEvent )( event_args_t* ) )
{
	event_hook_t *hook;

	RecEngCL_HookEvent(name, pfnEvent);

	if (!name || !name[0])
		return Con_Printf(const_cast<char*>(__FUNCTION__ ":  Must provide a valid event name\n"));

	if (!pfnEvent)
		return Con_Printf(const_cast<char*>(__FUNCTION__ ":  Must provide an event hook callback\n"));

	hook = CL_FindEventHook(name);

	if (hook)
	{
		Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  Called on existing hook, updating event hook\n"));
		hook->pfnEvent = pfnEvent;
	}
	else
	{
		hook = (event_hook_t*)Mem_ZeroMalloc(sizeof(event_hook_t));
		hook->name = Mem_Strdup(name);
		hook->next = g_pEventHooks;
		hook->pfnEvent = pfnEvent;
		g_pEventHooks = hook;
	}
}

void CL_InitEventSystem()
{
	g_pEventHooks = NULL;
}

void CL_FlushEventSystem()
{
	event_hook_t* ev, *next;

	if (!g_pEventHooks)
		return;

	ev = g_pEventHooks;
	while (true)
	{
		next = ev->next;
		Mem_Free(ev->name);
		Mem_Free(ev);
		if (!next)
			break;
		ev = next;
	}

	g_pEventHooks = NULL;
}

void CL_ParseFileTxferFailed()
{
	char* name;

	name = MSG_ReadString();

	if (name == NULL || !name[0])
		return Host_Error("CL_ParseFileTxferFailed:  empty filename\n");

	if (cls.demoplayback == false)
		CL_ProcessFile(false, name);
}

void CL_Send_CvarValue()
{
	char *name; 
	cvar_t *cvar; 
	const char* strFailureMessage = NULL;

	name = MSG_ReadString();

	MSG_WriteByte(&cls.netchan.message, clc_cvarvalue);

	if (strlen(name) < 255 && (cvar = Cvar_FindVar(name)) != NULL)
	{
		if (cvar->flags & FCVAR_PRIVILEGED)
			strFailureMessage = "CVAR is privileged";
		if (cvar->flags & FCVAR_SERVER)
			strFailureMessage = "CVAR is server-only";
		if (cvar->flags & FCVAR_PROTECTED)
			strFailureMessage = "CVAR is is protected";

		if (strFailureMessage != NULL)
			return MSG_WriteString(&cls.netchan.message, strFailureMessage);

		MSG_WriteString(&cls.netchan.message, cvar->string);
	}
	else
		MSG_WriteString(&cls.netchan.message, "Bad CVAR request");
}

void CL_Send_CvarValue2()
{
	char *name;
	cvar_t *cvar;
	int requestID;
	const char* strFailureMessage = NULL;

	requestID = MSG_ReadLong();
	name = MSG_ReadString();
	
	MSG_WriteByte(&cls.netchan.message, clc_cvarvalue2);
	MSG_WriteLong(&cls.netchan.message, requestID);
	MSG_WriteString(&cls.netchan.message, name);

	if (strlen(name) < 255 && (cvar = Cvar_FindVar(name)) != NULL)
	{
		if (cvar->flags & FCVAR_PRIVILEGED)
			strFailureMessage = "CVAR is privileged";
		if (cvar->flags & FCVAR_SERVER)
			strFailureMessage = "CVAR is server-only";
		if (cvar->flags & FCVAR_PROTECTED)
			strFailureMessage = "CVAR is is protected";

		if (strFailureMessage != NULL)
			return MSG_WriteString(&cls.netchan.message, strFailureMessage);

		MSG_WriteString(&cls.netchan.message, cvar->string);
	}
	else
		MSG_WriteString(&cls.netchan.message, "Bad CVAR request");
}

//       tfc`            
void CL_Exec()
{
	char pchMapName[4096], pchExecBuf[1024];
	int nExecType, nClassNum;
	static char *sClassCfgs[] = {
		const_cast<char*>(""),
		const_cast<char*>("exec scout.cfg\n"),
		const_cast<char*>("exec sniper.cfg\n"),
		const_cast<char*>("exec soldier.cfg\n"),
		const_cast<char*>("exec demoman.cfg\n"),
		const_cast<char*>("exec medic.cfg\n"),
		const_cast<char*>("exec hwguy.cfg\n"),
		const_cast<char*>("exec pyro.cfg\n"),
		const_cast<char*>("exec spy.cfg\n"),
		const_cast<char*>("exec engineer.cfg\n"),
		const_cast<char*>(""),
		const_cast<char*>("exec civilian.cfg\n")
	};

	nExecType = MSG_ReadByte();

	if (nExecType && nExecType == 1)
	{
		nClassNum = MSG_ReadByte();
		if (nClassNum >= 0 && g_bIsTFC && nClassNum < 12)
			Cbuf_AddText(sClassCfgs[nClassNum]);
	}
	else if (g_bIsTFC)
	{
		Cbuf_AddText(const_cast<char*>("exec mapdefault.cfg\n"));
		if (Q_strlen(cl.levelname) < sizeof(pchMapName) - 1)
		{
			COM_FileBase(cl.levelname, pchMapName);
			if (pchMapName[0])
			{
				_snprintf(pchExecBuf, sizeof(pchExecBuf), "exec %s.cfg\n", pchMapName);
				Cbuf_AddText(pchExecBuf);
			}
		}
	}
}
