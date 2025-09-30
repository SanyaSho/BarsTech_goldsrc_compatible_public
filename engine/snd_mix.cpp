/**
*	@file
*
	portable code to mix sounds for snd_dma.cpp
*/

#define CINTERFACE

#include "quakedef.h"
#include "sound.h"
#include "host.h"
#include "client.h"
#include "cl_main.h"
#include "voice.h"
#include "voice_sound_engine_interface.h"
#include "pr_cmds.h"
#include <dsound.h>

// hard clip input value to -32767 <= y <= 32767

#define CLIP(x) ((x) > 32767 ? 32767 : ((x) < -32767 ? -32767 : (x)))

#define CLIP_DSP(x) ((x) > 32767 ? 32767 : ((x) < -32767 ? -32767 : (x)))

extern "C"
{
	extern void Snd_WriteLinearBlastStereo16(void);
	extern void SND_PaintChannelFrom8(channel_t *ch, sfxcache_t* sc, int endtime);

	int		snd_scaletable[32][256];
	int* snd_p, snd_linear_count, snd_vol;
	short* snd_out;
};


portable_samplepair_t paintbuffer[(PAINTBUFFER_SIZE + 1) * 2];

static char		voxperiod[] = "_period";				// vocal pause
static char		voxcomma[] = "_comma";				// vocal pause

int cszrawsentences;
char* rgpszrawsentence[CVOXFILESENTENCEMAX];

char* rgpparseword[CVOXWORDMAX];

voxword_t rgrgvoxword[CBSENTENCENAME_MAX][CVOXWORDMAX];

void SetCareerAudioState(qboolean st)
{
	s_careerAudioPaused = st;
}

void VOX_Shutdown()
{
	for (int i = 0; i < cszrawsentences; i++)
		Mem_Free(rgpszrawsentence[i]);

	cszrawsentences = 0;
}

void Wavstream_Close(int idx)
{
	if (wavstreams[idx].hFile)
		FS_Close(wavstreams[idx].hFile);

	Q_memset(&wavstreams[idx], 0, sizeof(wavstream_t));
	wavstreams[idx].hFile = NULL;
}

void SND_ForceCloseMouth(int entnum)
{
	if (entnum >= 0 && entnum < cl.max_edicts)
		cl_entities[entnum].mouth.mouthopen = 0;
}

void SND_CloseMouth(channel_t* chan)
{
	if (chan->entnum > 0)
	{
		if (chan->entchannel == CHAN_VOICE || chan->entchannel == CHAN_STREAM)
			SND_ForceCloseMouth(chan->entnum);
	}
}

void S_FreeChannel(channel_t* chan)
{
	if (chan->entchannel == CHAN_STREAM)
	{
		// да кому нужны эти индексы?!
		int i = chan - channels;
		Wavstream_Close(i);
		Cache_Free(&chan->sfx->cache);
	}
	else if (chan->entchannel >= CHAN_NETWORKVOICE_BASE && chan->entchannel <= CHAN_NETWORKVOICE_END)
	{
		VoiceSE_NotifyFreeChannel(chan->entchannel);
	}

	if (chan->isentence >= 0)
		rgrgvoxword[chan->isentence][0].sfx = nullptr;

	chan->isentence = -1;
	chan->sfx = nullptr;

	SND_CloseMouth(chan);
}

// called when voice channel is first opened on this entity

void SND_InitMouth(int entnum, int entchannel)
{
	if ((entchannel == CHAN_VOICE || entchannel == CHAN_STREAM) && entnum > 0 && entnum < cl.max_edicts)
	{
		cl_entities[entnum].mouth.mouthopen = 0;
		cl_entities[entnum].mouth.sndavg = 0;
		cl_entities[entnum].mouth.sndcount = 0;
	}
}

int Wavstream_Init(void)
{
	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		Q_memset(&wavstreams[i], 0, sizeof(wavstream_t));
		wavstreams[i].hFile = 0;
	}

	return TRUE;
}

void Wavstream_GetNextChunk(channel_t *ch, sfx_t *s)
{
	sfxcache_t *sc;
	BYTE* data;
	int cbread;
	int i = ch - channels;
	
	sc = (sfxcache_t*)Cache_Check(&s->cache);
	wavstreams[i].lastposloaded = wavstreams[i].info.dataofs + wavstreams[i].csamplesplayed;
	data = COM_LoadFileLimit(NULL, wavstreams[i].lastposloaded, 32768, &cbread, &wavstreams[i].hFile);

	sc->length = wavstreams[i].csamplesinmem = min(cbread, wavstreams[i].info.samples - wavstreams[i].csamplesplayed);

	ResampleSfx(s, sc->speed, sc->width, data
#ifdef _2020_PATCH
		, cbread
#endif
	);
}

void VOX_ReadSentenceFile(void)
{
	char *pch, *pBuf;
	int fileSize;
	char c;
	char *pchlast, *pSentenceData;
	int nSentenceCount; 
	char *pszSentenceValue = NULL;

	VOX_Shutdown();

	pBuf = (char*)COM_LoadFileForMe(const_cast<char*>("sound/sentences.txt"), &fileSize);

	if (!pBuf)
	{
		Con_DPrintf(const_cast<char*>("VOX_ReadSentenceFile: Couldn't load %s\n"), "sound/sentences.txt");
		return;
	}

	pch = pBuf;
	pchlast = pch + fileSize;
	nSentenceCount = 0;
	while (pch < pchlast && nSentenceCount < CVOXFILESENTENCEMAX)
	{
		// Only process this pass on sentences
		pSentenceData = NULL;

		// skip newline, cr, tab, space

		c = *pch;
		while (pch < pchlast && (c == '\r' || c == '\n' || c == '\t' || c == ' '))
			c = *(++pch);

		// skip entire line if first char is /
		if (*pch != '/')
		{
			pSentenceData = pch;

			// scan forward to first space, insert null terminator
			// after sentence name

			c = *pch;
			while (pch < pchlast && c != ' ' && c != '\t')
				c = *(++pch);

			if (pch < pchlast)
				*pch++ = 0;

			// A sentence may have some line commands, make an extra pass
			pszSentenceValue = pch;
		}
		// scan forward to end of sentence or eof
		while (pch < pchlast && pch[0] != '\n' && pch[0] != '\r')
			pch++;

		// insert null terminator
		if (pch < pchlast)
			*pch++ = 0;

		// If we have some sentence data, parse out any line commands
		if (pSentenceData)
		{
			int nTotalSize = (strlen(pszSentenceValue) + 1) + (strlen(pSentenceData) + 1);
			rgpszrawsentence[nSentenceCount] = (char*)Mem_Malloc(nTotalSize);
			Q_memcpy(rgpszrawsentence[nSentenceCount++], pSentenceData, nTotalSize);
		}
	}

	cszrawsentences = nSentenceCount;
	Mem_Free(pBuf);
}

void VOX_Init()
{
	Q_memset(rgrgvoxword, 0, sizeof(rgrgvoxword));

	VOX_ReadSentenceFile();
}

void SND_InitScaletable(void)
{
	int		i, j;

	for (i = 0; i<32; i++)
	for (j = 0; j<256; j++)
		snd_scaletable[i][j] = ((signed char)j) * i * 8;
}

void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count)
{
	int 	data;
	int		*lscale, *rscale;
	unsigned char *sfx;
	int		i;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;
		
	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = (unsigned char *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}
	
	ch->pos += count;
}

void VOX_SetChanVol(channel_t *ch)
{
	if (ch->isentence < 0)
		return;

	int vol = rgrgvoxword[ch->isentence][ch->iword].volume;

	if (vol != 100)
	{
		float scale = (float)vol / 100;
		if (scale == 1.0)
			return;

		ch->rightvol = (int)(ch->rightvol * scale);
		ch->leftvol = (int)(ch->leftvol * scale);
	}
}

sentenceEntry_s* SequenceGetSentenceByIndex(unsigned int index);

// scan g_Sentences, looking for pszin sentence name
// return pointer to sentence data if found, null if not
// CONSIDER: if we have a large number of sentences, should
// CONSIDER: sort strings in g_Sentences and do binary search.
char *VOX_LookupString(const char *pSentenceName, int *psentencenum)
{
	int i;

	if (pSentenceName[0] == CHAR_DRYMIX)
	{
		sentenceEntry_s* se = SequenceGetSentenceByIndex(atol(&pSentenceName[1]));
		if (se != NULL)
			return se->data;
	}

	for (i = 0; i < cszrawsentences; i++)
	{
		if (!Q_strcasecmp((char*)pSentenceName, rgpszrawsentence[i]))
		{
			if (psentencenum)
				*psentencenum = i;

			char* cptr = (rgpszrawsentence[i] + Q_strlen(rgpszrawsentence[i]) + 1);
			while (*cptr == ' ' || *cptr == '\t')
				cptr++;

			return cptr;
		}
	}
	return NULL;
}

// parse a null terminated string of text into component words, with
// pointers to each word stored in rgpparseword
// note: this code actually alters the passed in string!

