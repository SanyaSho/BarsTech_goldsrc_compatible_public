#include "quakedef.h"
#include "pr_edict.h"
#include "progs.h"
#include "server.h"
#include "host.h"
#include "world.h"

void ReleaseEntityDLLFields(edict_t* pEdict)
{
	FreeEntPrivateData(pEdict);
}

edict_t *EDICT_NUM(int n)
{
	if (n < 0 || n >= sv.max_edicts)
	{
		Sys_Error(__FUNCTION__ ": bad number %i", n);
	}
	if (n == 1)
		OutputDebugString("");
	return &sv.edicts[n];
}

int NUM_FOR_EDICT(const edict_t *e)
{
	int b;
	b = e - sv.edicts;

	if (b < 0 || b >= sv.num_edicts)
	{
		Sys_Error(__FUNCTION__ ": bad pointer");
	}

	return b;
}

void InitEntityDLLFields(edict_t *pEdict)
{
	pEdict->v.pContainingEntity = pEdict;
}

void ED_ClearEdict(edict_t *e)
{
	Q_memset(&e->v, 0, sizeof(e->v));
	e->free = FALSE;
	ReleaseEntityDLLFields(e);
	InitEntityDLLFields(e);
}

void ED_Count(void)
{
	int i;
	edict_t *ent;
	int active = 0, models = 0, solid = 0, step = 0;

	for (i = 0; i < sv.num_edicts; i++)
	{
		ent = &sv.edicts[i];
		if (!ent->free)
		{
			++active;
			models += (ent->v.model) ? 1 : 0;
			solid += (ent->v.solid) ? 1 : 0;
			step += (ent->v.movetype == MOVETYPE_STEP) ? 1 : 0;
		}
	}

	Con_Printf(const_cast<char*>("num_edicts:%3i\n"), sv.num_edicts);
	Con_Printf(const_cast<char*>("active    :%3i\n"), active);
	Con_Printf(const_cast<char*>("view      :%3i\n"), models);
	Con_Printf(const_cast<char*>("touch     :%3i\n"), solid);
	Con_Printf(const_cast<char*>("step      :%3i\n"), step);
}

char *ED_NewString(const char *string)
{
	char *new_s;

	int l = Q_strlen(string);
	new_s = (char *)Hunk_Alloc(l + 1);
	char* new_p = new_s;

	for (int i = 0; i < l; i++, new_p++)
	{
		if (string[i] == '\\')
		{
			if (string[i + 1] == 'n')
				*new_p = '\n';
			else
				*new_p = '\\';
			i++;
		}
		else
		{
			*new_p = string[i];
		}
	}
	*new_p = 0;

	return new_s;
}

// высосать имя класса!
bool SuckOutClassname(char *szInputStream, edict_t *pEdict)
{
	char szKeyName[256];
	KeyValueData kvd;

	// key
	szInputStream = COM_Parse(szInputStream);
	while (szInputStream && com_token[0] != '}')
	{
		Q_strncpy(szKeyName, com_token, ARRAYSIZE(szKeyName) - 1);
		szKeyName[ARRAYSIZE(szKeyName) - 1] = 0;

		// value
		szInputStream = COM_Parse(szInputStream);

		if (!Q_strcmp(szKeyName, "classname"))
		{
			kvd.szClassName = NULL;
			kvd.szKeyName = szKeyName;
			kvd.szValue = com_token;
			kvd.fHandled = FALSE;

			gEntityInterface.pfnKeyValue(pEdict, &kvd);

			if (kvd.fHandled == FALSE)
			{
				Host_Error(__FUNCTION__ ": parse error");
			}

			return true;
		}

		if (!szInputStream)
		{
			break;
		}

		// next key
		szInputStream = COM_Parse(szInputStream);
	}

	// classname not found
	return false;
}

