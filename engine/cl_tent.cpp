#include "quakedef.h"
#include "client.h"
#include "cl_tent.h"
#include "pr_cmds.h"
#include "spritegn.h"
#include "cl_main.h"
#include "r_studio.h"
#if defined(GLQUAKE)
#include "gl_model.h"
#include "glquake.h"
#else
#include "model.h"
#endif
#include "r_local.h"
#include "cl_pmove.h"
#include "r_part.h"
#include "r_trans.h"
#include "pmove.h"
#include "pmovetst.h"
#include "host.h"
#include "tmessage.h"
#include "cl_ents.h"
#include "entity_types.h"

static TEMPENTITY gTempEnts[ MAX_TEMP_ENTITIES ];

static TEMPENTITY* gpTempEntActive = nullptr;
static TEMPENTITY* gpTempEntFree = nullptr;

cvar_t egon_amplitude = { const_cast<char*>("egon_amplitude"), const_cast<char*>("0.0") };
cvar_t tracerSpeed = { const_cast<char*>("tracerspeed"), const_cast<char*>("6000") };
cvar_t tracerOffset = { const_cast<char*>("traceroffset"), const_cast<char*>("30") };
cvar_t tracerLength = { const_cast<char*>("tracerlength"), const_cast<char*>("0.8")};
cvar_t tracerRed = { const_cast<char*>("tracerred"), const_cast<char*>("0.8") };
cvar_t tracerGreen = { const_cast<char*>("tracergreen"), const_cast<char*>("0.8") };
cvar_t tracerBlue = { const_cast<char*>("tracerblue"), const_cast<char*>("0.4") };
cvar_t tracerAlpha = { const_cast<char*>("traceralpha"), const_cast<char*>("0.5") };

sfx_t* cl_sfx_geiger1;
sfx_t* cl_sfx_geiger2;
sfx_t* cl_sfx_geiger3;
sfx_t* cl_sfx_geiger4;
sfx_t* cl_sfx_geiger5;
sfx_t* cl_sfx_geiger6;

sfx_t* cl_sfx_flesh1;
sfx_t* cl_sfx_flesh2;
sfx_t* cl_sfx_flesh3;
sfx_t* cl_sfx_flesh4;
sfx_t* cl_sfx_flesh5;
sfx_t* cl_sfx_flesh6;

sfx_t* cl_sfx_concrete1;
sfx_t* cl_sfx_concrete2;
sfx_t* cl_sfx_concrete3;

sfx_t* cl_sfx_glass1;
sfx_t* cl_sfx_glass2;
sfx_t* cl_sfx_glass3;

sfx_t* cl_sfx_metal1;
sfx_t* cl_sfx_metal2;
sfx_t* cl_sfx_metal3;

sfx_t* cl_sfx_wood1;
sfx_t* cl_sfx_wood2;
sfx_t* cl_sfx_wood3;

sfx_t* cl_sfx_sshell1;
sfx_t* cl_sfx_sshell2;
sfx_t* cl_sfx_sshell3;

sfx_t* cl_sfx_pl_shell1;
sfx_t* cl_sfx_pl_shell2;
sfx_t* cl_sfx_pl_shell3;

sfx_t* cl_sfx_r_exp1;
sfx_t* cl_sfx_r_exp2;
sfx_t* cl_sfx_r_exp3;

sfx_t* cl_sfx_ric1;
sfx_t* cl_sfx_ric2;
sfx_t* cl_sfx_ric3;
sfx_t* cl_sfx_ric4;
sfx_t* cl_sfx_ric5;

model_t* cl_sprite_dot, *cl_sprite_lightning, *cl_sprite_glow, *cl_sprite_muzzleflash[3], *cl_sprite_ricochet;
model_t *cl_sprite_shell;
model_t *cl_sprite_white;

BOOL CL_ShowTEBlood()
{
	return violence_hblood.value != 0.0 || violence_ablood.value != 0.0;
}

model_t * CL_TentModel(const char *pName)
{
	model_t *mod;

	mod = Mod_ForName(pName, true, false);
	Mod_MarkClient(mod);
	return mod;
}

void CL_InitTEnts()
{
	Cvar_RegisterVariable(&tracerSpeed);
	Cvar_RegisterVariable(&tracerOffset);
	Cvar_RegisterVariable(&tracerLength);
	Cvar_RegisterVariable(&tracerRed);
	Cvar_RegisterVariable(&tracerGreen);
	Cvar_RegisterVariable(&tracerBlue);
	Cvar_RegisterVariable(&tracerAlpha);

	cl_sfx_ric1 = S_PrecacheSound(const_cast<char*>("weapons/ric1.wav"));
	cl_sfx_ric2 = S_PrecacheSound(const_cast<char*>("weapons/ric2.wav"));
	cl_sfx_ric3 = S_PrecacheSound(const_cast<char*>("weapons/ric3.wav"));
	cl_sfx_ric4 = S_PrecacheSound(const_cast<char*>("weapons/ric4.wav"));
	cl_sfx_ric5 = S_PrecacheSound(const_cast<char*>("weapons/ric5.wav"));
	cl_sfx_r_exp1 = S_PrecacheSound(const_cast<char*>("weapons/explode3.wav"));
	cl_sfx_r_exp2 = S_PrecacheSound(const_cast<char*>("weapons/explode4.wav"));
	cl_sfx_r_exp3 = S_PrecacheSound(const_cast<char*>("weapons/explode5.wav"));
	cl_sfx_pl_shell1 = S_PrecacheSound(const_cast<char*>("player/pl_shell1.wav"));
	cl_sfx_pl_shell2 = S_PrecacheSound(const_cast<char*>("player/pl_shell2.wav"));
	cl_sfx_pl_shell3 = S_PrecacheSound(const_cast<char*>("player/pl_shell3.wav"));
	cl_sfx_sshell1 = S_PrecacheSound(const_cast<char*>("weapons/sshell1.wav"));
	cl_sfx_sshell2 = S_PrecacheSound(const_cast<char*>("weapons/sshell2.wav"));
	cl_sfx_sshell3 = S_PrecacheSound(const_cast<char*>("weapons/sshell3.wav"));
	cl_sfx_wood1 = S_PrecacheSound(const_cast<char*>("debris/wood1.wav"));
	cl_sfx_wood2 = S_PrecacheSound(const_cast<char*>("debris/wood2.wav"));
	cl_sfx_wood3 = S_PrecacheSound(const_cast<char*>("debris/wood3.wav"));
	cl_sfx_metal1 = S_PrecacheSound(const_cast<char*>("debris/metal1.wav"));
	cl_sfx_metal2 = S_PrecacheSound(const_cast<char*>("debris/metal2.wav"));
	cl_sfx_metal3 = S_PrecacheSound(const_cast<char*>("debris/metal3.wav"));
	cl_sfx_glass1 = S_PrecacheSound(const_cast<char*>("debris/glass1.wav"));
	cl_sfx_glass2 = S_PrecacheSound(const_cast<char*>("debris/glass2.wav"));
	cl_sfx_glass3 = S_PrecacheSound(const_cast<char*>("debris/glass3.wav"));
	cl_sfx_concrete1 = S_PrecacheSound(const_cast<char*>("debris/concrete1.wav"));
	cl_sfx_concrete2 = S_PrecacheSound(const_cast<char*>("debris/concrete2.wav"));
	cl_sfx_concrete3 = S_PrecacheSound(const_cast<char*>("debris/concrete3.wav"));
	cl_sfx_flesh1 = S_PrecacheSound(const_cast<char*>("debris/flesh1.wav"));
	cl_sfx_flesh2 = S_PrecacheSound(const_cast<char*>("debris/flesh2.wav"));
	cl_sfx_flesh3 = S_PrecacheSound(const_cast<char*>("debris/flesh3.wav"));
	cl_sfx_flesh4 = S_PrecacheSound(const_cast<char*>("debris/flesh5.wav"));
	cl_sfx_flesh5 = S_PrecacheSound(const_cast<char*>("debris/flesh6.wav"));
	cl_sfx_flesh6 = S_PrecacheSound(const_cast<char*>("debris/flesh7.wav"));
	cl_sfx_geiger1 = S_PrecacheSound(const_cast<char*>("player/geiger1.wav"));
	cl_sfx_geiger2 = S_PrecacheSound(const_cast<char*>("player/geiger2.wav"));
	cl_sfx_geiger3 = S_PrecacheSound(const_cast<char*>("player/geiger3.wav"));
	cl_sfx_geiger4 = S_PrecacheSound(const_cast<char*>("player/geiger4.wav"));
	cl_sfx_geiger5 = S_PrecacheSound(const_cast<char*>("player/geiger5.wav"));
	cl_sfx_geiger6 = S_PrecacheSound(const_cast<char*>("player/geiger6.wav"));

	// кэширование процедурных текстур
	cl_sprite_dot = CL_TentModel("sprites/dot.spr");
	cl_sprite_lightning = CL_TentModel("sprites/lgtning.spr");
	cl_sprite_white = CL_TentModel("sprites/white.spr");
	cl_sprite_glow = CL_TentModel("sprites/animglow01.spr");
	cl_sprite_muzzleflash[0] = CL_TentModel("sprites/muzzleflash1.spr");
	cl_sprite_muzzleflash[1] = CL_TentModel("sprites/muzzleflash2.spr");
	cl_sprite_muzzleflash[2] = CL_TentModel("sprites/muzzleflash3.spr");
	cl_sprite_ricochet = CL_TentModel("sprites/richo1.spr");
	cl_sprite_shell = CL_TentModel("sprites/shellchrome.spr");

	CL_TempEntInit();
}

void R_RicochetSound(vec_t* pos)
{
	switch (RandomLong(0, 4))
	{
	case 0:
		S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric1, pos, 1.0, 1.0, 0, PITCH_NORM);
		break;
	case 1:
		S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric2, pos, 1.0, 1.0, 0, PITCH_NORM);
		break;
	case 2:
		S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric3, pos, 1.0, 1.0, 0, PITCH_NORM);
		break;
	case 3:
		S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric4, pos, 1.0, 1.0, 0, PITCH_NORM);
		break;
	case 4:
		S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric5, pos, 1.0, 1.0, 0, PITCH_NORM);
		break;
	default:
		break;
	}
}

void R_TracerEffect(vec_t* start, vec_t* end)
{
	vec3_t temp, vel;
	float flLen, scale, out;

	if (tracerSpeed.value <= 0.0)
		tracerSpeed.value = 3.0;

	VectorSubtract(end, start, temp);

	flLen = Length(temp);
	scale = 1.0 / flLen;

	VectorScale(temp, scale, temp);
	VectorScale(temp, tracerOffset.value + RandomLong(-10, 9), vel);

	VectorAdd(start, vel, start);
	VectorScale(temp, tracerSpeed.value, vel);

	out = flLen / tracerSpeed.value;
	efx.R_TracerParticles(start, vel, out);
}

