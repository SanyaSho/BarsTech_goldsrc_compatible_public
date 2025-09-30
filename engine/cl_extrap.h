#ifndef _CL_EXTRAP_H
#define _CL_EXTRAP_H

#include <common\usercmd.h>
#include <common\cl_entity.h>

void CL_InitExtrap();
void CL_ComputeClientInterpolationAmount(usercmd_t *cmd);
void CL_ComputePlayerOrigin(cl_entity_t *ent);
int CL_InterpolateModel(cl_entity_t *e);
qboolean CL_FindInterpolationUpdates(cl_entity_t* ent, float targettime, position_history_t** ph0, position_history_t** ph1, int* ph0Index);
void CL_UpdatePositions(cl_entity_t* ent);

extern float g_flInterpolationAmount;

#endif