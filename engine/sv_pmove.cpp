#include "sv_pmove.h"
#include "sound.h"
#include "host.h"
#include "sv_main.h"
#include "pm_shared\pm_movevars.h"
#include "pmove.h"
#include "pr_cmds.h"


void PM_SV_PlaybackEventFull(int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
{
	EV_SV_Playback(flags | FEV_NOTHOST, clientindex, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
}

void PM_SV_PlaySound(int channel, const char  *sample, float volume, float attenuation, int fFlags, int pitch)
{
	SV_StartSound(1, host_client->edict, channel, sample, (int)(volume * 255.0), attenuation, fFlags, pitch);
}

const char *PM_SV_TraceTexture(int ground, vec_t *vstart, vec_t *vend)
{
	if (ground < 0 || ground >= pmove->numphysent)
		return NULL;

	physent_t *pe = &pmove->physents[ground];
	if (!pe->model || pe->info < 0 || pe->info >= sv.max_edicts)
		return NULL;

	edict_t *pent = &sv.edicts[pe->info];

	/* Unreachable code*/
	if (!pent)
		return NULL;
	
	return TraceTexture(pent, vstart, vend);
}