//-----------------------------------------------------------------------------
// Purpose: Create a fizz effect
// Input  : *pent - 
//			modelIndex - 
//			density - 
//-----------------------------------------------------------------------------
void R_FizzEffect(cl_entity_t* pent, int modelIndex, int density)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int iFrames, count, width, depth;
	float maxHeight, xspeed, yspeed, speed, sy, cy, yangle;
	vec3_t origin;

	if (!modelIndex || !pent->model)
		return;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	count = density + 1;

	maxHeight = pent->model->maxs[2] - pent->model->mins[2];
	width = pent->model->maxs[0] - pent->model->mins[0];
	depth = pent->model->maxs[1] - pent->model->mins[1];
	speed = (float)pent->curstate.rendercolor.r * 256.0 + (float)pent->curstate.rendercolor.g;

	yangle = (M_PI / 180) * pent->angles[YAW];
	sy = sin(yangle);
	cy = cos(yangle);

	if (pent->curstate.rendercolor.b)
		speed = -speed;

	xspeed = sy * speed;
	yspeed = cy * speed;

	iFrames = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		origin[0] = pent->model->mins[0] + RandomLong(0, width - 1);
		origin[1] = pent->model->mins[1] + RandomLong(0, depth - 1);
		origin[2] = pent->model->mins[2];

		ptent = efx.CL_TempEntAlloc(origin, pmod);

		if (ptent == NULL)
			return;

		ptent->flags |= FTENT_SINEWAVE;

		ptent->entity.baseline.origin[0] = yspeed;
		ptent->entity.baseline.origin[1] = xspeed;
		ptent->x = origin[0];
		ptent->y = origin[1];

		float zspeed = RandomLong(80, 140);

		ptent->entity.baseline.origin[ROLL] = zspeed;
		ptent->die = maxHeight / ptent->entity.baseline.origin[ROLL] + cl.time - 0.1;
		ptent->entity.curstate.frame = (float)RandomLong(0, iFrames - 1);
		ptent->entity.curstate.rendermode = kRenderTransAlpha;
		ptent->entity.curstate.renderamt = 255;
		ptent->entity.curstate.scale = 1.0 / RandomFloat(2.0, 5.0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubbles
// Input  : *mins - 
//			*maxs - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void R_Bubbles(vec_t* mins, vec_t* maxs, float height, int modelIndex, int count, float speed)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	float flRand, s, c;
	vec3_t origin;

	if (!modelIndex)
		return;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	frameCount = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		origin[0] = RandomLong(mins[0], maxs[0]);
		origin[1] = RandomLong(mins[1], maxs[1]);
		origin[2] = RandomLong(mins[2], maxs[2]);

		ptent = efx.CL_TempEntAlloc(origin, pmod);

		if (ptent == NULL)
			return;

		ptent->flags |= FTENT_SINEWAVE;
		ptent->x = origin[0];
		ptent->y = origin[1];

		flRand = RandomLong(-3, 3);
		s = sin(flRand);
		c = cos(flRand);

		ptent->entity.baseline.origin[PITCH] = c * speed;
		ptent->entity.baseline.origin[YAW] = s * speed;
		ptent->entity.baseline.origin[ROLL] = RandomLong(80, 140);

		ptent->die = (height - (origin[2] - mins[2])) / ptent->entity.baseline.origin[ROLL] + cl.time - 0.1;
		ptent->entity.curstate.frame = (float)RandomLong(0, frameCount - 1);
		ptent->entity.curstate.rendermode = kRenderTransAlpha;
		ptent->entity.curstate.renderamt = 255;
		ptent->entity.curstate.scale = 1.0 / RandomFloat(2.0, 5.0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubble trail
// Input  : *start - 
//			*end - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void R_BubbleTrail(vec_t* start, vec_t* end, float height, int modelIndex, int count, float speed)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	float flRand, s, c;
	vec3_t origin;

	if (!modelIndex)
		return;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	frameCount = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		flRand = RandomFloat(0.0, 1.0);
		for (int j = 0; j < 3; j++)
			origin[j] = start[j] + (end[j] - start[j]) * flRand;

		ptent = efx.CL_TempEntAlloc(origin, pmod);

		if (ptent == NULL)
			return;

		ptent->flags |= FTENT_SINEWAVE;
		ptent->x = origin[0];
		ptent->y = origin[1];

		flRand = RandomLong(-3, 3);
		s = sin(flRand);
		c = cos(flRand);

		ptent->entity.baseline.origin[PITCH] = c * speed;
		ptent->entity.baseline.origin[YAW] = s * speed;
		ptent->entity.baseline.origin[ROLL] = RandomLong(80, 140);

		ptent->die = (height - (origin[2] - start[2])) / ptent->entity.baseline.origin[ROLL] + cl.time - 0.1;
		ptent->entity.curstate.frame = (float)RandomLong(0, frameCount - 1);
		ptent->entity.curstate.rendermode = kRenderTransAlpha;
		ptent->entity.curstate.renderamt = 255;
		ptent->entity.curstate.scale = 1.0 / RandomFloat(2.0, 5.0);
	}
}

void R_Projectile(vec_t* origin, vec_t* velocity, int modelIndex, int life, int owner, void(*hitcallback)(TEMPENTITY*, pmtrace_t*))
{
	TEMPENTITY* ptent;
	model_t* pmod;
	vec3_t orientation;
	int frameCount;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	ptent = efx.CL_TempEntAllocHigh(origin, pmod);

	if (ptent == NULL)
	{
		Con_Printf(const_cast<char*>("Couldn't allocate temp ent in R_Projectile!\n"));
		return;
	}

	if (!CL_IsPlayerIndex(owner))
	{
		Con_Printf(const_cast<char*>("Bad ent in R_Projectile!\n"));
		return;
	}

	ptent->flags |= FTENT_COLLIDEKILL | FTENT_PERSIST | FTENT_COLLIDEALL;
	VectorCopy(velocity, ptent->entity.baseline.origin);

	ptent->clientIndex = owner;
	if (ptent->entity.model->type == mod_sprite)
	{
		frameCount = ModelFrameCount(pmod);
		ptent->frameMax = (float)frameCount;
		ptent->flags |= FTENT_SPRANIMATE;
		if (frameCount > 9)
			ptent->entity.curstate.framerate = ptent->frameMax / (float)life;
		else
		{
			ptent->entity.curstate.framerate = 10.0;
			ptent->flags |= FTENT_SPRANIMATELOOP;
		}
	}
	else
	{
		ptent->frameMax = 0.0;
		VectorNormalize(velocity);
		VectorAngles(velocity, orientation);
		VectorCopy(orientation, ptent->entity.angles);
	}

	ptent->entity.curstate.rendermode = kRenderNormal;
	ptent->entity.curstate.renderfx = kRenderFxNone;
	ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 255;
	ptent->die = life + cl.time;
	ptent->entity.curstate.frame = 0.0;
	ptent->entity.curstate.body = 0;
	ptent->entity.curstate.scale = 1.0;
	ptent->entity.curstate.rendercolor.r = ptent->entity.curstate.rendercolor.g = ptent->entity.curstate.rendercolor.b = 255;
	ptent->hitcallback = hitcallback;
}

void R_Sprite_Trail(int type, vec_t* start, vec_t* end, int modelIndex, int count, float life, float size, float amplitude, int renderamt, float speed)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	vec3_t delta, dir, pos;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	frameCount = ModelFrameCount(pmod);
	VectorSubtract(end, start, delta);
	VectorCopy(delta, dir);
	VectorNormalize(dir);

	for (int i = 0; i < count; i++)
	{
		if (i)
			VectorMA(start, (float)i / float(count - 1), delta, pos);
		else
			VectorMA(start, 0, delta, pos);

		ptent = efx.CL_TempEntAlloc(pos, pmod);

		if (ptent == NULL)
			return;

		ptent->flags |= FTENT_SPRCYCLE | FTENT_FADEOUT | FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;
		VectorScale(dir, speed, ptent->entity.baseline.origin);
		for (int j = 0; j < 3; j++)
			ptent->entity.baseline.origin[j] += RandomLong(-127, 128) * amplitude / 256.0f;
		ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = renderamt;
		ptent->entity.curstate.scale = size;
		ptent->entity.curstate.rendermode = kRenderGlow;
		ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
		ptent->die = RandomFloat(0.0, 4.0) + life + cl.time;
		ptent->entity.curstate.rendercolor.r = ptent->entity.curstate.rendercolor.g = ptent->entity.curstate.rendercolor.b = 255;
		ptent->entity.curstate.frame = RandomLong(0, frameCount - 1);
		ptent->frameMax = frameCount;
	}
}

void R_TempSphereModel(float* pos, float speed, float life, int count, int modelIndex)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;

	if (!modelIndex)
		return;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	frameCount = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		ptent = efx.CL_TempEntAlloc(pos, pmod);

		if (ptent == NULL)
			return;

		ptent->entity.curstate.body = RandomLong(0, frameCount - 1);
		ptent->flags |= (RandomLong(0, 255) > 9) ? FTENT_GRAVITY : FTENT_SLOWGRAVITY;

		if (RandomLong(0, 255) <= 219)
		{
			ptent->flags |= FTENT_ROTATE;
			for (int j = 0; j < 3; j++)
				ptent->entity.baseline.angles[j] = RandomFloat(-256.0, -255.0);
		}

		if (RandomLong(0, 255) <= 99)
			ptent->flags |= FTENT_SMOKETRAIL;

		ptent->flags |= FTENT_COLLIDEWORLD | FTENT_FLICKER;
		ptent->entity.curstate.rendermode = kRenderNormal;
		ptent->entity.curstate.effects = i & 31;
		for (int j = 0; j < 3; j++)
			ptent->entity.baseline.origin[j] = RandomFloat(-1.0, 1.0);
		VectorNormalize(ptent->entity.baseline.origin);
		VectorScale(ptent->entity.baseline.origin, speed, ptent->entity.baseline.origin);
		ptent->die = life + cl.time;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create model shattering shards
// Input  : *pos - 
//			*size - 
//			*dir - 
//			random - 
//			life - 
//			count - 
//			modelIndex - 
//			flags - 
//-----------------------------------------------------------------------------
void R_BreakModel(float* pos, float* size, float* dir, float random, float life, int count, int modelIndex, char flags)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	vec3_t vecSpot;
	byte type;

	if (!modelIndex)
		return;

	// фильтрация говна
	type = flags & (FTENT_FLICKER | FTENT_SLOWGRAVITY | FTENT_ROTATE | FTENT_GRAVITY | FTENT_SINEWAVE);

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	frameCount = ModelFrameCount(pmod);

	if (!count)
		count = int((size[0] * size[2] + size[0] * size[1] + size[1] * size[2]) / 432.0);

	count = min(count, 100);

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < 3; j++)
			vecSpot[j] = RandomFloat(-0.5, 0.5) * size[j] + pos[j];

		ptent = efx.CL_TempEntAlloc(vecSpot, pmod);

		if (ptent == NULL)
			return;

		ptent->hitSound = type;

		if (pmod->type == mod_sprite)
			ptent->entity.curstate.frame = (float)RandomLong(0, frameCount - 1);
		else if (pmod->type == mod_studio)
			ptent->entity.curstate.body = RandomLong(0, frameCount - 1);

		ptent->flags |= FTENT_FADEOUT | FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;

		if (RandomLong(0, 255) <= 199)
		{
			ptent->flags |= FTENT_ROTATE;
			for (int j = 0; j < 3; j++)
				ptent->entity.baseline.angles[j] = RandomFloat(-256.0, 255.0);
		}

		if (RandomLong(0, 255) <= 99 && (flags & FTENT_SMOKETRAIL))
			ptent->flags |= FTENT_SMOKETRAIL;

		if (type == FTENT_SINEWAVE || (flags & FTENT_COLLIDEWORLD))
		{
			ptent->entity.curstate.rendermode = kRenderTransTexture;
			ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 128;
		}
		else
		{
			ptent->entity.curstate.rendermode = kRenderNormal;
			ptent->entity.baseline.renderamt = 255;
		}

		for (int j = 0; j < 3; j++)
			ptent->entity.baseline.origin[j] = RandomFloat(j == 2 ? 0 : -random, random) + dir[j];

		ptent->die = RandomFloat(0.0, 1.0) + life + cl.time;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create some simple physically simulated models
//-----------------------------------------------------------------------------
TEMPENTITY* R_TempModel(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype)
{
	TEMPENTITY* ptent;
	model_t* pmod;

	if (!modelIndex)
		return NULL;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return NULL;
	}

	ptent = efx.CL_TempEntAlloc(pos, pmod);

	if (ptent == NULL)
		return NULL;

	VectorCopy(angles, ptent->entity.angles);
	ptent->frameMax = (float)ModelFrameCount(pmod);
	ptent->flags |= FTENT_COLLIDEWORLD | FTENT_GRAVITY;

	switch (soundtype)
	{
	case TE_BOUNCE_NULL:
		ptent->hitSound = 0;
		break;
	case TE_BOUNCE_SHELL:
		ptent->hitSound = BOUNCE_SHELL;
		ptent->flags |= FTENT_ROTATE;
		ptent->entity.baseline.angles[0] = RandomFloat(-512.0, 511.0);
		ptent->entity.baseline.angles[1] = RandomFloat(-256.0, 255.0);
		ptent->entity.baseline.angles[2] = RandomFloat(-256.0, 255.0);
		break;
	case TE_BOUNCE_SHOTSHELL:
		ptent->hitSound = BOUNCE_SHOTSHELL;
		ptent->flags |= FTENT_SLOWGRAVITY | FTENT_ROTATE;
		ptent->entity.baseline.angles[0] = RandomFloat(-512.0, 511.0);
		ptent->entity.baseline.angles[1] = RandomFloat(-256.0, 255.0);
		ptent->entity.baseline.angles[2] = RandomFloat(-256.0, 255.0);
		break;
	}

	VectorCopy(dir, ptent->entity.baseline.origin);
	ptent->die = life + cl.time;

	if (pmod->type == mod_sprite)
		ptent->entity.curstate.frame = (float)RandomLong(0, ptent->frameMax - 1);
	else
		ptent->entity.curstate.body = RandomLong(0, ptent->frameMax - 1);

	return ptent;
}

