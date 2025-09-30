#ifndef _R_TRANS_H_
#define _R_TRANS_H_

#include "cl_entity.h"

struct transObjRef
{
	cl_entity_t *pEnt;
	float distance;
};

void R_DestroyObjects();
void R_AllocObjects(int nMax);
void AppendTEntity(cl_entity_t *pEnt);
void AddTEntity(cl_entity_t *pEnt);
void R_DrawTEntitiesOnList(qboolean clientOnly);

extern int r_intentities;
#endif