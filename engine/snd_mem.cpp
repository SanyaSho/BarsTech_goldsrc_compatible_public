#include "quakedef.h"
#include "sound.h"
#include "client.h"
#include "voice.h"
#include "cl_main.h"
#include "voice_sound_engine_interface.h"

qboolean GetWavinfo(char* name, byte* wav, int wavlength, wavinfo_t* info);

/*
================
ResampleSfx
в патче от 2020 года принимаемых аргументов 5
================
*/
void ResampleSfx(sfx_t *sfx, int inrate, int inwidth, byte *data
#ifdef _2020_PATCH
	, int datasize
#endif
)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, samplefrac, fracstep;
	sfxcache_t	*sc = NULL;

	struct cache_system_t2
	{
		int						size;		// including this header
		cache_user_t* user;
		char					name[64];
		cache_system_t2* prev, * next;
		cache_system_t2* lru_prev, * lru_next;	// for LRU flushing	
	};


	cache_system_t2*cs = ((cache_system_t2*)sfx->cache.data) - 1;

	sc = (sfxcache_t*)Cache_Check(&sfx->cache);
	if (!sc)
		return;

	if (shm == NULL)
	{
		stepscale = 1;

		outcount = sc->length / stepscale;
		sc->length = outcount;

		if (sc->loopstart != -1)
			sc->loopstart /= stepscale;
	}
	else
	{
		stepscale = (float)inrate / (float)shm->speed;	// this is usually 0.5, 1, or 2

		if (stepscale != 2 || hisound.value == 0.0)
		{
			outcount = sc->length / stepscale;
			sc->length = outcount;
			if (sc->loopstart != -1)
				sc->loopstart = sc->loopstart / stepscale;

			sc->speed = shm->speed;
		}
		else
		{
			outcount = sc->length;
			stepscale = 1;
		}
	}
	if (loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
#ifdef _2020_PATCH
		if (outcount > datasize)
		{
			Con_DPrintf(const_cast<char*>(__FUNCTION__ ": %s has invalid sample count: %d datasize: %d\n"), sfx->name, outcount, datasize);
			memset(sc->data, 0, outcount);
			return;
		}
#endif

		// fast special case
		for (i = 0; i<outcount; i++)
			sc->data[i] = data[i] + 128;
	}
	else
	{
		if (stepscale != 1.0 && stepscale != 2.0)
			Con_DPrintf(const_cast<char*>("WARNING! %s is causing runtime sample conversion!\n"), sfx->name);

		// general case
		samplefrac = 0;
		fracstep = stepscale * 256;
		
#ifdef _2020_PATCH
		// тк сравнение идёт с максимальным значением подписанного в большую сторону, то тип исходного неподписанный
		unsigned int sampleprod = fracstep * (outcount - 1);

		// js в отношении 32-бит регистра - 31 бит <=> проверка на отрицательность
		if (int(sampleprod) < 0 && (sampleprod / 256) >= datasize)
		{
			Con_DPrintf(const_cast<char*>(__FUNCTION__ ": %s has invalid resample count: %d step: %d datasize: %d\n"), sfx->name, outcount, fracstep, datasize);
			memset(sc->data, 0, outcount);
			return;
		}
#endif

		for (i = 0; i<outcount; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort(((short *)data)[srcsample]);
			else
				sample = (int)((unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				sc->data[i] = sample >> 8;
		}
	}
}

sfxcache_t* S_LoadStreamSound(sfx_t *s, channel_t *ch)
{
	char *dest, *src;
	int count;
	BYTE* data = NULL;
	char namebuffer[256]; 
	char wavname[64]; 
	wavinfo_t info; 
	int cbread;
	qboolean firstLoad = false;

	if (cl.fPrecaching)
		return NULL;

	Q_strncpy(wavname, &s->name[1], sizeof(wavname) - 1);
	wavname[sizeof(wavname) - 1] = 0;

	int i = ch - ::channels;
	sfxcache_t* sc = (sfxcache_t*)Cache_Check(&s->cache);
	if (sc != NULL)
	{
		if (wavstreams[i].hFile)
			return sc;
	}

	Q_strcpy(namebuffer, "sound");
	if (wavname[0] != '/')
		strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));
	strncat(namebuffer, wavname, sizeof(namebuffer) - 1 - strlen(namebuffer));

	firstLoad = wavstreams[i].hFile == NULL;

	data = COM_LoadFileLimit(namebuffer, wavstreams[i].lastposloaded, 32768, &cbread, &wavstreams[i].hFile);

	if (!data)
	{
		Con_DPrintf(const_cast<char*>("S_LoadStreamSound: Couldn't load %s\n"), namebuffer);
		return NULL;
	}

	if (firstLoad)
	{
		if (!GetWavinfo(s->name, data, cbread, &info))
		{
			Con_DPrintf(const_cast<char*>("Failed loading %s\n"), wavname);
			return NULL;
		}

		if (info.channels != 1)
		{
			Con_DPrintf(const_cast<char*>("%s is a stereo sample\n"), wavname);
			return NULL;
		}

		if (info.width != 1)
		{
			Con_DPrintf(const_cast<char*>("%s is a 16 bit sample\n"), wavname);
			return NULL;
		}

		if (shm && info.rate != shm->speed)
		{
			Con_DPrintf(const_cast<char*>("%s ignored, not stored at playback sample rate!\n"), wavname);
			return NULL;
		}

		wavstreams[i].csamplesinmem = cbread - info.dataofs;
	}
	else
	{
		info = wavstreams[i].info;
		wavstreams[i].csamplesinmem = cbread;
	}

	wavstreams[i].csamplesinmem = min(wavstreams[i].csamplesinmem, info.samples - wavstreams[i].csamplesplayed);
	wavstreams[i].info = info;

	if (sc == NULL)
		sc = (sfxcache_t*)Cache_Alloc(&s->cache, min(cbread, 0x8000) + sizeof(sfxcache_t), wavname);
	if (sc == NULL)
		return NULL;

	sc->length = wavstreams[i].csamplesinmem;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	if (!firstLoad)
		ResampleSfx(s, sc->speed, sc->width, data
#ifdef _2020_PATCH
		, cbread - info.dataofs
#endif
		);
	else
		ResampleSfx(s, sc->speed, sc->width, data + info.dataofs
#ifdef _2020_PATCH
		, cbread
#endif
		);

	return sc;
}

