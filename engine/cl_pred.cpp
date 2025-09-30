#include "quakedef.h"
#include "cl_pred.h"
#include "cl_ents.h"
#include "pmove.h"
#include "client.h"
#include "cl_main.h"
#include "host.h"
#include "pmovetst.h"
#include "cl_pmove.h"

predicted_player predicted_players[MAX_CLIENTS];
int pushed;
int oldphysent;
int oldvisent;
double cl_correction_time;
int g_i;
int g_lastupdate_sequence = -1;
int g_lastground = -1;

void CL_CheckPredictionError()
{
	vec3_t delta;
	float len;
	static int pos;
	int frame, command;

	command = cls.netchan.incoming_acknowledged & CL_UPDATE_MASK;
	frame = cl.parsecountmod;

	VectorSubtract(cl.frames[frame].playerstate[cl.playernum].origin, cl.predicted_origins[command], delta);

	len = Length(delta);

	if (len > MAX_PREDICTION_ERROR)
	{
		VectorCopy(vec3_origin, cl.prediction_error);

		len = 0.0f;
	}
	else
	{
		if (cl_showerror.value != 0.0 && len > MIN_PREDICTION_EPSILON)
		{
			pos++;
			Con_NPrintf((pos & 3) + 10, const_cast<char*>("pred. error %.3f units"), len);
		}

		VectorCopy(cl.frames[frame].playerstate[cl.playernum].origin, cl.predicted_origins[frame]);
		VectorCopy(delta, cl.prediction_error);
	}

	if (len > MIN_CORRECTION_DISTANCE && cl.maxclients != 1)
		cl_correction_time = cl_smoothtime.value;
}

