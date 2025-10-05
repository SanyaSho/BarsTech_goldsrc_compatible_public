#ifndef ENGINE_SND_H
#define ENGINE_SND_H

#include "quakedef.h"
#include "winquake.h"

#define	MAX_CHANNELS			128
#define	MAX_DYNAMIC_CHANNELS	8

// sound engine rate defines
#define SOUND_DMA_SPEED		22050		// hardware playback rate
#define SOUND_11k			11025		// 11khz sample rate
#define SOUND_22k			22050		// 22khz sample rate

#define AVG(a,b)			(((a) + (b)) >> 1 )

//=====================================================================
// FX presets
//=====================================================================

#define SXROOM_OFF			0		

#define SXROOM_GENERIC		1		// general, low reflective, diffuse room

#define SXROOM_METALIC_S	2		// highly reflective, parallel surfaces
#define SXROOM_METALIC_M	3
#define SXROOM_METALIC_L	4

#define SXROOM_TUNNEL_S		5		// resonant reflective, long surfaces
#define SXROOM_TUNNEL_M		6
#define SXROOM_TUNNEL_L		7

#define SXROOM_CHAMBER_S	8		// diffuse, moderately reflective surfaces
#define SXROOM_CHAMBER_M	9
#define SXROOM_CHAMBER_L	10

#define SXROOM_BRITE_S		11		// diffuse, highly reflective
#define SXROOM_BRITE_M		12
#define SXROOM_BRITE_L		13

#define SXROOM_WATER1		14		// underwater fx
#define SXROOM_WATER2		15
#define SXROOM_WATER3		16

#define SXROOM_CONCRETE_S	17		// bare, reflective, parallel surfaces
#define SXROOM_CONCRETE_M	18
#define SXROOM_CONCRETE_L	19

#define SXROOM_OUTSIDE1		20		// echoing, moderately reflective
#define SXROOM_OUTSIDE2		21		// echoing, dull
#define SXROOM_OUTSIDE3		22		// echoing, very dull

#define SXROOM_CAVERN_S		23		// large, echoing area
#define SXROOM_CAVERN_M		24
#define SXROOM_CAVERN_L		25

#define SXROOM_WEIRDO1		26		
#define SXROOM_WEIRDO2		27
#define SXROOM_WEIRDO3		28

#define CSXROOM				29


#define CVOXWORDMAX		32
#define CVOXSENTENCEMAX	24

#define CVOXZEROSCANMAX	255			// scan up to this many samples for next zero crossing

#define	PAINTBUFFER_SIZE	512

#if defined( FEATURE_HL25 )
#define CVOXFILESENTENCEMAX 2048
#else
#define CVOXFILESENTENCEMAX 1536
#endif // FEATURE_HL25

#define CBSENTENCENAME_MAX	16


// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

typedef struct sfx_s
{
	char 	name[MAX_QPATH];
	cache_user_t	cache;
	int servercount;
} sfx_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int 	length;
	intptr_t 	loopstart;
	int 	speed;
	int 	width;
	int 	stereo;
	byte	data[1];		// variable sized
} sfxcache_t;

typedef struct voxword_s voxword_t;

typedef struct voxword_s
{
	int		volume;						// increase percent, ie: 125 = 125% increase
	int		pitch;						// pitch shift up percent
	int		start;						// offset start of wave percent
	int		end;						// offset end of wave percent
	int		cbtrim;						// end of wave after being trimmed to 'end'
	int		fKeepCached;				// 1 if this word was already in cache before sentence referenced it
	int		samplefrac;					// if pitch shifting, this is position into wav * 256
	int		timecompress;				// % of wave to skip during playback (causes no pitch shift)
	sfx_t	*sfx;					// name and cache pointer
} voxword_t;

typedef struct
{
	qboolean		gamealive;
	qboolean		soundalive;
	qboolean		splitbuffer;
	int				channels;
	int				samples;				// mono samples in rgba
	int				submission_chunk;		// don't mix less than this #
	int				samplepos;				// in mono samples
	int				samplebits;
	int				speed;
	int				dmaspeed;
	unsigned char	*buffer;
} dma_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	sfx_t	*sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int 	pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int		master_vol;		// 0-255 master volume
	int		isentence;
	int		iword;
	int		pitch;
} channel_t;

struct wavinfo_t
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
};

struct wavstream_t
{
	int				csamplesplayed;
	int				csamplesinmem;
	FileHandle_t	hFile;
	wavinfo_t		info;
	int				lastposloaded;
};