void R_SparkShower(float* pos)
{
	TEMPENTITY* ptent;

	ptent = efx.CL_TempEntAllocNoModel(pos);
	if (ptent == NULL)
		return;

	ptent->flags |= FTENT_SPARKSHOWER | FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;
	for (int j = 0; j < 3; j++)
	{
		ptent->entity.baseline.origin[j] = RandomFloat(-300.0, 300.0);
		ptent->entity.baseline.angles[j] = 0.0;
	}
	ptent->die = cl.time + 0.5;
	ptent->entity.curstate.framerate = RandomFloat(0.5, 1.5);
	ptent->entity.curstate.scale = cl.time;

}

//-----------------------------------------------------------------------------
// Purpose: Create sprite TE
// Input  : *pos - 
//			*dir - 
//			scale - 
//			modelIndex - 
//			rendermode - 
//			renderfx - 
//			a - 
//			life - 
//			flags - 
// Output : TEMPENTITY
//-----------------------------------------------------------------------------
TEMPENTITY* R_TempSprite(float* pos, float* dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;

	if (!modelIndex)
		return NULL;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return NULL;
	}

	frameCount = ModelFrameCount(pmod);

	ptent = efx.CL_TempEntAlloc(pos, pmod);

	if (ptent == NULL)
		return NULL;

	ptent->frameMax = (float)frameCount;
	ptent->entity.curstate.framerate = 10.0;
	ptent->entity.curstate.scale = scale;
	ptent->entity.curstate.rendermode = rendermode;
	ptent->entity.curstate.rendercolor.r = ptent->entity.curstate.rendercolor.g = ptent->entity.curstate.rendercolor.b = 255;
	ptent->entity.curstate.renderfx = renderfx;
	ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = int(a * 255.0);
	ptent->flags |= flags;

	VectorCopy(dir, ptent->entity.baseline.origin);
	VectorCopy(pos, ptent->entity.origin);

	ptent->die = (life == .0f) ? ptent->frameMax * 0.1 + cl.time + 1.0 : life + cl.time;
	ptent->entity.curstate.frame = 0.0;

	return ptent;
}

//-----------------------------------------------------------------------------
// Purpose: Spray sprite
// Input  : *pos - 
//			*dir - 
//			modelIndex - 
//			count - 
//			speed - 
//			iRand - 
//-----------------------------------------------------------------------------
void R_Sprite_Spray(vec_t* pos, vec_t* dir, int modelIndex, int count, int speed, int iRand)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	float noise, znoise;

	noise = (float)iRand / 100.0f;
	znoise = min(noise * 1.5f, 1.0f);

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return;
	}

	frameCount = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		ptent = efx.CL_TempEntAlloc(pos, pmod);

		if (ptent == NULL)
			return;

		ptent->entity.curstate.rendermode = kRenderTransAlpha;
		ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 255;
		ptent->flags |= FTENT_FADEOUT | FTENT_SLOWGRAVITY;
		ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
		ptent->entity.curstate.framerate = 0.5;
		ptent->fadeSpeed = 2.0;

		ptent->entity.baseline.origin[0] = dir[ 0 ] + RandomFloat ( -noise, noise );
		ptent->entity.baseline.origin[1] = dir[ 1 ] + RandomFloat ( -noise, noise );
		ptent->entity.baseline.origin[2] = dir[ 2 ] + RandomFloat ( 0, znoise );

		VectorScale(ptent->entity.baseline.origin, RandomFloat((float)speed * 0.8, (float)speed * 1.2), ptent->entity.baseline.origin);
		VectorCopy(pos, ptent->entity.origin);
		ptent->die = cl.time + 0.35;
		ptent->entity.curstate.frame = (float)RandomLong(0, frameCount - 1);
	}
}

void R_Spray(vec_t* pos, vec_t* dir, int modelIndex, int count, int speed, int spread, int rendermode)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	float noise, znoise;

	noise = (float)spread / 100.0;
	znoise = min(noise * 1.5, 1.0);

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return;
	}

	frameCount = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		ptent = efx.CL_TempEntAlloc(pos, pmod);

		if (ptent == NULL)
			return;

		ptent->entity.curstate.rendermode = rendermode;
		ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 255;
		ptent->flags |= FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;
		ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
		ptent->entity.curstate.framerate = 1;

		if (frameCount > 2)
		{
			ptent->flags |= FTENT_SPRANIMATE;
			ptent->entity.curstate.framerate = 10;
			ptent->die = (float)frameCount * 0.1 + cl.time;
		}
		else
			ptent->die = cl.time + 0.35;

		ptent->entity.baseline.origin[0] = dir[ 0 ] + RandomFloat ( -noise, noise );
		ptent->entity.baseline.origin[1] = dir[ 1 ] + RandomFloat ( -noise, noise );
		ptent->entity.baseline.origin[2] = dir[ 2 ] + RandomFloat ( 0, znoise );

		VectorScale(ptent->entity.baseline.origin, RandomFloat((float)speed * 0.8, (float)speed * 1.2), ptent->entity.baseline.origin);
		VectorCopy(pos, ptent->entity.origin);

		ptent->entity.curstate.frame = 0;
		ptent->frameMax = frameCount - 1;
	}
}

void R_PlayerSprites(int client, int modelIndex, int count, int size)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	vec3_t position, dir;
	float flMinScale;

	flMinScale = 1.0 - (float)size / 100.0;

	if (!CL_IsPlayerIndex(client))
	{
		Con_Printf(const_cast<char*>("Bad ent in R_PlayerSprites!\n"));
		return;
	}

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return;
	}

	frameCount = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		position[0] = RandomFloat(-10, 10) + cl_entities[client].origin[0];
		position[1] = RandomFloat(-10, 10) + cl_entities[client].origin[1];
		position[2] = RandomFloat(-20, 36) + cl_entities[client].origin[2];

		ptent = efx.CL_TempEntAlloc(position, pmod);

		if (ptent == NULL)
			return;

		VectorSubtract(ptent->entity.origin, cl_entities[client].origin, ptent->tentOffset);
		if (i)
		{
			ptent->flags |= FTENT_PLYRATTACHMENT;
			ptent->clientIndex = client;
		}
		else
		{
			VectorSubtract(position, cl_entities[client].origin, dir);
			VectorNormalize(dir);
			VectorScale(dir, 60.0, dir);
			VectorCopy(dir, ptent->entity.baseline.origin);
			ptent->entity.baseline.origin[2] = RandomFloat(20.0, 60.0);
		}

		ptent->entity.curstate.rendermode = kRenderNormal;
		ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 255;
		ptent->entity.baseline.renderfx = kRenderFxNoDissipation;
		ptent->entity.curstate.framerate = RandomFloat(flMinScale, 1.0);

		if (frameCount > 2)
		{
			ptent->flags |= FTENT_SPRANIMATE;
			ptent->entity.curstate.framerate = 20;
			ptent->die = (float)frameCount * 0.05 + cl.time;
		}
		else
			ptent->die = cl.time + 0.35;

		ptent->entity.curstate.frame = 0;
		ptent->frameMax = frameCount - 1;
	}
}

void R_FireField(vec_t* org, int radius, int modelIndex, int count, int flags, float life)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;
	vec3_t position;
	float flTick;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return;
	}

	frameCount = ModelFrameCount(pmod);

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < 3; j++)
			position[j] = ((flags & TEFIRE_FLAG_PLANAR) && j == 2) ? org[j] : RandomFloat(-radius, radius) + org[j];

		ptent = efx.CL_TempEntAlloc(position, pmod);

		if (ptent == NULL)
			return;

		if (flags & TEFIRE_FLAG_ALPHA)
		{
			ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 128;
			ptent->entity.curstate.rendermode = kRenderTransAlpha;
			ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
		}
		else if (flags & TEFIRE_FLAG_ADDITIVE)
		{
			ptent->entity.curstate.rendermode = kRenderTransAdd;
			ptent->entity.curstate.renderamt = 180;
		}
		else
		{
			ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
			ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 255;
			ptent->entity.curstate.rendermode = kRenderNormal;
		}

		ptent->entity.curstate.framerate = RandomFloat(0.75, 1.25);
		flTick = RandomFloat(-0.25, 0.5) + life;
		ptent->die = flTick + cl.time;

		if (frameCount > 2)
		{
			ptent->entity.curstate.framerate = (float)frameCount / flTick;
			ptent->flags |= FTENT_SPRANIMATE;
			if (flags & TEFIRE_FLAG_LOOP)
			{
				ptent->entity.curstate.framerate = 15.0;
				ptent->flags |= FTENT_SPRANIMATELOOP;
			}
		}

		ptent->frameMax = frameCount;
		ptent->entity.curstate.frame = 0;
		ptent->entity.baseline.origin[0] = ptent->entity.baseline.origin[1] = ptent->entity.baseline.origin[2] = 0;

		if (flags & TEFIRE_FLAG_ALLFLOAT || flags & TEFIRE_FLAG_SOMEFLOAT && !RandomLong(0, 1))
			ptent->entity.baseline.origin[ROLL] = RandomFloat(10.0, 30.0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attaches entity to player
// Input  : client - 
//			modelIndex - 
//			zoffset - 
//			life - 
//-----------------------------------------------------------------------------
void R_AttachTentToPlayer2(int client, model_t* pModel, float zoffset, float life)
{
	vec3_t position;
	TEMPENTITY* ptent;

	if (!CL_IsPlayerIndex(client))
	{
		Con_Printf(const_cast<char*>("Bad client in R_AttachTentToPlayer()!\n"));
		return;
	}

	if (cl_entities[client].curstate.messagenum != cl.parsecount)
		return;

	VectorCopy(cl_entities[client].origin, position);

	position[ROLL] += zoffset;

	ptent = efx.CL_TempEntAllocHigh(position, pModel);

	if (ptent == NULL)
	{
		Con_Printf(const_cast<char*>("No temp ent.\n"));
		return;
	}

	ptent->entity.curstate.rendermode = kRenderNormal;
	ptent->tentOffset[ROLL] = zoffset;
	ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
	ptent->entity.curstate.renderamt = 255;
	ptent->entity.baseline.renderamt = 255;
	ptent->entity.curstate.framerate = 1.0;
	ptent->clientIndex = client;
	ptent->tentOffset[PITCH] = 0.0;
	ptent->tentOffset[YAW] = 0.0;
	ptent->die = life + cl.time;

	ptent->flags |= FTENT_PLYRATTACHMENT | FTENT_PERSIST;

	if (ptent->entity.model->type == mod_sprite)
	{
		ptent->flags |= FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP;
		ptent->entity.curstate.framerate = 10.0;
		ptent->frameMax = ModelFrameCount(pModel);
	}
	else
		ptent->frameMax = 0;

	ptent->entity.curstate.frame = 0;
}

void R_AttachTentToPlayer(int client, int modelIndex, float zoffset, float life)
{
	model_t* pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return;
	}

	R_AttachTentToPlayer2(client, pmod, zoffset, life);
}