/*
==============
S_LoadSound
==============
*/
sfxcache_t* S_LoadSound3(sfx_t* s, channel_t* ch)
{
	float len;
	float stepscale;
	double startTime;

	byte stackbuf[1024];
	char namebuffer[256];

	int size;
	int filesize;
	wavinfo_t info;

	FileHandle_t hFile = FILESYSTEM_INVALID_HANDLE;

	byte* data = nullptr;
	sfxcache_t* sc = nullptr;

	if (s->name[0] == '*')
		return S_LoadStreamSound(s, ch);

	//if (s->name[0] == '?') TODO: Implement
		//return VoiceSE_GetSFXCache(s);

	sc = (sfxcache_t*)Cache_Check(&s->cache);
	if (sc)
	{
		if (hisound.value != 0.0 || !shm || sc->speed <= shm->speed)
			return sc;
		Cache_Free(&s->cache);
	}

	if (fs_precache_timings.value != 0.0)
		startTime = Sys_FloatTime();

	Q_strcpy(namebuffer, "sound");

	if (s->name[0] != '/')
		strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));
	strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));

	hFile = FS_Open(namebuffer, "rb");

	if (!hFile)
	{
		namebuffer[0] = '\0';

		if (s->name[0] != '/')
			strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));

		strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));
		namebuffer[sizeof(namebuffer) - 1] = 0;

		data = COM_LoadStackFile(namebuffer, stackbuf, 1024, &filesize);

		if (!data)
		{
			Con_DPrintf((char*)"S_LoadSound: Couldn't load %s\n", namebuffer);
			return nullptr;
		}

		hFile = 0;
	}
	else
	{
		data = (byte*)FS_GetReadBuffer(hFile, &filesize);

		if (!data)
		{
			filesize = FS_Size(hFile);
			data = (byte*)Hunk_TempAlloc(filesize + 1);

			FS_Read(data, filesize, 1, hFile);
			FS_Close(hFile);

			if (!data)
			{
				namebuffer[0] = '\0';

				if (s->name[0] != '/')
					strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));

				strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));
				namebuffer[sizeof(namebuffer) - 1] = 0;

				data = COM_LoadStackFile(namebuffer, stackbuf, 1024, &filesize);

				if (!data)
				{
					Con_DPrintf((char*)"S_LoadSound: Couldn't load %s\n", namebuffer);
					return nullptr;
				}
			}

			hFile = 0;
		}
	}

	if (!GetWavinfo(s->name, data, filesize, &info))
	{
		Con_DPrintf((char*)"Failed loading %s\n", s->name);
		return nullptr;
	}

	if (info.channels != 1)
	{
		Con_DPrintf((char*)"%s is a stereo sample\n", s->name);
		return nullptr;
	}

	if (!info.rate)
	{
		Con_DPrintf((char*)"Invalid rate: %s\n", s->name);
		return nullptr;
	}

	if (info.dataofs >= filesize)
	{
		Con_DPrintf((char*)"%s has invalid data offset\n", s->name);
		return nullptr;
	}

	stepscale = 1.f;

	if ((shm && info.rate != shm->speed) && info.rate != 2 * shm->speed || hisound.value <= 0.0)
	{
		stepscale = (float)info.rate / (float)shm->speed;

		if (stepscale == 0.0)
		{
			Con_DPrintf((char*)"Invalid stepscale: %s\n", s->name);
			return nullptr;
		}
	}

	size = 24;
	len = (float)info.samples / stepscale;

	if (len)
	{
		if (0x7FFFFFFF / len < 1)
		{
			Con_DPrintf((char*)"Invalid length (s/c): %s\n", s->name);
			return nullptr;
		}

		if (0x7FFFFFFF / len < info.width)
		{
			Con_DPrintf((char*)"Invalid length (s/c/w): %s\n", s->name);
			return nullptr;
		}

		if ((uint32)(0x7FFFFFFF - len * info.width) < 24)
			Con_DPrintf((char*)"Invalid length (s/c/w/sfx): %s\n", s->name);

		size = len * info.width + 24;
	}

	sc = (sfxcache_t*)Cache_Alloc(&s->cache, size, s->name);

	if (sc)
	{
		sc->length = info.samples;
		sc->loopstart = info.loopstart;
		sc->speed = info.rate;
		sc->width = info.width;
		sc->stereo = info.channels;

		ResampleSfx(s, sc->speed, sc->width, data + info.dataofs, filesize - info.dataofs);

		if (hFile)
		{
			FS_ReleaseReadBuffer(hFile, data);
			FS_Close(hFile);
		}

		if (fs_precache_timings.value != 0.0)
		{
			// Print the loading time
			Con_DPrintf((char*)"fs_precache_timings: loaded sound %s in time %.3f sec\n", namebuffer, Sys_FloatTime() - startTime);
		}
	}

	return sc;
}
sfxcache_t *S_LoadSound(sfx_t *s, channel_t* ch)
{
	char	namebuffer[256];
	byte	*data = NULL;
	wavinfo_t	info;
	int		len;
	float	stepscale;
	sfxcache_t	*sc = NULL;
	byte	stackbuf[1 * 1024];		// avoid dirtying the cache heap
	double startTime;
	FileHandle_t hFile = FILESYSTEM_INVALID_HANDLE;
	int filesize;

	if (s->name[0] == CHAR_STREAM)
		return (sfxcache_t *)S_LoadStreamSound(s, ch);
	if (s->name[0] == CHAR_USERVOX)
		return (sfxcache_t *)VoiceSE_GetSFXCache(s);
	// stopline

	// see if still in memory
	sc = (sfxcache_t*)Cache_Check(&s->cache);
	if (sc)
	{
		if (hisound.value != 0.0 || !shm || sc->speed <= shm->speed)
			return sc;
		Cache_Free(&s->cache);
	}

	if (fs_precache_timings.value != 0.0)
		startTime = Sys_FloatTime();

	//Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
	// load it in
	Q_strcpy(namebuffer, "sound");
	if (s->name[0] != '/')
		strncat(namebuffer, "/", sizeof(namebuffer) - 1 - Q_strlen(namebuffer));
	strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - Q_strlen(namebuffer));

	//	Con_Printf ("loading %s\n",namebuffer);

	hFile = FS_Open(namebuffer, "rb");

	if (hFile != NULL)
	{
		data = (byte*)FS_GetReadBuffer(hFile, &filesize);

		if (data == NULL)
		{
			filesize = FS_Size(hFile);
			data = (byte*)Hunk_TempAlloc(filesize + 1);
			FS_Read(data, filesize, 1, hFile);
			FS_Close(hFile);

			if (data != NULL)
				hFile = NULL;
		}
	}

	if (data == NULL && hFile == NULL)
	{
		namebuffer[0] = '\0';
		if (s->name[0] != '/')
			strncat(namebuffer, "/", sizeof(namebuffer) - 1 - Q_strlen(namebuffer));
		strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - Q_strlen(namebuffer));
		namebuffer[sizeof(namebuffer) - 1] = '\0';

		data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf), &filesize);

		if (!data)
		{
			Con_DPrintf(const_cast<char*>(__FUNCTION__ ": Couldn't load %s\n"), namebuffer);
			return NULL;
		}
	}

	if (!GetWavinfo(s->name, data, filesize, &info))
	{
		Con_DPrintf(const_cast<char*>("Failed loading %s\n"), s->name);
		return NULL;
	}

	if (info.channels != 1)
	{
		Con_Printf(const_cast<char*>("%s is a stereo sample\n"), s->name);
		return NULL;
	}

	if (!info.rate)
	{
		Con_DPrintf(const_cast<char*>("Invalid rate: %s\n"), s->name);
		return NULL;
	}

