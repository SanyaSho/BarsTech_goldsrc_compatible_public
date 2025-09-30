#include "quakedef.h"
#include "pmove.h"
#include "pm_defs.h"
#include "pmovetst.h"
#include "pr_cmds.h"
#include "tmessage.h"
#include "cl_ents.h"

vec3_t player_mins[ MAX_MAP_HULLS ] = {
	{ -16.0f, -16.0f, -36.0f, },
	{ -16.0f, -16.0f, -18.0f, },
	{ 0.0f, 0.0f, 0.0f, },
	{ -32.0f, -32.0f, -32.0f, }
};

vec3_t player_maxs[ MAX_MAP_HULLS ] = {
	{ 16.0f, 16.0f, 36.0f, },
	{ 16.0f, 16.0f, 18.0f, },
	{ 0.0f, 0.0f, 0.0f, },
	{ 32.0f, 32.0f, 32.0f, }
};

const int PM_boxpnt[6][4] = 
{
	{0, 4, 6, 2},
	{0, 1, 5, 4},
	{0, 2, 3, 1},
	{7, 5, 1, 3},
	{7, 3, 2, 6},
	{7, 6, 4, 5}
};

cvar_t pm_showclip = { const_cast<char*>("pm_showclip"), const_cast<char*>("0") };

playermove_t* pmove = nullptr;

movevars_t movevars = {};

qboolean PM_AddToTouched(pmtrace_t tr, vec_t *impactvelocity)
{
	int i;
	for (i = 0; i < pmove->numtouch; i++)
	{
		if (pmove->touchindex[i].ent == tr.ent)
		{
			return FALSE;
		}
	}

	if (pmove->numtouch >= MAX_PHYSENTS)
	{
		pmove->Con_DPrintf(const_cast<char*>("Too many entities were touched!\n"));
	}

	VectorCopy(impactvelocity, tr.deltavelocity);
	pmove->touchindex[pmove->numtouch++] = tr;

	return TRUE;
}

void PM_StuckTouch(int hitent, pmtrace_t *ptraceresult)
{
	if (pmove->server)
	{
		int n = pmove->physents[hitent].info;
		edict_t *info = EDICT_NUM(n);	// looks like just entity index check
		PM_AddToTouched(*ptraceresult, pmove->velocity);
	}
}

void PM_Init( playermove_t* ppm )
{
	PM_InitBoxHull();
	for (int i = 0; i < MAX_MAP_HULLS; i++)
	{
		VectorCopy(player_mins[i], ppm->player_mins[i]);
		VectorCopy(player_maxs[i], ppm->player_maxs[i]);
	}

	ppm->movevars = &movevars;

	ppm->PM_Info_ValueForKey = Info_ValueForKey;
	ppm->PM_Particle = CL_Particle;
	ppm->PM_TestPlayerPosition = PM_TestPlayerPosition;
	ppm->Con_NPrintf = Con_NPrintf;
	ppm->Con_DPrintf = Con_DPrintf;
	ppm->Con_Printf = Con_Printf;
	ppm->Sys_FloatTime = Sys_FloatTime;
	ppm->PM_StuckTouch = PM_StuckTouch;
	ppm->PM_PointContents = PM_PointContents;
	ppm->PM_TruePointContents = PM_TruePointContents;
	ppm->PM_HullPointContents = PM_HullPointContents;
	ppm->PM_PlayerTrace = PM_PlayerTrace;
	ppm->PM_TraceLine = PM_TraceLine;
	ppm->PM_TraceModel = PM_TraceModel;
	ppm->RandomLong = RandomLong;
	ppm->RandomFloat = RandomFloat;
	ppm->PM_GetModelType = PM_GetModelType;
	ppm->PM_HullForBsp = /*(decltype(playermove_s::PM_HullForBsp))*/PM_HullForBsp;
	ppm->PM_GetModelBounds = PM_GetModelBounds;
	ppm->COM_FileSize = COM_FileSize;
	ppm->COM_LoadFile = COM_LoadFile;
	ppm->COM_FreeFile = COM_FreeFile;
	ppm->memfgets = memfgets;
	ppm->PM_PlayerTraceEx = PM_PlayerTraceEx;
	ppm->PM_TestPlayerPositionEx = PM_TestPlayerPositionEx;
	ppm->PM_TraceLineEx = PM_TraceLineEx;

}

void PM_ParticleLine(vec_t* start, vec_t* end, int pcolor, float life, float vert)
{
	vec3_t curpos, diff;
	float len;

	VectorSubtract(end, start, diff);
	len = VectorNormalize(diff);

	for (float i = 0; i <= len; i += 2)
	{
		for (int j = 0; j < 3; j++)
			curpos[j] = diff[j] * i + start[j];

		CL_Particle(curpos, pcolor, life, 0, vert);
	}
}

void PM_DrawRectangle(vec_t *tl, vec_t *bl, vec_t *tr, vec_t *br, int pcolor, float life)
{
	PM_ParticleLine(tl, bl, pcolor, life, 0.0);
	PM_ParticleLine(bl, br, pcolor, life, 0.0);
	PM_ParticleLine(br, tr, pcolor, life, 0.0);
	PM_ParticleLine(tr, tl, pcolor, life, 0.0);
}

void PM_DrawBBox(vec_t *mins, vec_t *maxs, vec_t *origin, int pcolor, float life)
{
	int i;
	vec3_t tmp[8];

	for (i = 0; i < 8; i++)
	{
		tmp[i][0] = (i & 1) ? mins[0] : maxs[0];
		tmp[i][1] = (i & 2) ? mins[1] : maxs[1];
		tmp[i][2] = (i & 4) ? mins[2] : maxs[2];
	}

	for (i = 0; i < 6; i++)
		PM_DrawRectangle(tmp[PM_boxpnt[i][1]], tmp[PM_boxpnt[i][0]], tmp[PM_boxpnt[i][2]], tmp[PM_boxpnt[i][3]], pcolor, life);
}