//-----------------------------------------------------------------------------
// Purpose: Detach entity from player
//-----------------------------------------------------------------------------
void R_KillAttachedTents(int client)
{
	if (!CL_IsPlayerIndex(client))
	{
		Con_Printf(const_cast<char*>("Bad client in R_KillAttachedTents()!\n"));
		return;
	}

	for (TEMPENTITY* ptent = gpTempEntActive; ptent != NULL; ptent = ptent->next)
	{
		if (ptent->flags & FTENT_FADEOUT && ptent->clientIndex == client)
			ptent->die = cl.time;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spark effect. Nothing but sparks.
// Input  : pos - origin point of effect
//-----------------------------------------------------------------------------
void R_SparkEffect(float* pos, int count, int velocityMin, int velocityMax)
{
	efx.R_SparkStreaks(pos, count, velocityMin, velocityMax);
	efx.R_RicochetSprite(pos, cl_sprite_ricochet, 0.1, RandomFloat(0.5, 1.0));
}

void R_FunnelSprite(float* org, int modelIndex, int reverse)
{
	TEMPENTITY *ptent;
	model_t *pmod;
	int frameCount;
	vec3_t dir, tmp;
	float flDist, flHigh;

	if (!modelIndex)
	{
		Con_Printf(const_cast<char*>("No modelindex for funnel!!\n"));
		return;
	}

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No model %d!\n"), modelIndex);
		return;
	}

	frameCount = ModelFrameCount(pmod);

	for (int i = -8; i < 8; i++)
	for (int j = -8; j < 8; j++)
	{
		ptent = efx.CL_TempEntAlloc(org, pmod);

		if (ptent == NULL)
			return;
	
		tmp[0] = org[0] + (float)(i * 64);
		tmp[1] = org[1] + (float)(j * 64);
		tmp[2] = org[2] + RandomFloat(100, 800);

		if (reverse)
		{
			VectorCopy(org, ptent->entity.origin);
			VectorSubtract(tmp, ptent->entity.origin, dir);
		}
		else
		{
			VectorCopy(tmp, ptent->entity.origin);
			VectorSubtract(org, ptent->entity.origin, dir);
		}

		ptent->entity.curstate.rendermode = kRenderGlow;
		ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
		ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 200;
		ptent->entity.curstate.framerate = 10;
		ptent->frameMax = (float)frameCount;

		ptent->flags = FTENT_FADEOUT | FTENT_ROTATE;
		ptent->entity.curstate.framerate = RandomFloat(0.1, 0.4);
		flDist = VectorNormalize(dir);
		flHigh = RandomFloat(64.0, 128.0) + min(tmp[2] * 0.125f, 64.0f);
		VectorScale(dir, flHigh, ptent->entity.baseline.origin);
		ptent->fadeSpeed = 2;
		ptent->die = flDist / flHigh - 0.5f + cl.time;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create ricochet sprite
// Input  : *pos - 
//			*pmodel - 
//			duration - 
//			scale - 
//-----------------------------------------------------------------------------
void R_RicochetSprite(float* pos, model_t* pmodel, float duration, float scale)
{
	TEMPENTITY* ptent;

	ptent = efx.CL_TempEntAlloc(pos, pmodel);

	if (ptent == NULL)
		return;

	ptent->entity.curstate.rendermode = kRenderGlow;
	ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
	ptent->entity.curstate.renderamt = ptent->entity.baseline.renderamt = 200;
	ptent->flags = FTENT_FADEOUT;
	for (int j = 0; j < 3; j++)
	{
		ptent->entity.origin[j] = pos[j];
		ptent->entity.baseline.origin[j] = 0.0;
	}
	ptent->entity.curstate.scale = scale;
	ptent->fadeSpeed = 8.0;
	ptent->die = cl.time;
	ptent->entity.curstate.frame = 0;
	ptent->entity.angles[ROLL] = (float)(RandomLong(0, 7) * 45);
}

void R_RocketFlare(float* pos)
{
	TEMPENTITY* ptent;
	int frameCount;

	if (cl.time == cl.oldtime)
		return;

	frameCount = ModelFrameCount(cl_sprite_glow);
	ptent = efx.CL_TempEntAlloc(pos, cl_sprite_glow);

	if (ptent == NULL)
		return;

	ptent->frameMax = (float)frameCount;
	ptent->entity.curstate.rendermode = kRenderGlow;
	ptent->entity.curstate.renderamt = 255;
	ptent->entity.curstate.renderfx = kRenderFxNoDissipation;
	ptent->entity.curstate.framerate = 1;
	VectorCopy(pos, ptent->entity.origin);
	ptent->die = cl.time + 0.01;
	ptent->entity.curstate.frame = (float)RandomLong(0, frameCount - 1);
}

//-----------------------------------------------------------------------------
// Purpose: Create blood sprite
// Input  : *org - 
//			colorindex - 
//			modelIndex - 
//			modelIndex2 - 
//			size - 
//-----------------------------------------------------------------------------
void R_BloodSprite(vec_t* org, int colorindex, int modelIndex, int modelIndex2, float size)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount, colIdx, lim;
	WORD impactcolor[3], partcolor[3];

	colIdx = RandomLong(1, 3) + colorindex;

	impactcolor[0] = ((PackedColorVec*)host_basepal)[colIdx].b;
	impactcolor[1] = ((PackedColorVec*)host_basepal)[colIdx].g;
	impactcolor[2] = ((PackedColorVec*)host_basepal)[colIdx].r;

	partcolor[0] = ((PackedColorVec*)host_basepal)[colIdx - 1].b;
	partcolor[1] = ((PackedColorVec*)host_basepal)[colIdx - 1].g;
	partcolor[2] = ((PackedColorVec*)host_basepal)[colIdx - 1].r;

	if (modelIndex2)
	{
		pmod = CL_GetModelByIndex(modelIndex2);

		if (pmod != NULL)
		{
			frameCount = ModelFrameCount(pmod);
			lim = RandomLong(1, 8) + RandomLong(1, 8) + size;

			for (int i = 0; i < lim; i++)
			{
				ptent = efx.CL_TempEntAlloc(org, pmod);

				if (ptent == NULL)
					break;

				ptent->entity.curstate.rendermode = kRenderNormal;
				ptent->entity.curstate.renderfx = kRenderFxClampMinScale;
				ptent->entity.baseline.renderamt = ptent->entity.curstate.renderamt = 250;

				ptent->flags = FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY | FTENT_ROTATE;
				ptent->entity.curstate.rendercolor.r = partcolor[0];
				ptent->entity.curstate.rendercolor.g = partcolor[1];
				ptent->entity.curstate.rendercolor.b = partcolor[2];
				ptent->entity.curstate.scale = RandomFloat(size / 15.0, size / 25.0);
				for (int j = 0; j < 3; j++)
				{
					ptent->entity.baseline.origin[j] = RandomFloat(j == 2 ? -32 : -96.0, 95.0);
					ptent->entity.baseline.angles[j] = RandomFloat(-256.0, -255.0);
				}
				ptent->entity.curstate.framerate = 0.0;
				ptent->die = RandomFloat(1.0, 2.0) + cl.time;
				ptent->entity.curstate.frame = RandomLong(1, frameCount - 1);
				if (ptent->entity.curstate.frame > 8.0)
					ptent->entity.curstate.frame = frameCount - 1;
				ptent->bounceFactor = 0.0;
				ptent->entity.angles[ROLL] = (float)RandomLong(0, 360);
			}
		}
	}

	if (!modelIndex)
		return;

	pmod = CL_GetModelByIndex(modelIndex);

	if (pmod == NULL)
		return;

	frameCount = ModelFrameCount(pmod);

	ptent = efx.CL_TempEntAllocHigh(org, pmod);

	if (ptent == NULL)
		return;

	ptent->entity.curstate.rendermode = kRenderNormal;
	ptent->entity.curstate.renderfx = kRenderFxClampMinScale;
	ptent->entity.baseline.renderamt = ptent->entity.curstate.renderamt = 250;

	ptent->flags = FTENT_SPRANIMATE;
	ptent->entity.baseline.origin[0] = ptent->entity.baseline.origin[1] = ptent->entity.baseline.origin[2] = 0.0;
	ptent->entity.curstate.rendercolor.r = impactcolor[0];
	ptent->entity.curstate.rendercolor.g = impactcolor[1];
	ptent->entity.curstate.rendercolor.b = impactcolor[2];
	ptent->entity.curstate.scale = RandomFloat(size / 25.0, size / 35.0);
	ptent->entity.curstate.framerate = 4 * frameCount;
	ptent->die = (float)frameCount / ptent->entity.curstate.framerate + cl.time;
	ptent->frameMax = frameCount;
	ptent->entity.curstate.frame = 0;
	ptent->bounceFactor = 0.0;
	ptent->entity.angles[ROLL] = (float)RandomLong(0, 360);
}

//-----------------------------------------------------------------------------
// Purpose: Create default sprite TE
// Input  : *pos - 
//			spriteIndex - 
//			framerate - 
// Output : TEMPENTITY
//-----------------------------------------------------------------------------
TEMPENTITY* R_DefaultSprite(float* pos, int spriteIndex, float framerate)
{
	TEMPENTITY* ptent;
	model_t* pmod;
	int frameCount;

	if (cl.time == cl.oldtime)
		return nullptr;

	if (!spriteIndex)
		return NULL;

	pmod = CL_GetModelByIndex(spriteIndex);
	if (pmod == NULL)
	{
		Con_Printf(const_cast<char*>("No Sprite %d!\n"), spriteIndex);
		return NULL;
	}

	frameCount = ModelFrameCount(pmod);

	ptent = efx.CL_TempEntAlloc(pos, pmod);

	if (ptent == NULL)
		return NULL;

	ptent->flags |= FTENT_SPRANIMATE;
	ptent->entity.curstate.framerate = framerate != 0.0 ? framerate : 10.0;
	ptent->frameMax = (float)frameCount;
	ptent->die = ptent->frameMax / ptent->entity.curstate.framerate + cl.time;
	ptent->entity.curstate.frame = 0.0;
	VectorCopy(pos, ptent->entity.origin);

	return ptent;
}

//-----------------------------------------------------------------------------
// Purpose: Create sprite smoke
// Input  : *pTemp - 
//			scale - 
//-----------------------------------------------------------------------------
void R_Sprite_Smoke(TEMPENTITY* pTemp, float scale)
{
	if (pTemp == NULL)
		return;

	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.baseline.origin[ROLL] = 30.0;
	pTemp->entity.curstate.rendercolor.r = pTemp->entity.curstate.rendercolor.g = pTemp->entity.curstate.rendercolor.b = RandomLong(20, 35);
	pTemp->entity.origin[ROLL] += 20;
	pTemp->entity.curstate.scale = scale;
}

void R_Sprite_Explode(TEMPENTITY* pTemp, float scale, int flags)
{
	if (pTemp == NULL)
		return;

	if (flags & FTENT_SINEWAVE)
	{
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.renderamt = 255;
	}
	else
	{
		pTemp->entity.curstate.rendermode = kRenderTransAdd;
		pTemp->entity.curstate.renderamt = 180;
	}
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[ROLL] = 8.0;
	pTemp->entity.curstate.rendercolor.r = pTemp->entity.curstate.rendercolor.g = pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->entity.origin[ROLL] += 10;
	pTemp->entity.curstate.scale = scale;
}

void R_Sprite_WallPuff(TEMPENTITY* pTemp, float scale)
{
	if (pTemp == NULL)
		return;

	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = 0;
	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.rendercolor.r = pTemp->entity.curstate.rendercolor.g = pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->entity.curstate.scale = scale;
	pTemp->die = cl.time + 0.01;
	pTemp->entity.curstate.frame = 0.0;
	pTemp->entity.angles[ROLL] = (float)RandomLong(0, 359);
}

void R_MultiGunshot(vec_t* org, vec_t* dir, vec_t* noise, int count, int decalCount, int* decalIndices)
{
	vec3_t vecDir, vecEnd, vecSpread1, vecSpread2, vecUp, vecRight, angles;
	pmtrace_t pmtrace;
	float x, y;
	int iDecal;

	VectorAngles(dir, angles);
	AngleVectors(angles, 0, vecRight, vecUp);

	for (int i = 0; i < count; i++)
	{
		do
		{
			x = RandomFloat(-0.5, 0.5) + RandomFloat(-0.5, 0.5);
			y = RandomFloat(-0.5, 0.5) + RandomFloat(-0.5, 0.5);
		} while (x * x + y * y > 1.0);

		iDecal = RandomLong(0, decalCount - 1);
		VectorScale(vecRight, x * noise[0], vecSpread1);
		VectorScale(vecUp, y * noise[1], vecSpread2);
		for (int j = 0; j < 3; j++)
			vecDir[j] = vecSpread1[j] + vecSpread2[j] + dir[j];
		VectorScale(vecDir, 4096.0, vecEnd);
		VectorAdd(vecEnd, org, vecEnd);
		pmove->usehull = 2;
		pmtrace = PM_PlayerTrace(org, vecEnd, 1, -1);

		if (pmtrace.fraction == 1)
			continue;

		if (i & 2)
			efx.R_RicochetSound(pmtrace.endpos);

		efx.R_BulletImpactParticles(pmtrace.endpos);

		if (!r_decals.value)
			continue;

		efx.R_DecalShoot(efx.Draw_DecalIndex(decalIndices[iDecal]), pmove->physents[pmtrace.ent].info, 0, pmtrace.endpos, 0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play muzzle flash
// Input  : *pos1 - 
//			type - 
//-----------------------------------------------------------------------------
void R_MuzzleFlash(float* pos1, int type)
{
	TEMPENTITY* ptent;
	int frameCount;
	float scale;

	scale = float(type / 10) * 0.1;
	if (!scale)
		scale = 0.5;
	type %= 10;

	frameCount = ModelFrameCount(cl_sprite_muzzleflash[type % 3]);
	ptent = efx.CL_TempEntAlloc(pos1, cl_sprite_muzzleflash[type % 3]);

	if (ptent == NULL)
		return;

	ptent->entity.curstate.rendermode = kRenderTransAdd;
	ptent->entity.curstate.renderamt = 255;

	ptent->entity.curstate.scale = scale;
	ptent->entity.curstate.renderfx = kRenderFxNone;
	ptent->entity.curstate.rendercolor.r = ptent->entity.curstate.rendercolor.g = ptent->entity.curstate.rendercolor.b = 255;
	VectorCopy(pos1, ptent->entity.origin);
	ptent->die = cl.time + 0.01;
	ptent->entity.curstate.frame = (float)RandomLong(0, frameCount - 1);
	ptent->entity.angles[0] = ptent->entity.angles[1] = ptent->entity.angles[2] = 0;
	ptent->frameMax = (float)frameCount;

	ptent->entity.angles[ROLL] = (type % 3) != 0 ? RandomLong(0, 359) : RandomLong(0, 20);
	AppendTEntity(&ptent->entity);
}

void R_ParticleLine(float* start, float* end, byte r, byte g, byte b, float life)
{
	PM_ParticleLine(start, end, R_LookupColor(r, g, b), life, 0.0);
}

void R_ParticleBox(float* mins, float* maxs, byte r, byte g, byte b, float life)
{
	vec3_t mmin, mmax, origin;

	for (int i = 0; i < 3; i++)
	{
		origin[i] = (mins[i] + maxs[i]) / 2;
		mmax[i] = maxs[i] - origin[i];
		mmin[i] = mins[i] - origin[i];
	}

	PM_DrawBBox(mmin, mmax, origin, R_LookupColor(r, g, b), life);
}

void CL_ParseTextMessage(void)
{
	client_textmessage_t* pNetMessage;

	int channel = MSG_ReadByte() % 4;

	pNetMessage = &gNetworkTextMessage[channel];

	pNetMessage->x = (float)MSG_ReadShort() / 8192.f;
	pNetMessage->y = (float)MSG_ReadShort() / 8192.f;

	pNetMessage->effect = MSG_ReadByte();

	pNetMessage->r1 = MSG_ReadByte();
	pNetMessage->g1 = MSG_ReadByte();
	pNetMessage->b1 = MSG_ReadByte();
	pNetMessage->a1 = MSG_ReadByte();
	pNetMessage->r2 = MSG_ReadByte();
	pNetMessage->g2 = MSG_ReadByte();
	pNetMessage->b2 = MSG_ReadByte();
	pNetMessage->a2 = MSG_ReadByte();

	pNetMessage->fadein = (float)MSG_ReadWord() / 256.f;
	pNetMessage->fadeout = (float)MSG_ReadWord() / 256.f;
	pNetMessage->holdtime = (float)MSG_ReadWord() / 256.f;

	if (pNetMessage->effect == 2)
		pNetMessage->fxtime = (float)MSG_ReadWord() / 256.f;

	pNetMessage->pMessage = gNetworkTextMessageBuffer[channel];
	pNetMessage->pName = gNetworkMessageNames[channel];

	Q_strncpy((char*)pNetMessage->pMessage, MSG_ReadString(), 511);
	((char*)pNetMessage->pMessage)[511] = 0;

	CL_HudMessage(pNetMessage->pName);
}

void CL_ParseTEnt(void)
{
	int type;
	/*byte startFrame;
	int startEnt, endEnt, radius, modelIndex, spriteIndex, decalIndex, entIndex;
	vec3_t pos, endpos, dir, size;
	float framerate, scale, life, width, length, zoffset, amplitude, brightness, r, g, b, speed, height;
	dlight_t *pdl;
	int colorStart, colorLength, spread, rendermode, owner, playerindex, decIdx, count, soundtype, color, ivel, flags, density;
	vec_t mins[4], maxs[4], origin[4];
	customization_t *pcustom;
	texture_t *pdecal;*/

	// первый байт сообщения типа tent - сигнал
	type = MSG_ReadByte();

	switch (type)
	{
	case TE_BEAMPOINTS:
	case TE_BEAMENTPOINT:
	case TE_BEAMENTS:
	{
		int startEnt, endEnt, modelIndex, startFrame;
		vec3_t pos, endpos;
		float framerate, life, r, g, b, brightness, speed;
		int width, noise;

		if (type == TE_BEAMENTS)
		{
			startEnt = MSG_ReadShort();
			endEnt = MSG_ReadShort();
		}
		else
		{
			if (type == TE_BEAMENTPOINT)
			{
				startEnt = MSG_ReadShort();
			}
			else
			{
				pos[0] = MSG_ReadCoord(&net_message);
				pos[1] = MSG_ReadCoord(&net_message);
				pos[2] = MSG_ReadCoord(&net_message);
			}
			endpos[0] = MSG_ReadCoord(&net_message);
			endpos[1] = MSG_ReadCoord(&net_message);
			endpos[2] = MSG_ReadCoord(&net_message);
		}

		modelIndex = MSG_ReadShort();

		// framestart
		startFrame = MSG_ReadByte();

		framerate = (float)MSG_ReadByte() / 10.f;
		life = (float)MSG_ReadByte() / 10.f;

		width = MSG_ReadByte();

		// noise amplitude
		noise = MSG_ReadByte();

		r = (float)MSG_ReadByte() / 255.f;
		g = (float)MSG_ReadByte() / 255.f;
		b = (float)MSG_ReadByte() / 255.f;

		// renderamt
		brightness = (float)MSG_ReadByte() / 255.f;

		speed = (float)MSG_ReadByte() / 10.f;

		switch (type)
		{
		case TE_BEAMPOINTS:
			efx.R_BeamPoints(pos, endpos, modelIndex, life, (float)width / 10.f, (float)noise / 100.f, brightness, speed, startFrame, framerate, r, g, b);
			break;
		case TE_BEAMENTPOINT:
			efx.R_BeamEntPoint(startEnt, endpos, modelIndex, life, (float)width / 10.f, (float)noise / 100.f, brightness, speed, startFrame, framerate, r, g, b);
			break;
		case TE_BEAMENTS:
			efx.R_BeamEnts(startEnt, endEnt, modelIndex, life, (float)width / 10.f, (float)noise / 100.f, brightness, speed, startFrame, framerate, r, g, b);
			break;
		}
	}
		break;
	case TE_GUNSHOT:
	{
		vec3_t pos;
		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		efx.R_RunParticleEffect(pos, vec3_origin, 0, 20);
		efx.R_RicochetSound(pos);
	}
		break;
	case TE_EXPLOSION:
	{
		vec3_t pos;
		int spriteIndex, flags;
		float scale, framerate;
		dlight_t* dl;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		spriteIndex = MSG_ReadShort();
		scale = (float)MSG_ReadByte() / 10.f;
		framerate = (float)MSG_ReadByte();
		flags = MSG_ReadByte();
		
		R_Explosion(pos, spriteIndex, scale, framerate, flags);	
	}
		break;
	case TE_TAREXPLOSION:
	{
		vec3_t pos;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		efx.R_BlobExplosion(pos);
		S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_r_exp1, pos, VOL_NORM, 1.0, 0, PITCH_NORM);
	}
		break;
	case TE_SMOKE:
	{
		vec3_t pos;
		int spriteIndex;
		float scale, framerate;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		spriteIndex = MSG_ReadShort();
		scale = (float)MSG_ReadByte() * 10.f;
		framerate = (float)MSG_ReadByte();
		efx.R_Sprite_Smoke(R_DefaultSprite(pos, spriteIndex, framerate), scale);
	}
		break;
	case TE_TRACER:
	{
		vec3_t pos, endpos;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);
		efx.R_TracerEffect(pos, endpos);
	}
		break;
	// unused lighning effect
	case TE_LIGHTNING:
	{
		vec3_t pos, endpos;
		float life, width, amplitude;
		int modelIndex;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);

		life = (float)MSG_ReadByte() / 10.f;
		width = (float)MSG_ReadByte() / 10.f;
		amplitude = (float)MSG_ReadByte() / 100.f;

		modelIndex = MSG_ReadShort();

		efx.R_BeamLightning(pos, endpos, modelIndex, life, width, amplitude, 0.6, 3.5);
	}
		break;
	case TE_SPARKS:
	{
		vec3_t pos;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		efx.R_SparkEffect(pos, 8, -200, 200);
	}
		break;
	// unused quake effect
	case TE_LAVASPLASH:
	{
		vec3_t pos;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		efx.R_LavaSplash(pos);
	}
		break;
	// unused quake effect
	case TE_TELEPORT:
	{
		vec3_t pos;
		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		efx.R_TeleportSplash(pos);
		break;
	}
	// unused quake effect
	case TE_EXPLOSION2:
	{
		dlight_t* dl;
		vec3_t pos;
		int colorStart, colorLength;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();
		efx.R_ParticleExplosion2(pos, colorStart, colorLength);
		dl = efx.CL_AllocDlight(NULL);
		VectorCopy(pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_r_exp1, pos, VOL_NORM, 0.6, 0, PITCH_NORM);
	}
		break;
	case TE_BSPDECAL:
	case TE_DECAL:
	case TE_WORLDDECAL:
	case TE_WORLDDECALHIGH:
	case TE_DECALHIGH:
	{
		vec3_t pos;
		int decalTextureIndex, entNumber, modelIndex, flags;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		if (type == TE_BSPDECAL)
		{
			decalTextureIndex = MSG_ReadShort();
			entNumber = MSG_ReadShort();
			modelIndex = !entNumber ? entNumber : MSG_ReadShort();
			flags = FDECAL_PERMANENT;
		}
		else
		{
			decalTextureIndex = MSG_ReadByte();
			entNumber = (type == TE_DECALHIGH || type == TE_DECAL) ? MSG_ReadShort() : 0;
			modelIndex = 0;
			flags = 0;
		}

		if (type == TE_WORLDDECALHIGH || type == TE_DECALHIGH)
			decalTextureIndex += 256;

		if (entNumber >= cl.max_edicts)
			Sys_Error("Decal: entity = %i", entNumber);

		if (r_decals.value)
			efx.R_DecalShoot(efx.Draw_DecalIndex(decalTextureIndex), entNumber, modelIndex, pos, flags);
	}
		break;
	case TE_IMPLOSION:
	{
		vec3_t pos;
		float radius, life;
		int count;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		radius = (float)MSG_ReadByte();
		count = MSG_ReadByte();
		life = (float)MSG_ReadByte() / 10.f;

		efx.R_Implosion(pos, radius, count, life);
	}
		break;
	// unused ricocheting sprite
	case TE_SPRITETRAIL:
	{
		vec3_t pos, endpos;
		int modelIndex, count;
		float life, scale, speed, noise;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		life = (float)MSG_ReadByte() / 10.f;
		scale = (float)MSG_ReadByte();

		if (!scale)
			scale = 1;
		else
			scale *= 0.1;

		noise = (float)MSG_ReadByte() * 10.f;
		speed = (float)MSG_ReadByte() * 10.f;

		efx.R_Sprite_Trail(TE_SPRITETRAIL, pos, endpos, modelIndex, count, life, scale, noise, 255, speed);
	}
		break;
	case TE_SPRITE:
	{
		vec3_t pos;
		int modelIndex;
		float scale, brightness;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		modelIndex = MSG_ReadShort();
		scale = (float)MSG_ReadByte() / 10.f;
		brightness = (float)MSG_ReadByte() / 255.f;

		efx.R_TempSprite(pos, vec3_origin, scale, modelIndex, kRenderTransAdd, kRenderFxNone, brightness, 0, FTENT_SPRANIMATE);
	}
		break;
	case TE_BEAMSPRITE:
	{
		vec3_t pos, endpos;
		int spriteIndex, endSpriteIndex;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);
		spriteIndex = MSG_ReadShort();
		endSpriteIndex = MSG_ReadShort();
		// setup beam
		efx.R_BeamPoints(pos, endpos, spriteIndex, 0.01, 0.4, 0, RandomFloat(0.5, 0.655), 5.0, 0, 0, 1.0, 0.0, 0.0);
		efx.R_TempSprite(endpos, vec3_origin, 0.1, endSpriteIndex, kRenderTransAdd, kRenderFxNone, 0.35, 0.01, 0);
	}
		break;
	case TE_BEAMTORUS:
	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
	{
		vec3_t pos, endpos;
		int modelIndex, startFrame;
		float framerate, life, width, noise, r, g, b, brightness, speed;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		startFrame = MSG_ReadByte();
		framerate = (float)MSG_ReadByte() / 10.f;
		life = (float)MSG_ReadByte() / 10.f;
		width = (float)MSG_ReadByte();
		noise = (float)MSG_ReadByte() / 100.f;
		r = (float)MSG_ReadByte() / 255.f;
		g = (float)MSG_ReadByte() / 255.f;
		b = (float)MSG_ReadByte() / 255.f;
		brightness = (float)MSG_ReadByte() / 255.f;
		speed = (float)MSG_ReadByte() / 10.f;

		efx.R_BeamCirclePoints(type, pos, endpos, modelIndex, life, width, noise, brightness, speed, startFrame, framerate, r, g, b);
	}
		break;
	case TE_BEAMFOLLOW:
	{
		int startEnt, modelIndex;
		float life, width, r, g, b, brightness;

		startEnt = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		life = (float)MSG_ReadByte() / 10.f;
		width = (float)MSG_ReadByte();
		r = (float)MSG_ReadByte() / 255.f;
		g = (float)MSG_ReadByte() / 255.f;
		b = (float)MSG_ReadByte() / 255.f;
		brightness = (float)MSG_ReadByte() / 255.f;
		efx.R_BeamFollow(startEnt, modelIndex, life, width, r, g, b, brightness);
	}
		break;
	case TE_GLOWSPRITE:
	{
		vec3_t pos;
		int modelIndex;
		float life, scale, brightness;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		modelIndex = MSG_ReadShort();
		life = (float)MSG_ReadByte() / 10.f;
		scale = (float)MSG_ReadByte() / 10.f;
		brightness = (float)MSG_ReadByte() / 255.f;
		efx.R_TempSprite(pos, vec3_origin, scale, modelIndex, kRenderGlow, kRenderFxNoDissipation, brightness, life, FTENT_FADEOUT);
	}
		break;
	case TE_BEAMRING:
	{
		int startEnt, endEnt, modelIndex, startFrame;
		float framerate, life, width, noise, r, g, b, brightness, speed;

		startEnt = MSG_ReadShort();
		endEnt = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		startFrame = MSG_ReadByte();
		framerate = (float)MSG_ReadByte() / 10.f;
		life = (float)MSG_ReadByte() / 10.f;
		width = (float)MSG_ReadByte() / 10.f;
		noise = (float)MSG_ReadByte() / 100.f;
		r = (float)MSG_ReadByte() * 255.f;
		g = (float)MSG_ReadByte() * 255.f;
		b = (float)MSG_ReadByte() * 255.f;
		brightness = (float)MSG_ReadByte() * 255.f;
		speed = (float)MSG_ReadByte() / 10.f;
		efx.R_BeamRing(startEnt, endEnt, modelIndex, life, width, noise, brightness, speed, startFrame, framerate, r, g, b);
	}
		break;
	case TE_STREAK_SPLASH:
	{
		vec3_t pos, endpos, dir;
		int color, count, ivel;
		float speed;

		// origin
		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		// direction
		dir[0] = MSG_ReadCoord(&net_message);
		dir[1] = MSG_ReadCoord(&net_message);
		dir[2] = MSG_ReadCoord(&net_message);

		// streak color
		color = MSG_ReadByte();

		count = MSG_ReadShort();
		speed = (float)MSG_ReadShort();
		ivel = MSG_ReadShort();

		efx.R_StreakSplash(pos, dir, color, count, speed, -ivel, ivel);
	}
		break;
	case TE_DLIGHT:
	{
		vec3_t pos;
		dlight_t* dl;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		dl = efx.CL_AllocDlight(NULL);
		VectorCopy(pos, dl->origin);
		dl->radius = float(MSG_ReadByte() * 10.f);
		dl->color.r = MSG_ReadByte();
		dl->color.g = MSG_ReadByte();
		dl->color.b = MSG_ReadByte();
		dl->die = (float)MSG_ReadByte() / 10.f + cl.time;
		dl->decay = float(MSG_ReadByte() * 10);
	}
		break;
	case TE_ELIGHT:
	{
		float life;
		dlight_t* dl;
		int startEnt;

		startEnt = MSG_ReadShort();
		dl = efx.CL_AllocElight(startEnt);
		dl->origin[0] = MSG_ReadCoord(&net_message);
		dl->origin[1] = MSG_ReadCoord(&net_message);
		dl->origin[2] = MSG_ReadCoord(&net_message);
		dl->radius = MSG_ReadCoord(&net_message);
		dl->color.r = MSG_ReadByte();
		dl->color.g = MSG_ReadByte();
		dl->color.b = MSG_ReadByte();
		life = (float)MSG_ReadByte() / 10.f;
		dl->die = life + cl.time;
		dl->decay = MSG_ReadCoord(&net_message);
		if (life != 0.f)
			dl->decay /= life;
	}
		break;
	case TE_TEXTMESSAGE:
		CL_ParseTextMessage();
		break;
	case TE_LINE:
	{
		vec3_t pos, endpos;
		float life;
		byte br, bg, bb;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);

		life = (float)MSG_ReadShort() / 10.f;
		br = MSG_ReadByte();
		bg = MSG_ReadByte();
		bb = MSG_ReadByte();

		R_ParticleLine(pos, endpos, br, bg, bb, life);
	}
		break;
	case TE_BOX:
	{
		vec3_t pos, endpos;
		float life;
		byte br, bg, bb;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);
		life = (float)MSG_ReadShort() / 10.f;

		br = MSG_ReadByte();
		bg = MSG_ReadByte();
		bb = MSG_ReadByte();

		R_ParticleBox(pos, endpos, br, bg, bb, life);
	}
		break;
	case TE_KILLBEAM:
	{
		int startEnt = MSG_ReadShort();
		efx.R_BeamKill(startEnt);
	}
		break;
	case TE_LARGEFUNNEL:
	{
		vec3_t pos;
		int modelIndex, reverse;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		modelIndex = MSG_ReadShort();
		reverse = MSG_ReadShort();
		efx.R_LargeFunnel(pos, reverse);
		efx.R_FunnelSprite(pos, modelIndex, reverse);
	}
		break;
	case TE_BLOODSTREAM:
	{
		vec3_t pos, dir;
		int color, speed;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		dir[0] = MSG_ReadCoord(&net_message);
		dir[1] = MSG_ReadCoord(&net_message);
		dir[2] = MSG_ReadCoord(&net_message);

		color = MSG_ReadByte();
		speed = MSG_ReadByte();
		if (CL_ShowTEBlood())
			efx.R_BloodStream(pos, dir, color, speed);
	}
		break;
	case TE_SHOWLINE:
	{
		vec3_t pos, dir, endpos;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		endpos[0] = MSG_ReadCoord(&net_message);
		endpos[1] = MSG_ReadCoord(&net_message);
		endpos[2] = MSG_ReadCoord(&net_message);
		efx.R_ShowLine(pos, endpos);
	}
		break;
	case TE_BLOOD:
	{
		vec3_t pos, dir;
		int color, speed;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		dir[0] = MSG_ReadCoord(&net_message);
		dir[1] = MSG_ReadCoord(&net_message);
		dir[2] = MSG_ReadCoord(&net_message);

		color = MSG_ReadByte();
		speed = MSG_ReadByte();

		if (CL_ShowTEBlood())
			efx.R_Blood(pos, dir, color, speed);
	}
		break;
	case TE_FIZZ:
	{
		int entnumber, spriteIndex, density;

		entnumber = MSG_ReadShort();
		if (entnumber >= cl.max_edicts)
			Sys_Error("Bubble: entity = %i", entnumber);
		// bubble model
		spriteIndex = MSG_ReadShort();
		density = MSG_ReadByte();
		efx.R_FizzEffect(cl_entities + entnumber, spriteIndex, density);
	}
		break;
	case TE_MODEL:
	{
		vec3_t pos, dir, endpos;
		int modelIndex, soundtype;
		float life;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		dir[0] = MSG_ReadCoord(&net_message);
		dir[1] = MSG_ReadCoord(&net_message);
		dir[2] = MSG_ReadCoord(&net_message);
		endpos[0] = endpos[2] = 0;
		// rotation
		endpos[YAW] = MSG_ReadAngle();
		modelIndex = MSG_ReadShort();

		// fix: 27.12
		soundtype = MSG_ReadByte();

		life = (float)MSG_ReadByte() / 10.f;
		efx.R_TempModel(pos, dir, endpos, life, modelIndex, soundtype);
	}
		break;
	case TE_EXPLODEMODEL:
	{
		vec3_t org;
		float speed;
		int entnumber, count;
		float life;

		org[0] = MSG_ReadCoord(&net_message);
		org[1] = MSG_ReadCoord(&net_message);
		org[2] = MSG_ReadCoord(&net_message);

		speed = MSG_ReadCoord(&net_message);

		entnumber = MSG_ReadShort();
		count = MSG_ReadShort();
		life = (float)MSG_ReadByte() / 10.f;

		efx.R_TempSphereModel(org, speed, life, count, entnumber);
	}
		break;
	case TE_BREAKMODEL:
	{
		vec3_t pos, size, vel;
		float speed, life;
		int entnumber, count;
		char c;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		size[0] = MSG_ReadCoord(&net_message);
		size[1] = MSG_ReadCoord(&net_message);
		size[2] = MSG_ReadCoord(&net_message);

		vel[0] = MSG_ReadCoord(&net_message);
		vel[1] = MSG_ReadCoord(&net_message);
		vel[2] = MSG_ReadCoord(&net_message);

		speed = (float)MSG_ReadByte() * 10.f;
		entnumber = MSG_ReadShort();
		count = MSG_ReadByte();
		life = (float)MSG_ReadByte() / 10.f;
		c = (char)MSG_ReadByte();

		efx.R_BreakModel(pos, size, vel, speed, life, count, entnumber, c);
	}
		break;
	case TE_GUNSHOTDECAL:
	{
		vec3_t pos;
		int entnumber, decalTextureIndex, iRand;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);
		entnumber = MSG_ReadShort();
		decalTextureIndex = MSG_ReadByte();
		efx.R_BulletImpactParticles(pos);

		iRand = RandomLong(0, 32767);
		if (iRand < 16383)
		{
			switch (iRand % 5)
			{
			case 0:
				S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric1, pos, VOL_NORM, 1.0, 0, PITCH_NORM);
				break;
			case 1:
				S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric2, pos, VOL_NORM, 1.0, 0, PITCH_NORM);
				break;
			case 2:
				S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric3, pos, VOL_NORM, 1.0, 0, PITCH_NORM);
				break;
			case 3:
				S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric4, pos, VOL_NORM, 1.0, 0, PITCH_NORM);
				break;
			case 4:
				S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_ric5, pos, VOL_NORM, 1.0, 0, PITCH_NORM);
				break;
			default:
				break;
			}
		}

		if (entnumber >= cl.max_edicts)
			Sys_Error("Decal: entity = %i", entnumber);

		if (r_decals.value)
			efx.R_DecalShoot(efx.Draw_DecalIndex(decalTextureIndex), entnumber, 0, pos, 0);
	}
		break;
	case TE_SPRITE_SPRAY:
	{
		vec3_t pos, dir;
		int modelIndex, count, speed, iRand;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		dir[0] = MSG_ReadCoord(&net_message);
		dir[1] = MSG_ReadCoord(&net_message);
		dir[2] = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		speed = MSG_ReadByte();
		iRand = MSG_ReadByte();

		efx.R_Sprite_Spray(pos, dir, modelIndex, count, 2 * speed, iRand);
	}
		break;
	case TE_ARMOR_RICOCHET:
	{
		vec3_t pos;
		float scale;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		scale = (float)MSG_ReadByte() / 10.f;

		efx.R_RicochetSprite(pos, cl_sprite_ricochet, 0.1f, scale);
		efx.R_RicochetSound(pos);
	}
		break;
	case TE_PLAYERDECAL:
	{
		vec3_t pos;
		int playernum, entnumber, decalTextureIndex;
		customization_t* pCust;
		texture_t* pTexture;

		pCust = NULL;

		playernum = MSG_ReadByte() - 1;
		if (playernum < MAX_CLIENTS)
			pCust = cl.players[playernum].customdata.pNext;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		entnumber = MSG_ReadShort();

		decalTextureIndex = MSG_ReadByte();

		if (entnumber >= cl.max_edicts)
			Sys_Error("Decal: entity = %i", entnumber);

		pTexture = NULL;

		if (pCust != NULL && pCust->pBuffer != NULL && pCust->pInfo != NULL)
		{
			if (pCust->resource.ucFlags & FDECAL_CUSTOM && pCust->resource.type == t_decal && pCust->bTranslated)
			{
				decalTextureIndex = min(decalTextureIndex, ((cachewad_t*)pCust->pInfo)->lumpCount - 1);
				pCust->nUserData1 = decalTextureIndex;
				pTexture = (texture_t*)Draw_CustomCacheGet((cachewad_t*)pCust->pInfo, pCust->pBuffer, pCust->resource.nDownloadSize, decalTextureIndex);
			}
		}

		if (r_decals.value)
		{
			extern void DbgPrint(FILE*, const char* format, ...);
			extern FILE* m_fMessages;
			DbgPrint(m_fMessages, "trying to shoot decal %d custom <%s#%d>\r\n", decalTextureIndex, pCust == NULL, __FILE__, __LINE__);

			if (pCust != NULL)
			{
				if (pTexture != NULL)
					R_CustomDecalShoot(pTexture, playernum, entnumber, 0, pos, FDECAL_CUSTOM);
			}
			else
				efx.R_DecalShoot(efx.Draw_DecalIndex(decalTextureIndex), entnumber, 0, pos, FDECAL_CUSTOM);

		}
	}
		break;
	case TE_BUBBLES:
	{
		vec3_t mins, maxs;
		int modelIndex, count;
		float height, speed;

		mins[0] = MSG_ReadCoord(&net_message);
		mins[1] = MSG_ReadCoord(&net_message);
		mins[2] = MSG_ReadCoord(&net_message);

		maxs[0] = MSG_ReadCoord(&net_message);
		maxs[1] = MSG_ReadCoord(&net_message);
		maxs[2] = MSG_ReadCoord(&net_message);

		height = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();

		speed = MSG_ReadCoord(&net_message);

		efx.R_Bubbles(mins, maxs, height, modelIndex, count, speed);
	}
		break;
	case TE_BUBBLETRAIL:
	{
		vec3_t mins, maxs;
		int modelIndex, count;
		float height, speed;

		mins[0] = MSG_ReadCoord(&net_message);
		mins[1] = MSG_ReadCoord(&net_message);
		mins[2] = MSG_ReadCoord(&net_message);

		maxs[0] = MSG_ReadCoord(&net_message);
		maxs[1] = MSG_ReadCoord(&net_message);
		maxs[2] = MSG_ReadCoord(&net_message);

		height = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();

		speed = MSG_ReadCoord(&net_message);

		efx.R_BubbleTrail(mins, maxs, height, modelIndex, count, speed);
		break;
	}
	case TE_BLOODSPRITE:
	{
		vec3_t org;
		int modelIndex, modelIndex2, color;
		float scale;

		org[0] = MSG_ReadCoord(&net_message);
		org[1] = MSG_ReadCoord(&net_message);
		org[2] = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		modelIndex2 = MSG_ReadShort();
		color = MSG_ReadByte();

		scale = (float)MSG_ReadByte();

		if (CL_ShowTEBlood())
			efx.R_BloodSprite(org, color, modelIndex, modelIndex2, scale);
	}
		break;
	case TE_PROJECTILE:
	{
		vec3_t org, vel;
		int modelIndex, life, entnumber;

		org[0] = MSG_ReadCoord(&net_message);
		org[1] = MSG_ReadCoord(&net_message);
		org[2] = MSG_ReadCoord(&net_message);

		vel[0] = MSG_ReadCoord(&net_message);
		vel[1] = MSG_ReadCoord(&net_message);
		vel[2] = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		life = MSG_ReadByte();
		entnumber = MSG_ReadByte();

		efx.R_Projectile(org, vel, modelIndex, life, entnumber, NULL);
	}
		break;
	case TE_SPRAY:
	{
		vec3_t pos, dir;
		int modelIndex, count, speed, iRand;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		dir[0] = MSG_ReadCoord(&net_message);
		dir[1] = MSG_ReadCoord(&net_message);
		dir[2] = MSG_ReadCoord(&net_message);

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		speed = MSG_ReadByte();
		// noise
		iRand = MSG_ReadByte();
		type = MSG_ReadByte();

		efx.R_Spray(pos, dir, modelIndex, count, speed, iRand, type);
	}
		break;
	case TE_PLAYERSPRITES:
	{
		int entnumber, modelIndex, count, isize;

		entnumber = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		isize = MSG_ReadByte();
		efx.R_PlayerSprites(entnumber, modelIndex, count, isize);
	}
		break;
	case TE_PARTICLEBURST:
	{
		vec3_t pos;
		int radius, color, life;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		radius = MSG_ReadShort();
		color = MSG_ReadByte();
		life = (float)MSG_ReadByte() / 10.f;
		efx.R_ParticleBurst(pos, radius, color, life);
	}
		break;
	case TE_FIREFIELD:
	{
		vec3_t pos;
		int radius, modelIndex, count, flags;
		float life;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		radius = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		flags = MSG_ReadByte();
		life = (float)MSG_ReadByte() / 10.f;
		efx.R_FireField(pos, radius, modelIndex, count, flags, life);
	}
		break;
	case TE_PLAYERATTACHMENT:
	{
		int entnumber, offset, modelIndex;
		float life;

		entnumber = MSG_ReadByte();
		offset = MSG_ReadCoord(&net_message);
		modelIndex = MSG_ReadShort();
		life = (float)MSG_ReadShort() / 10.f;

		efx.R_AttachTentToPlayer(entnumber, modelIndex, offset, life);
	}
		break;
	case TE_KILLPLAYERATTACHMENTS:
	{
		int entnumber = MSG_ReadByte();

		efx.R_KillAttachedTents(entnumber);
	}
		break;
	case TE_MULTIGUNSHOT:
	{
		int index_array[1];
		vec3_t pos, dir, size;
		int count, modelIndex;

		pos[0] = MSG_ReadCoord(&net_message);
		pos[1] = MSG_ReadCoord(&net_message);
		pos[2] = MSG_ReadCoord(&net_message);

		dir[0] = MSG_ReadCoord(&net_message);
		dir[1] = MSG_ReadCoord(&net_message);
		dir[2] = MSG_ReadCoord(&net_message);

		size[0] = MSG_ReadCoord(&net_message) / 100.0f;
		size[1] = MSG_ReadCoord(&net_message) / 100.0f;
		size[2] = 0;

		count = MSG_ReadByte();
		modelIndex = MSG_ReadByte();

		index_array[0] = modelIndex;

		efx.R_MultiGunshot(pos, dir, size, count, sizeof(index_array), index_array);
	}
		break;
	case TE_USERTRACER:
	{
		vec3_t org, vel;
		float life, length;
		int color;

		org[0] = MSG_ReadCoord(&net_message);
		org[1] = MSG_ReadCoord(&net_message);
		org[2] = MSG_ReadCoord(&net_message);

		vel[0] = MSG_ReadCoord(&net_message);
		vel[1] = MSG_ReadCoord(&net_message);
		vel[2] = MSG_ReadCoord(&net_message);

		life = (float)MSG_ReadByte() / 10.f;
		color = MSG_ReadByte();
		length = (float)MSG_ReadByte() / 10.f;

		efx.R_UserTracerParticle(org, vel, life, color, length, 0, NULL);
	}
		break;
	default:
		Sys_Error("CL_ParseTEnt: bad type");
		break;
	}
}