void VOX_ParseString(char *psz)
{
	int i;
	int fdone = 0;
	char *pszscan = psz;
	char c;

	Q_memset(rgpparseword, 0, sizeof(char *) * CVOXWORDMAX);

	if (!psz)
		return;

	i = 0;
	rgpparseword[i++] = psz;

	while (!fdone && i < CVOXWORDMAX)
	{
		// scan up to next word
		c = *pszscan;
		while (c && !(c == '.' || c == ' ' || c == ','))
			c = *(++pszscan);

		// if '(' then scan for matching ')'
		if (c == '(' /*|| c == '{'*/)
		{
			while (*pszscan != ')')
				pszscan++;

			c = *(++pszscan);
			if (!c)
				fdone = 1;
		}

		if (fdone || !c)
			fdone = 1;
		else
		{
			// if . or , insert pause into rgpparseword,
			// unless this is the last character
			if ((c == '.' || c == ',') && *(pszscan + 1) != '\n' && *(pszscan + 1) != '\r'
				&& *(pszscan + 1) != 0)
			{
				if (c == '.')
					rgpparseword[i++] = voxperiod;
				else
					rgpparseword[i++] = voxcomma;

				if (i >= CVOXWORDMAX)
					break;
			}

			// null terminate substring
			*pszscan++ = 0;

			// skip whitespace
			c = *pszscan;
			while (c && (c == '.' || c == ' ' || c == ','))
				c = *(++pszscan);

			if (!c)
				fdone = 1;
			else
				rgpparseword[i++] = pszscan;
		}
	}
}

// backwards scan psz for last '/'
// return substring in szpath null terminated
// if '/' not found, return 'vox/'

char *VOX_GetDirectory(char *szpath, char *psz
#ifdef _2020_PATCH
	, int nsize
#endif
)
{
	char c;
	int cb = 0;
	char *pszscan = psz + Q_strlen(psz) - 1;

	// scan backwards until first '/' or start of string
	c = *pszscan;
	while (pszscan > psz && c != '/')
	{
		c = *(--pszscan);
		cb++;
	}

	if (c != '/')
	{
		// didn't find '/', return default directory
		Q_strcpy(szpath, "vox/");
		return psz;
	}

	cb = Q_strlen(psz) - cb;
#ifdef _2020_PATCH
	if (cb >= nsize)
	{
		Con_DPrintf(const_cast<char*>(__FUNCTION__ ": invalid directory in: %s\n"), psz);
		return NULL;
	}
#endif
	Q_memcpy(szpath, psz, cb);
	szpath[cb] = 0;
	return pszscan + 1;
}

//===============================================================================
//  Get any pitch, volume, start, end params into voxword
//  and null out trailing format characters
//  Format: 
//		someword(v100 p110 s10 e20)
//		
//		v is volume, 0% to n%
//		p is pitch shift up 0% to n%
//		s is start wave offset %
//		e is end wave offset %
//		t is timecompression %
//
//	pass fFirst == 1 if this is the first string in sentence
//  returns 1 if valid string, 0 if parameter block only.
//
//  If a ( xxx ) parameter block does not directly follow a word, 
//  then that 'default' parameter block will be used as the default value
//  for all following words.  Default parameter values are reset
//  by another 'default' parameter block.  Default parameter values
//  for a single word are overridden for that word if it has a parameter block.
// 
//===============================================================================

int VOX_ParseWordParams(char *psz, voxword_t *pvoxword, int fFirst)
{
	char *pszsave = psz;
	char c;
	char ct;
	char sznum[8];
	int i;
	static voxword_t voxwordDefault;

	// init to defaults if this is the first word in string.
	if (fFirst)
	{
		voxwordDefault.pitch = -1;
		voxwordDefault.volume = 100;
		voxwordDefault.start = 0;
		voxwordDefault.end = 100;
		voxwordDefault.fKeepCached = 0;
		voxwordDefault.timecompress = 0;
	}

	*pvoxword = voxwordDefault;

	// look at next to last char to see if we have a 
	// valid format:

	c = *(psz + Q_strlen(psz) - 1);

	if (c != ')')
		return 1;		// no formatting, return

	// scan forward to first '('
	c = *psz;

	while (c != '(' && c != ')')
		c = *(++psz);

	if (c == ')')
		return 0;		// bogus formatting

	// null terminate

	*psz = 0;
	ct = *(++psz);

	while (1)
	{
		// scan until we hit a character in the commandSet

		while (ct && ct != 'v' && ct != 'p' && ct != 's' && ct != 'e' && ct != 't' && c != ')')
			ct = *(++psz);

		if (ct == ')')
			break;

		memset(sznum, 0, sizeof(sznum));
		i = 0;

		c = *(++psz);

		if (!isdigit(c))
			break;

		// read number
		while (isdigit(c) && i < sizeof(sznum) - 1)
		{
			sznum[i++] = c;
			c = *(++psz);
		}

		// get value of number
		i = Q_atoi(sznum);

		switch (ct)
		{
		case 'v': pvoxword->volume = i; break;
		case 'p': pvoxword->pitch = i; break;
		case 's': pvoxword->start = i; break;
		case 'e': pvoxword->end = i; break;
		case 't': pvoxword->timecompress = i; break;
		}

		ct = c;
	}

	// if the string has zero length, this was an isolated
	// parameter block.  Set default voxword to these
	// values

	if (Q_strlen(pszsave) == 0)
	{
		voxwordDefault = *pvoxword;
		return 0;
	}
	else
		return 1;
}

int VOX_IFindEmptySentence()
{
	for (int i = 0; i < CBSENTENCENAME_MAX; i++)
	{
		if (!rgrgvoxword[i][0].sfx)
			return i;
	}

	Con_DPrintf(const_cast<char*>("Sentence or Pitch shift ignored. > 16 playing!\n"));
	return -1;
}

void VOX_MakeSingleWordSentence(channel_t *ch, int pitch)
{
	int seidx = VOX_IFindEmptySentence();

	if (seidx < 0)
	{
		ch->pitch = PITCH_NORM;
		ch->isentence = -1;
	}
	else
	{
		rgrgvoxword[seidx][0].volume = 100;
		rgrgvoxword[seidx][0].pitch = PITCH_NORM;
		rgrgvoxword[seidx][0].start = 0;
		rgrgvoxword[seidx][0].end = 100;
		rgrgvoxword[seidx][0].fKeepCached = 1;
		rgrgvoxword[seidx][0].samplefrac = 0;
		rgrgvoxword[seidx][0].timecompress = 0;
		rgrgvoxword[seidx][0].sfx = ch->sfx;

		rgrgvoxword[seidx][1].sfx = 0;

		ch->isentence = seidx;
		ch->iword = 0;
		ch->pitch = pitch;
	}
}

