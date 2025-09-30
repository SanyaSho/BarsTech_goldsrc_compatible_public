#include "quakedef.h"
#include "cl_pmove.h"
#include "cl_ents.h"
#include "pmove.h"
#include "client.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "host.h"

cvar_t ex_interp = { const_cast<char*>("ex_interp"), const_cast<char*>("0"), FCVAR_PRINTABLEONLY | FCVAR_ARCHIVE };
float g_flInterpolationAmount = 0.1f;

void CL_InitExtrap()
{
	Cvar_RegisterVariable(&ex_interp);
}

qboolean CL_FindInterpolationUpdates(cl_entity_t *ent, float targettime, position_history_t **ph0, position_history_t **ph1, int *ph0Index)
{
	qboolean found = true;
	int idx0, idx1;

	idx0 = ent->current_position & HISTORY_MASK;
	idx1 = (ent->current_position - 1) & HISTORY_MASK;

	if (ent->ph[idx0].animtime >= targettime)
	{
		for (int i = 1; i < HISTORY_MAX - 1; i++)
		{
			if (!ent->ph[(ent->current_position - i) & HISTORY_MASK].animtime)
				break;

			if (ent->ph[(ent->current_position - i) & HISTORY_MASK].animtime < targettime)
			{
				idx0 = (ent->current_position - i + 1) & HISTORY_MASK;
				idx1 = (ent->current_position - i) & HISTORY_MASK;
				found = false;
				break;
			}
		}
	}

	if (ph0)
		*ph0 = &ent->ph[idx0];
	if (ph1)
		*ph1 = &ent->ph[idx1];
	if (ph0Index)
		*ph0Index = idx0;

	return found;
}

void CL_BoundInterpolationFraction(float *frac)
{
	*frac = clamp(*frac, 0.f, 1.2f);
}

void CL_UpdatePositions(cl_entity_t *ent)
{
	position_history_t *pph;

	ent->current_position = (ent->current_position + 1) & HISTORY_MASK;
	pph = &ent->ph[ent->current_position];
	pph->animtime = ent->curstate.animtime;
	VectorCopy(ent->curstate.origin, pph->origin);
	VectorCopy(ent->curstate.angles, pph->angles);
}

void CL_ResetPositions(cl_entity_t* ent)
{
	position_history_t store;

	if (ent == NULL)
		return;

	store = ent->ph[ent->current_position];
	ent->current_position = 1;
	Q_memset(ent->ph, 0, sizeof(ent->ph));
	Q_memcpy(&ent->ph[1], &store, sizeof(store));
	Q_memcpy(&ent->ph[0], &store, sizeof(store));

}

void CL_PureOrigin(cl_entity_t *ent, float t, vec_t *outorigin, vec_t *outangles)
{
	position_history_t *ph0, *ph1;
	float scale;
	vec3_t delta, pos, angles;

	CL_FindInterpolationUpdates(ent, t, &ph0, &ph1, NULL);

	if (!ph0 || !ph1)
		return;

	if (!ph0->animtime)
	{
		VectorCopy(ph1->origin, outorigin);
		VectorCopy(ph1->angles, outangles);
		return;
	}

	VectorSubtract(ph0->origin, ph1->origin, delta);

	if (ph0->animtime == ph1->animtime)
		scale = 1.f;
	else
		scale = (t - ph1->animtime) / (ph0->animtime - ph1->animtime);

	CL_BoundInterpolationFraction(&scale);

	VectorMA(ph1->origin, scale, delta, pos);
	InterpolateAngles(ph1->angles, ph0->angles, angles, scale);

	VectorCopy(pos, outorigin);
	VectorCopy(angles, outangles);
}

void CL_ComputePlayerOrigin(cl_entity_t *ent)
{
	vec3_t target_origin, angles;

	if (!ent->player)
		return;

	if (ent->index - 1 == cl.playernum)
		return;

	CL_PureOrigin(ent, cl.time - ex_interp.value, target_origin, angles);
	VectorCopy(angles, ent->angles);
	VectorCopy(target_origin, ent->origin);
}

void CL_InterpolateEntity(cl_entity_t *ent)
{
	vec3_t target_origin, angles;

	CL_PureOrigin(ent, cl.time - ex_interp.value, target_origin, angles);
	VectorCopy(angles, ent->angles);
	VectorCopy(target_origin, ent->origin);
}