void CL_TempEntInit()
{
	Q_memset( gTempEnts, 0, sizeof( gTempEnts ) );

	//Fix up pointers
	for( int i = 0;  i < ARRAYSIZE( gTempEnts ) - 1; ++i )
	{
		gTempEnts[ i ].next = &gTempEnts[ i + 1 ];
	}

	gTempEnts[ ARRAYSIZE( gTempEnts ) - 1 ].next = nullptr;

	gpTempEntFree = gTempEnts;
	gpTempEntActive = nullptr;
}

void CL_TempEntPrepare(TEMPENTITY *pTemp, model_t *model)
{
	Q_memset(&pTemp->entity, 0, sizeof(cl_entity_t));
	pTemp->flags = 0;
	pTemp->entity.curstate.colormap = 0;
	pTemp->die = cl.time + 0.75;
	pTemp->entity.model = model;
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->fadeSpeed = 0.5;
	pTemp->hitSound = 0;
	pTemp->clientIndex = -1;
	pTemp->bounceFactor = 1.0;
	pTemp->hitcallback = 0;
	pTemp->callback = 0;
}

TEMPENTITY* CL_TempEntAlloc( vec_t* org, model_t* model )
{
	TEMPENTITY *ptent;

	if (gpTempEntFree == NULL)
	{
		Con_DPrintf(const_cast<char*>("Overflow %d temporary ents!\n"), MAX_TEMP_ENTITIES);
		return nullptr;
	}

	if (model == NULL)
	{
		Con_DPrintf(const_cast<char*>("efx.CL_TempEntAlloc: No model\n"));
		return NULL;
	}

	ptent = gpTempEntFree;
	gpTempEntFree = gpTempEntFree->next;
	CL_TempEntPrepare(ptent, model);
	ptent->priority = 0;
	VectorCopy(org, ptent->entity.origin);
	ptent->next = gpTempEntActive;
	gpTempEntActive = ptent;

	return ptent;
}

