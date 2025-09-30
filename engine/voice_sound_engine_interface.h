//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//          
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_SOUND_ENGINE_INTERFACE_H
#define VOICE_SOUND_ENGINE_INTERFACE_H
#pragma once


/*! @defgroup VoiceSoundEngineInterface VoiceSoundEngineInterface
Abstracts out the sound engine for the voice code.
GoldSrc and Src each have a different implementation of this.
@{
*/



//! Max number of receiving voice channels.
#define VOICE_NUM_CHANNELS			5	

// ----------------------------------------------------------------------------- //
// Functions for voice.cpp.
// ----------------------------------------------------------------------------- //

typedef struct
{
	sfx_t m_SFX;
	sfxcache_t m_SFXCache;
	unsigned short m_PrevUpsampleValue;
} VoiceSE_SFX;


typedef int(*VoiceCallback)(sfxcache_t* sc, void* pstackbuf, int psbsize, int pos, int count);
#define VoiceSE_GetSoundDataCallbackPtr(a, b, c, d, e) ((VoiceCallback)a->loopstart)(a, b, c, d, e)

extern VoiceSE_SFX g_VoiceSE_SFX[VOICE_NUM_CHANNELS];
extern float g_SND_VoiceOverdrive;

void VoiceSE_NotifyFreeChannel(int entchan);
sfxcache_t* VoiceSE_GetSFXCache(sfx_t *pSFX);

//! Initialize the sound engine interface.
bool VoiceSE_Init();

//! Shutdown the sound engine interface.
void VoiceSE_Term();

//! Called each frame.
void VoiceSE_Idle(float frametime);


//! Start audio playback on the specified voice channel.
//! Voice_GetChannelAudio is called by the mixer for each active channel.
void VoiceSE_StartChannel(
	//! Which channel to start.
	int iChannel
	);

//! Stop audio playback on the specified voice channel.
void VoiceSE_EndChannel(
	//! Which channel to stop.
	int iChannel
	);

//! Starts the voice overdrive (lowers volume of all sounds other than voice).
void VoiceSE_StartOverdrive();
void VoiceSE_EndOverdrive();

//! Control mouth movement for an entity.
void VoiceSE_InitMouth(int entnum);
void VoiceSE_CloseMouth(int entnum);
void VoiceSE_MoveMouth(int entnum, short *pSamples, int nSamples);


// ----------------------------------------------------------------------------- //
// Functions for voice.cpp to implement.
// ----------------------------------------------------------------------------- //

//! This function is implemented in voice.cpp. Gives 16-bit signed mono samples to the mixer.
//! \return Number of samples actually gotten.
int Voice_GetOutputData(
	//! The voice channel it wants samples from.
	const int iChannel,
	//! The rgba to copy the samples into.
	char *copyBuf,
	//! Maximum size of copyBuf.
	const int copyBufSize,
	//! Which sample to start at.
	const int samplePosition,
	//! How many samples to get.
	const int sampleCount
	);


/*! @} End VoiceSoundEngineInterface group */


#endif // VOICE_SOUND_ENGINE_INTERFACE_H