void CL_RunUsercmd( local_state_t* from, local_state_t* to, usercmd_t* u, bool runfuncs, double* pfElapsed, unsigned int random_seed )
{
	usercmd_t cmd, split;
	local_state_t temp;

	// recursive calling
	if (u->msec > 50.f)
	{
		split = *u;
		split.msec = ((float)split.msec / 2.0f);
		CL_RunUsercmd(from, &temp, &split, runfuncs, pfElapsed, random_seed);
		split.impulse = split.weaponselect = 0;
		CL_RunUsercmd(&temp, to, &split, runfuncs, pfElapsed, random_seed);
		return;
	}

	cmd = *u;
	
	pmove->time = *pfElapsed * 1000.f;
	pmove->server = false;
	pmove->runfuncs = runfuncs;
	pmove->multiplayer = cl.maxclients > 1;
	pmove->frametime = (float)u->msec / 1000.f;

	*to = *from;
	pmove->player_index = from->playerstate.number - 1;
	pmove->flags = from->client.flags;

	pmove->bInDuck = from->client.bInDuck;
	pmove->flTimeStepSound = from->client.flTimeStepSound;
	pmove->flDuckTime = (float)from->client.flDuckTime;
	pmove->flSwimTime = (float)from->client.flSwimTime;
	pmove->waterjumptime = (float)from->client.waterjumptime;
	pmove->waterlevel = from->client.waterlevel;
	pmove->watertype = from->client.watertype;
	pmove->onground = from->client.flags & FL_ONGROUND;
	pmove->deadflag = from->client.deadflag;

	VectorCopy(from->client.velocity, pmove->velocity);
	VectorCopy(from->client.view_ofs, pmove->view_ofs);
	VectorCopy(from->client.origin, pmove->origin);
	VectorCopy(from->playerstate.basevelocity, pmove->basevelocity);
	VectorCopy(from->client.punchangle, pmove->punchangle);
	VectorCopy(from->playerstate.angles, pmove->angles);
	VectorCopy(from->playerstate.angles, pmove->oldangles);

	pmove->friction = from->playerstate.friction;
	pmove->usehull = from->playerstate.usehull;
	pmove->oldbuttons = from->playerstate.oldbuttons;
	pmove->dead = cl.stats[STAT_HEALTH] <= 0;

	pmove->spectator = cls.spectator != false;
	pmove->movetype = from->playerstate.movetype;
	pmove->gravity = from->playerstate.gravity;
	pmove->effects = from->playerstate.effects;
	pmove->iStepLeft = from->playerstate.iStepLeft;
	pmove->flFallVelocity = from->playerstate.flFallVelocity;

	Q_strncpy(pmove->physinfo, cls.physinfo, sizeof(pmove->physinfo) - 1);
	pmove->physinfo[sizeof(pmove->physinfo) - 1] = 0;

	pmove->maxspeed = movevars.maxspeed;
	pmove->clientmaxspeed = from->client.maxspeed;

	pmove->cmd = cmd;

	pmove->iuser1 = from->client.iuser1;
	pmove->iuser2 = from->client.iuser2;
	pmove->iuser3 = from->client.iuser3;
	pmove->iuser4 = from->client.iuser4;
	pmove->fuser1 = from->client.fuser1;
	pmove->fuser2 = from->client.fuser2;
	pmove->fuser3 = from->client.fuser3;
	pmove->fuser4 = from->client.fuser4;

	VectorCopy(from->client.vuser1, pmove->vuser1);
	VectorCopy(from->client.vuser2, pmove->vuser2);
	VectorCopy(from->client.vuser3, pmove->vuser3);
	VectorCopy(from->client.vuser4, pmove->vuser4);

	pmove->PM_PlaySound = PM_CL_PlaySound;
	pmove->PM_TraceTexture = PM_CL_TraceTexture;
	pmove->PM_PlaybackEventFull = PM_CL_PlaybackEventFull;

	ClientDLL_MoveClient(pmove);

	g_lastground = pmove->onground;
	if (g_lastground > 0)
		g_lastground = pmove->physents[g_lastground].info;

	to->client.flags = pmove->flags;
	to->client.bInDuck = pmove->bInDuck;
	to->client.flTimeStepSound = pmove->flTimeStepSound;
	to->client.flDuckTime = (int)pmove->flDuckTime;
	to->client.flSwimTime = (int)pmove->flSwimTime;
	to->client.waterjumptime = (int)pmove->waterjumptime;
	to->client.waterlevel = pmove->waterlevel;
	to->client.watertype = pmove->watertype;
	to->client.maxspeed = pmove->clientmaxspeed;
	to->client.deadflag = pmove->deadflag;

	VectorCopy(pmove->velocity, to->client.velocity);
	VectorCopy(pmove->view_ofs, to->client.view_ofs);
	VectorCopy(pmove->origin, to->playerstate.origin);
	VectorCopy(pmove->basevelocity, to->playerstate.basevelocity);
	VectorCopy(pmove->punchangle, to->client.punchangle);

	to->playerstate.oldbuttons = pmove->cmd.buttons;
	to->playerstate.friction = pmove->friction;
	to->playerstate.movetype = pmove->movetype;
	to->playerstate.effects = pmove->effects;
	to->playerstate.usehull = pmove->usehull;
	to->playerstate.iStepLeft = pmove->iStepLeft;
	to->playerstate.flFallVelocity = pmove->flFallVelocity;
	to->client.iuser1 = pmove->iuser1;
	to->client.iuser2 = pmove->iuser2;
	to->client.iuser3 = pmove->iuser3;
	to->client.iuser4 = pmove->iuser4;
	to->client.fuser1 = pmove->fuser1;
	to->client.fuser2 = pmove->fuser2;
	to->client.fuser3 = pmove->fuser3;
	to->client.fuser4 = pmove->fuser4;

	VectorCopy(pmove->vuser1, to->client.vuser1);
	VectorCopy(pmove->vuser2, to->client.vuser2);
	VectorCopy(pmove->vuser3, to->client.vuser3);
	VectorCopy(pmove->vuser4, to->client.vuser4);

	ClientDLL_PostRunCmd(from, to, &cmd, runfuncs, *pfElapsed, random_seed);

	*pfElapsed += (float)cmd.msec / 1000.f;
}

