/**
*	@file
*
*	main control for any streaming sound output device
*/
#define CINTERFACE
#include "quakedef.h"
#include "sound.h"
#include "vgui_int.h"
#include "host.h"
#include "client.h"
#include "cl_main.h"
#include "protocol.h"
#include "pr_cmds.h"
#include <dsound.h>

void S_Play(void);
void S_PlayVol(void);
void S_Say(void);
void S_Say_Reliable(void);
void S_SoundList(void);
void S_Update_();
void S_StopAllSounds(qboolean clear);
void S_StopAllSoundsC(void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t   channels[MAX_CHANNELS];
int			total_channels;

int				snd_blocked = 0;
static qboolean	snd_ambient = 1;
qboolean		snd_initialized = false;

qboolean fakedma = false;

// pointer should go away
volatile dma_t  *shm = 0;
volatile dma_t sn;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist = 1000.0;

int			soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS


#define	MAX_SFX		1024
sfx_t		*known_sfx = NULL;		// hunk allocated [MAX_SFX]
int			num_sfx = 0;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

int 		desired_speed = 11025;
int 		desired_bits = 16;

qboolean sound_started = false;

cvar_t s_a3d = { const_cast<char*>("s_a3d"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t s_eax = { const_cast<char*>("s_eax"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t bgmvolume = { const_cast<char*>("bgmvolume"), const_cast<char*>("1"), FCVAR_ARCHIVE };
cvar_t MP3Volume = { const_cast<char*>("MP3Volume"), const_cast<char*>("0.8"), FCVAR_ARCHIVE | FCVAR_FILTERABLE };
cvar_t MP3FadeTime = { const_cast<char*>("MP3FadeTime"), const_cast<char*>("2.0"), FCVAR_ARCHIVE };
cvar_t volume = { const_cast<char*>("volume"), const_cast<char*>("0.7"), FCVAR_ARCHIVE | FCVAR_FILTERABLE };
cvar_t suitvolume = { const_cast<char*>("suitvolume"), const_cast<char*>("0.25"), FCVAR_ARCHIVE };
cvar_t hisound = { const_cast<char*>("hisound"), const_cast<char*>("1.0"), FCVAR_ARCHIVE };

cvar_t nosound = { const_cast<char*>("nosound"), const_cast<char*>("0") };
//cvar_t precache = { const_cast<char*>("precache"), const_cast<char*>("1") };
cvar_t loadas8bit = { const_cast<char*>("loadas8bit"), const_cast<char*>("0") };
///cvar_t bgmbuffer = { const_cast<char*>("bgmbuffer"), const_cast<char*>("4096") };
cvar_t ambient_level = { const_cast<char*>("ambient_level"), const_cast<char*>("0.3") };
cvar_t ambient_fade = { const_cast<char*>("ambient_fade"), const_cast<char*>("100") };
cvar_t snd_noextraupdate = { const_cast<char*>("snd_noextraupdate"), const_cast<char*>("0") };
cvar_t s_show = { const_cast<char*>("s_show"), const_cast<char*>("0") };
cvar_t snd_show = { const_cast<char*>("snd_show"), const_cast<char*>("0"), FCVAR_SPONLY };
cvar_t _snd_mixahead = { const_cast<char*>("_snd_mixahead"), const_cast<char*>("0.1"), FCVAR_ARCHIVE };
cvar_t speak_enable = { const_cast<char*>("speak_enabled"), const_cast<char*>("1"), FCVAR_FILTERABLE };

extern LPDIRECTSOUNDBUFFER pDSBuf;
wavstream_t wavstreams[MAX_CHANNELS];

void S_Update_(void);

void S_AmbientOff(void)
{
	snd_ambient = false;
}


void S_AmbientOn(void)
{
	snd_ambient = true;
}


void S_SoundInfo_f(void)
{
	if (!sound_started || !shm)
	{
		Con_Printf(const_cast<char*>("sound system not started\n"));
		return;
	}

	Con_Printf(const_cast<char*>("%5d stereo\n"), shm->channels - 1);
	Con_Printf(const_cast<char*>("%5d samples\n"), shm->samples);
	Con_Printf(const_cast<char*>("%5d samplepos\n"), shm->samplepos);
	Con_Printf(const_cast<char*>("%5d samplebits\n"), shm->samplebits);
	Con_Printf(const_cast<char*>("%5d submission_chunk\n"), shm->submission_chunk);
	Con_Printf(const_cast<char*>("%5d speed\n"), shm->speed);
	Con_Printf(const_cast<char*>("0x%x dma buffer\n"), shm->buffer);
	Con_Printf(const_cast<char*>("%5d total_channels\n"), total_channels);
}

void S_Init()
{
	Con_DPrintf(const_cast<char*>("Sound Initialization\n"));

	VOX_Init();

	if (COM_CheckParm(const_cast<char*>("-nosound")))
		return;

	if (COM_CheckParm(const_cast<char*>("-simsound")))
		fakedma = true;

	Cmd_AddCommand(const_cast<char*>("play"), S_Play);
	Cmd_AddCommand(const_cast<char*>("playvol"), S_PlayVol);
	Cmd_AddCommand(const_cast<char*>("speak"), S_Say);
	Cmd_AddCommand(const_cast<char*>("spk"), S_Say_Reliable);
	Cmd_AddCommand(const_cast<char*>("stopsound"), S_StopAllSoundsC);
	Cmd_AddCommand(const_cast<char*>("soundlist"), S_SoundList);
	Cmd_AddCommand(const_cast<char*>("soundinfo"), S_SoundInfo_f);

	Cvar_RegisterVariable(&s_show);
	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&suitvolume);
	Cvar_RegisterVariable(&hisound);
	Cvar_RegisterVariable(&loadas8bit);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&MP3Volume);
	Cvar_RegisterVariable(&MP3FadeTime);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);
	Cvar_RegisterVariable(&speak_enable);

	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set(const_cast<char*>("loadas8bit"), const_cast<char*>("1"));
		Con_DPrintf(const_cast<char*>("loading all sounds as 8bit\n"));
	}

	snd_initialized = true;

	S_Startup();

	SND_InitScaletable();

	known_sfx = (sfx_t*)Hunk_AllocName(MAX_SFX * sizeof(sfx_t), const_cast<char*>("sfx_t"));
	num_sfx = 0;

	// create a piece of DMA memory

	if (fakedma)
	{
		shm = (dma_t *)Hunk_AllocName(sizeof(*shm), const_cast<char*>("shm"));
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = SOUND_DMA_SPEED;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = (unsigned char*)Hunk_AllocName(1 << 16, const_cast<char*>("shmbuf"));

		Con_DPrintf(const_cast<char*>("Sound sampling rate: %i\n"), shm->speed);
	}

	// provides a tick sound until washed clean

	//	if (shm->buffer)
	//		shm->buffer[4] = shm->buffer[5] = 0x7f;	// force a pop for debugging

	// ambient_sfx[AMBIENT_WATER] = S_PrecacheSound("ambience/water1.wav");
	// ambient_sfx[AMBIENT_SKY] = S_PrecacheSound("ambience/wind2.wav");

	S_StopAllSounds(true);
	SX_Init();
	Wavstream_Init();
}

/*
================
S_Startup
================
*/

void S_Startup(void)
{
	int		rc;

	if (!snd_initialized)
		return;

	if (fakedma || SNDDMA_Init())
	{
		sound_started = true;
	}
	else
	{
		Con_Printf(const_cast<char*>("S_Startup: SNDDMA_Init failed.\n"));
		sound_started = false;
	}
}


void S_Shutdown()
{
	VOX_Shutdown();
	if (sound_started)
	{
		if (shm)
			shm->gamealive = false;

		shm = NULL;
		sound_started = false;
		if (!fakedma)
			SNDDMA_Shutdown();
	}
}

sfx_t* S_PrecacheSound( char* name )
{
	sfx_t	*sfx;

	if (!sound_started || nosound.value || Q_strlen(name) >= MAX_QPATH)
		return NULL;

	if (name[0] == CHAR_STREAM || name[0] == CHAR_SENTENCE)
		return S_FindName(name, 0);

	sfx = S_FindName(name, 0);

	// cache it in
	if (!fs_lazy_precache.value)
		S_LoadSound(sfx, NULL);

	return sfx;

}

void S_StopSound(int entnum, int entchannel)
{
	for (int i = NUM_AMBIENTS; i < total_channels; i++)
	{
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel)
			S_FreeChannel(&channels[i]);
	}
}