sfxcache_t* VOX_LoadSound(channel_t *pchan, char *pszin)
{
	char buffer[512];
	int i, cword, j;
	char	pathbuffer[64];
	char	szpath[32];
	voxword_t rgvoxword[CVOXWORDMAX];
	char *psz;
	sfxcache_t* sc;

	if (!pszin)
		return NULL;

	Q_memset(rgvoxword, 0, sizeof (voxword_t) * CVOXWORDMAX);
	Q_memset(buffer, 0, sizeof(buffer));

	psz = VOX_LookupString(pszin, NULL);

	if (!psz)
	{
		Con_DPrintf(const_cast<char*>("VOX_LoadSound: no sentence named %s\n"), pszin);
		return NULL;
	}

	// get directory from string, advance psz
	psz = VOX_GetDirectory(szpath, psz
#ifdef _2020_PATCH
		, CVOXWORDMAX
#endif
	);

#ifdef _2020_PATCH
	if (!psz)
	{
		Con_DPrintf(const_cast<char*>("VOX_LoadSound: failed getting directory for %s\n"), pszin);
		return NULL;
	}
#endif

	if (Q_strlen(psz) > sizeof(buffer) - 1)
	{
		Con_DPrintf(const_cast<char*>("VOX_LoadSound: sentence is too long %s\n"), psz);
		return NULL;
	}

	// copy into buffer
	Q_strncpy(buffer, psz, sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';
	psz = buffer;

	// parse sentence (also inserts null terminators between words)

	VOX_ParseString(psz);

	// for each word in the sentence, construct the filename,
	// lookup the sfx and save each pointer in a temp array	

	i = 0;
	cword = 0;

	while (rgpparseword[i])
	{
		// Get any pitch, volume, start, end params into voxword

		if (VOX_ParseWordParams(rgpparseword[i], &rgvoxword[cword], i == 0))
		{
			// this is a valid word (as opposed to a parameter block)
			snprintf(pathbuffer, sizeof(pathbuffer), "%s%s.wav", szpath, rgpparseword[i]);
			pathbuffer[sizeof(pathbuffer) - 1] = '\0';

			if (Q_strlen(pathbuffer) >= sizeof(pathbuffer))
				continue;

			// find name, if already in cache, mark voxword
			// so we don't discard when word is done playing
			rgvoxword[cword].sfx = S_FindName(pathbuffer,
				&(rgvoxword[cword].fKeepCached));
			cword++;
		}
		i++;
	}

	i = VOX_IFindEmptySentence();
	if (i == 0)
		return NULL;

	j = 0;
	while (rgvoxword[j].sfx != NULL)
		rgrgvoxword[i][j] = rgvoxword[j++];

	pchan->isentence = i;
	// terminate list
	rgrgvoxword[i][j].sfx = NULL;

	pchan->iword = 0;
	pchan->sfx = rgrgvoxword[i][0].sfx;

	sc = S_LoadSound(rgvoxword[0].sfx, NULL);

	if (!sc)
	{
		S_FreeChannel(pchan);
		return NULL;
	}

	return sc;
}

void VOX_TrimStartEndTimes(channel_t *ch, sfxcache_t *sc)
{
	float sstart, send;
	int skiplen;
	int data;
	int i, srcsample;
	char* pdata;
	int length;

	if (ch->isentence >= 0)
	{
		send = rgrgvoxword[ch->isentence][ch->iword].end;
		sstart = rgrgvoxword[ch->isentence][ch->iword].start;
		
		length = sc->length;
		pdata = (char*)sc->data;

		rgrgvoxword[ch->isentence][ch->iword].cbtrim = sc->length;

		if (sstart < send)
		{
			if (sstart > 0 && sstart < 100)
			{
				skiplen = length * (sstart / 100.0f);
				ch->pos += skiplen;
				srcsample = ch->pos;

				for (i = 0; i < CVOXZEROSCANMAX && ch->pos < length; i++, srcsample++)
				{
					if (pdata[srcsample] >= -2 && pdata[srcsample] <= 2)
					{
						ch->pos += i;
						ch->end -= skiplen + i;
						break;
					}
				}
				// старый вариант
				/*if (length > ch->pos)
				{
					for (i = 0; i < 255 && srcsample < length; i++, srcsample++)
					{
						if (pdata[srcsample] >= -2 && pdata[srcsample] <= 2)
						{
							ch->pos += i;
							ch->end -= skiplen + i;
							break;
						}
					}
				}*/

				if (rgrgvoxword[ch->isentence][ch->iword].pitch != PITCH_NORM)
					rgrgvoxword[ch->isentence][ch->iword].samplefrac += ch->pos << 8;
			}

			if (send > 0 && send < 100)
			{
				skiplen = length * ((100.0f - send) / 100.0f);
				ch->end -= skiplen;
				srcsample = length - skiplen;

				for (i = 0; i < CVOXZEROSCANMAX && srcsample > ch->pos; i++, srcsample--)
				{
					if (pdata[srcsample] >= -2 && pdata[srcsample] <= 2)
					{
						ch->end -= i;
						rgrgvoxword[ch->isentence][ch->iword].cbtrim -= i + skiplen;
						break;
					}
				}
			}
		}
	}
}

#define CAVGSAMPLES 10

void SND_MoveMouth(channel_t *ch, sfxcache_t *sc, int count)
{
	int 	data;
	char	*pdata = NULL;
	int		i;
	int		savg;
	int		scount;

	cl_entity_t *ent = &cl_entities[ch->entnum];

	i = 0;
	scount = ent->mouth.sndcount;
	savg = 0;

	while (i < count && scount < CAVGSAMPLES)
	{
		data = (char)sc->data[ch->pos + i];
		savg += abs(data);

		i += 80 + ((byte)data & 0x1F);
		scount++;
	}
	
	ent->mouth.sndavg += savg;
	ent->mouth.sndcount = (byte)scount;

	if (ent->mouth.sndcount >= CAVGSAMPLES)
	{
		ent->mouth.mouthopen = ent->mouth.sndavg / CAVGSAMPLES;
		ent->mouth.sndavg = 0;
		ent->mouth.sndcount = 0;
	}
}

//===============================================================================
// Client entity mouth movement code.  Set entity mouthopen variable, based
// on the sound envelope of the voice channel playing.
// KellyB 10/22/97
//===============================================================================

void SND_ForceInitMouth(int entnum)
{
	if (entnum < 0 || entnum >= cl.max_edicts)
		return; // Bad entity number

	cl_entity_t* ent = &cl_entities[entnum];

	ent->mouth.mouthopen = 0;
	ent->mouth.sndavg = 0;
	ent->mouth.sndcount = 0;
}

void SND_MoveMouth16(int entnum, short* pdata, int count)
{
	int data;
	int i = 0;
	int savg = 0;
	int scount;
	cl_entity_t* pent = NULL;

	if (entnum < 0 || entnum >= cl.max_edicts)
		return; // Bad entity number

	scount = cl_entities[entnum].mouth.sndcount;
	pent = &cl_entities[entnum];

	while (i < count && scount < CAVGSAMPLES)
	{
		data = pdata[i];
		savg += abs(data);

		i += 80 + ((byte)data & 0x1F);
		scount++;
	}

	pent->mouth.sndavg += savg;
	pent->mouth.sndcount = (byte)scount;

	if (pent->mouth.sndcount >= CAVGSAMPLES)
	{
		pent->mouth.mouthopen = pent->mouth.sndavg / CAVGSAMPLES;
		pent->mouth.sndavg = 0;
		pent->mouth.sndcount = 0;
	}
}

void SND_PaintChannelFrom8Offs(portable_samplepair_t *paintbuffer, channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int data;
	int *lscale, *rscale;
	int i;
	unsigned char* sfx;

	ch->leftvol = min(255, ch->leftvol);
	ch->rightvol = min(255, ch->rightvol);

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];

	sfx = (unsigned char *)sc->data + ch->pos;

	for (i = 0; i < count; i++)
	{
		data = sfx[i];
		paintbuffer[offset + i].left += lscale[data];
		paintbuffer[offset + i].right += rscale[data];
	}

	ch->pos += count;
}

int VOX_FPaintPitchChannelFrom8Offs(portable_samplepair_t *paintbuffer, channel_t *ch, sfxcache_t *sc, int count, int pitch, int timecompress, int offset)
{
	int data;
	int *lscale, *rscale;
	int i, j;
	int cb, samplefrac;
	float stepscale;
	int fracstep, fracbase;
	int srcsample, lowsample;
	int chunksize, skipbytes;
	int cdata;
	int playcount;
	int posold;
	portable_samplepair_t* pbuffer = &paintbuffer[offset];
	byte* sfx;

	if (ch->isentence < 0)
		return false;

	ch->leftvol = min(255, ch->leftvol);
	ch->rightvol = min(255, ch->rightvol);

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];

	sfx = sc->data;

	cb = rgrgvoxword[ch->isentence][ch->iword].cbtrim;
	samplefrac = rgrgvoxword[ch->isentence][ch->iword].samplefrac;

	stepscale = (float)pitch / 100;
	fracstep = stepscale * 256;

	fracbase = fracstep + samplefrac;
	srcsample = samplefrac / 256;

	if (timecompress)
	{
		chunksize = cb >> 3;
		skipbytes = timecompress * chunksize / 100;

		// MARK: 19.12.23
		i = 0;

		for ( ; (i < count) && (srcsample < cb); )
		{
			lowsample = srcsample % chunksize;
			if (srcsample < skipbytes || lowsample >= skipbytes)
				cdata = chunksize - lowsample;
			else
			{
				for (j = 0; (i < count) && (srcsample < cb) && (j < CVOXZEROSCANMAX); j++, fracbase += fracstep, i++)
				{
					data = sfx[srcsample];
					if (data <= 2) break;
					pbuffer[i].left += lscale[data];
					pbuffer[i].right += rscale[data];
					srcsample = fracbase / 256;
				}

				if (i > PAINTBUFFER_SIZE)
					Con_DPrintf(const_cast<char*>("timecompress scan forward: overwrote paintbuffer!"));

				if (srcsample >= cb || i >= count)
					break;

				while (true)
				{
					if (srcsample % chunksize < skipbytes)
					{
						fracbase += (skipbytes - srcsample % chunksize) * 256;
						srcsample += skipbytes - srcsample % chunksize;
					}

					if (srcsample >= cb)
						break;

					for (j = 0; j < CVOXZEROSCANMAX && sc->data[srcsample] > 2 && srcsample < cb; j++, fracbase += fracstep)
						srcsample = fracbase / 256;

					if (srcsample >= cb)
						break;

					lowsample = srcsample % chunksize;
					if (lowsample >= skipbytes)
					{
						cdata = chunksize - lowsample;
						// MARK: 19.12.23
						break;
					}
				}
			}

			playcount = min(cdata * 256 / fracstep, count - i);
			
			if (playcount == 0)
				playcount = 1;

			// условная конструкция поставлена здесь, т.к. после запускается цикл, в котором присутствует такая же проверка
			// с учётом того, что это условие является условием продолжения главного цикла, логично прервать выполнение
			//if (i >= count)
			//	break;
			// -barspinOff. 2023

			for (j = 0; i < count && j < playcount && srcsample < cb; j++, i++, fracbase += fracstep)
			{
				data = sfx[srcsample];
				pbuffer[i].left += lscale[data];
				pbuffer[i].right += rscale[data];
				srcsample = fracbase / 256;
			}
		}
	}
	else
	{
		for (i = 0; (i < count) && (srcsample < cb); i++, fracbase += fracstep)
		{
			data = sc->data[srcsample];
			paintbuffer[offset + i].left += lscale[data];
			paintbuffer[offset + i].right += rscale[data];
			srcsample = fracbase / 256;
		}
	}

	rgrgvoxword[ch->isentence][ch->iword].samplefrac = fracbase - fracstep;
	posold = ch->pos;
	ch->end += i - (fracbase / 256 - posold);
	ch->pos = fracbase / 256;

	return srcsample >= cb;
}