void CL_SetIdealPitch()
{
	vec3_t	forward;
	vec3_t	top, bottom;
	float	floor_height[MAX_FORWARD];
	int		i, j;
	int		step, dir, steps;
	pmtrace_t tr;

	if (cl.onground == -1)
		return;

	AngleVectors(cl.simangles, forward, NULL, NULL);
	forward[2] = 0;

	// Now move forward by 36, 48, 60, etc. units from the eye position and drop lines straight down
	//  160 or so units to see what's below
	for (i = 0; i<MAX_FORWARD; i++)
	{
		VectorMA(cl.simorg, (i + 3) * 12, forward, top);

		top[2] += cl.viewheight[2];

		VectorCopy(top, bottom);

		bottom[2] -= 160;

		tr = PM_PlayerTrace(top, bottom, PM_STUDIO_BOX, -1);

		// looking at a wall, leave ideal the way it was
		if (tr.allsolid)
			return;

		// near a dropoff/ledge
		if (tr.fraction == 1)
			return;

		floor_height[i] = top[2] + tr.fraction*(bottom[2] - top[2]);
	}

	dir = 0;
	steps = 0;
	for (j = 1; j<i; j++)
	{
		step = floor_height[j] - floor_height[j - 1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		if (dir && (step - dir > ON_EPSILON || step - dir < -ON_EPSILON))
			return;		// mixed changes

		steps++;
		dir = step;
	}

	if (!dir)
	{
		cl.idealpitch = 0;
		return;
	}

	if (steps < 2)
		return;
	cl.idealpitch = -dir * cl_idealpitchscale.value;
}

void CL_InterpolateVector(float frac, vec_t *src, vec_t *dest, vec_t *output)
{
	for (int i = 0; i < 3; i++)
		output[i] = src[i] + (dest[i] - src[i]) * frac;
}

void CL_GetTargetTime(float *target)
{
	*target = realtime;
}

void CL_PredictMove(qboolean repredicting)
{
	static vec3_t lastsimorg;
	float targettime;
	vec3_t delta;
	cmd_t *fcmd;
	frame_t *pframe;
	local_state_t *from, *to = NULL;
	cmd_t *pcmd = NULL;
	int i;
	float f;
	double dTime;

	if (cls.state != ca_active || cls.spectator)
		return;

	pmove = &g_clmove;
	CL_SetUpPlayerPrediction(false, false);
	CL_GetTargetTime(&targettime);

	if (cl.intermission || !cl.validsequence || cls.netchan.outgoing_sequence + 1 - cls.netchan.incoming_acknowledged >= CL_UPDATE_MASK)
		return;

	VectorCopy(cl.viewangles, cl.simangles);

	if (cls.demoplayback)
		return;

	pframe = &cl.frames[cl.parsecountmod];

	cl.predicted_frames[cl.parsecountmod].playerstate = pframe->playerstate[cl.playernum];
	cl.predicted_frames[cl.parsecountmod].client = pframe->clientdata;

	Q_memcpy(cl.predicted_frames[cl.parsecountmod].weapondata, pframe->weapondata, sizeof(cl.predicted_frames[cl.parsecountmod].weapondata));

	if (pframe->invalid)
		return;

	fcmd = &cl.commands[cls.netchan.incoming_acknowledged & CL_UPDATE_MASK];
	cl.onground = -1;

	CL_PushPMStates();

	CL_SetSolidPlayers(cl.playernum);

	dTime = pframe->time;
	from = &cl.predicted_frames[cl.parsecountmod];
	for (i = 1, to = NULL, pcmd = NULL; i < CL_UPDATE_BACKUP - 1; i++, from = to, fcmd = pcmd)
	{
		int stoppoint = i + cls.netchan.incoming_acknowledged >= !repredicting + cls.netchan.outgoing_sequence;

		if (stoppoint)
			break;

		g_i = i;

		pcmd = &cl.commands[(cls.netchan.incoming_acknowledged + i) & CL_UPDATE_MASK];
		to = &cl.predicted_frames[(cl.parsecountmod + i) & CL_UPDATE_MASK];

		CL_RunUsercmd(from, to, &pcmd->cmd, !repredicting && !pcmd->processedfuncs, &dTime, cls.netchan.incoming_acknowledged + i);
		pcmd->processedfuncs = true;
		VectorCopy(to->playerstate.origin, cl.predicted_origins[(cls.netchan.incoming_acknowledged + i) & CL_UPDATE_MASK]);

		if (pcmd->senttime >= targettime)
			break;
	}

	CL_PopPMStates();

	if (i >= CL_UPDATE_MASK)
		return;

	if (!to && !repredicting)
		return;

	if (!to)
	{
		to = from;
		pcmd = fcmd;
	}

	if (pcmd->senttime == fcmd->senttime)
		f = 0.f;
	else
		f = max((targettime - fcmd->senttime) / (pcmd->senttime - fcmd->senttime), 0.f);

	local_state_t *fs = from, *ts = to;

	// is player teleported ?
	if (
		fabs(ts->playerstate.origin[0] - fs->playerstate.origin[0]) > 128.f ||
		fabs(ts->playerstate.origin[1] - fs->playerstate.origin[1]) > 128.f ||
		fabs(ts->playerstate.origin[2] - fs->playerstate.origin[2]) > 128.f
		)
	{
		VectorCopy(ts->client.velocity, cl.simvel);
		VectorCopy(ts->playerstate.origin, cl.simorg);
		VectorCopy(ts->client.punchangle, cl.punchangle);
		VectorCopy(ts->client.view_ofs, cl.viewheight);
	}
	else
	{
		CL_InterpolateVector(f, fs->playerstate.origin, ts->playerstate.origin, cl.simorg);
		CL_InterpolateVector(f, fs->client.velocity, ts->client.velocity, cl.simvel);
		CL_InterpolateVector(f, fs->client.punchangle, ts->client.punchangle, cl.punchangle);

		if (fs->playerstate.usehull == ts->playerstate.usehull)
		{
			CL_InterpolateVector(f, fs->client.view_ofs, ts->client.view_ofs, cl.viewheight);
		}
		else
		{
			VectorCopy(ts->client.view_ofs, cl.viewheight);
		}
	}

	if (ts->client.flags & FL_ONGROUND)
		cl.onground = g_lastground;
	else
		cl.onground = -1;

	cl.waterlevel = ts->client.waterlevel;
	cl.usehull = ts->playerstate.usehull;
	cl.moving = 0;
	cl.stats[STAT_WEAPON] = ts->client.viewmodel;

	if (cl.onground > 0 && cl.onground < cl.num_entities)
	{
		VectorSubtract(cl_entities[cl.onground].curstate.origin, cl_entities[i].prevstate.origin, delta);

		if (Length(delta) > 0.f)
			cl.moving = 1;

		if (cl.moving)
		{
			cl_correction_time = 0.f;
			VectorCopy(cl.simorg, lastsimorg);
			CL_SetIdealPitch();
			return;
		}
	}

	if (cl_correction_time > 0.0 && cl_nosmooth.value == 0.0 && cl_smoothtime.value != 0.0)
	{
		if (!repredicting)
			cl_correction_time = cl_correction_time - host_frametime;
	
		if (cl_smoothtime.value <= 0.0)
			Cvar_DirectSet(&cl_smoothtime, const_cast<char*>("0.1"));

		cl_correction_time = clamp(cl_correction_time, 0.0, (double)cl_smoothtime.value);

		VectorSubtract(cl.simorg, lastsimorg, delta);
		VectorScale(delta, 1.0f - (cl_correction_time / cl_smoothtime.value), delta);
		VectorAdd(lastsimorg, delta, cl.simorg);
	}
}

void CL_PushPMStates()
{
	if (pushed)
		return Con_Printf(const_cast<char*>(__FUNCTION__ " called with pushed stack\n"));

	oldphysent = pmove->numphysent;
	oldvisent = pmove->numvisent;
	pushed = 1;
}

void CL_PopPMStates()
{
	if (!pushed)
		return Con_Printf(const_cast<char*>(__FUNCTION__ " called without stack\n"));

	pushed = 0;
	pmove->numphysent = oldphysent;
	pmove->numvisent = oldvisent;
}

void CL_SetLastUpdate()
{
	g_lastupdate_sequence = cls.netchan.incoming_sequence;
}

void CL_RedoPrediction()
{
	if (cls.netchan.incoming_sequence != g_lastupdate_sequence)
	{
		CL_PredictMove(true);
		CL_CheckPredictionError();
	}
}

void CL_InitPrediction()
{
	Cvar_RegisterVariable(&cl_smoothtime);
	Cvar_RegisterVariable(&cl_nosmooth);
	Cvar_RegisterVariable(&cl_showerror);
}