void S_ClearBuffer(void)
{
	if (!sound_started || !shm)
		return;

	if (shm->buffer != NULL || pDSBuf != NULL)
	{
		int clear = shm->samplebits != 8 ? 0 : 0x80;

		if (pDSBuf != NULL)
		{
			DWORD	dwSize;
			DWORD	*pData;
			int		reps;
			HRESULT	hresult;

			reps = 0;

			while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, (LPVOID*)&pData, &dwSize, NULL, NULL, 0)) != DS_OK)
			{
				if (hresult != DSERR_BUFFERLOST)
				{
					Con_Printf(const_cast<char*>("S_ClearBuffer: DS::Lock Sound Buffer Failed\n"));
					S_Shutdown();
					return;
				}

				if (++reps > 10000)
				{
					Con_Printf(const_cast<char*>("S_ClearBuffer: DS: couldn't restore buffer\n"));
					S_Shutdown();
					return;
				}
			}

			Q_memset(pData, clear, shm->samples * shm->samplebits / 8);

			pDSBuf->lpVtbl->Unlock(pDSBuf, pData, dwSize, NULL, 0);
		}
		else
		{
			Q_memset(shm->buffer, clear, shm->samples * shm->samplebits / 8);
		}
	}
}

void S_StopAllSounds( bool clear )
{
	if (!sound_started) return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (int i = 0; i<MAX_CHANNELS; i++)
		if (channels[i].sfx)
			S_FreeChannel(&channels[i]);

	Q_memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));
	Wavstream_Init();

	if (clear)
		S_ClearBuffer();

}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	if (shm != NULL)
		fullsamples = shm->samples / shm->channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();


	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds(true);
		}
	}
	oldsamplepos = samplepos;

	if (shm != NULL)
		soundtime = buffers*fullsamples + samplepos / shm->channels;
}

