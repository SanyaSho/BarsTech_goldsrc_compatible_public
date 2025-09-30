#ifndef ENGINE_CL_DEMO_H
#define ENGINE_CL_DEMO_H

#include "cdll_int.h"
#include "demo_api.h"
#include "common/Sequence.h"
#include "pm_shared/pm_movevars.h"

#define DEMO_PROTOCOL			5
#define DEMO_MAX_DIR_ENTRIES	1024

#define GG_MAX_FRAMES			300

#define FDEMO_TITLE				(1<<0)
#define FDEMO_BIT2				(1<<1)
#define FDEMO_CDA				(1<<2)
#define FDEMO_FADEIN_SLOW		(1<<3)
#define FDEMO_FADEIN_FAST		(1<<4)
#define FDEMO_FADEOUT_SLOW		(1<<5)
#define FDEMO_FADEOUT_FAST		(1<<6)

#define DEMO_STARTUP			0
#define DEMO_NORMAL				1

#define MAX_DEMO_FRAMES			32

#define MAX_POSSIBLE_MSG		65535

enum
{
	// it's a startup message, process as fast as possible
	dem_norewind = 1,
	// move the demostart time value forward by this amount.
	dem_jumptime,
	dem_cmd,
	dem_angles,
	dem_read,
	dem_event,
	dem_wpnanim,
	dem_playsound,
	dem_clientdll,
	dem_lastcmd = dem_clientdll
};


typedef struct sequence_info_s
{
	int incoming_sequence;
	int incoming_acknowledged;
	int incoming_reliable_acknowledged;
	int incoming_reliable_sequence;
	int outgoing_sequence;
	int reliable_sequence;
	int last_reliable_sequence;
	int length;
} sequence_info_t;

typedef struct demoentry_s {
	int nEntryType;				// DEMO_STARTUP or DEMO_NORMAL
	char szDescription[64];
	int nFlags;
	int nCDTrack;
	float fTrackTime;			// time of track
	int nFrames;				// # of frames in track
	int nOffset;				// file offset of track data
	int nFileLength;			// length of track
} demoentry_t;

struct demodirectory_t
{
	DWORD nEntries;
	demoentry_t* p_rgEntries;
};

typedef struct demoheader_s {
	char szFileStamp[6];
	int nDemoProtocol;			// should be DEMO_PROTOCOL
	int nNetProtocolVersion;	// should be PROTOCOL_VERSION
	char szMapName[260];		// name of map
	char szDllDir[260];			// name of game directory
	CRC32_t mapCRC;
	int nDirectoryOffset;		// offset of Entry Directory.
} demoheader_t;

typedef struct demo_info_s
{
	float timestamp;
	ref_params_t rp;
	usercmd_t cmd;
	movevars_t movevars;
	vec3_t view;
	int viewmodel;
} demo_info_t;

#include <pshpack1.h>
typedef struct demo_command_s
{
	byte cmd;
	float time;
	int frame;
} demo_command_t;

typedef struct hud_command_s : demo_command_s
{
	char szNameBuf[64];
} hud_command_t;

typedef struct cl_demo_data_s : demo_command_s
{
	vec3_t origin;
	vec3_t viewangles;
	int iWeaponBits;
	float fov;
} cl_demo_data_t;

typedef struct demo_anim_s : demo_command_s
{
	int anim;
	int body;
} demo_anim_t;

typedef struct demo_event_s : demo_command_s
{
	int flags;
	int idx;
	float delay;
} demo_event_t;

typedef struct demo_sound1_s : demo_command_s
{
	int channel;
	int length;
} demo_sound1_t;
#include <poppack.h>

typedef struct demo_sound2_s
{
	float volume;
	float attenuation;
	int flags;
	int pitch;
} demo_sound2_t;

extern client_textmessage_t tm_demomessage;
extern demodirectory_t demodir;
extern cvar_t cl_gamegauge;
extern int g_rgFPS[GG_MAX_FRAMES];
extern int g_iMinFPS, g_iMaxFPS;
extern int g_iSecond;
extern demoentry_t* pCurrentEntry;
extern demoheader_t demoheader;
extern int nCurEntryIndex;
extern float g_demotimescale;

BOOL CL_DemoAPIRecording();

BOOL CL_DemoAPIPlayback();

BOOL CL_DemoAPITimedemo();

void CL_WriteClientDLLMessage( int size, byte* buf );

void CL_WriteDLLUpdate( client_data_t* cdat );

void CL_DemoAnim( int anim, int body );

void CL_DemoEvent( int flags, int idx, float delay, event_args_t* pargs );

void CL_SetDemoViewInfo(ref_params_t *rp, vec_t *view, int viewmodel);

void CL_GetDemoViewInfo(ref_params_t *rp, float *view, int *viewmodel);

void CL_GGSpeeds(float flCurTime);

void CL_WriteDemoStartup(int start, sizebuf_t *msg);

void CL_WriteDemoMessage(int start, sizebuf_t *msg);

void CL_RecordHUDCommand(const char* pszCmd);

void CL_WriteDemoStartup(int start, sizebuf_t* msg);

void CL_StopPlayback();

void CL_FinishTimeDemo();

void CL_FinishGameGauge();

int CL_ReadDemoMessage_OLD();

int CL_ReadDemoMessage();

void CL_BeginDemoStartup();

float CL_DemoOutTime();

float CL_DemoInTime();

void CL_PlayDemo_f();

void CL_TimeDemo_f();

void CL_WavePlayLen_f();

void CL_GameGauge_f();

void CL_Stop_f();

void CL_ViewDemo_f();

void CL_Record_f();

void CL_SetDemoInfo_f();

void CL_SwapDemo_f();

void CL_RemoveDemo_f();

void CL_AppendDemo_f();

void CL_ListDemo_f();

void CL_DemoQueueSound(int channel, const char* sample, float volume, float attenuation, int flags, int pitch);

#endif //ENGINE_CL_DEMO_H
