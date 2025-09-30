#ifndef ENGINE_CL_ENTS_H
#define ENGINE_CL_ENTS_H

#include "pm_defs.h"

extern cvar_t cl_showerror, cl_nosmooth, cl_smoothtime;
extern cvar_t r_bmodelinterp;
extern cvar_t cl_fixmodelinterpolationartifacts;

void CL_SetSolidPlayers( int playernum );

void CL_SetUpPlayerPrediction( bool dopred, bool bIncludeLocalClient );

void CL_AddStateToEntlist( physent_t* pe, entity_state_t* state );
void CL_Particle(float *origin, int color, float life, int zpos, int zvel);
void CL_ParsePacketEntities(qboolean delta, int *playerbits);
void CL_ResetPositions(cl_entity_t *ent);
int CL_AddVisibleEntity(cl_entity_t *pEntity);
void CL_EmitEntities(void);
qboolean CL_IsPlayerIndex(int index);
void CL_SetSolidEntities();
void CL_ParseClientdata(void);

#endif //ENGINE_CL_ENTS_H