void S_ExtraUpdate (void)
{
	if (!VGuiWrap2_IsGameUIVisible())
	{
		SDL_PumpEvents();
		ClientDLL_IN_Accumulate();
	}

	if (snd_noextraupdate.value)
		return;		// don't pollute timings

	S_Update_();
}

void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started || snd_blocked > 0)
		return;

	// Updates DMA time
	GetSoundtime();

	if (shm != NULL)
	{
		endtime = soundtime + _snd_mixahead.value * shm->dmaspeed;
		samps = shm->samples >> byte(shm->channels - 1);
	}

	endtime = min((int)endtime, soundtime + samps);

#ifdef _WIN32
	// if the buffer was lost or stopped, restore it and/or restart it
	{
		if (pDSBuf)
		{
			DWORD	dwStatus;

			if (pDSBuf->lpVtbl->GetStatus(pDSBuf, &dwStatus) != DS_OK)
				Con_Printf(const_cast<char*>("Couldn't get sound buffer status\n"));

			if (dwStatus & DSBSTATUS_BUFFERLOST)
				pDSBuf->lpVtbl->Restore(pDSBuf);

			if (!(dwStatus & DSBSTATUS_PLAYING))
				pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
		}
	}
#endif

	S_PaintChannels(endtime >> 1);

	SNDDMA_Submit();

}

/*
==================
S_FindName

==================
*/
// Return sfx and set pfInCache to 1 if 
// name is in name cache. Otherwise, alloc
// a new spot in name cache and return 0 
// in pfInCache.

