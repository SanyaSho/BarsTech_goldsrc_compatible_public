#ifndef _SV_PMOVE_H_
#define _SV_PMOVE_H_

#include "quakedef.h"

void PM_SV_PlaybackEventFull(int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
void PM_SV_PlaySound(int channel, const char  *sample, float volume, float attenuation, int fFlags, int pitch);
const char *PM_SV_TraceTexture(int ground, vec_t *vstart, vec_t *vend);

#endif