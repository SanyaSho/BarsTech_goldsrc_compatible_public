#ifndef ENGINE_SV_USER_H
#define ENGINE_SV_USER_H

typedef struct sv_adjusted_positions_s
{
	int active;
	int needrelink;
	vec3_t neworg;
	vec3_t oldorg;
	vec3_t initial_correction_org;
	vec3_t oldabsmin;
	vec3_t oldabsmax;
	int deadflag;
	vec3_t temp_org;
	int temp_org_setflag;
} sv_adjusted_positions_t;

typedef struct clc_func_s
{
	unsigned char opcode;
	char *pszname;
	void(*pfnParse)(client_t *);
} clc_func_t;

extern cvar_t sv_edgefriction, sv_maxspeed, sv_accelerate, sv_footsteps, sv_rollspeed, sv_rollangle, sv_unlag, sv_maxunlag, sv_unlagpush, sv_unlagsamples, mp_consistency, sv_voiceenable;
extern edict_t *sv_player;
void SV_SendConsistencyList(sizebuf_t *msg);

extern ipfilter_t ipfilters[MAX_IPFILTERS];
extern int numipfilters;
extern userfilter_t userfilters[MAX_USERFILTERS];
extern int numuserfilters;

void SV_ExecuteClientMessage(client_t *cl);
qboolean SV_FileInConsistencyList(const char *filename, consistency_t **ppconsist);
int SV_TransferConsistencyInfo(void);
void SV_FullUpdate_f(void);
void SV_SendEnts_f(void);
void SV_ShowServerinfo_f(void);
void SV_RunCmd(usercmd_t* ucmd, int random_seed);
void SV_PreRunCmd(void);

#endif