sfx_t *S_FindName(char *name, int *pfInCache)
{
	int		i;
	sfx_t	*sfx = NULL;

	if (!name)
		Sys_Error("S_FindName: NULL\n");

	if (Q_strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

	// see if already loaded
	for (i = 0; i < num_sfx; i++)
	{
		if (!Q_strcasecmp(known_sfx[i].name, name))
		{
			if (pfInCache)
				*pfInCache = Cache_Check(&known_sfx[i].cache) != NULL;

			if (known_sfx[i].servercount > 0)
				known_sfx[i].servercount = cl.servercount;

			return &known_sfx[i];
		}

		if (sfx == NULL && known_sfx[i].servercount > 0 && known_sfx[i].servercount != cl.servercount)
			sfx = &known_sfx[i];
	}

	if (sfx != NULL)
	{
		if (Cache_Check(&sfx->cache))
			Cache_Free(&sfx->cache);
	}
	else
	{
		if (num_sfx == MAX_SFX)
			Sys_Error("S_FindName: out of sfx_t");

		sfx = &known_sfx[i];

		num_sfx++;
	}

	Q_strncpy(sfx->name, name, sizeof(sfx_s::name));
	sfx->name[sizeof(sfx_s::name) - 1] = '\0';

	if (pfInCache)
		*pfInCache = NULL;

	sfx->servercount = cl.servercount;

	return sfx;
}


// search through all channels for a channel that matches this
// soundsource, entchannel and sfx, and perform alteration on channel
// as indicated by 'flags' parameter. If shut down request and
// sfx contains a sentence name, shut off the sentence.
// returns TRUE if sound was altered,
// returns FALSE if sound was not found (sound is not playing)
int S_AlterChannel(int entnum, int entchannel, sfx_t *sfx, int vol, int pitch, int flags)
{
	int ch_idx;
	if (sfx->name[0] == CHAR_SENTENCE)
	{
		for (ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++)
		{
			if (entnum == channels[ch_idx].entnum 
				&& entchannel == channels[ch_idx].entchannel 
				&& channels[ch_idx].sfx != NULL 
				&& channels[ch_idx].isentence >= 0)
			{
				if (flags & SND_CHANGE_PITCH)
					channels[ch_idx].pitch = pitch;
				if (flags & SND_CHANGE_VOL)
					channels[ch_idx].master_vol = vol;
				if (flags & SND_STOP)
					S_FreeChannel(&channels[ch_idx]);

				return true;
			}
		}
		// channel not found
		return false;
	}

	// regular sound or streaming sound
	
	for (ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++)
	{
		if (entnum == channels[ch_idx].entnum && entchannel == channels[ch_idx].entchannel && channels[ch_idx].sfx == sfx)
		{
			if (flags & SND_CHANGE_PITCH)
				channels[ch_idx].pitch = pitch;
			if (flags & SND_CHANGE_VOL)
				channels[ch_idx].master_vol = vol;
			if (flags & SND_STOP)
				S_FreeChannel(&channels[ch_idx]);

			return true;
		}
	}

	return false;
}

qboolean SND_FStreamIsPlaying(sfx_t* sfx)
{
	int ch_idx;

	for (ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++)
	{
		if (channels[ch_idx].sfx == sfx)
			return true;
	}

	return false;
}

/*
=================
SND_PickDynamicChannel
Select a channel from the dynamic channel allocation area.  For the given entity,
override any other sound playing on the same channel (see code comments below for
exceptions).
=================
*/
channel_t *SND_PickDynamicChannel(int entnum, int entchannel, sfx_t *sfx)
{
	int	ch_idx;
	int	first_to_die;
	int	life_left;

	if (entchannel == CHAN_STREAM && SND_FStreamIsPlaying(sfx))
		return NULL;

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;

	ch_idx = NUM_AMBIENTS;

	for (; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		channel_t *ch = &channels[ch_idx];

		// Never override a streaming sound that is currently playing or
		// voice over IP data that is playing or any sound on CHAN_VOICE( acting )
		if (wavstreams[ch_idx].hFile &&
			(ch->entchannel == CHAN_STREAM))
		{
			if (entchannel == CHAN_VOICE)
				return NULL;

			continue;
		}

		if (entchannel != CHAN_AUTO		// channel 0 never overrides
			&& ch->entnum == entnum  
			&& (ch->entchannel == entchannel || entchannel == -1))
		{
			// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (ch->sfx &&
			ch->entnum == cl.viewentity &&
			entnum != cl.viewentity)
			continue;

		// try to pick the sound with the least amount of data left to play
		int timeleft = ch->end - paintedtime;

		if (timeleft < life_left)
		{
			life_left = timeleft;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
	{
		S_FreeChannel(&channels[first_to_die]);
		channels[first_to_die].sfx = NULL;
	}

	return &channels[first_to_die];
}

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize(channel_t *ch)
{
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;
	sfx_t *snd;

	// anything coming from the view entity will allways be full volume
	if (ch->entnum == cl.viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		VOX_SetChanVol(ch);
		return;
	}

	if (ch->entnum > 0 && ch->entnum < cl.num_entities)
	{
		cl_entity_t* pent = &cl_entities[ch->entnum];
		if (pent != NULL)
		{
			model_t* clmodel = pent->model;
			if (clmodel != NULL && pent->curstate.messagenum == cl.parsecount)
			{
				VectorCopy(pent->origin, ch->origin);
				if (clmodel->type == mod_brush)
				{
					ch->origin[0] += (clmodel->maxs[0] - clmodel->mins[0]) * 0.5;
					ch->origin[1] += (clmodel->maxs[1] - clmodel->mins[1]) * 0.5;
					ch->origin[2] += (clmodel->maxs[2] - clmodel->mins[2]) * 0.5;
				}
			}
		}
	}

	// calculate stereo seperation and distance attenuation

	VectorSubtract(ch->origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec) * ch->dist_mult;

	dot = DotProduct(listener_right, source_vec);

	if (shm && shm->channels == 1)
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int)(ch->master_vol * scale);

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int)(ch->master_vol * scale);

	VOX_SetChanVol(ch);

	if (ch->rightvol < 0)
		ch->rightvol = 0;

	if (ch->leftvol < 0)
		ch->leftvol = 0;
}

void S_StartDynamicSound(int entnum, int entchannel, sfx_t *sfx, vec_t *origin, float fvol, float attenuation, int flags, int pitch)
{
	channel_t *target_chan = NULL;
	int		vol;
	int		fsentence = 0;
	sfxcache_t	*sc = NULL;

	if (!sound_started || !sfx || nosound.value != 0.0)
		return;

	if (sfx->name[0] == CHAR_STREAM)
		entchannel = CHAN_STREAM;

	if (entchannel == CHAN_STREAM)
	{
		if (pitch != PITCH_NORM)
		{
			pitch = PITCH_NORM;
			Con_DPrintf(const_cast<char*>("Warning: pitch shift ignored on stream sound %s\n"), sfx->name);
		}
	}

	vol = fvol * 255;

	if (vol > 255)
	{
		Con_DPrintf(const_cast<char*>("S_StartDynamicSound: %s volume > 255"), sfx->name);
		vol = 255;
	}

	if (flags & (SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH))
	{
		if (S_AlterChannel(entnum, entchannel, sfx, vol, pitch, flags))
			return;
		if (flags & SND_STOP)
			return;
		// fall through - if we're not trying to stop the sound, 
		// and we didn't find it (it's not playing), go ahead and start it up
	}

	if (pitch == 0)
	{
		Con_DPrintf(const_cast<char*>("Warning: S_StartDynamicSound Ignored, called with pitch 0"));
		return;
	}

	// pick a channel to play on
	target_chan = SND_PickDynamicChannel(entnum, entchannel, sfx);

	if (!target_chan)
		return;

	if (sfx->name[0] == CHAR_SENTENCE || sfx->name[0] == CHAR_DRYMIX)
		fsentence = 1;

	// spatialize
	Q_memset(target_chan, 0, sizeof(*target_chan));

	target_chan->master_vol = vol;
	VectorCopy(origin, target_chan->origin);
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	target_chan->pitch = (float)pitch * ((sys_timescale.value + 1.0) * 0.5);
	target_chan->isentence = -1;
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;

	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol && entchannel != CHAN_STREAM)
	{
		target_chan->sfx = NULL;
		return;		// not audible at all
	}

	// new channel
	if (fsentence)
	{
		char voxname[MAX_QPATH];
		Q_strncpy(voxname, &sfx->name[1], sizeof(voxname) - 1);
		voxname[sizeof(voxname)-1] = '\0';
		sc = VOX_LoadSound(target_chan, voxname);
	}
	else
	{
		sc = S_LoadSound(sfx, target_chan);
		target_chan->sfx = sfx;
	}

	if (!sc)
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

//	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + sc->length;

	if (!fsentence && target_chan->pitch != PITCH_NORM)
		VOX_MakeSingleWordSentence(target_chan, target_chan->pitch);
	
	VOX_TrimStartEndTimes(target_chan, sc);

	SND_InitMouth(entnum, entchannel);

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder

	// UNDONE: this should be implemented using a start delay timer in the 
	// mixer, instead of skipping forward in the wave.  Either method also cause
	// phasing problems for skip times of < 10 milliseconds. KB

	int		ch_idx;
	int		skip;
	channel_t *check = &channels[NUM_AMBIENTS];
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = RandomLong(0, (long)(0.1*shm->speed));		// skip up to 0.1 seconds of audio
			skip = min(skip, target_chan->end - 1);

			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}

	}
}