void SND_PaintChannelFrom16Offs(portable_samplepair_t *paintbuffer, channel_t *ch, short *sfx, int count, int offset)
{
	int leftvol, rightvol;
	int i;
	int data;
	int left, right;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;

	for (i = 0; i < count; i++)
	{
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		paintbuffer[offset + i].left += left;
		paintbuffer[offset + i].right += right;
	}

	ch->pos += count;
}

qboolean S_CheckWavEnd(channel_t *ch, sfxcache_t **psc, int ltime, int ichan)
{
	sfxcache_t* sc;

	sc = *psc;

	if ((*psc)->length == 0x40000000)
		return false;

	if (sc->loopstart < 0)
	{
		if (ch->entchannel == CHAN_STREAM)
		{
			if (wavstreams[ichan].csamplesplayed < wavstreams[ichan].info.samples)
			{
				// 19.12.23
				Wavstream_GetNextChunk(ch, ch->sfx);
				
				ch->pos = 0;
				ch->end = sc->length + ltime;
				return false;
			}
		}
		else
		{
			if (ch->isentence >= 0)
			{
				if (rgrgvoxword[ch->isentence][ch->iword].sfx != NULL && rgrgvoxword[ch->isentence][ch->iword].fKeepCached == 0)
					Cache_Free(&rgrgvoxword[ch->isentence][ch->iword].sfx->cache);

				ch->sfx = rgrgvoxword[ch->isentence][ch->iword + 1].sfx;

				if (ch->sfx != NULL)
				{
					*psc = S_LoadSound(ch->sfx, ch);
					if (*psc)
					{
						ch->pos = 0;
						ch->iword++;
						ch->end = (*psc)->length + ltime;
						VOX_TrimStartEndTimes(ch, *psc);
						return false;
					}
				}
			}
		}

		S_FreeChannel(ch);
		return true;
	}

	ch->pos = sc->loopstart;
	ch->end = sc->length + ltime - sc->loopstart;
	if (ch->isentence > 0)
		rgrgvoxword[ch->isentence][ch->iword].samplefrac = 0;

	return false;
}

//===============================================================================
//
// Digital Signal Processing algorithms for audio FX.
//
// KellyB 1/24/97
//===============================================================================


#define SXDLY_MAX		0.400							// max delay in seconds
#define SXRVB_MAX		0.100							// max reverb reflection time
#define SXSTE_MAX		0.100							// max stereo delay line time

typedef short sample_t;									// delay lines must be 32 bit, now that we have 16 bit samples

typedef struct dlyline_s {
	int cdelaysamplesmax;								// size of delay line in samples
	int lp;												// lowpass flag 0 = off, 1 = on

	int idelayinput;									// i/o indices into circular delay line
	int idelayoutput;

	int idelayoutputxf;									// crossfade output pointer
	int xfade;											// crossfade value

	int delaysamples;									// current delay setting
	int delayfeed;										// current feedback setting

	int lp0, lp1, lp2;									// lowpass filter buffer

	int mod;											// sample modulation count
	int modcur;

	HANDLE hdelayline;									// handle to delay line buffer
	sample_t *lpdelayline;								// buffer
} dlyline_t;

#define CSXDLYMAX		4

#define ISXMONODLY		0								// mono delay line
#define ISXRVB			1								// first of the reverb delay lines
#define CSXRVBMAX		2
#define ISXSTEREODLY	3								// 50ms left side delay

dlyline_t rgsxdly[CSXDLYMAX];							// array of delay lines

#define gdly0 (rgsxdly[ISXMONODLY])
#define gdly1 (rgsxdly[ISXRVB])
#define gdly2 (rgsxdly[ISXRVB + 1])
#define gdly3 (rgsxdly[ISXSTEREODLY])

#define CSXLPMAX		10								// lowpass filter memory

int rgsxlp[CSXLPMAX];

int sxamodl, sxamodr;									// amplitude modulation values
int sxamodlt, sxamodrt;									// modulation targets
int sxmod1, sxmod2;
int sxmod1cur, sxmod2cur;

// Mono Delay parameters

cvar_t sxdly_delay = { const_cast<char*>("room_delay"), const_cast<char*>("0") };			// current delay in seconds
cvar_t sxdly_feedback = { const_cast<char*>("room_feedback"), const_cast<char*>("0.2") };		// cyles
cvar_t sxdly_lp = { const_cast<char*>("room_dlylp"), const_cast<char*>("1") };			// lowpass filter

float sxdly_delayprev;									// previous delay setting value

// Mono Reverb parameters

cvar_t sxrvb_size = { const_cast<char*>("room_size"), const_cast<char*>("0") };			// room size 0 (off) 0.1 small - 0.35 huge
cvar_t sxrvb_feedback = { const_cast<char*>("room_refl"), const_cast<char*>("0.7") };			// reverb decay 0.1 short - 0.9 long
cvar_t sxrvb_lp = { const_cast<char*>("room_rvblp"), const_cast<char*>("1") };			// lowpass filter		

float sxrvb_sizeprev;

// Stereo delay (no feedback)

cvar_t sxste_delay = { const_cast<char*>("room_left"), const_cast<char*>("0") };			// straight left delay
float sxste_delayprev;

// Underwater/special fx modulations

cvar_t sxmod_lowpass = { const_cast<char*>("room_lp"), const_cast<char*>("0") };
cvar_t sxmod_mod = { const_cast<char*>("room_mod"), const_cast<char*>("0") };

// Main interface

cvar_t sxroom_type = { const_cast<char*>("room_type"), const_cast<char*>("0") };
cvar_t sxroomwater_type = { const_cast<char*>("waterroom_type"), const_cast<char*>("14") };
float sxroom_typeprev;

cvar_t sxroom_off = { const_cast<char*>("room_off"), const_cast<char*>("0") };

int sxhires = 0;
int sxhiresprev = 0;

qboolean SXDLY_Init(int idelay, float delay);
void SXDLY_Free(int idelay);
void SXDLY_DoDelay(int count);
void SXRVB_DoReverb(int count);
void SXDLY_DoStereoDelay(int count);
void SXRVB_DoAMod(int count);

//=====================================================================
// Init/release all structures for sound effects
//=====================================================================

void SX_Init( void )
{
	Q_memset(rgsxdly, 0, sizeof(dlyline_t) * CSXDLYMAX);
	Q_memset(rgsxlp, 0, sizeof(int) * CSXLPMAX);

	sxdly_delayprev = -1.0;
	sxrvb_sizeprev = -1.0;
	sxste_delayprev = -1.0;
	sxroom_typeprev = -1.0;

	// flag hires sound mode
	sxhires = hisound.value != 0.0;
	sxhiresprev = sxhires;

	// init amplitude modulation params

	sxamodl = sxamodr = 255;
	sxamodlt = sxamodrt = 255;

	if (shm)
	{
		sxmod1 = 350 * (shm->speed / SOUND_11k);	// 11k was the original sample rate all dsp was tuned at
		sxmod2 = 450 * (shm->speed / SOUND_11k);
	}

	sxmod1cur = sxmod1;
	sxmod2cur = sxmod2;

	Cvar_RegisterVariable(&sxdly_delay);
	Cvar_RegisterVariable(&sxdly_feedback);
	Cvar_RegisterVariable(&sxdly_lp);
	Cvar_RegisterVariable(&sxrvb_size);
	Cvar_RegisterVariable(&sxrvb_feedback);
	Cvar_RegisterVariable(&sxrvb_lp);
	Cvar_RegisterVariable(&sxste_delay);
	Cvar_RegisterVariable(&sxmod_lowpass);
	Cvar_RegisterVariable(&sxmod_mod);
	Cvar_RegisterVariable(&sxroom_type);
	Cvar_RegisterVariable(&sxroomwater_type);
	Cvar_RegisterVariable(&sxroom_off);
}

// This routine updates both left and right output with 
// the mono delayed signal.  Delay is set through console vars room_delay
// and room_feedback.

void SXDLY_DoDelay(int count)
{
	int val;
	int valt;
	int left;
	int right;
	sample_t sampledly;
	portable_samplepair_t *pbuf;
	int countr;


	// process mono delay line if active

	if (rgsxdly[ISXMONODLY].lpdelayline)
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...

		while (countr--)
		{

			// get delay line sample

			sampledly = *(gdly0.lpdelayline + gdly0.idelayoutput);

			left = pbuf->left;
			right = pbuf->right;

			// only process if delay line and paintbuffer samples are non zero

			if (sampledly || left || right)
			{
				// get current sample from delay buffer

				// calculate delayed value from avg of left and right channels

				val = ((left + right) >> 1) + ((gdly0.delayfeed * sampledly) >> 8);

				// limit val to short
				val = CLIP(val);

				// lowpass

				if (gdly0.lp)
				{
					//valt = (gdly0.lp0 + gdly0.lp1 + val) / 3;  // performance
					valt = (gdly0.lp0 + gdly0.lp1 + (val<<1)) >> 2;

					gdly0.lp0 = gdly0.lp1;
					gdly0.lp1 = val;
				}
				else
				{
					valt = val;
				}

				// store delay output value into output buffer

				*(gdly0.lpdelayline + gdly0.idelayinput) = valt;

				// mono delay in left and right channels

				// decrease output value by max gain of delay with feedback
				// to provide for unity gain reverb
				// note: this gain varies with the feedback value.

				pbuf->left = (valt >> 2) + left;
				pbuf->left = CLIP(pbuf->left);
				pbuf->right = (valt >> 2) + right;
				pbuf->right = CLIP(pbuf->right);
			}
			else
			{
				// not playing samples, but must still flush lowpass buffer and delay line
				valt = gdly0.lp0 = gdly0.lp1 = 0;

				*(gdly0.lpdelayline + gdly0.idelayinput) = valt;

			}

			// update delay buffer pointers

			if (++gdly0.idelayinput >= gdly0.cdelaysamplesmax)
				gdly0.idelayinput = 0;

			if (++gdly0.idelayoutput >= gdly0.cdelaysamplesmax)
				gdly0.idelayoutput = 0;

			pbuf++;
		}
	}
}