extern	channel_t   channels[MAX_CHANNELS];
extern wavstream_t wavstreams[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern "C" portable_samplepair_t paintbuffer[(PAINTBUFFER_SIZE + 1) * 2];
extern int cszrawsentences;
extern char* rgpszrawsentence[CVOXFILESENTENCEMAX];
extern	int			total_channels;

#ifdef _WIN32
extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

void SNDDMA_Shutdown();
#endif

#define CHAR_STREAM			'*'		// as one of 1st 2 chars in name, indicates streaming wav data
#define CHAR_USERVOX		'?'		// as one of 1st 2 chars in name, indicates user realtime voice data
#define CHAR_SENTENCE		'!'		// as one of 1st 2 chars in name, indicates sentence wav
#define CHAR_DRYMIX			'#'		// as one of 1st 2 chars in name, indicates wav bypasses dsp fx
#define CHAR_DOPPLER		'>'		// as one of 1st 2 chars in name, indicates doppler encoded stereo wav
#define CHAR_DIRECTIONAL	'<'		// as one of 1st 2 chars in name, indicates mono or stereo wav has direction cone
#define CHAR_DISTVARIANT	'^'		// as one of 1st 2 chars in name, indicates distance variant encoded stereo wav
#define CHAR_OMNI			'@'		// as one of 1st 2 chars in name, indicates non-directional wav (default mono or stereo)


//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern qboolean 		fakedma;
extern int 			fakedma_updates;
extern int		paintedtime;
extern int sound_started;
extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;
extern volatile dma_t *shm;
extern volatile dma_t sn;
extern vec_t sound_nominal_clip_dist;

extern	cvar_t loadas8bit;
extern	cvar_t bgmvolume;
extern	cvar_t volume;

extern	cvar_t snd_show;
extern	cvar_t s_a3d;
extern	cvar_t s_eax;
extern	cvar_t MP3Volume;
extern	cvar_t MP3FadeTime;
extern	cvar_t suitvolume;
extern	cvar_t hisound;

extern qboolean	snd_initialized;
extern qboolean snd_isdirect;

extern int		snd_blocked;
extern int		snd_buffer_count;

extern qboolean g_fUseDInput;

extern DWORD gSndBufSize;

void S_Init();

void S_Shutdown();

sfx_t* S_PrecacheSound( char* name );

void S_StartDynamicSound( int entnum, int entchannel, sfx_t* sfx, vec_t* origin, float fvol, float attenuation, int flags, int pitch );
void S_StartStaticSound(int entnum, int entchannel, sfx_t *sfxin, vec_t* origin, float fvol, float attenuation, int flags, int pitch);
void S_StopSound( int entnum, int entchannel );

void S_StopAllSounds( bool clear );

void Snd_AcquireBuffer();
void Snd_ReleaseBuffer();

void SetMouseEnable( int fState );

void VOX_Init();

void SetCareerAudioState(qboolean st);

void S_FreeChannel(channel_t* chan);
int Wavstream_Init(void);
void SND_ForceCloseMouth(int entnum);
int SNDDMA_GetDMAPos();
sfxcache_t *S_LoadSound(sfx_t *s, channel_t* ch);
void ResampleSfx(sfx_t *sfx, int inrate, int inwidth, byte *data
#ifdef _2020_PATCH
	, int datasize
#endif
);
void VOX_Shutdown();
void S_Startup(void);
int SNDDMA_Init(void);
void VOX_Init();
void SND_InitScaletable(void);
void SX_Init(void);
void VOX_SetChanVol(channel_t *ch);
char *VOX_LookupString(const char *pSentenceName, int *psentencenum);
sfx_t *S_FindName(char *name, int *pfInCache);
void SND_InitMouth(int entnum, int entchannel);
sfxcache_t* VOX_LoadSound(channel_t *pchan, char *pszin);
void VOX_MakeSingleWordSentence(channel_t *ch, int pitch);
void VOX_TrimStartEndTimes(channel_t* ch, sfxcache_t* sc);
void SX_RoomFX(int count, int fFilter, int fTimefx);
void S_PaintChannels(int endtime);
void SNDDMA_Submit(void);
void S_ExtraUpdate(void);
void S_ClearBuffer(void);
void S_BeginPrecaching(void);
void S_EndPrecaching(void);
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up);
void S_PrintStats();
void SND_ForceInitMouth(int entnum);
void SND_MoveMouth16(int entnum, short* pdata, int count);

#endif //ENGINE_SND_H