channel_t* SND_PickStaticChannel(int entnum, int entchannel, sfx_t *sfx)
{
	int i;
	channel_t *ch = NULL;

	if (sfx->name[0] == CHAR_STREAM && SND_FStreamIsPlaying(sfx))
		return NULL;

	// Check for replacement sound, or find the best one to replace
	for (i = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i<total_channels; i++)
		if (channels[i].sfx == NULL)
			break;


	if (i < total_channels)
	{
		// reuse an empty static sound channel
		ch = &channels[i];
	}
	else
	{
		// no empty slots, alloc a new static sound channel
		if (total_channels == MAX_CHANNELS)
		{
			Con_DPrintf(const_cast<char*>("total_channels == MAX_CHANNELS\n"));
			return NULL;
		}


		// get a channel for the static sound

		ch = &channels[total_channels];
		total_channels++;
	}

	return ch;
}

/*
=================
S_StartStaticSound
=================
Start playback of a sound, loaded into the static portion of the channel array.
Currently, this should be used for looping ambient sounds, looping sounds
that should not be interrupted until complete, non-creature sentences,
and one-shot ambient streaming sounds.  Can also play 'regular' sounds one-shot,
in case designers want to trigger regular game sounds.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
*/
void S_StartStaticSound(int entnum, int entchannel, sfx_t *sfxin, vec_t* origin, float fvol, float attenuation, int flags, int pitch)
{
	channel_t	*ch = NULL;
	int fvox = 0;
	int vol;
	sfxcache_t	*sc = NULL;
	char name[MAX_QPATH];

	if (!sfxin)
		return;

	if (sfxin->name[0] == CHAR_STREAM)
		entchannel = CHAN_STREAM;

	if (entchannel == CHAN_STREAM)
	{
		if (pitch != PITCH_NORM)
		{
			pitch = PITCH_NORM;
			Con_DPrintf(const_cast<char*>("Warning: pitch shift ignored on stream sound %s\n"), sfxin->name);
		}
	}

	vol = fvol * 255;

	if (vol > 255)
	{
		Con_DPrintf(const_cast<char*>("S_StartStaticSound: %s volume > 255"), sfxin->name);
		vol = 255;
	}

	if (flags & (SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH))
	{
		if (S_AlterChannel(entnum, entchannel, sfxin, vol, pitch, flags))
			return;
		if (flags & SND_STOP)
			return;
		// fall through - if we're not trying to stop the sound, 
		// and we didn't find it (it's not playing), go ahead and start it up
	}

	if (pitch == 0)
	{
		Con_DPrintf(const_cast<char*>("Warning: S_StartStaticSound Ignored, called with pitch 0"));
		return;
	}

	// pick a channel to play on
	ch = SND_PickStaticChannel(entnum, entchannel, sfxin);

	if (!ch)
		return;

	if (sfxin->name[0] == CHAR_SENTENCE || sfxin->name[0] == CHAR_DRYMIX)
	{
		// this is a sentence. link words to play in sequence.

		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 

		// link all words and load the first word
		Q_strncpy(name, &sfxin->name[1], sizeof(name) - 1);
		name[sizeof(name) - 1] = '\0';
		sc = VOX_LoadSound(ch, name);
		fvox = 1;
	}
	else
	{
		sc = S_LoadSound(sfxin, ch);
		ch->isentence = -1;
		ch->sfx = sfxin;
	}

	if (!sc)
	{
		ch->sfx = NULL;
		return;
	}

	// spatialize
	ch->pos = 0;
	VectorCopy(origin, ch->origin);
	ch->master_vol = vol;
	ch->end = paintedtime + sc->length;
	ch->entnum = entnum;
	ch->pitch = (float)pitch * ((sys_timescale.value + 1.0) * 0.5);
	ch->entchannel = entchannel;
	ch->dist_mult = attenuation / sound_nominal_clip_dist;

	if (!fvox && ch->pitch != PITCH_NORM)
		VOX_MakeSingleWordSentence(ch, ch->pitch);

	VOX_TrimStartEndTimes(ch, sc);

	SND_Spatialize(ch);
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
	static int hash = 345;
	int 	i;
	char name[256];
	sfx_t	*sfx;

	if (speak_enable.value == 0)
		return;

	i = 1;
	while (i<Cmd_Argc())
	{
		// 251 + strlen(.wav)
		Q_strncpy(name, (char*)Cmd_Argv(i), 251);
		name[251] = 0;

		if (!Q_strrchr((char*)Cmd_Argv(i), '.'))
			Q_strcat(name, const_cast<char*>(".wav"));

		sfx = S_PrecacheSound(name);
		S_StartDynamicSound(hash++, CHAN_AUTO, sfx, listener_origin, VOL_NORM, 1.0, 0, PITCH_NORM);
		i++;
	}
}