// Check for a parameter change on the reverb processor

#define RVB_XFADE (32 /* SOUND_DMA_SPEED / SOUND_11k*/)		// xfade time between new delays
#define RVB_MODRATE1 (500 * (shm->speed / SOUND_11k))	// how often, in samples, to change delay (1st rvb)
#define RVB_MODRATE2 (700 * (shm->speed / SOUND_11k))	// how often, in samples, to change delay (2nd rvb)

// main routine for updating the paintbuffer with new reverb values.
// This routine updates both left and right lines with 
// the mono reverb signal.  Delay is set through console vars room_reverb
// and room_feedback.  2 reverbs operating in parallel.

void SXRVB_DoReverb(int count)
{
	int val;
	int valt;
	int left;
	int right;
	sample_t sampledly;
	sample_t samplexf;
	portable_samplepair_t *pbuf;
	int countr;
	int voutm;
	int vlr;

	// process reverb lines if active

	if (rgsxdly[ISXRVB].lpdelayline)
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...

		while (countr--)
		{

			left = pbuf->left;
			right = pbuf->right;
			voutm = 0;
			vlr = (left + right) >> 1;

			// UNDONE: ignored
			if (--gdly1.modcur < 0)
				gdly1.modcur = gdly1.mod;

			// ========================== ISXRVB============================	

			// get sample from delay line

			sampledly = *(gdly1.lpdelayline + gdly1.idelayoutput);

			// only process if something is non-zero

			if (gdly1.xfade || sampledly || left || right)
			{
				// modulate delay rate
				// UNDONE: modulation disabled
				if (!gdly1.xfade && /*!gdly1.modcur &&*/ gdly1.mod)
				{
					// set up crossfade to new delay value, if we're not already doing an xfade

					//gdly1.idelayoutputxf = gdly1.idelayoutput + 
					//		((RandomLong(0,0x7FFF) * gdly1.delaysamples) / (RAND_MAX * 2)); // performance

					gdly1.idelayoutputxf = gdly1.idelayoutput +
						((RandomLong(0, 0xFF) * gdly1.delaysamples) >> 9); // 100 = ~ 9ms

					if (gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax)
						gdly1.idelayoutputxf -= gdly1.cdelaysamplesmax;

					gdly1.xfade = RVB_XFADE;
				}

				// modify sampledly if crossfading to new delay value

				if (gdly1.xfade)
				{
					samplexf = (*(gdly1.lpdelayline + gdly1.idelayoutputxf) * (RVB_XFADE - gdly1.xfade)) / RVB_XFADE;
					sampledly = ((sampledly * gdly1.xfade) / RVB_XFADE) + samplexf;

					if (++gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax)
						gdly1.idelayoutputxf = 0;

					if (--gdly1.xfade == 0)
						gdly1.idelayoutput = gdly1.idelayoutputxf;
				}

				if (sampledly)
				{
					// get current sample from delay buffer

					// calculate delayed value from avg of left and right channels

					val = vlr + ((gdly1.delayfeed * sampledly) >> 8);

					// limit to short
					val = CLIP(val);

				}
				else
				{
					val = vlr;
				}

				// lowpass

				if (gdly1.lp)
				{
					valt = (gdly1.lp0 + val) >> 1;
					gdly1.lp0 = val;
				}
				else
				{
					valt = val;
				}

				// store delay output value into output buffer

				*(gdly1.lpdelayline + gdly1.idelayinput) = valt;

				voutm = valt;
			}
			else
			{
				// not playing samples, but still must flush lowpass buffer & delay line

				gdly1.lp0 = gdly1.lp1 = 0;
				*(gdly1.lpdelayline + gdly1.idelayinput) = 0;

				voutm = 0;
			}

			// update delay buffer pointers

			if (++gdly1.idelayinput >= gdly1.cdelaysamplesmax)
				gdly1.idelayinput = 0;

			if (++gdly1.idelayoutput >= gdly1.cdelaysamplesmax)
				gdly1.idelayoutput = 0;

			// ========================== ISXRVB + 1========================

			// UNDONE: ignored
			if (--gdly2.modcur < 0)
				gdly2.modcur = gdly2.mod;

			if (gdly2.lpdelayline)
			{
				// get sample from delay line

				sampledly = *(gdly2.lpdelayline + gdly2.idelayoutput);

				// only process if something is non-zero

				if (gdly2.xfade || sampledly || left || right)
				{
					// UNDONE: modulation disabled
					if (/*0 &&*/ !gdly2.xfade && /*gdly2.modcur &&*/ gdly2.mod)
					{
						// set up crossfade to new delay value, if we're not already doing an xfade

						//gdly2.idelayoutputxf = gdly2.idelayoutput + 
						//		((RandomLong(0,RAND_MAX) * gdly2.delaysamples) / (RAND_MAX * 2)); // performance

						gdly2.idelayoutputxf = gdly2.idelayoutput +
							((RandomLong(0, 0xFF) * gdly2.delaysamples) >> 9); // 100 = ~ 9ms


						if (gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax)
							gdly2.idelayoutputxf -= gdly2.cdelaysamplesmax;

						gdly2.xfade = RVB_XFADE;
					}

					// modify sampledly if crossfading to new delay value

					if (gdly2.xfade)
					{
						samplexf = (*(gdly2.lpdelayline + gdly2.idelayoutputxf) * (RVB_XFADE - gdly2.xfade)) / RVB_XFADE;
						sampledly = ((sampledly * gdly2.xfade) / RVB_XFADE) + samplexf;

						if (++gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax)
							gdly2.idelayoutputxf = 0;

						if (--gdly2.xfade == 0)
							gdly2.idelayoutput = gdly2.idelayoutputxf;
					}

					if (sampledly)
					{
						// get current sample from delay buffer

						// calculate delayed value from avg of left and right channels

						val = vlr + ((gdly2.delayfeed * sampledly) >> 8);

						// limit to short
						val = CLIP(val);	
					}
					else
					{
						val = vlr;
					}

					// lowpass

					if (gdly2.lp)
					{
						valt = (gdly2.lp0 + val) >> 1;
						gdly2.lp0 = val;
					}
					else
					{
						valt = val;
					}

					// store delay output value into output buffer

					*(gdly2.lpdelayline + gdly2.idelayinput) = valt;

					voutm += valt;
				}
				else
				{
					// not playing samples, but still must flush lowpass buffer

					gdly2.lp0 = gdly2.lp1 = 0;
					*(gdly2.lpdelayline + gdly2.idelayinput) = 0;
				}

				// update delay buffer pointers

				if (++gdly2.idelayinput >= gdly2.cdelaysamplesmax)
					gdly2.idelayinput = 0;

				if (++gdly2.idelayoutput >= gdly2.cdelaysamplesmax)
					gdly2.idelayoutput = 0;
			}

			// ============================ Mix================================

			// add mono delay to left and right channels

			// drop output by inverse of cascaded gain for both reverbs
			voutm = (11 * voutm) >> 6;
			// voutm = CLIP( voutm );

			left += voutm;
			left = CLIP(left);
			right += voutm;
			right = CLIP(right);

			pbuf->left = left;
			pbuf->right = right;

			pbuf++;
		}
	}
}


// amplitude modulator, low pass filter for underwater weirdness

void SXRVB_DoAMod(int count)
{

	int valtl, valtr;
	int left;
	int right;
	portable_samplepair_t *pbuf;
	int countr;
	int fLowpass;
	int fmod;

	// process reverb lines if active

	if (sxmod_lowpass.value != 0.0 || sxmod_mod.value != 0.0) {

		pbuf = paintbuffer;
		countr = count;

		fLowpass = (sxmod_lowpass.value != 0.0);
		fmod = (sxmod_mod.value != 0.0);

		// process each sample in the paintbuffer...

		while (countr--) {

			left = pbuf->left;
			right = pbuf->right;

			// only process if non-zero

			if (fLowpass) {

				valtl = left;
				valtr = right;

				left = (rgsxlp[0] + rgsxlp[1] + rgsxlp[2] + rgsxlp[3] + rgsxlp[4] + left);
				right = (rgsxlp[5] + rgsxlp[6] + rgsxlp[7] + rgsxlp[8] + rgsxlp[9] + right);

				left >>= 2; // ((left << 1) + (left << 3)) >> 6; // * 10/64
				right >>= 2; // ((right << 1) + (right << 3)) >> 6; // * 10/64

				rgsxlp[3] = valtl;
				rgsxlp[9] = valtr;

				rgsxlp[0] = rgsxlp[1];
				rgsxlp[1] = rgsxlp[2];
				rgsxlp[2] = rgsxlp[3];
				rgsxlp[5] = rgsxlp[6];
				rgsxlp[4] = rgsxlp[5];
				rgsxlp[7] = rgsxlp[8];
				rgsxlp[6] = rgsxlp[7];
				rgsxlp[8] = rgsxlp[9];

			}


			if (fmod) {
				if (--sxmod1cur < 0)
					sxmod1cur = sxmod1;

				if (!sxmod1)
					sxamodlt = RandomLong(32, 255);

				if (--sxmod2cur < 0)
					sxmod2cur = sxmod2;

				if (!sxmod2)
					sxamodlt = RandomLong(32, 255);

				left = (left * sxamodl) >> 8;
				right = (right * sxamodr) >> 8;

				if (sxamodl < sxamodlt)
					sxamodl++;
				else if (sxamodl > sxamodlt)
					sxamodl--;

				if (sxamodr < sxamodrt)
					sxamodr++;
				else if (sxamodr > sxamodrt)
					sxamodr--;
			}

			left = CLIP(left);
			right = CLIP(right);

			pbuf->left = left;
			pbuf->right = right;

			pbuf++;
		}
	}
}

