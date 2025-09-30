#include "quakedef.h"
#include "cl_pmove.h"
#include "cl_ents.h"
#include "pmove.h"
#include "client.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "cl_demo.h"
#include "r_studio.h"

float g_laststepgap;

void PM_CL_PlaybackEventFull(int flags, int clientindex, word eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
{
	event_args_t eargs;

	if (flags & FEV_SERVER)
		return;

	Q_memset(&eargs, 0, sizeof(eargs));
	VectorCopy(origin, eargs.origin);
	VectorCopy(angles, eargs.angles);
	eargs.fparam1 = fparam1;
	eargs.fparam2 = fparam2;
	eargs.iparam1 = iparam1;
	eargs.iparam2 = iparam2;
	eargs.bparam1 = bparam1;
	eargs.bparam2 = bparam2;
	CL_ClientQueueEvent(flags, eventindex, clientindex, delay, &eargs);
}

const char* PM_CL_TraceTexture( int ground, float* vstart, float* vend )
{
	vec3_t temp, end, right, forward, up, delta;
	msurface_t *psurf;
	
	if (ground < 0)
		return NULL;
	if (ground >= pmove->numphysent)
		return NULL;
	if (!pmove->physents[ground].model)
		return NULL;

	if (ground < 1)
	{
		VectorCopy(vstart, temp);
		VectorCopy(vend, end);
	}
	else
	{
		VectorAdd(pmove->physents[ground].model->hulls[0].clip_mins, pmove->physents[ground].origin, delta);
		VectorSubtract(vstart, delta, temp);
		VectorSubtract(vend, delta, end);

		if (!VectorCompare(pmove->physents[ground].angles, vec_origin))
		{
			AngleVectors(pmove->physents[ground].angles, forward, right, up);
			temp[0] = DotProduct(temp, forward);
			temp[1] = -DotProduct(temp, right);
			temp[2] = DotProduct(temp, up);

			end[0] = DotProduct(end, forward);
			end[1] = -DotProduct(end, right);
			end[2] = DotProduct(end, up);
		}
	}

	if (pmove->physents[ground].model->type == mod_brush && pmove->physents[ground].model->nodes)
	{
		psurf = SurfaceAtPoint(pmove->physents[ground].model, &pmove->physents[ground].model->nodes[0], temp, end);
		if (psurf)
			return psurf->texinfo->texture->name;
	}

	return NULL;
}

void PM_CL_PlaySound(int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch)
{
	static float last;
	sfx_t sfxsentence, *psfx;
	
	if (!pmove->runfuncs)
		return;

	g_laststepgap = cl.time - last;
	last = cl.time;

	if (fFlags & SND_SENTENCE)
	{
		_snprintf(sfxsentence.name, sizeof(sfxsentence.name), "!%s", rgpszrawsentence[Q_atoi(&sample[1])]);
		psfx = &sfxsentence;
	}
	else
	{
		psfx = CL_LookupSound(sample);
	}

	if (cls.demorecording)
		CL_DemoQueueSound(channel, sample, volume, attenuation, fFlags, pitch);

	if (channel == CHAN_STATIC)
		S_StartStaticSound(pmove->player_index + 1, CHAN_STATIC, psfx, pmove->origin, volume, attenuation, fFlags, pitch);
	else
		S_StartDynamicSound(pmove->player_index + 1, channel, psfx, pmove->origin, volume, attenuation, fFlags, pitch);
}
