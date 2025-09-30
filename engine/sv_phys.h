#ifndef ENGINE_SV_PHYS_H
#define ENGINE_SV_PHYS_H

#define STOP_EPSILON  0.1f

extern cvar_t sv_gravity;
extern cvar_t sv_maxvelocity;
extern cvar_t sv_bounce;
extern cvar_t sv_stepsize;
extern cvar_t sv_friction;
extern cvar_t sv_stopspeed;

extern vec3_t *g_moved_from;
extern edict_t **g_moved_edict;

void SV_Physics();
void SV_Impact(edict_t *e1, edict_t *e2, trace_t *ptrace);
trace_t SV_Trace_Toss(edict_t* ent, edict_t* ignore);

#endif //ENGINE_SV_PHYS_H