#ifdef _2020_PATCH
	if (info.dataofs >= filesize)
	{
		Con_DPrintf(const_cast<char*>("%s has invalid data offset\n"), s->name);
		return NULL;
	}
#endif

	if ((shm != NULL && info.rate != shm->speed) && info.rate != shm->speed * 2 || hisound.value <= 0)
	{
		stepscale = (float)info.rate / (float)(shm->speed);

		if (stepscale == 0.0f)
		{
			Con_DPrintf(const_cast<char*>("Invalid stepscale: %s\n"), s->name);
			return NULL;
		}
	}
	else 
		stepscale = 1;

	len = (float)info.samples / stepscale;

	if (len != 0)
	{
		if (0x7FFFFFFF / len < 1)
		{
			Con_DPrintf(const_cast<char*>("Invalid length (s/c): %s\n"), s->name);
			return NULL;
		}

		if (0x7FFFFFFF / len < info.width)
		{
			Con_DPrintf(const_cast<char*>("Invalid length (s/c/w): %s\n"), s->name);
			return NULL;
		}

		len *= info.width;

		if (0x7FFFFFFF - len < sizeof(sfxcache_t))
			Con_DPrintf(const_cast<char*>("Invalid length (s/c/w/sfx): %s\n"), s->name);
	}

	sc = (sfxcache_t*)Cache_Alloc(&s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
		return NULL;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;
	
	ResampleSfx(s, sc->speed, sc->width, (byte*)data + info.dataofs
#ifdef _2020_PATCH
	, filesize - info.dataofs
#endif
	);

	if (hFile != NULL)
	{
		FS_ReleaseReadBuffer(hFile, data);
		FS_Close(hFile);
	}

	if (fs_precache_timings.value != 0.0)
		Con_DPrintf(const_cast<char*>("fs_precache_timings: loaded sound %s in time %.3f sec\n"), namebuffer, Sys_FloatTime() - startTime);

	return sc;
}