// Set up a delay line buffer allowing a max delay of 'delay' seconds 
// Frees current buffer if it already exists. idelay indicates which of 
// the available delay lines to init.

qboolean SXDLY_Init(int idelay, float delay) {
	int cbsamples;
	HANDLE		hData;
	HPSTR		lpData;
	dlyline_t *pdly;

	pdly = &(rgsxdly[idelay]);

	if (delay > SXDLY_MAX)
		delay = SXDLY_MAX;

	if (pdly->lpdelayline) {
		GlobalUnlock(pdly->hdelayline);
		GlobalFree(pdly->hdelayline);
		pdly->hdelayline = NULL;
		pdly->lpdelayline = NULL;
	}

	if (delay == 0.0)
		return true;

	pdly->cdelaysamplesmax = (int)((float)shm->speed * delay) << sxhires;
	pdly->cdelaysamplesmax += 1;

	cbsamples = pdly->cdelaysamplesmax * sizeof (sample_t);

	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, cbsamples);
	if (!hData)
	{
		Con_SafePrintf("Sound FX: Out of memory.\n");
		return false;
	}

	lpData = (char *)GlobalLock(hData);
	if (!lpData)
	{
		Con_SafePrintf("Sound FX: Failed to lock.\n");
		GlobalFree(hData);
		return false;
	}

	Q_memset(lpData, 0, cbsamples);

	pdly->hdelayline = hData;
	pdly->lpdelayline = (sample_t *)lpData;

	// init delay loop input and output counters.

	// NOTE: init of idelayoutput only valid if pdly->delaysamples is set
	// NOTE: before this call!

	pdly->idelayinput = 0;
	pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;
	pdly->xfade = 0;
	pdly->lp = 1;
	pdly->mod = 0;
	pdly->modcur = 0;

	// init lowpass filter memory

	pdly->lp0 = pdly->lp1 = pdly->lp2 = 0;

	return true;
}

// release delay buffer and deactivate delay

void SXDLY_Free(int idelay) {
	dlyline_t *pdly = &(rgsxdly[idelay]);

	if (pdly->lpdelayline) {
		GlobalUnlock(pdly->hdelayline);
		GlobalFree(pdly->hdelayline);
		pdly->hdelayline = NULL;
		pdly->lpdelayline = NULL;				// this deactivates the delay
	}
}

// check for new stereo delay param

void SXDLY_CheckNewStereoDelayVal() {
	dlyline_t *pdly = &(rgsxdly[ISXSTEREODLY]);
	int delaysamples;

	// set up stereo delay

	if (sxste_delay.value != sxste_delayprev) {
		if (sxste_delay.value == 0.0) {

			// deactivate delay line

			SXDLY_Free(ISXSTEREODLY);
			sxste_delayprev = 0.0;

		}
		else {

			delaysamples = (int)(min(sxste_delay.value, (float)SXSTE_MAX) * shm->speed) << sxhires;

			// init delay line if not active

			if (pdly->lpdelayline == NULL) {

				pdly->delaysamples = delaysamples;

				SXDLY_Init(ISXSTEREODLY, SXSTE_MAX);
			}

			// do crossfade to new delay if delay has changed

			if (delaysamples != pdly->delaysamples) {

				// set up crossfade from old pdly->delaysamples to new delaysamples

				pdly->idelayoutputxf = pdly->idelayinput - delaysamples;

				if (pdly->idelayoutputxf < 0)
					pdly->idelayoutputxf += pdly->cdelaysamplesmax;

				pdly->xfade = 128;
			}

			sxste_delayprev = sxste_delay.value;

			// UNDONE: modulation disabled
			//pdly->mod = 500 * (shm->speed / SOUND_11k);		// change delay every n samples
			pdly->mod = 0;
			pdly->modcur = pdly->mod;

			// deactivate line if rounded down to 0 delay

			if (pdly->delaysamples == 0)
				SXDLY_Free(ISXSTEREODLY);

		}
	}
}

void SXDLY_DoStereoDelay(int count) {
	int left;
	sample_t sampledly;
	sample_t samplexf;
	portable_samplepair_t *pbuf;
	int countr;

	// process delay line if active

	if (rgsxdly[ISXSTEREODLY].lpdelayline) {

		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...

		while (countr--) {

			if (gdly3.mod && (--gdly3.modcur < 0))
				gdly3.modcur = gdly3.mod;

			// get delay line sample from left line

			sampledly = *(gdly3.lpdelayline + gdly3.idelayoutput);
			left = pbuf->left;

			// only process if left value or delayline value are non-zero or xfading

			if (gdly3.xfade || sampledly || left) {

				// if we're not crossfading, and we're not modulating, but we'd like to be modulating,
				// then setup a new crossfade.

				if (!gdly3.xfade && !gdly3.modcur && gdly3.mod) {

					// set up crossfade to new delay value, if we're not already doing an xfade

					//gdly3.idelayoutputxf = gdly3.idelayoutput + 
					//		((RandomLong(0,0x7FFF) * gdly3.delaysamples) / (RAND_MAX * 2)); // 100 = ~ 9ms

					gdly3.idelayoutputxf = gdly3.idelayoutput +
						((RandomLong(0, 0xFF) * gdly3.delaysamples) >> 9); // 100 = ~ 9ms

					if (gdly3.idelayoutputxf >= gdly3.cdelaysamplesmax)
						gdly3.idelayoutputxf -= gdly3.cdelaysamplesmax;

					gdly3.xfade = 128;
				}

				// modify sampledly if crossfading to new delay value

				if (gdly3.xfade) {
					samplexf = (*(gdly3.lpdelayline + gdly3.idelayoutputxf) * (128 - gdly3.xfade)) >> 7;
					sampledly = ((sampledly * gdly3.xfade) >> 7) + samplexf;

					if (++gdly3.idelayoutputxf >= gdly3.cdelaysamplesmax)
						gdly3.idelayoutputxf = 0;

					if (--gdly3.xfade == 0)
						gdly3.idelayoutput = gdly3.idelayoutputxf;
				}

				// save output value into delay line

				left = CLIP(left);

				*(gdly3.lpdelayline + gdly3.idelayinput) = left;

				// save delay line sample into output buffer
				pbuf->left = sampledly;

			}
			else
			{
				// keep clearing out delay line, even if no signal in or out

				*(gdly3.lpdelayline + gdly3.idelayinput) = 0;
			}

			// update delay buffer pointers

			if (++gdly3.idelayinput >= gdly3.cdelaysamplesmax)
				gdly3.idelayinput = 0;

			if (++gdly3.idelayoutput >= gdly3.cdelaysamplesmax)
				gdly3.idelayoutput = 0;

			pbuf++;
		}

	}
}



void SXRVB_CheckNewReverbVal()
{
	dlyline_t *pdly;
	int delaysamples;
	int i;
	int	mod;


	if (sxrvb_size.value != sxrvb_sizeprev)
	{

		sxrvb_sizeprev = sxrvb_size.value;

		if (sxrvb_size.value == 0.0)
		{
			// deactivate all delay lines

			SXDLY_Free(ISXRVB);
			SXDLY_Free(ISXRVB + 1);

		}
		else
		{

			for (i = ISXRVB; i < ISXRVB + CSXRVBMAX; i++)
			{
				// init delay line if not active

				pdly = &(rgsxdly[i]);

				switch (i) {
				case ISXRVB:
					delaysamples = (int)(min(sxrvb_size.value, (float)SXRVB_MAX) * (float)shm->speed) << sxhires;
					pdly->mod = RVB_MODRATE1 << sxhires;
					break;
				case ISXRVB + 1:
					delaysamples = (int)(min(sxrvb_size.value * 0.71, SXRVB_MAX) * (float)shm->speed) << sxhires;
					pdly->mod = RVB_MODRATE2 << sxhires;
					break;
				default:
					assert(false);
					delaysamples = 0;
					break;
				}

				mod = pdly->mod;				// KB: bug, SXDLY_Init clears mod, modcur, xfade and lp - save mod before call

				if (pdly->lpdelayline == NULL)
				{
					pdly->delaysamples = delaysamples;

					SXDLY_Init(i, SXRVB_MAX);
				}

				pdly->modcur = pdly->mod = mod;	// KB: bug, SXDLY_Init clears mod, modcur, xfade and lp - restore mod after call

				// do crossfade to new delay if delay has changed

				if (delaysamples != pdly->delaysamples)
				{
					// set up crossfade from old pdly->delaysamples to new delaysamples

					pdly->idelayoutputxf = pdly->idelayinput - delaysamples;

					if (pdly->idelayoutputxf < 0)
						pdly->idelayoutputxf += pdly->cdelaysamplesmax;

					pdly->xfade = RVB_XFADE;
				}

				// deactivate line if rounded down to 0 delay

				if (pdly->delaysamples == 0)
					SXDLY_Free(i);
			}
		}
	}

	rgsxdly[ISXRVB].delayfeed = (sxrvb_feedback.value) * 255;
	rgsxdly[ISXRVB].lp = sxrvb_lp.value;

	rgsxdly[ISXRVB + 1].delayfeed = (sxrvb_feedback.value) * 255;
	rgsxdly[ISXRVB + 1].lp = sxrvb_lp.value;

}