char *ED_ParseEdict(char *data, edict_t *ent)
{
	qboolean init = FALSE;
	char keyname[256];
	int n;
	ENTITYINIT pEntityInit;
	char *className;
	KeyValueData kvd;

	if (ent != sv.edicts)
		Q_memset(&ent->v, 0, sizeof(ent->v));

	InitEntityDLLFields(ent);

	if (SuckOutClassname(data, ent))
	{
		className = (char *)(pr_strings + ent->v.classname);

		pEntityInit = GetEntityInit(className);
		if (pEntityInit)
		{
			pEntityInit(&ent->v);
			init = TRUE;
		}
		else
		{
			pEntityInit = GetEntityInit("custom");
			if (pEntityInit)
			{
				pEntityInit(&ent->v);
				kvd.szClassName = const_cast<char*>("custom");
				kvd.szKeyName = const_cast<char*>("customclass");
				kvd.szValue = className;
				kvd.fHandled = FALSE;
				gEntityInterface.pfnKeyValue(ent, &kvd);
				init = TRUE;
			}
			else
			{
				Con_DPrintf(const_cast<char*>("Can't init %s\n"), className);
				init = FALSE;
			}
		}

		while (true)
		{
			data = COM_Parse(data);

			if (com_token[0] == '}')
				break;

			if (!data)
				Host_Error(__FUNCTION__ ": EOF without closing brace");

			Q_strncpy(keyname, com_token, ARRAYSIZE(keyname) - 1);
			keyname[ARRAYSIZE(keyname) - 1] = 0;
			// Remove tail spaces
			for (n = Q_strlen(keyname) - 1; n >= 0 && keyname[n] == ' '; n--)
				keyname[n] = 0;

			data = COM_Parse(data);
			if (!data)
			{
				Host_Error(__FUNCTION__ ": EOF without closing brace");
			}
			if (com_token[0] == '}')
			{
				Host_Error(__FUNCTION__ ": closing brace without data");
			}

			if (className != NULL && !Q_strcmp(className, com_token))
				continue;

			if (!Q_strcmp(keyname, "angle"))
			{
				float value = (float)Q_atof(com_token);
				if (value >= 0.0)
				{
					_snprintf(com_token, sizeof(com_token), "%f %f %f", ent->v.angles[0], value, ent->v.angles[2]);
				}
				else if ((int)value == -1)
				{
					_snprintf(com_token, sizeof(com_token), "-90 0 0");
				}
				else
				{
					_snprintf(com_token, sizeof(com_token), "90 0 0");
				}

				Q_strcpy(keyname, "angles");
			}

			kvd.szClassName = className;
			kvd.szKeyName = keyname;
			kvd.szValue = com_token;
			kvd.fHandled = FALSE;
			gEntityInterface.pfnKeyValue(ent, &kvd);
		}
	}

	if (!init)
	{
		ent->free = 1;
		ent->serialnumber++;
	}
	return data;
}

void FreeEntPrivateData(edict_t* pEdict)
{
	int iEdict = pEdict - sv.edicts;
	Con_Printf((char*)"trying to free pv of edict %d\n", iEdict);
	if (pEdict->pvPrivateData)
	{
		if (gNewDLLFunctions.pfnOnFreeEntPrivateData)
		{
			gNewDLLFunctions.pfnOnFreeEntPrivateData(pEdict);
		}

		Mem_Free(pEdict->pvPrivateData);
		pEdict->pvPrivateData = 0;
	}
}

void* PvAllocEntPrivateData(edict_t *pEdict, int32 cb)
{
	FreeEntPrivateData(pEdict);

	int iEdict = pEdict - sv.edicts;
	Con_Printf((char*)"trying to allocate pv (%d bytes) of edict %d\n", cb, iEdict);
	if (cb <= 0)
	{
		return NULL;
	}

	pEdict->pvPrivateData = Mem_Calloc(1, cb);

	return pEdict->pvPrivateData;
}

void* PvEntPrivateData(edict_t *pEdict)
{
	if (!pEdict)
	{
		return NULL;
	}

	return pEdict->pvPrivateData;
}

void FreeAllEntPrivateData(void)
{
	for (int i = 0; i < sv.num_edicts; i++)
	{
		FreeEntPrivateData(&sv.edicts[i]);
	}
}

edict_t* PEntityOfEntOffset(int iEntOffset)
{
	return PROG_TO_EDICT(iEntOffset);
}

int EntOffsetOfPEntity(const edict_t *pEdict)
{
	return (char *)pEdict - (char *)sv.edicts;
}

int IndexOfEdict(const edict_t *pEdict)
{
	int index = 0;
	if (pEdict)
	{
		index = pEdict - sv.edicts;
		if (index < 0 || index > sv.max_edicts)
		{
			Sys_Error("Bad entity in " __FUNCTION__ "()");
		}
	}
	return index;
}

edict_t* PEntityOfEntIndex(int iEntIndex)
{
	if (iEntIndex < 0 || iEntIndex >= sv.max_edicts)
	{
		return NULL;
	}

	edict_t *pEdict = EDICT_NUM(iEntIndex);

	if ((!pEdict || pEdict->free || !pEdict->pvPrivateData) && (iEntIndex >= svs.maxclients || pEdict->free))
	{
		if (iEntIndex >= svs.maxclients || pEdict->free)
		{
			pEdict = NULL;
		}
	}

	return pEdict;
}

const char* SzFromIndex(int iString)
{
	return (const char *)(pr_strings + iString);
}

entvars_t* GetVarsOfEnt(edict_t *pEdict)
{
	return &pEdict->v;
}

edict_t* FindEntityByVars(entvars_t *pvars)
{
	for (int i = 0; i < sv.num_edicts; i++)
	{
		edict_t *pEdict = &sv.edicts[i];
		if (&pEdict->v == pvars)
		{
			return pEdict;
		}
	}
	return NULL;
}