TEMPENTITY* CL_TempEntAllocNoModel( vec_t* org )
{
	TEMPENTITY *ptent;

	if (gpTempEntFree == NULL)
	{
		Con_DPrintf(const_cast<char*>("Overflow %d temporary ents!\n"), MAX_TEMP_ENTITIES);
		return nullptr;
	}

	ptent = gpTempEntFree;
	gpTempEntFree = gpTempEntFree->next;
	CL_TempEntPrepare(ptent, NULL);
	ptent->priority = 0;
	ptent->flags |= FTENT_NOMODEL;
	VectorCopy(org, ptent->entity.origin);
	ptent->next = gpTempEntActive;
	gpTempEntActive = ptent;

	return ptent;
}

TEMPENTITY* CL_TempEntAllocHigh( vec_t* org, model_t* model )
{
	TEMPENTITY *ptent;

	if (!model)
	{
		Con_DPrintf(const_cast<char*>("temporary ent model invalid\n"));
		return nullptr;
	}

	if (gpTempEntFree != NULL)
	{
		ptent = gpTempEntFree;
		gpTempEntFree = gpTempEntFree->next;
		ptent->next = gpTempEntActive;
		gpTempEntActive = ptent;
	}
	else
	{
		for (ptent = gpTempEntActive; ptent != NULL; ptent = ptent->next)
		{
			if (!ptent->priority)
				break;
		}

		if (ptent == NULL)
		{
			Con_DPrintf(const_cast<char*>("Couldn't alloc a high priority TENT!\n"));
			return NULL;
		}
	}

	CL_TempEntPrepare(ptent, model);

	ptent->priority = 1;
	VectorCopy(org, ptent->entity.origin);

	return ptent;
}