// If sxdly_delay or sxdly_feedback have changed, update delaysamples
// and delayfeed values.  This applies only to delay 0, the main echo line.

void SXDLY_CheckNewDelayVal() {
	dlyline_t *pdly = &(rgsxdly[ISXMONODLY]);

	if (sxdly_delay.value != sxdly_delayprev) {

		if (sxdly_delay.value == 0.0) {

			// deactivate delay line

			SXDLY_Free(ISXMONODLY);
			sxdly_delayprev = sxdly_delay.value;

		}
		else {
			// init delay line if not active

			pdly->delaysamples = (int)(min(sxdly_delay.value, (float)SXDLY_MAX) * (float)shm->speed) << sxhires;

			if (pdly->lpdelayline == NULL)
				SXDLY_Init(ISXMONODLY, SXDLY_MAX);

			// flush delay line and filters

			if (pdly->lpdelayline) {
				Q_memset(pdly->lpdelayline, 0, pdly->cdelaysamplesmax * sizeof(sample_t));
				pdly->lp0 = 0;
				pdly->lp1 = 0;
				pdly->lp2 = 0;
			}

			// init delay loop input and output counters

			pdly->idelayinput = 0;
			pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;

			sxdly_delayprev = sxdly_delay.value;

			// deactivate line if rounded down to 0 delay

			if (pdly->delaysamples == 0)
				SXDLY_Free(ISXMONODLY);

		}
	}

	pdly->lp = (int)(sxdly_lp.value);
	pdly->delayfeed = sxdly_feedback.value * 255;
}


struct sx_preset_t
{
	float room_lp;					// for water fx, lowpass for entire room
	float room_mod;					// stereo amplitude modulation for room

	float room_size;				// reverb: initial reflection size
	float room_refl;				// reverb: decay time
	float room_rvblp;				// reverb: low pass filtering level

	float room_delay;				// mono delay: delay time
	float room_feedback;			// mono delay: decay time
	float room_dlylp;				// mono delay: low pass filtering level

	float room_left;				// left channel delay time
};


sx_preset_t rgsxpre[CSXROOM] =
{

	// SXROOM_OFF					0

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 2.0, 0.0 },

	// SXROOM_GENERIC				1		// general, low reflective, diffuse room

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.065, 0.1, 0.0, 0.01 },

	// SXROOM_METALIC_S				2		// highly reflective, parallel surfaces
	// SXROOM_METALIC_M				3
	// SXROOM_METALIC_L				4

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.02, 0.75, 0.0, 0.01 }, // 0.001
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.03, 0.78, 0.0, 0.02 }, // 0.002
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.06, 0.77, 0.0, 0.03 }, // 0.003


	// SXROOM_TUNNEL_S				5		// resonant reflective, long surfaces
	// SXROOM_TUNNEL_M				6
	// SXROOM_TUNNEL_L				7

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.05, 0.85, 1.0, 0.008, 0.96, 2.0, 0.01 }, // 0.01
	{ 0.0, 0.0, 0.05, 0.88, 1.0, 0.010, 0.98, 2.0, 0.02 }, // 0.02
	{ 0.0, 0.0, 0.05, 0.92, 1.0, 0.015, 0.995, 2.0, 0.04 }, // 0.04

	// SXROOM_CHAMBER_S				8		// diffuse, moderately reflective surfaces
	// SXROOM_CHAMBER_M				9
	// SXROOM_CHAMBER_L				10

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.05, 0.84, 1.0, 0.0, 0.0, 2.0, 0.012 }, // 0.003
	{ 0.0, 0.0, 0.05, 0.90, 1.0, 0.0, 0.0, 2.0, 0.008 }, // 0.002
	{ 0.0, 0.0, 0.05, 0.95, 1.0, 0.0, 0.0, 2.0, 0.004 }, // 0.001

	// SXROOM_BRITE_S				11		// diffuse, highly reflective
	// SXROOM_BRITE_M				12
	// SXROOM_BRITE_L				13

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.05, 0.7, 0.0, 0.0, 0.0, 2.0, 0.012 }, // 0.003
	{ 0.0, 0.0, 0.055, 0.78, 0.0, 0.0, 0.0, 2.0, 0.008 }, // 0.002
	{ 0.0, 0.0, 0.05, 0.86, 0.0, 0.0, 0.0, 2.0, 0.002 }, // 0.001

	// SXROOM_WATER1				14		// underwater fx
	// SXROOM_WATER2				15
	// SXROOM_WATER3				16

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 2.0, 0.01 },
	{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.06, 0.85, 2.0, 0.02 },
	{ 1.0, 0.0, 0.0, 0.0, 1.0, 0.2, 0.6, 2.0, 0.05 },

	// SXROOM_CONCRETE_S			17		// bare, reflective, parallel surfaces
	// SXROOM_CONCRETE_M			18
	// SXROOM_CONCRETE_L			19

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.05, 0.8, 1.0, 0.0, 0.48, 2.0, 0.016 }, // 0.15 delay, 0.008 left
	{ 0.0, 0.0, 0.06, 0.9, 1.0, 0.0, 0.52, 2.0, 0.01 }, // 0.22 delay, 0.005 left
	{ 0.0, 0.0, 0.07, 0.94, 1.0, 0.3, 0.6, 2.0, 0.008 }, // 0.001

	// SXROOM_OUTSIDE1				20		// echoing, moderately reflective
	// SXROOM_OUTSIDE2				21		// echoing, dull
	// SXROOM_OUTSIDE3				22		// echoing, very dull

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.3, 0.42, 2.0, 0.0 },
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.35, 0.48, 2.0, 0.0 },
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.38, 0.6, 2.0, 0.0 },

	// SXROOM_CAVERN_S				23		// large, echoing area
	// SXROOM_CAVERN_M				24
	// SXROOM_CAVERN_L				25

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 0.0, 0.05, 0.9, 1.0, 0.2, 0.28, 0.0, 0.0 },
	{ 0.0, 0.0, 0.07, 0.9, 1.0, 0.3, 0.4, 0.0, 0.0 },
	{ 0.0, 0.0, 0.09, 0.9, 1.0, 0.35, 0.5, 0.0, 0.0 },

	// SXROOM_WEIRDO1				26	
	// SXROOM_WEIRDO2				27
	// SXROOM_WEIRDO3				28
	// SXROOM_WEIRDO3				29

	//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{ 0.0, 1.0, 0.01, 0.9, 0.0, 0.0, 0.0, 2.0, 0.05 },
	{ 0.0, 0.0, 0.0, 0.0, 1.0, 0.009, 0.999, 2.0, 0.04 },
	{ 0.0, 0.0, 0.001, 0.999, 0.0, 0.2, 0.8, 2.0, 0.05 }

};



// force next call to sx_roomfx to reload all room parameters.
// used when switching to/from hires sound mode.

void SX_ReloadRoomFX()
{
	// reset all roomtype parms

	sxroom_typeprev = -1.0;

	sxdly_delayprev = -1.0;
	sxrvb_sizeprev = -1.0;
	sxste_delayprev = -1.0;

	// UNDONE: handle sxmod and mod parms? 
}

// main routine for processing room sound fx
// if fFilter is TRUE, then run in-line filter (for underwater fx)
// if fTimefx is TRUE, then run reverb and delay fx
// NOTE: only processes preset room_types from 0-29 (CSXROOM)