void S_PlayVol(void)
{
	static int hash = 543;
	int i;
	float vol;
	char name[256];
	sfx_t	*sfx;

	if (speak_enable.value == 0)
		return;

	i = 1;
	while (i<Cmd_Argc())
	{
		Q_strncpy(name, (char*)Cmd_Argv(i), 251);
		name[251] = 0;

		if (!Q_strrchr((char*)Cmd_Argv(i), '.'))
			Q_strcat(name, const_cast<char*>(".wav"));

		sfx = S_PrecacheSound(name);
		vol = Q_atof((char*)Cmd_Argv(i + 1));
		S_StartDynamicSound(hash++, CHAN_AUTO, sfx, listener_origin, vol, 1.0, 0, PITCH_NORM);
		i += 2;
	}
}

void S_Say(void)
{
	sfx_t *sfx;
	char sound[256];

	if (nosound.value || !sound_started || Cmd_Argc() < 2)
		return;

	if (speak_enable.value == 0)
		return;

	Q_strncpy(sound, (char*)Cmd_Argv(1), sizeof(sound) - 1);
	sound[sizeof(sound) - 1] = 0;

	// DEBUG - test performance of dsp code
	if (!Q_strcmp(sound, "dsp"))
	{
		unsigned time;
		int i;
		int count = 10000;

		for (i = 0; i < PAINTBUFFER_SIZE; i++)
		{
			paintbuffer[i].left = RandomLong(0, 2999);
			paintbuffer[i].right = RandomLong(0, 2999);
		}

		Con_Printf(const_cast<char*>("Start profiling 10,000 calls to DSP\n"));

		// get system time

		time = timeGetTime();

		for (i = 0; i < count; i++)
		{
			SX_RoomFX(PAINTBUFFER_SIZE, TRUE, TRUE);
		}
		// display system time delta 
		Con_Printf(const_cast<char*>("%d milliseconds \n"), timeGetTime() - time);
		return;

	}
	else if (!Q_strcmp(sound, "paint"))
	{
		unsigned time;
		int count = 10000;
		static int hash = 543;
		int psav = paintedtime;

		Con_Printf(const_cast<char*>("Start profiling S_PaintChannels\n"));

		sfx = S_PrecacheSound(const_cast<char*>("ambience/labdrone1.wav"));
		S_StartDynamicSound(hash++, CHAN_AUTO, sfx, listener_origin, VOL_NORM, 1.0, 0, PITCH_NORM);

		// get system time
		time = timeGetTime();

		// paint a boatload of sound

		S_PaintChannels(paintedtime + 512 * count);

		// display system time delta 
		Con_Printf(const_cast<char*>("%d milliseconds \n"), timeGetTime() - time);
		paintedtime = psav;
		return;
	}
	// DEBUG

	if (sound[0] != CHAR_SENTENCE)
	{
		// build a fake sentence name, then play the sentence text
		snprintf(sound, sizeof(sound), "xxtestxx %s", Cmd_Argv(1));

		// insert null terminator after sentence name
		sound[8] = 0;

		rgpszrawsentence[cszrawsentences++] = sound;

		sfx = S_PrecacheSound(const_cast<char*>("!xxtestxx"));
		if (!sfx)
		{
			Con_Printf(const_cast<char*>("S_Say: can't cache %s\n"), sound);
			return;
		}

		S_StartDynamicSound(cl.viewentity, -1, sfx, vec3_origin, VOL_NORM, 1.0, 0, PITCH_NORM);

		// remove last
		rgpszrawsentence[--cszrawsentences] = NULL;
	}
	else
	{
		sfx = S_FindName(sound, NULL);
		if (!sfx)
		{
			Con_Printf(const_cast<char*>("S_Say: can't find sentence name %s\n"), sound);
			return;
		}

		S_StartDynamicSound(cl.viewentity, -1, sfx, vec3_origin, VOL_NORM, 1.0f, 0, PITCH_NORM);
	}
}