/*
===============================================================================

WAV loading

===============================================================================
*/


byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;


short GetLittleShort(void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	data_p += 2;
	return val;
}

int GetLittleLong(void)
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	val = val + (*(data_p + 2) << 16);
	val = val + (*(data_p + 3) << 24);
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		data_p = last_chunk;

		if (last_chunk >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0 || !_stricmp(name, "LIST") && (iff_chunk_len + data_p) > iff_end)
		{
			data_p = NULL;
			return;
		}

		if (iff_chunk_len > 10240*10240)
		{
//			Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ((iff_chunk_len + 1) & ~1);
		if (!Q_strncmp((char*)data_p, name, 4))
			return;
	}
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk(name);
}


void DumpChunks(void)
{
	char	str[5];

	str[4] = 0;
	data_p = iff_data;
	do
	{
		memcpy(str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf(const_cast<char*>("0x%x : %s (%d)\n"), (uintptr_t)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

/*
============
GetWavinfo
============
*/
qboolean GetWavinfo(char *name, byte *wav, int wavlength, wavinfo_t* info)
{
	int     i;
	int     format;
	int		samples;

	if (!wav || !info)
		return false;

	Q_memset(info, 0, sizeof(wavinfo_t));

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	FindChunk(const_cast<char*>("RIFF"));
	if (!(data_p && !Q_strncmp((char*)data_p + 8, "WAVE", 4)))
	{
		Con_Printf(const_cast<char*>("Missing RIFF/WAVE chunks\n"));
		return false;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;
	// DumpChunks ();

	FindChunk(const_cast<char*>("fmt "));
	if (!data_p)
	{
		Con_Printf(const_cast<char*>("Missing fmt chunk\n"));
		return false;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Con_Printf(const_cast<char*>("Microsoft PCM format only\n"));
		return false;
	}

	info->channels = GetLittleShort();
	info->rate = GetLittleLong();
	data_p += 4 + 2;
	info->width = GetLittleShort() / 8;

	if (info->width == 0)
	{
		Con_Printf(const_cast<char*>("Invalid sample width\n"));
		return false;
	}
	
	// get cue chunk
	FindChunk(const_cast<char*>("cue "));
	if (data_p)
	{
		data_p += 32;
		info->loopstart = GetLittleLong();
		//		Con_Printf("loopstart=%d\n", sfx->loopstart);

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk(const_cast<char*>("LIST"));
		if (data_p)
		{
			if (!Q_strncmp((char*)data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong();	// samples in loop
				info->samples = info->loopstart + i;
				//				Con_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info->loopstart = -1;

	// find data chunk
	FindChunk(const_cast<char*>("data"));
	if (!data_p)
	{
		Con_Printf(const_cast<char*>("Missing data chunk\n"));
		return false;
	}

	data_p += 4;
	samples = GetLittleLong() / info->width;

	if (info->samples)
	{
		if (samples < info->samples)
			Sys_Error("Sound %s has a bad loop length", name);
	}
	else
		info->samples = samples;

	info->dataofs = data_p - wav;

	if (info->channels < 0 || info->rate < 0 || info->width < 0 || info->samples < 0)
	{
		Con_Printf(const_cast<char*>("Invalid wav info: %s\n"), name);
		return false;
	}

	return true;
}