void CL_TempEntPlaySound(TEMPENTITY* pTemp, float damp)
{
	sfx_t* rgsfx[6];
	int sndmax, gun = 0, pitch, z;
	float volume = 0.8f;

	Q_memset(rgsfx, 0, sizeof(rgsfx));

	switch (pTemp->hitSound)
	{
	case BOUNCE_GLASS:
		sndmax = 3;
		rgsfx[0] = cl_sfx_glass1;
		rgsfx[1] = cl_sfx_glass2;
		rgsfx[2] = cl_sfx_glass3;
		break;
	case BOUNCE_METAL:
		sndmax = 3;
		rgsfx[0] = cl_sfx_metal1;
		rgsfx[1] = cl_sfx_metal2;
		rgsfx[2] = cl_sfx_metal3;
		break;
	case BOUNCE_FLESH:
		sndmax = 6;
		rgsfx[0] = cl_sfx_flesh1;
		rgsfx[1] = cl_sfx_flesh2;
		rgsfx[2] = cl_sfx_flesh3;
		rgsfx[3] = cl_sfx_flesh4;
		rgsfx[4] = cl_sfx_flesh5;
		rgsfx[5] = cl_sfx_flesh6;
		break;
	case BOUNCE_WOOD:
		sndmax = 3;
		rgsfx[0] = cl_sfx_wood1;
		rgsfx[1] = cl_sfx_wood2;
		rgsfx[2] = cl_sfx_wood3;
		break;
	case BOUNCE_SHRAP:
		sndmax = 5;
		rgsfx[0] = cl_sfx_ric1;
		rgsfx[1] = cl_sfx_ric2;
		rgsfx[2] = cl_sfx_ric3;
		rgsfx[3] = cl_sfx_ric4;
		rgsfx[4] = cl_sfx_ric5;
		break;
	case BOUNCE_SHELL:
		sndmax = 3;
		gun = 1;
		rgsfx[0] = cl_sfx_sshell1;
		rgsfx[1] = cl_sfx_sshell2;
		rgsfx[2] = cl_sfx_sshell3;
		break;
	case BOUNCE_CONCRETE:
		sndmax = 3;
		rgsfx[0] = cl_sfx_concrete1;
		rgsfx[1] = cl_sfx_concrete2;
		rgsfx[2] = cl_sfx_concrete3;
		break;
	case BOUNCE_SHOTSHELL:
		sndmax = 3;
		gun = 1;
		volume = 0.5f;
		rgsfx[0] = cl_sfx_sshell1;
		rgsfx[1] = cl_sfx_sshell2;
		rgsfx[2] = cl_sfx_sshell3;
		break;
	default:
		return;
	}

	// V O
	z = abs(pTemp->entity.baseline.origin[ROLL]);

	// только при нулевом результате проиграется звук
	if (RandomLong(0, gun ? 3 : 5) && !(gun && z >= 200))
		return;

	if (damp > 0)
	{
		if (gun)
			volume *= min((float)z / 350.0f, 1.0f);
		else
			volume *= min((float)z / 450.0f, 1.0f);

		if (RandomLong(0, 3) || gun)
			pitch = PITCH_NORM;
		else
			pitch = RandomLong(90, 124);

		S_StartDynamicSound(-1, CHAN_AUTO, rgsfx[RandomLong(0, sndmax - 1)], pTemp->entity.origin, volume, 1.0, 0, pitch);
	}
}