float CVarGetFloat(const char *szVarName)
{
	return Cvar_VariableValue(const_cast<char*>(szVarName));
}

const char* CVarGetString(const char *szVarName)
{
	return Cvar_VariableString(const_cast<char*>(szVarName));
}

cvar_t* CVarGetPointer(const char *szVarName)
{
	return Cvar_FindVar(szVarName);
}

void CVarSetFloat(const char *szVarName, float flValue)
{
	Cvar_SetValue(const_cast<char*>(szVarName), flValue);
}

void CVarSetString(const char *szVarName, const char *szValue)
{
	Cvar_Set(const_cast<char*>(szVarName), const_cast<char*>(szValue));
}

void CVarRegister(cvar_t *pCvar)
{
	if (pCvar)
	{
		pCvar->flags |= FCVAR_EXTDLL;
		Cvar_RegisterVariable(pCvar);
	}
}

int AllocEngineString(const char *szValue)
{
	return ED_NewString(szValue) - pr_strings;
}

void SaveSpawnParms(edict_t *pEdict)
{
	int eoffset = NUM_FOR_EDICT(pEdict);
	if (eoffset < 1 || eoffset > svs.maxclients)
	{
		Host_Error("Entity is not a client");
	}
	// Nothing more for this function even on client-side
}

void* GetModelPtr(edict_t *pEdict)
{
	if (!pEdict)
	{
		return NULL;
	}

	return Mod_Extradata(sv.models[pEdict->v.modelindex]);
}

void ED_Free(edict_t *ed)
{
	if (!ed->free)
	{
		int iIndex = ed - sv.edicts;
		if (iIndex <= 1)
			OutputDebugString("");
		SV_UnlinkEdict(ed);
		FreeEntPrivateData(ed);
		ed->serialnumber++;
		ed->freetime = (float)sv.time;
		ed->free = TRUE;
		ed->v.flags = 0;
		ed->v.model = 0;

		ed->v.takedamage = 0;
		ed->v.modelindex = 0;
		ed->v.colormap = 0;
		ed->v.skin = 0;
		ed->v.frame = 0;
		ed->v.scale = 0;
		ed->v.gravity = 0;
		ed->v.nextthink = -1.0;
		ed->v.solid = SOLID_NOT;

		VectorCopy(vec3_origin, ed->v.origin);
		VectorCopy(vec3_origin, ed->v.angles);
	}
}

void ED_LoadFromFile(char *data)
{
	edict_t *ent;
	int inhibit;

	gGlobalVariables.time = (float)sv.time;

	ent = NULL;
	inhibit = 0;
	while (true)
	{
		data = COM_Parse(data);
		if (!data)
		{
			break;
		}
		if (com_token[0] != '{')
		{
			Host_Error(__FUNCTION__ ": found %s when expecting {", com_token);
		}

		if (ent)
		{
			ent = ED_Alloc();
		}
		else
		{
			ent = sv.edicts;
			ReleaseEntityDLLFields(sv.edicts);	
			InitEntityDLLFields(ent);
		}

		data = ED_ParseEdict(data, ent);

		int iEdict = ent - sv.edicts;
		Con_Printf((char*)"processing edict %d\n", iEdict);

		if (ent->free)
		{
			continue;
		}

		if (deathmatch.value != 0.0 && (ent->v.spawnflags & SF_NOTINDEATHMATCH))
		{
			ED_Free(ent);
			++inhibit;
		}
		else
		{
			if (ent->v.classname)
			{
				if (gEntityInterface.pfnSpawn(ent) < 0 || (ent->v.flags & FL_KILLME))
				{
					ED_Free(ent);
				}
			}
			else
			{
				Con_Printf(const_cast<char*>("No classname for:\n"));
				ED_Free(ent);
			}
		}
	}
	Con_DPrintf(const_cast<char*>("%i entities inhibited\n"), inhibit);
}

edict_t* ED_Alloc(void)
{
	int i;
	edict_t* e;

	// Search for free entity
	for (i = svs.maxclients + 1; i < sv.num_edicts; i++)
	{
		e = &sv.edicts[i];
		if (e->free && (e->freetime <= 2.0 || sv.time - e->freetime >= 0.5))
		{
			ED_ClearEdict(e);
			return e;
		}
	}

	// Check if we are out of free edicts
	if (i >= sv.max_edicts)
	{
		if (!sv.max_edicts)
		{
			Sys_Error(__FUNCTION__ ": no edicts yet");
		}
		Sys_Error(__FUNCTION__ ": no free edicts");
	}

	// Use new one
	++sv.num_edicts;
	e = &sv.edicts[i];

	ED_ClearEdict(e);
	return e;
}