void S_Say_Reliable(void)
{
	sfx_t *sfx;
	char sound[256];

	if (nosound.value || !sound_started || Cmd_Argc() < 2)
		return;

	if (speak_enable.value == 0)
		return;

	Q_strncpy(sound, (char*)Cmd_Argv(1), sizeof(sound) - 1);
	sound[sizeof(sound) - 1] = 0;

	if (sound[0] != CHAR_SENTENCE)
	{
		// build a fake sentence name, then play the sentence text
		snprintf(sound, sizeof(sound), "xxtestxx %s", Cmd_Argv(1));

		// insert null terminator after sentence name
		sound[8] = 0;

		rgpszrawsentence[cszrawsentences++] = sound;

		sfx = S_PrecacheSound(const_cast<char*>("!xxtestxx"));
		if (!sfx)
		{
			Con_Printf(const_cast<char*>("S_Say_Reliable: can't cache %s\n"), sound);
			return;
		}

		S_StartStaticSound(cl.viewentity, CHAN_STATIC, sfx, vec3_origin, VOL_NORM, 1.0f, 0, PITCH_NORM);

		// remove last
		rgpszrawsentence[--cszrawsentences] = NULL;
	}
	else
	{
		sfx = S_FindName(sound, NULL);
		if (!sfx)
		{
			Con_Printf(const_cast<char*>("S_Say_Reliable: can't find sentence name %s\n"), sound);
			return;
		}

		S_StartStaticSound(cl.viewentity, CHAN_STATIC, sfx, vec3_origin, VOL_NORM, 1.0f, 0, PITCH_NORM);
	}
}