int CL_AddVisibleEntity(cl_entity_t* pEntity)
{
	vec3_t mins, maxs;

	if (!pEntity->model)
		return 0;

	for (int i = 0; i < 3; i++)
	{
		mins[i] = pEntity->model->mins[i] + pEntity->origin[i];
		maxs[i] = pEntity->model->maxs[i] + pEntity->origin[i];
	}

	if (PVSNode(cl.worldmodel->nodes, mins, maxs))
	{
		VectorCopy(pEntity->angles, pEntity->curstate.angles);
		VectorCopy(pEntity->angles, pEntity->latched.prevangles);
		ClientDLL_AddEntity(ET_TEMPENTITY, pEntity);

		return true;
	}

	return false;
}

void CL_TempEntUpdate()
{
	if (cl.worldmodel)
		ClientDLL_TempEntUpdate(cl.time - cl.oldtime, cl.time, movevars.gravity, &gpTempEntFree, &gpTempEntActive, CL_AddVisibleEntity, CL_TempEntPlaySound);
}

int CL_FxBlend(cl_entity_t *e)
{
	int	blend = 0;
	float	offset, dist;
	vec3_t	tmp;

	offset = float(e->curstate.number) * 363.0f; // Use ent index to de-sync these fx

	switch (e->curstate.renderfx)
	{
	case kRenderFxPulseSlow:
		blend = e->curstate.renderamt + 0x10 * sin(cl.time * 2 + offset);
		break;
	case kRenderFxPulseFast:
		blend = e->curstate.renderamt + 0x10 * sin(cl.time * 8 + offset);
		break;
	case kRenderFxPulseSlowWide:
		blend = e->curstate.renderamt + 0x40 * sin(cl.time * 2 + offset);
		break;
	case kRenderFxPulseFastWide:
		blend = e->curstate.renderamt + 0x40 * sin(cl.time * 8 + offset);
		break;
	case kRenderFxFadeSlow:
		if (e->curstate.renderamt > 0)
			e->curstate.renderamt -= 1;
		else e->curstate.renderamt = 0;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxFadeFast:
		if (e->curstate.renderamt > 3)
			e->curstate.renderamt -= 4;
		else e->curstate.renderamt = 0;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidSlow:
		if (e->curstate.renderamt < 255)
			e->curstate.renderamt += 1;
		else e->curstate.renderamt = 255;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidFast:
		if (e->curstate.renderamt < 252)
			e->curstate.renderamt += 4;
		else e->curstate.renderamt = 255;
		blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin(cl.time * 4 + offset);
		if (blend < 0) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin(cl.time * 16 + offset);
		if (blend < 0) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin(cl.time * 36 + offset);
		if (blend < 0) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin(cl.time * 2) + sin(cl.time * 17 + offset));
		if (blend < 0) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin(cl.time * 16) + sin(cl.time * 23 + offset));
		if (blend < 0) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxDistort:
	case kRenderFxHologram:
		VectorCopy(e->origin, tmp);
		VectorSubtract(tmp, r_origin, tmp);
		dist = DotProduct(tmp, vpn);

		// turn off distance fade
		if (e->curstate.renderfx == kRenderFxDistort)
			dist = 1;

		if (dist <= 0)
		{
			blend = 0;
		}
		else
		{
			e->curstate.renderamt = 180;
			if (dist <= 100) blend = e->curstate.renderamt;
			else blend = (int)((1.0f - (dist - 100) * (1.0f / 400.0f)) * e->curstate.renderamt);
			blend += RandomLong(-32, 31);
		}
		break;
	default:
		blend = e->curstate.renderamt;
		break;
	}

	blend = clamp(blend, 0, 255);

	return blend;
}

void CL_FxTransform(cl_entity_t *ent, float transform[3][4])
{
	switch( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if ( RandomLong(0,49) == 0 )
		{
			int axis = RandomLong(0,1);
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			VectorScale( transform[axis], RandomFloat(1,1.484), transform[axis] );
		}
		else if ( RandomLong(0,49) == 0 )
		{
			float offset;
			int axis = RandomLong(0,1);
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			offset = RandomFloat(-10,10);
			transform[RandomLong(0,2)][3] += offset;
		}
	break;
	case kRenderFxExplode:
		{
			float scale;

			scale = 1.0 + ( cl.time - ent->curstate.animtime) * 10.0;
			if ( scale > 2 )	// Don't blow up more than 200%
				scale = 2;
			transform[0][1] *= scale;
			transform[1][1] *= scale;
			transform[2][1] *= scale;
		}
	break;

	}
}

mspriteframe_t* R_GetSpriteFrame(msprite_t* pSprite, int nframe)
{
	if (!pSprite)
	{
		Con_Printf(const_cast<char*>("Sprite:  no pSprite!!!\n"));
		return NULL;
	}

	if (pSprite->numframes == 0)
	{
		Con_Printf(const_cast<char*>("Sprite:  pSprite has no frames!!!\n"));
		return NULL;
	}

	if (nframe >= pSprite->numframes || nframe < 0)
	{
		Con_DPrintf(const_cast<char*>("Sprite: no such frame %d\n"), nframe);
		nframe = NULL;
	}

	if (pSprite->frames[nframe].type == SPR_SINGLE)
		return pSprite->frames[nframe].frameptr;

	return NULL;
}

void R_GetSpriteAxes(cl_entity_t* ent, short type, vec_t* vpn, vec_t* vright, vec_t* vup)
{
	int				i;
	msprite_t		*psprite;
	vec3_t			tvec;
	float			dot, angle, sr, cr;

	if (ent->angles[2] != 0.0f && type == SPR_VP_PARALLEL)
		type = SPR_VP_PARALLEL_ORIENTED;

	switch (type)
	{
	case SPR_VP_PARALLEL_UPRIGHT:
		dot = ::vpn[2];	// same as DotProduct (tvec, r_spritedesc.vup) because
		//  r_spritedesc.vup is 0, 0, 1
		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;
		vup[0] = 0;
		vup[1] = 0;
		vup[2] = 1;
		vright[0] = ::vpn[1];
		// CrossProduct(r_spritedesc.vup, -modelorg,
		vright[1] = ::vpn[0];
		//              r_spritedesc.vright)
		vright[2] = 0;
		VectorNormalize(vright);
		vpn[0] = -vright[1];
		vpn[1] = vright[0];
		vpn[2] = 0;
		// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
		//  r_spritedesc.vpn)
		break;
	case SPR_FACING_UPRIGHT:
		// generate the sprite's axes, with vup straight up in worldspace, and
		// r_spritedesc.vright perpendicular to modelorg.
		// This will not work if the view direction is very close to straight up or
		// down, because the cross product will be between two nearly parallel
		// vectors and starts to approach an undefined state, so we don't draw if
		// the two vectors are less than 1 degree apart
		tvec[0] = -modelorg[0];
		tvec[1] = -modelorg[1];
		tvec[2] = -modelorg[2];
		VectorNormalize(tvec);
		dot = tvec[2];	// same as DotProduct (tvec, r_spritedesc.vup) because
		//  r_spritedesc.vup is 0, 0, 1
		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;
		vup[0] = 0;
		vup[1] = 0;
		vup[2] = 1;
		vright[0] = tvec[1];
		// CrossProduct(r_spritedesc.vup, -modelorg,
		vright[1] = -tvec[0];
		//              r_spritedesc.vright)
		vright[2] = 0;
		VectorNormalize(vright);
		vpn[0] = -vright[1];
		vpn[1] = vright[0];
		vpn[2] = 0;
		// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
		//  r_spritedesc.vpn)
		break;
	case SPR_VP_PARALLEL:
		// generate the sprite's axes, completely parallel to the viewplane. There
		// are no problem situations, because the sprite is always in the same
		// position relative to the viewer
		for (i = 0; i<3; i++)
		{
			vup[i] = ::vup[i];
			vright[i] = ::vright[i];
			vpn[i] = ::vpn[i];
		}

		break;
	case SPR_ORIENTED:
		// generate the sprite's axes, according to the sprite's world orientation
		AngleVectors(ent->angles, vpn, vright, vup);
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		// generate the sprite's axes, parallel to the viewplane, but rotated in
		// that plane around the center according to the sprite entity's roll
		// angle. So vpn stays the same, but vright and vup rotate
		angle = currententity->angles[ROLL] * (M_PI * 2 / 360);
		sr = sin(angle);
		cr = cos(angle);

		for (i = 0; i<3; i++)
		{
			vpn[i] = ::vpn[i];
			vright[i] = ::vright[i] * cr + ::vup[i] * sr;
			vup[i] = ::vright[i] * -sr + ::vup[i] * cr;
		}
		break;
	default:
		Sys_Error("R_DrawSprite: Bad sprite type %d", type);
		break;
	}
}

void R_SpriteColor(colorVec* pColor, cl_entity_t* pEntity, int alpha)
{	
	int a = 256;

	if (pEntity->curstate.rendermode == kRenderTransAdd || pEntity->curstate.rendermode == kRenderGlow)
		a = alpha;

	if (pEntity->curstate.rendercolor.r != 0 || pEntity->curstate.rendercolor.g != 0 || pEntity->curstate.rendercolor.b != 0)
	{
		pColor->r = (a * pEntity->curstate.rendercolor.r) >> 8;
		pColor->g = (a * pEntity->curstate.rendercolor.g) >> 8;
		pColor->b = (a * pEntity->curstate.rendercolor.b) >> 8;

		if (filterMode)
		{
			pColor->r = (float)pColor->r * filterColorRed;
			pColor->g = (float)pColor->g * filterColorGreen;
			pColor->b = (float)pColor->b * filterColorBlue;
		}
	}
	else
	{
		pColor->r = pColor->g = pColor->b = (a * 255) >> 8;
	}
}

float* R_GetAttachmentPoint(int entity, int attachment)
{
	if (attachment)
		return cl_entities[entity].attachment[attachment - 1];

	return cl_entities[entity].origin;
}

void R_Explosion( float* pos, int model, float scale, float framerate, int flags )
{
	dlight_t *pdl;

	if (scale != 0.0)
	{
		efx.R_Sprite_Explode(R_DefaultSprite(pos, model, framerate), scale, flags);
		
		if (flags & FTENT_SLOWGRAVITY)
			efx.R_FlickerParticles(pos);

		if (flags & FTENT_GRAVITY)
		{
			pdl = efx.CL_AllocDlight(0);
			VectorCopy(pos, pdl->origin);
			pdl->radius = 200.0;
			pdl->color.r = 250;
			pdl->color.g = 250;
			pdl->color.b = 150;
			pdl->die = cl.time + 0.01;
			pdl->decay = 800.0;

			pdl = efx.CL_AllocDlight(0);
			VectorCopy(pos, pdl->origin);
			pdl->radius = 150.0;
			pdl->color.r = 255;
			pdl->color.g = 190;
			pdl->color.b = 40;
			pdl->die = cl.time + 1.0;
			pdl->decay = 200.0;
		}
	}

	if (!(flags & FTENT_ROTATE))
	{
		switch (RandomLong(0, 2))
		{
		case 0:
			S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_r_exp1, pos, 1.0, 0.3, 0, PITCH_NORM);
			break;
		case 1:
			S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_r_exp2, pos, 1.0, 0.3, 0, PITCH_NORM);
			break;
		case 2:
			S_StartDynamicSound(-1, CHAN_AUTO, cl_sfx_r_exp3, pos, 1.0, 0.3, 0, PITCH_NORM);
			break;
		}
	}
}

TEMPENTITY* CL_AllocCustomTempEntity(float* origin, model_t* model, int high, void(*callback)(TEMPENTITY*, float, float))
{
	TEMPENTITY* tent = high ? efx.CL_TempEntAllocHigh(origin, model) : efx.CL_TempEntAlloc(origin, model);

	if (tent == NULL)
		return nullptr;

	tent->flags |= FTENT_CLIENTCUSTOM;
	tent->callback = callback;
	tent->die = cl.time;

	return tent;
}
