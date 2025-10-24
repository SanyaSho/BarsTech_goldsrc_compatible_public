#ifndef ENGINE_VOICE_H
#define ENGINE_VOICE_H

#include "ivoicetweak.h"
#include "voice_gain.h"
#include "sound.h"

extern IVoiceTweak g_VoiceTweakAPI;

#define VOICE_OUTPUT_SAMPLE_RATE			11025	// Sample rate that we feed to the mixer.

#define VOICE_CHANNEL_ERROR			-1
#define VOICE_CHANNEL_IN_TWEAK_MODE	-2	// Returned by AssignChannel if currently in tweak mode (not an error).

extern cvar_t voice_dsound;
extern cvar_t voice_avggain;
extern cvar_t voice_maxgain;
extern cvar_t voice_scale;
extern cvar_t voice_loopback;
extern cvar_t voice_fadeouttime;
extern cvar_t voice_profile;
extern cvar_t voice_showchannels;
extern cvar_t voice_showincoming;	// show incoming voice data

extern cvar_t voice_enable;
extern cvar_t voice_forcemicrecord;

qboolean Voice_Init( const char* pCodecName, int quality );
void Voice_Deinit();

int VoiceTweak_StartVoiceTweakMode();

void VoiceTweak_EndVoiceTweakMode();

void VoiceTweak_SetControlFloat( VoiceTweakControl iControl, float value );

float VoiceTweak_GetControlFloat( VoiceTweakControl iControl );

int VoiceTweak_GetSpeakingVolume();

void Voice_NotifyFreeChannel(int entchan);

#ifdef __cplusplus
extern "C"
#endif
void Voice_RegisterCvars();

void Voice_LocalPlayerTalkingAck();

int Voice_GetChannel(int nEntity);

int Voice_AssignChannel(int nEntity);

int Voice_AddIncomingData(int nChannel, const char *pchData, int nCount, int iSequenceNumber);

BOOL Voice_GetLoopback();

qboolean Voice_IsRecording();

int Voice_GetCompressedData(char *pchDest, int nCount, qboolean bFinal);

void Voice_Idle(float frametime);

qboolean Voice_RecordStart(
	const char* pUncompressedFile,
	const char* pDecompressedFile,
	const char* pMicInputFile);

qboolean Voice_RecordStop();

#endif //ENGINE_VOICE_H