void S_StopAllSoundsC(void)
{
	S_StopAllSounds(true);
}

void S_SoundList(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx = known_sfx, i = 0; i<num_sfx; i++, sfx++)
	{
		sc = (sfxcache_t*)Cache_Check(&sfx->cache);
		if (!sc)
			continue;
		size = sc->length*sc->width*(sc->stereo + 1);
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf(const_cast<char*>("L"));
		else
			Con_Printf(const_cast<char*>(" "));
		Con_Printf(const_cast<char*>("(%2db) %6i : %s\n"), sc->width * 8, size, sfx->name);
	}
	Con_Printf(const_cast<char*>("Total resident: %i\n"), total);
}


void S_LocalSound(char *sound)
{
	sfx_t	*sfx;
	
	if (nosound.value)
		return;
	if (!sound_started)
		return;

	sfx = S_PrecacheSound(sound);
	if (!sfx)
	{
		Con_Printf(const_cast<char*>("S_LocalSound: can't cache %s\n"), sound);
		return;
	}
	S_StartDynamicSound(cl.viewentity, -1, sfx, vec3_origin, VOL_NORM, 1.0f, 0, PITCH_NORM);
}


void S_ClearPrecache(void)
{
}


void S_BeginPrecaching(void)
{
	cl.fPrecaching = 1;
}


void S_EndPrecaching(void)
{
	cl.fPrecaching = 0;
}

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds(void)
{
	mleaf_t		*l;
	float		vol;
	int			ambient_channel;
	channel_t	*chan;

	if (!snd_ambient)
		return;

	// calc ambient sound levels
	if (!cl.worldmodel)
		return;

	l = Mod_PointInLeaf(listener_origin, cl.worldmodel);
	if (!l || !ambient_level.value)
	{
		for (ambient_channel = 0; ambient_channel< NUM_AMBIENTS; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0; ambient_channel< NUM_AMBIENTS; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_frametime * ambient_fade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_frametime * ambient_fade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}


/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i, j;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds();

	combine = NULL;

	// update spatialization for static and dynamic sounds	
	ch = channels + NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i<total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch);         // respatialize channel
	}

	//
	// debugging output
	//
	if (snd_show.value)
	{
		total = 0;
		ch = channels;
		for (i = 0; i<total_channels; i++, ch++)
		if (ch->sfx && (ch->leftvol || ch->rightvol))
		{
			Con_Printf(const_cast<char*>("%3i %3i %s\n"), ch->leftvol, ch->rightvol, ch->sfx->name);
			total++;
		}

		Con_Printf(const_cast<char*>("----(%i)----\n"), total);
	}

	// mix some sound
	S_Update_();
}

void S_PrintStats()
{
	if (!s_show.value || cl.maxclients > 2)
		return;
	
	for (int i = 0; i < total_channels; i++)
	{
		if (channels[i].sfx)
			Con_NPrintf(i & CON_MAX_DEBUG_AREAS - 1, const_cast<char*>("%s %03i %02i %20s"), vstr(channels[i].origin), channels[i].entnum, channels[i].entchannel, channels[i].sfx->name);
		else
			Con_NPrintf(i & CON_MAX_DEBUG_AREAS - 1, const_cast<char*>(""));
	}
}
