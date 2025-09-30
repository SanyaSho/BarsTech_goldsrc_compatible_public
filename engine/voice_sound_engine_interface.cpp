#include "quakedef.h"
#include "sound.h"
#include "client.h"
#include "voice_sound_engine_interface.h"
#include "voice.h"

VoiceSE_SFX g_VoiceSE_SFX[VOICE_NUM_CHANNELS];
static float	g_VoiceOverdriveDuration = 0;
static bool		g_bVoiceOverdriveOn = false;

// When voice is on, all other sounds are decreased by this factor.
static cvar_t voice_overdrive = { const_cast<char*>("voice_overdrive"), const_cast<char*>("2") };
static cvar_t voice_overdrivefadetime = { const_cast<char*>("voice_overdrivefadetime"), const_cast<char*>("0.4") }; // How long it takes to fade in and out of the voice overdrive.

float g_SND_VoiceOverdrive = 1.0;

void Voice_RegisterCvars()
{
	Cvar_RegisterVariable(&voice_loopback);
	Cvar_RegisterVariable(&voice_maxgain);
	Cvar_RegisterVariable(&voice_avggain);
	Cvar_RegisterVariable(&voice_scale);
	Cvar_RegisterVariable(&voice_fadeouttime);
	Cvar_RegisterVariable(&voice_profile);
	Cvar_RegisterVariable(&voice_showchannels);
	Cvar_RegisterVariable(&voice_showincoming);
	Cvar_RegisterVariable(&voice_enable);
	Cvar_RegisterVariable(&voice_dsound);
	Cvar_RegisterVariable(&voice_forcemicrecord);
	Cvar_RegisterVariable(&voice_overdrive);
	Cvar_RegisterVariable(&voice_overdrivefadetime);
}

void VoiceSE_EndChannel(int iChannel)
{
	S_StopSound(cl.viewentity, iChannel + CHAN_NETWORKVOICE_BASE);
}

void VoiceSE_CloseMouth(int entnum)
{
	SND_ForceCloseMouth(entnum);
}

void VoiceSE_MoveMouth(int entnum, short* pSamples, int nSamples)
{
	SND_MoveMouth16(entnum, pSamples, nSamples);
}

void VoiceSE_NotifyFreeChannel(int entchan)
{
	Voice_NotifyFreeChannel(entchan - CHAN_NETWORKVOICE_BASE);
}

void VoiceSE_InitMouth(int entnum)
{
	SND_ForceInitMouth(entnum);
}

sfxcache_t* VoiceSE_GetSFXCache(sfx_t *pSFX)
{
	for (int i = 0; i < ARRAYSIZE(g_VoiceSE_SFX); i++)
	{
		if (pSFX == &g_VoiceSE_SFX[i].m_SFX)
			return &g_VoiceSE_SFX[i].m_SFXCache;
	}
	return NULL;
}

int VoiceSE_GetSoundDataCallback(sfxcache_t *pCache, char *pCopyBuf, int maxOutDataSize, int samplePos, int sampleCount)
{
	int iChannel, OutputData;
	VoiceSE_SFX* chan = NULL;
	short* pCopyBuf16;

	for (iChannel = 0; iChannel < VOICE_NUM_CHANNELS; iChannel++)
	{
		if (pCache == &g_VoiceSE_SFX[iChannel].m_SFXCache)
		{
			chan = &g_VoiceSE_SFX[iChannel];
			break;
		}
	}

	if (!chan)
		return 0;

	if (iChannel == 5)
		return 0;

	OutputData = Voice_GetOutputData(iChannel, pCopyBuf, maxOutDataSize, samplePos, sampleCount >> 1);

	pCopyBuf16 = (short*)pCopyBuf;

	for (int i = OutputData - 1; i > 0; i--)
	{
		pCopyBuf16[2 * i + 1] = pCopyBuf16[i];
		pCopyBuf16[2 * i] = AVG(pCopyBuf16[i], pCopyBuf16[i - 1]);
	}

	if (2 * OutputData > 1)
	{
		pCopyBuf16[1] = pCopyBuf16[0]; 
		pCopyBuf16[0] = AVG(pCopyBuf16[0], chan->m_PrevUpsampleValue);
	}

	chan->m_PrevUpsampleValue = pCopyBuf16[2 * OutputData - 1];

	return 2 * OutputData;
}

bool VoiceSE_Init()
{
	if (!sound_started)
		return false;

	Q_memset(g_VoiceSE_SFX, 0, sizeof(g_VoiceSE_SFX));

	for (int i = 0; i < VOICE_NUM_CHANNELS; i++)
	{
		g_VoiceSE_SFX[i].m_SFXCache.length = 0x40000000;
		g_VoiceSE_SFX[i].m_SFXCache.loopstart = (uintptr_t)VoiceSE_GetSoundDataCallback;
		g_VoiceSE_SFX[i].m_SFXCache.speed = 22050;
		g_VoiceSE_SFX[i].m_SFXCache.width = 2;
	}

	return true;
}

void VoiceSE_StartOverdrive()
{
	g_bVoiceOverdriveOn = true;
}

void VoiceSE_EndOverdrive()
{
	g_bVoiceOverdriveOn = false;
}

void VoiceSE_Term()
{
}

void VoiceSE_StartChannel(
	int iChannel	//! Which channel to start.
	)
{
	if (iChannel > 4)
		return;

	// Start the sound.
	VoiceSE_SFX *sfx = &g_VoiceSE_SFX[iChannel];
	sfx->m_PrevUpsampleValue = 0;
	Q_memset(&sfx->m_SFX, 0, sizeof(sfx_t));
	Q_strcpy(sfx->m_SFX.name, "?VoiceData");
	S_StartDynamicSound(cl.viewentity, (CHAN_NETWORKVOICE_BASE + iChannel), &sfx->m_SFX, vec3_origin, 1.0, 1.0, 0, PITCH_NORM);
}

void VoiceSE_Idle(float frametime)
{
	if (g_bVoiceOverdriveOn)
	{
		g_VoiceOverdriveDuration = min(g_VoiceOverdriveDuration + frametime, voice_overdrivefadetime.value);
	}
	else
	{
		if (g_VoiceOverdriveDuration == 0)
			return;

		g_VoiceOverdriveDuration = max(g_VoiceOverdriveDuration - frametime, 0.f);
	}

	float percent = g_VoiceOverdriveDuration / voice_overdrivefadetime.value;
	percent = (float)(-cos(percent * 3.1415926535) * 0.5 + 0.5);		// Smooth it out..
	float voiceOverdrive = 1 + (voice_overdrive.value - 1) * percent;
	g_SND_VoiceOverdrive = voiceOverdrive;
}