void SX_RoomFX(int count, int fFilter, int fTimefx)
{
	int fReset = FALSE;
	int i;
	int sampleCount = count;
	float roomType;

	// return right away if fx processing is turned off

	if (sxroom_off.value != 0.0)
		return;

	// detect changes in hires sound param

	sxhires = (hisound.value == 0 ? 0 : 1);

	if (sxhires != sxhiresprev)
	{
		SX_ReloadRoomFX();
		sxhiresprev = sxhires;
	}

	if (cl.waterlevel > 2)
		roomType = sxroomwater_type.value;
	else
		roomType = sxroom_type.value;

	// only process legacy roomtypes here

//	if ((int)roomType >= CSXROOM)
//		return;

	if (roomType != sxroom_typeprev)
	{

		// Con_Printf ("Room_type: %2.1f\n", roomType);

		sxroom_typeprev = roomType;

		i = (int)(roomType);

		if (i < CSXROOM && i >= 0)
		{
			Cvar_SetValue(const_cast<char*>("room_lp"), rgsxpre[i].room_lp);
			Cvar_SetValue(const_cast<char*>("room_mod"), rgsxpre[i].room_mod);
			Cvar_SetValue(const_cast<char*>("room_size"), rgsxpre[i].room_size);
			Cvar_SetValue(const_cast<char*>("room_refl"), rgsxpre[i].room_refl);
			Cvar_SetValue(const_cast<char*>("room_rvblp"), rgsxpre[i].room_rvblp);
			Cvar_SetValue(const_cast<char*>("room_delay"), rgsxpre[i].room_delay);
			Cvar_SetValue(const_cast<char*>("room_feedback"), rgsxpre[i].room_feedback);
			Cvar_SetValue(const_cast<char*>("room_dlylp"), rgsxpre[i].room_dlylp);
			Cvar_SetValue(const_cast<char*>("room_left"), rgsxpre[i].room_left);
		}

		SXRVB_CheckNewReverbVal();
		SXDLY_CheckNewDelayVal();
		SXDLY_CheckNewStereoDelayVal();

		fReset = TRUE;
	}

	if (fReset || roomType != 0.0)
	{
		// debug code
		SXRVB_CheckNewReverbVal();
		SXDLY_CheckNewDelayVal();
		SXDLY_CheckNewStereoDelayVal();
		// debug code

		if (fFilter)
			SXRVB_DoAMod(sampleCount);

		if (fTimefx)
		{
			SXRVB_DoReverb(sampleCount);
			SXDLY_DoDelay(sampleCount);
			SXDLY_DoStereoDelay(sampleCount);
		}
	}
}

void S_MixChannelsToPaintbuffer(int end, qboolean fPaintHiresSounds, qboolean voiceOnly)
{
	int		i;
	channel_t *ch;
	wavstream_t* wave;
	int		sampleCount;
	qboolean bVoice;
	int timecompress;
	int ltime;
	int pitch;
	sfxcache_t* sc;
	qboolean hires;
	int fhitend = FALSE;
	int chend;
	int deltatime;
	int count;
	int left;
	int oldleft, oldright;
	char copyBuf[4096];

	// mix each channel into paintbuffer

	hires = fPaintHiresSounds != 0;
	ch = channels;
	wave = wavstreams;

	for (i = 0; i < total_channels; i++, ch++, wave++)
	{
		if (!ch->sfx)
		{
			continue;
		}

		sfxcache_t* sc = S_LoadSound(ch->sfx, ch);
		// stopline
		bVoice = sc->length == 0x40000000;

		if ((bVoice && !voiceOnly) || (!bVoice && voiceOnly))
			continue;

		if (ch->leftvol || ch->rightvol)
		{
			if ((fPaintHiresSounds && sc->speed > shm->speed) || (!fPaintHiresSounds && sc->speed == shm->speed))
			{
				fhitend = FALSE;
				ltime = paintedtime;
				pitch = ch->pitch;

				if (ch->isentence >= 0)
				{
					if (rgrgvoxword[ch->isentence][ch->iword].pitch <= 0)
						pitch = ch->pitch;
					else 
						pitch = ch->pitch + rgrgvoxword[ch->isentence][ch->iword].pitch - PITCH_NORM;

					timecompress = rgrgvoxword[ch->isentence][ch->iword].timecompress;
				}
				else
					timecompress = 0;

				while (ltime < end)
				{
					if (hires)
						chend = (sc->length >> 1) + ch->end - sc->length;
					else 
						chend = ch->end;

					left = min(end, chend) - ltime;
					if (bVoice && (sc->width != 2 || sc->stereo))
						break;

					count = left << hires;
					deltatime = (ltime - paintedtime) << hires;

					if (hires)
						deltatime <<= 1;

					if (count <= 0)
					{
						if (ltime >= chend)
						{
							fhitend = FALSE;
							if (S_CheckWavEnd(ch, &sc, ltime, i))
								break;
						}
					}
					else
					{
						if (sc->width == 1 && ch->entnum > 0 &&
							(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM))
						{
							SND_MoveMouth(ch, sc, count);
						}
						
						oldleft = ch->leftvol;
						oldright = ch->rightvol;

						if (!bVoice)
						{
							ch->leftvol = oldleft / g_SND_VoiceOverdrive;
							ch->rightvol = oldright / g_SND_VoiceOverdrive;
						}

						if (sc->width == 1)
						{
							if (pitch == PITCH_NORM && !timecompress || ch->isentence < 0)
							{
								if (ltime == paintedtime)
									SND_PaintChannelFrom8(ch, sc, count);
								else
									SND_PaintChannelFrom8Offs(paintbuffer, ch, sc, count, deltatime);
							}
							else
							{
								fhitend = VOX_FPaintPitchChannelFrom8Offs(paintbuffer, ch, sc, count, pitch, timecompress, deltatime);
							}
						}
						else
						{
							if (bVoice)
								count = VoiceSE_GetSoundDataCallbackPtr(sc, copyBuf, sizeof(copyBuf), ch->pos, count);

							SND_PaintChannelFrom16Offs(paintbuffer, ch, bVoice ? (short*)copyBuf : (short*)&sc->data[ch->pos * sizeof(short)], count, deltatime);
						}

						ltime += count >> hires;

						ch->leftvol = oldleft;
						ch->rightvol = oldright;

						if (ch->entchannel == CHAN_STREAM)
							wave->csamplesplayed += count;
							//wave->csamplesinmem += count;

						if (fhitend || (!fhitend && (ltime >= chend)))
						{
							fhitend = FALSE;
							if (S_CheckWavEnd(ch, &sc, ltime, i))
								break;
						}
					}
				}
			}
		}
		else if (sc->loopstart < 0)
		{
			S_FreeChannel(ch);
		}
	}
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

void Snd_WriteLinearBlastStereo16(void)
{
	int		i;
	int		val;

	for (i = 0; i < snd_linear_count; i += 2)
	{
		val = (snd_p[i] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i + 1] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i + 1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i + 1] = (short)0x8000;
		else
			snd_out[i + 1] = val;
	}
}

void S_TransferStereo16(int end)
{
	int		lpos;
	int		lpaintedtime;
	DWORD	*pbuf;
#ifdef _WIN32
	int		reps;
	DWORD	dwSize, dwSize2;
	DWORD	*pbuf2;
	HRESULT	hresult;
#endif
	int endtime;

	snd_vol = volume.value * 256;

	snd_p = (int *)paintbuffer;
	lpaintedtime = paintedtime * 2;
	endtime = end * 2;

#ifdef _WIN32
	if (pDSBuf)
	{
		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, (LPVOID*)&pbuf, &dwSize,
			(LPVOID*)&pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf(const_cast<char*>("S_TransferStereo16: DS::Lock Sound Buffer Failed\n"));
				S_Shutdown();
				S_Startup();
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf(const_cast<char*>("S_TransferStereo16: DS: couldn't restore buffer\n"));
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = (DWORD *)shm->buffer;
	}

	while (lpaintedtime < endtime)
	{
		// handle recirculating buffer issues
		lpos = lpaintedtime & ((shm->samples >> 1) - 1);

		snd_out = (short *)pbuf + (lpos << 1);

		snd_linear_count = (shm->samples >> 1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		Snd_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}

#ifdef _WIN32
	if (pDSBuf)
		pDSBuf->lpVtbl->Unlock(pDSBuf, pbuf, dwSize, NULL, 0);
#endif
}

void S_TransferPaintBuffer(int endtime)
{
	S_TransferStereo16(endtime);
}

void S_PaintChannels(int endtime)
{
	int 	i;
	int 	end;
	channel_t *ch;
	sfxcache_t	*sc;
	int		ltime, count;
	static portable_samplepair_t paintprev;

	while (paintedtime < endtime)
	{
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		ltime = paintedtime;
		count = end - ltime;

		// clear the paint buffer
		Q_memset(paintbuffer, 0, count * sizeof(portable_samplepair_t));

		if (!cl.paused && !s_careerAudioPaused)
			S_MixChannelsToPaintbuffer(end, FALSE, FALSE);

		if (hisound.value == 0.0)
			SX_RoomFX(count, TRUE, TRUE);

		for (i = count - 1; i > 0; i--)
		{
			paintbuffer[2 * i + 1].left = paintbuffer[i].left;
			paintbuffer[2 * i + 1].right = paintbuffer[i].right;
			paintbuffer[2 * i].left = AVG(paintbuffer[i].left, paintbuffer[i - 1].left);
			paintbuffer[2 * i].right = AVG(paintbuffer[i].right, paintbuffer[i - 1].right);
		}

		// stopline
		paintbuffer[1].left = paintbuffer[0].left;
		paintbuffer[1].right = paintbuffer[0].right;

		paintbuffer[0].left = AVG(paintbuffer[1].left, paintprev.left);
		paintbuffer[0].right = AVG(paintbuffer[1].right, paintprev.right);

		paintprev.left = paintbuffer[2 * count - 1].left;
		paintprev.right = paintbuffer[2 * count - 1].right;

		if (hisound.value > 0.0)
		{
			if (!cl.paused && !s_careerAudioPaused)
				S_MixChannelsToPaintbuffer(end, TRUE, FALSE);

			SX_RoomFX(2 * count, 1, 1);
		}

		// paint in the channels.
		if (!cl.paused && !s_careerAudioPaused)
			S_MixChannelsToPaintbuffer(end, TRUE, TRUE);

		// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}

}
