#ifndef ENGINE_PR_EDICT_H
#define ENGINE_PR_EDICT_H

#include "const.h"

#define SF_NOTINDEATHMATCH (1<<11)

void ReleaseEntityDLLFields( edict_t* pEdict );
void InitEntityDLLFields(edict_t *pEdict);
void ED_LoadFromFile(char *data);
int IndexOfEdict(const edict_t *pEdict);
void FreeAllEntPrivateData(void);
void SaveSpawnParms(edict_t* pEdict);
void CVarRegister(cvar_t* pCvar);
void CVarSetString(const char* szVarName, const char* szValue);
void CVarSetFloat(const char* szVarName, float flValue);
cvar_t* CVarGetPointer(const char* szVarName);
const char* CVarGetString(const char* szVarName);
float CVarGetFloat(const char* szVarName);
void* PvAllocEntPrivateData(edict_t* pEdict, int32 cb);
void* PvEntPrivateData(edict_t* pEdict);
void FreeEntPrivateData(edict_t* pEdict);
const char* SzFromIndex(int iString);
int AllocEngineString(const char* szValue);
entvars_t* GetVarsOfEnt(edict_t* pEdict);
edict_t* PEntityOfEntOffset(int iEntOffset);
int EntOffsetOfPEntity(const edict_t* pEdict);
edict_t* FindEntityByVars(entvars_t* pvars);
edict_t* PF_CreateFakeClient_I(const char* netname);


#endif //ENGINE_PR_EDICT_H
