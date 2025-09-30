#ifndef _SV_MOVE_H_
#define _SV_MOVE_H_

#include "quakedef.h"

qboolean SV_CheckBottom(edict_t *ent);
void SV_MoveToOrigin_I(edict_t* ent, const float* pflGoal, float dist, int iMoveType);
qboolean SV_movetest(edict_t* ent, vec_t* move, qboolean relink);
qboolean SV_movestep(edict_t* ent, vec_t* move, qboolean relink);

#endif