#ifndef ENGINE_CL_PRED_H
#define ENGINE_CL_PRED_H

#include "entity_state.h"
#include "usercmd.h"

struct predicted_player
{
	int movetype;
	int solid;
	int usehull;
	qboolean active;
	vec3_t origin;
	vec3_t angles;
};

// point on plane side epsilon
#define	ON_EPSILON		0.1			

#define	MAX_FORWARD	6

// Use smoothing if error is > this
#define	MIN_CORRECTION_DISTANCE		0.25f

// Complain if error is > this and we have cl_showerror set
#define MIN_PREDICTION_EPSILON		0.5f

// Above this is assumed to be a teleport, don't smooth, etc.
#define MAX_PREDICTION_ERROR		64.0f

extern predicted_player predicted_players[MAX_CLIENTS];

void CL_PushPMStates();

void CL_PopPMStates();

void CL_RunUsercmd( local_state_t* from, local_state_t* to, usercmd_t* u, bool runfuncs, double* pfElapsed, unsigned int random_seed );

void CL_InitPrediction();

void CL_SetLastUpdate();

void CL_PredictMove(qboolean repredicting);

void CL_RedoPrediction();

void CL_CheckPredictionError();

#endif //ENGINE_CL_PRED_H
