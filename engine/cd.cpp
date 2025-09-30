#include "quakedef.h"
#include <mmsystem.h>
#include <stdlib.h>
#include <time.h>

#include "host.h" // we need realtime
#include <mss.h>

#include "cd.h"
#include "cd_internal.h"
#include "ithread.h"

extern cvar_t bgmvolume;
extern cvar_t MP3Volume;
extern cvar_t MP3FadeTime;

HDIGDRIVER MP3digitalDriver;
HSTREAM MP3stream;

static CCDAudio g_CDAudio;
ICDAudio* cdaudio = (ICDAudio*)&g_CDAudio;

// Output : 
// Wait 2 extra seconds before stopping playback.
const float CCDAudio::TRACK_EXTRA_TIME = 2.0f;

char* g_pszMP3trackFileMap[200];
int g_iMP3FirstMalloc, g_iMP3NumTracks;

extern SDL_Window* pmainwindow;

HWND GetMainWindow( void )
{
	return *((HWND*)pmainwindow);
}

CCDAudio* GetInteralCDAudio( void )
{
	return &g_CDAudio;
}

// Play or stop a cd track
void CD_Command_f( void )
{
	g_CDAudio.CD_f();
}

// Receive track ID by name
int MP3_GetTrack( const char* pszTrack )
{
	int		i;

	if (!pszTrack || strstr(pszTrack, ":") || strstr(pszTrack, ".."))
		return 0;

	const char* pszFormat;
	if (!strstr(pszTrack, ".mp3"))
		pszFormat = "%s.mp3";
	else
		pszFormat = "%s";

	char szTemp[MAX_PATH];
	snprintf(szTemp, sizeof(szTemp) - 1, pszFormat, pszTrack);
	szTemp[sizeof(szTemp) - 1] = 0;

	COM_FixSlashes(szTemp);

	for (i = 0; i < ARRAYSIZE(g_pszMP3trackFileMap); i++)
	{
		if (!g_pszMP3trackFileMap[i])
		{
			g_pszMP3trackFileMap[i] = Mem_Strdup(szTemp);
			g_iMP3NumTracks = i + 1; // increase total tracks counter
			return i;
		}

		if (!strcmp(g_pszMP3trackFileMap[i], szTemp))
			return i;
	}

	return 0;
}

// Handle "mp3" console command to play the track
void MP3_Command_f( void )
{
	if (Cmd_Argc() < 2)
		return;

	const char* pszCommand = Cmd_Argv(1);
	const char* pszTrack = Cmd_Argv(2);

	if (!stricmp(pszCommand, "play"))
	{
		g_CDAudio.Stop();
		g_CDAudio.Play(MP3_GetTrack(pszTrack), false);
	}

	if (!stricmp(pszCommand, "playfile"))
	{
		g_CDAudio.Stop();
		g_CDAudio.PlayFile(pszTrack, false);
	}

	if (!stricmp(pszCommand, "loop"))
	{
		g_CDAudio.Stop();
		g_CDAudio.Play(MP3_GetTrack(pszTrack), true);
	}

	if (!stricmp(pszCommand, "loopfile"))
	{
		g_CDAudio.Stop();
		g_CDAudio.PlayFile(pszTrack, true);
	}

	if (!stricmp(pszCommand, "stop"))
	{
		g_CDAudio.Stop();
	}
}

void PrimeMusicStream( char* filename, int looping )
{
	g_CDAudio.PrimeTrack(filename, looping);
}

CCDAudio::CCDAudio( void )
{
	m_nMaxCDTrack = 0;
	m_uiDeviceID = 0;

	ResetCDTimes();

	m_bIsCDValid = false;
	m_bIsPlaying = false;
	m_bIsPrimed = false;
	m_bIsInMiddleOfPriming = false;
	m_bWasPlaying = false;
	m_bInitialized = false;
	m_bEnabled = false;
	m_bIsLooping = false;

	m_flVolume = 1.0;
	m_flMP3Volume = 1.0;
	m_dFadeOutTime = 0.0;

	m_nPlayTrack = 0;
	m_szPendingPlayFilename[0] = '\0';

	m_bResumeOnSwitch = false;

	memset(m_rgRemapCD, 0, sizeof(m_rgRemapCD));
}

CCDAudio::~CCDAudio( void )
{
}

// Reset clock
void CCDAudio::ResetCDTimes( void )
{
	m_flPlayTime = 0.0;
	m_dStartTime = 0.0;
	m_dPauseTime = 0.0;
}

// Stop playing cd
void CCDAudio::_Stop( int, int )
{
	if (!m_bEnabled)
		return;

	if ((!m_bIsPlaying) && (!m_bWasPlaying))
		return;

	m_bIsPlaying = false;
	m_bWasPlaying = false;

	m_szPendingPlayFilename[0] = '\0';

	MP3_StopStream();

	m_MP3.inuse = false;
	m_MP3.suspended = false;
	m_MP3.playing = false;
	m_MP3.trackname[0] = '\0';
	m_MP3.tracknum = 0;
	m_MP3.looping = false;
	m_MP3.volume = m_flMP3Volume;

	mciSendCommand(m_uiDeviceID, MCI_STOP, 0, (DWORD)NULL);

	ResetCDTimes();
}

// Pause playback
void CCDAudio::_Pause( int, int )
{
	MCI_GENERIC_PARMS	mciGenericParms;

	if (!m_bEnabled)
		return;

	if (!m_bIsPlaying)
		return;

	if (m_MP3.inuse)
	{
		MP3_SetPause(true);
	}
	else
	{
		mciGenericParms.dwCallback = (DWORD)GetMainWindow();
		mciSendCommand(m_uiDeviceID, MCI_PAUSE, 0, (DWORD)(LPVOID)&mciGenericParms);
	}

	m_bWasPlaying = m_bIsPlaying;
	m_bIsPlaying = false;

	m_szPendingPlayFilename[0] = '\0';

	m_dPauseTime = realtime;
}

// Eject the disk
void CCDAudio::_Eject( int, int )
{
	DWORD	dwReturn;

	_Stop(0, 0);

	dwReturn = mciSendCommand(m_uiDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD)NULL);

	ResetCDTimes();
}

// Close the cd drive door
void CCDAudio::_CloseDoor( int, int )
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand(m_uiDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD)NULL);

	ResetCDTimes();
}

// Retrieve validity and max track data
void CCDAudio::_GetAudioDiskInfo( int, int )
{
	DWORD				dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;


	m_bIsCDValid = false;
	
	mciStatusParms.dwItem = MCI_STATUS_READY;

	dwReturn = mciSendCommand(m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms);
	if (dwReturn)
	{
		return;
	}
	if (!mciStatusParms.dwReturn)
	{
		return;
	}

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	dwReturn = mciSendCommand(m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms);
	if (dwReturn)
	{
		return;
	}

	if (!mciStatusParms.dwReturn)
	{
		return;
	}

	m_bIsCDValid = true;
	m_nMaxCDTrack = (int)mciStatusParms.dwReturn;
}

// Play cd track
void CCDAudio::_Play( int track, int looping )
{
	int mins, secs;
	DWORD               dwm_flPlayTime;
	DWORD				dwReturn;
	MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;

	if (!m_bEnabled)
		return;

	if (!track && strlen(m_szPendingPlayFilename))
	{
		MP3_PlayTrack(m_szPendingPlayFilename, looping != 0);
		m_szPendingPlayFilename[0] = '\0';
		return;
	}

	m_szPendingPlayFilename[0] = '\0';

	_GetAudioDiskInfo(0, 0);

	if (!m_bIsCDValid)
	{
		MP3_PlayTrack(track, looping != 0);
		return;
	}

	if (track > m_nMaxCDTrack)
		MP3_PlayTrack(track, looping != 0);

	track = m_rgRemapCD[track];

	if (track < 1)
		return;

	// don't try to play a non-audio track
	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = track;

	dwReturn = mciSendCommand(m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms);

	if (dwReturn)
	{
		char szErr[256];
		int nErr = 256;

		mciGetErrorString(dwReturn, szErr, nErr);
		return;
	}

	if (mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO)
	{
		return;
	}

	// get the length of the track to be played
	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = track;

	dwReturn = mciSendCommand(m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms);

	if (dwReturn)
	{
		return;
	}

	if (m_bIsPlaying)
	{
		if (m_nPlayTrack == track)
			return;

		_Stop(0, 0);
	}

	dwm_flPlayTime = mciStatusParms.dwReturn;

	mciPlayParms.dwFrom = MCI_MAKE_TMSF(track, 0, 0, 0);
	mciPlayParms.dwTo = (mciStatusParms.dwReturn << 8) | track;
	mciPlayParms.dwCallback = (DWORD)GetMainWindow();

	dwReturn = mciSendCommand(m_uiDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD)(LPVOID)&mciPlayParms);

	if (dwReturn)
	{
		return;
	}

	// Clear any old data.
	ResetCDTimes();

	m_dStartTime = realtime;

	mins = MCI_MSF_MINUTE(dwm_flPlayTime);
	secs = MCI_MSF_SECOND(dwm_flPlayTime);

	m_flPlayTime = (float)(60.0f * mins + (float)secs);

	m_bIsLooping = looping ? true : false;
	m_nPlayTrack = track;
	m_bIsPlaying = true;

	if (m_flVolume == 0.0)
		_Pause(0, 0);
}

void CCDAudio::_PrimeTrack( int track, int looping )
{
	m_bIsInMiddleOfPriming = true;

	if (track)
	{
		if (track > 1 && track < g_iMP3NumTracks)
		{
			MP3_InitStream(track, looping != 0);
			m_bIsPrimed = true;
		}
	}
	else
	{
		if (strlen(m_szPendingPlayFilename))
		{
			MP3_InitStream(m_szPendingPlayFilename, looping != 0);
			m_bIsPrimed = true;
		}
	}

	m_bIsInMiddleOfPriming = false;
}

// Resume playing cd
void CCDAudio::_Resume( int, int )
{
	double curTime;
	DWORD			dwReturn;
	MCI_PLAY_PARMS	mciPlayParms;

	if (!m_bEnabled)
		return;

	if (!m_bWasPlaying)
		return;

	if (m_MP3.inuse)
	{
		MP3_SetPause(false);
	}
	else
	{
		mciPlayParms.dwFrom = MCI_MAKE_TMSF(m_nPlayTrack, 0, 0, 0);
		mciPlayParms.dwTo = MCI_MAKE_TMSF(m_nPlayTrack + 1, 0, 0, 0);
		mciPlayParms.dwCallback = (DWORD)GetMainWindow();

		dwReturn = mciSendCommand(m_uiDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD_PTR)&mciPlayParms);

		if (dwReturn)
		{
			ResetCDTimes();
			return;
		}
	}

	m_bIsPlaying = true;

	if (!m_MP3.inuse)
	{
		curTime = Sys_FloatTime();

		// Subtract the elapsed time from the current playback time. (i.e., add it to the start time).

		m_dStartTime += (curTime - m_dPauseTime);

		m_dPauseTime = 0.0;
	}
}

// Save state when going into launcher
void CCDAudio::_SwitchToLauncher( int, int )
{
	if (m_bEnabled && (m_bIsPlaying != false))
	{
		m_bResumeOnSwitch = true;
		// Pause device
		_Pause(0, 0);
	}
}

// Restore cd playback if needed
void CCDAudio::_SwitchToEngine( int, int )
{
	if (m_bResumeOnSwitch)
	{
		m_bResumeOnSwitch = false;
		_Resume(0, 0);
	}
}

// Handle "cd" console command
void CCDAudio::CD_f( void )
{
	if (Cmd_Argc() < 2)
		return;

	char* command;
	int		ret;
	int		n;

	command = (char*)Cmd_Argv(1);

	if (stricmp(command, "on") == 0)
	{
		m_bEnabled = true;
		return;
	}

	if (stricmp(command, "off") == 0)
	{
		if (m_bIsPlaying)
			Stop();

		m_bEnabled = false;
		return;
	}

	if (stricmp(command, "reset") == 0)
	{
		m_bEnabled = true;

		if (m_bIsPlaying)
			Stop();

		for (n = 0; n < 100; n++)
		{
			m_rgRemapCD[n] = n;
		}

		GetAudioDiskInfo();
		return;
	}

	if (stricmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;

		if (ret > 0)
		{
			for (n = 1; n <= ret; n++)
			{
				m_rgRemapCD[n] = atoi(Cmd_Argv(n + 1));
			}
		}
		return;
	}

	if (stricmp(command, "close") == 0)
	{
		CloseDoor();
		return;
	}

	if (stricmp(command, "mp3info") == 0)
	{
		Con_Printf(const_cast<char*>("Current MP3 Title: %s\n"), m_MP3.trackname);
		Con_Printf(const_cast<char*>("Current MP3 Track: %i\n"), m_MP3.tracknum);
		Con_Printf(const_cast<char*>("Current MP3 Volume: %i\n"), (int)m_MP3.volume);
		return;
	}

	if (Cmd_Argc() > 2)
	{
		if (stricmp(command, "mp3track") == 0)
		{
			n = atoi(Cmd_Argv(2)) + 1;

			if (MP3_PlayTrack(n, false))
			{
				ResetCDTimes();

				m_nPlayTrack = n;
				m_bIsLooping = false;
				m_bIsPlaying = true;
			}
			return;
		}
	}

	if (stricmp(command, "play") == 0)
	{
		Play(atoi(Cmd_Argv(2)), false);
		return;
	}

	if (stricmp(command, "playfile") == 0)
	{
		PlayFile(Cmd_Argv(2), false);
		return;
	}

	if (stricmp(command, "loop") == 0)
	{
		Play(atoi(Cmd_Argv(2)), true);
		return;
	}

	if (stricmp(command, "loopfile") == 0)
	{
		PlayFile(Cmd_Argv(2), true);
		return;
	}

	if (stricmp(command, "stop") == 0)
	{
		Stop();
		return;
	}

	if (stricmp(command, "fadeout") == 0)
	{
		FadeOut();
		return;
	}

	if (stricmp(command, "pause") == 0)
	{
		Pause();
		return;
	}

	if (stricmp(command, "resume") == 0)
	{
		Resume();
		return;
	}

	if (stricmp(command, "eject") == 0)
	{
		if (m_bIsPlaying)
			Stop();

		Eject();
		m_bIsCDValid = false;
		return;
	}

	if (stricmp(command, "info") == 0)
	{
		Con_Printf(const_cast<char*>("%u tracks\n"), g_iMP3NumTracks - 1);

		if (m_bIsPlaying)
		{
			Con_Printf(const_cast<char*>("Currently %s track %u\n"), m_bIsLooping ? "looping" : "playing", m_nPlayTrack);
		}
		else if (m_bWasPlaying)
		{
			Con_Printf(const_cast<char*>("Paused %s track %u\n"), m_bIsLooping ? "looping" : "playing", m_nPlayTrack);
		}

		Con_Printf(const_cast<char*>("Volume is %f\n"), m_flVolume);
		return;
	}
}

// Frame update
void CCDAudio::_CDUpdate( int, int )
{
	if (!m_bIsPlaying)
		return;

	double curVolume;
	double current;

	curVolume = MP3Volume.value;

	if (m_dFadeOutTime != 0.0)
	{
		current = Sys_FloatTime();

		if (current < m_dFadeOutTime)
		{
			curVolume = (m_dFadeOutTime - current) / MP3FadeTime.value * MP3Volume.value;
		}
		else
		{
			_Stop(0, 0);
			m_dFadeOutTime = 0;
		}

		MP3_SetVolume(curVolume);

		if (thread->AddThreadItem(&CCDAudio::_CDUpdate, 0, 0))
		{
			Sleep(0);
		}
	}

	if (m_MP3.inuse)
	{
		m_bIsPlaying = AIL_stream_status(MP3stream) == SEQ_PLAYING;

		if (!m_bIsPlaying)
			m_szPendingPlayFilename[0] = '\0';
	}

	// Make sure we set a valid track length.
	if (!m_flPlayTime || (m_dStartTime == 0.0))
		return;

	if ((float)(realtime - m_dStartTime) >= (m_flPlayTime + TRACK_EXTRA_TIME))
	{
		if (m_bIsPlaying)
		{
			m_bIsPlaying = false;
			m_szPendingPlayFilename[0] = '\0';

			if (m_bIsLooping)
				Play(m_nPlayTrack, true);
		}
	}
}

// Initialize cd audio device handling
int CCDAudio::Init( void )
{
	m_MP3.inuse = false;
	m_MP3.suspended = false;
	m_MP3.playing = false;

	m_MP3.trackname[0] = '\0';
	m_MP3.tracknum = 0;
	m_MP3.looping = 0;
	m_MP3.volume = 100.0;

	memset(g_pszMP3trackFileMap, 0, sizeof(g_pszMP3trackFileMap));

	// Initialize Half-Life 1 Soundtracks
	g_pszMP3trackFileMap[0] = const_cast<char*>("");
	g_pszMP3trackFileMap[1] = const_cast<char*>("");
	g_pszMP3trackFileMap[2] = const_cast<char*>("media\\Half-Life01.mp3");
	g_pszMP3trackFileMap[3] = const_cast<char*>("media\\Prospero01.mp3");
	g_pszMP3trackFileMap[4] = const_cast<char*>("media\\Half-Life12.mp3");
	g_pszMP3trackFileMap[5] = const_cast<char*>("media\\Half-Life07.mp3");
	g_pszMP3trackFileMap[6] = const_cast<char*>("media\\Half-Life10.mp3");
	g_pszMP3trackFileMap[7] = const_cast<char*>("media\\Suspense01.mp3");
	g_pszMP3trackFileMap[8] = const_cast<char*>("media\\Suspense03.mp3");
	g_pszMP3trackFileMap[9] = const_cast<char*>("media\\Half-Life09.mp3");
	g_pszMP3trackFileMap[10] = const_cast<char*>("media\\Half-Life02.mp3");
	g_pszMP3trackFileMap[11] = const_cast<char*>("media\\Half-Life13.mp3");
	g_pszMP3trackFileMap[12] = const_cast<char*>("media\\Half-Life04.mp3");
	g_pszMP3trackFileMap[13] = const_cast<char*>("media\\Half-Life15.mp3");
	g_pszMP3trackFileMap[14] = const_cast<char*>("media\\Half-Life14.mp3");
	g_pszMP3trackFileMap[15] = const_cast<char*>("media\\Half-Life16.mp3");
	g_pszMP3trackFileMap[16] = const_cast<char*>("media\\Suspense02.mp3");
	g_pszMP3trackFileMap[17] = const_cast<char*>("media\\Half-Life03.mp3");
	g_pszMP3trackFileMap[18] = const_cast<char*>("media\\Half-Life08.mp3");
	g_pszMP3trackFileMap[19] = const_cast<char*>("media\\Prospero02.mp3");
	g_pszMP3trackFileMap[20] = const_cast<char*>("media\\Half-Life05.mp3");
	g_pszMP3trackFileMap[21] = const_cast<char*>("media\\Prospero04.mp3");
	g_pszMP3trackFileMap[22] = const_cast<char*>("media\\Half-Life11.mp3");
	g_pszMP3trackFileMap[23] = const_cast<char*>("media\\Half-Life06.mp3");
	g_pszMP3trackFileMap[24] = const_cast<char*>("media\\Prospero03.mp3");
	g_pszMP3trackFileMap[25] = const_cast<char*>("media\\Half-Life17.mp3");
	g_pszMP3trackFileMap[26] = const_cast<char*>("media\\Prospero05.mp3");
	g_pszMP3trackFileMap[27] = const_cast<char*>("media\\Suspense05.mp3");
	g_pszMP3trackFileMap[28] = const_cast<char*>("media\\Suspense07.mp3");

	g_iMP3FirstMalloc = 29;
	g_iMP3NumTracks = 29;

	ResetCDTimes();

	// Initialize MP3
	if (MP3_Init())
	{
		MP3_SetVolume(MP3Volume.value);
	}

	// Mark system as initialized
	m_bInitialized = true;

	m_bEnabled = true;

	if (COM_CheckParm(const_cast<char*>("-nocdaudio")) || COM_CheckParm(const_cast<char*>("-nosound")))
	{
		m_bEnabled = false;
	}

	thread->AddThreadItem(&CCDAudio::_Init, 0, 0);
	return 0;
}

// Initialize cd audio device handling
void CCDAudio::_Init( int, int )
{
	DWORD	dwReturn;
	MCI_OPEN_PARMS	mciOpenParms;
	MCI_SET_PARMS	mciSetParms;
	int				n;

	if (COM_CheckParm(const_cast<char*>("-nocdaudio")))
	{
		return;
	}

	mciOpenParms.lpstrDeviceType = "cdaudio";

	if (dwReturn = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD)(LPVOID)&mciOpenParms))
	{
		return;
	}
	m_uiDeviceID = mciOpenParms.wDeviceID;

	// Set the time format to track/minute/second/frame (TMSF).
	mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
	if (dwReturn = mciSendCommand(m_uiDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)(LPVOID)&mciSetParms))
	{
		mciSendCommand(m_uiDeviceID, MCI_CLOSE, 0, (DWORD)NULL);
		return;
	}

	for (n = 0; n < 100; n++)
	{
		m_rgRemapCD[n] = n;
	}

	// set m_bIsValid flag
	GetAudioDiskInfo();
}

// Shutdown cd audio device
void CCDAudio::Shutdown( void )
{
	if (m_MP3.inuse)
	{
		MP3_SetPause(true);
		MP3_StopStream();
	}

	MP3_Shutdown();

	if (!m_bInitialized)
		return;

	Stop();
	mciSendCommand(m_uiDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD)NULL);

	ResetCDTimes();
}

// Shutown and restart system
void CCDAudio::_CDReset( int, int )
{
	Shutdown();
	Sleep(1000);
	Init();
}

void CCDAudio::Reset( void )
{
	thread->AddThreadItem(&CCDAudio::_CDReset, 0, 0);
}

void CCDAudio::Stop( void )
{
	thread->AddThreadItem(&CCDAudio::_Stop, 0, 0);
}

void CCDAudio::Pause( void )
{
	if (m_bInitialized)
		thread->AddThreadItem(&CCDAudio::_Pause, 0, 0);
}

void CCDAudio::Eject( void )
{
	thread->AddThreadItem(&CCDAudio::_Eject, 0, 0);
}

void CCDAudio::CloseDoor( void )
{
	thread->AddThreadItem(&CCDAudio::_CloseDoor, 0, 0);
}

void CCDAudio::GetAudioDiskInfo( void )
{
	thread->AddThreadItem(&CCDAudio::_GetAudioDiskInfo, 0, 0);
}

void CCDAudio::Play( int track, bool looping )
{
	m_dFadeOutTime = 0.0;
	thread->AddThreadItem(&CCDAudio::_Play, track, looping ? 1 : 0);
}

void CCDAudio::PlayFile( const char* filename, bool looping )
{
	m_dFadeOutTime = 0.0;
	strncpy(m_szPendingPlayFilename, filename, sizeof(m_szPendingPlayFilename));

	thread->AddThreadItem(&CCDAudio::_Play, 0, looping);

	Sleep(0);
}

void CCDAudio::PrimeTrack( char* filename, int looping )
{
	DWORD startTime, curTime;

	m_dFadeOutTime = 0.0;
	strncpy(m_szPendingPlayFilename, filename, sizeof(m_szPendingPlayFilename));

	thread->AddThreadItem(&CCDAudio::_PrimeTrack, 0, looping);

	Sleep(0);

	startTime = timeGetTime();

	for (curTime = startTime; m_bIsInMiddleOfPriming; startTime = timeGetTime())
	{
		if ((int)(startTime - curTime) >= 2000)
			break;

		Sleep(0);
	}
}

void CCDAudio::Resume( void )
{
	if (m_bInitialized)
		thread->AddThreadItem(&CCDAudio::_Resume, 0, 0);
}

void CCDAudio::SwitchToLauncher( void )
{
	thread->AddThreadItem(&CCDAudio::_SwitchToLauncher, 0, 0);
}

void CCDAudio::SwitchToEngine( void )
{
	thread->AddThreadItem(&CCDAudio::_SwitchToEngine, 0, 0);
}

void CCDAudio::FadeOut( void )
{
	if (m_bIsPlaying)
	{
		m_dFadeOutTime = MP3FadeTime.value + realtime;
		thread->AddThreadItem(&CCDAudio::_CDUpdate, 0, 0);
	}
}

void CCDAudio::Frame( void )
{
	if (!m_bEnabled)
		return;

	if (m_flVolume != bgmvolume.value)
	{
		if (m_flVolume)
		{
			m_flVolume = 0.0;
			Pause();
		}
		else
		{
			m_flVolume = 1.0;
			Resume();
		}

		Cvar_DirectSet(&bgmvolume, va(const_cast<char*>("%f"), m_flVolume));
	}

	if (m_dFadeOutTime == 0.0 && m_flMP3Volume != MP3Volume.value)
		m_flMP3Volume = MP3_SetVolume(MP3Volume.value);

	thread->AddThreadItem(&CCDAudio::_CDUpdate, 0, 0);
}

void CDAudio_Init( void )
{
	thread->Init();
}

void CDAudio_Shutdown( void )
{
	thread->Shutdown();
}

void CDAudio_Play( int track, int looping )
{
	cdaudio->Play(track, looping != 0);
}

void CDAudio_Pause( void )
{
	cdaudio->Pause();
}

void CDAudio_Resume( void )
{
	cdaudio->Resume();
}

void MP3_Resume_Audio( void )
{
	cdaudio->MP3_Resume_Audio();
}

void MP3_Suspend_Audio( void )
{
	cdaudio->MP3_Suspend_Audio();
}

// Start to play MP3 Track
bool CCDAudio::MP3_PlayTrack( int trackNum, bool looping )
{
	if (trackNum < 2 || trackNum >= g_iMP3NumTracks)
		return false;

	if (!m_bIsPrimed && !MP3_InitStream(trackNum, looping))
		return false;

	m_bIsPrimed = false;

	MP3_PlayTrackFinalize(trackNum, looping);
	return true;
}

// Start to play MP3 Track (by filename)
bool CCDAudio::MP3_PlayTrack( const char* filename, bool looping )
{
	if (!m_bIsPrimed && !MP3_InitStream(filename, looping))
		return false;

	m_bIsPrimed = false;

	MP3_PlayTrackFinalize(0, looping);
	return true;
}

void CCDAudio::MP3_PlayTrackFinalize( int trackNum, bool looping )
{
	MP3_StartStream();

	m_MP3.inuse = true;
	m_MP3.playing = true;
	m_MP3.tracknum = trackNum;
	m_MP3.looping = looping;

	ResetCDTimes();

	m_bIsLooping = looping;
	m_nPlayTrack = trackNum;
	m_bIsPlaying = true;

	if (m_flMP3Volume == 0.0)
		_Pause(0, 0);
}

// Initialize MP3
bool CCDAudio::MP3_Init( void )
{
	MP3_ReleaseDriver();

	AIL_set_redist_directory(".");

	AIL_startup();
	AIL_set_preference(DIG_DS_SAMPLE_CEILING, 0);
	AIL_set_preference(MDI_QUANT_ADVANCE, 10);

	MP3digitalDriver = AIL_open_digital_driver(22050, 16, 2, AIL_OPEN_DIGITAL_FORCE_PREFERENCE);

	return MP3digitalDriver != 0;
}

void CCDAudio::MP3_Shutdown( void )
{
	int i;

	MP3_StopStream();
	MP3_ReleaseDriver();

	// Shutdown MSS
	AIL_shutdown();

	// Clear MP3 map
	for (i = g_iMP3FirstMalloc; i < ARRAYSIZE(g_pszMP3trackFileMap); i++)
	{
		if (g_pszMP3trackFileMap[i])
			Mem_Free(g_pszMP3trackFileMap[i]);
	}
}

void CCDAudio::MP3_Resume_Audio( void )
{
	MP3_Init();

	if (m_MP3.playing && m_MP3.suspended)
	{
		if (m_MP3.tracknum)
			MP3_PlayTrack(m_MP3.tracknum, m_MP3.looping);
		else
			MP3_PlayTrack(m_MP3.trackname, m_MP3.looping);
	}
}

void CCDAudio::MP3_Suspend_Audio( void )
{
	m_MP3.playing = m_bIsPlaying;

	if (m_MP3.playing && MP3stream)
	{
		m_MP3.suspended = true;
	}

	MP3_StopStream();
	MP3_ReleaseDriver();
}

void CCDAudio::MP3_ReleaseDriver( void )
{
	if (MP3digitalDriver)
	{
		AIL_close_digital_driver(MP3digitalDriver);
		MP3digitalDriver = NULL;
	}
}

float CCDAudio::MP3_SetVolume( float NewVol )
{
	// Clamp to [0..1] range
	NewVol = clamp(NewVol, 0.0f, 1.0f);

	if (m_MP3.volume == NewVol)
		return m_MP3.volume;

	m_MP3.volume = NewVol;

	if (MP3stream)
	{
		AIL_set_sample_volume_pan(AIL_stream_sample_handle(MP3stream), m_MP3.volume, 0.5);
	}

	return m_MP3.volume;
}

bool CCDAudio::MP3_InitStream( int trackNum, bool looping )
{
	m_bIsPrimed = false;

	if (!MP3digitalDriver && !MP3_Init())
		return false;

	char fullPath[512];
	FS_GetLocalPath(g_pszMP3trackFileMap[trackNum], fullPath, sizeof(fullPath));
	FS_GetLocalCopy(fullPath);

	_Stop(0, 0);

	m_dFadeOutTime = 0.0;

	MP3stream = AIL_open_stream(MP3digitalDriver, fullPath, 0);

	if (!MP3stream)
	{
		Con_DPrintf(const_cast<char*>("warning: MP3_InitStream(%d, %s) failed\n"), trackNum, g_pszMP3trackFileMap[trackNum]);
		return false;
	}
	
	AIL_set_stream_user_data(MP3stream, 0, AIL_sample_playback_rate(AIL_stream_sample_handle(MP3stream)));
	AIL_set_sample_volume_pan(AIL_stream_sample_handle(MP3stream), m_MP3.volume, 0.5);

	strcpy(m_MP3.trackname, g_pszMP3trackFileMap[trackNum]);

	if (looping)
		MP3_Loop();

	Con_DPrintf(const_cast<char*>("MP3_InitStream(%d, %s) successful\n"), trackNum, g_pszMP3trackFileMap[trackNum]);
	return true;
}

bool CCDAudio::MP3_InitStream( const char* filename, bool looping )
{
	if (!filename || !strlen(filename))
		return false;

	int iTrackNum = MP3_GetTrack(filename);
	return MP3_InitStream(iTrackNum, looping);
}

void CCDAudio::MP3_StartStream( void )
{
	if (MP3stream)
		AIL_start_stream(MP3stream);
}

void CCDAudio::MP3_StopStream( void )
{
	if (MP3stream)
	{
		AIL_pause_stream(MP3stream, 1);
		AIL_close_stream(MP3stream);
		MP3stream = NULL;
	}

	m_bIsPrimed = false;
}

void CCDAudio::MP3_Loop( void )
{
	if (MP3stream)
		AIL_set_stream_loop_count(MP3stream, 0);
}

void CCDAudio::MP3_SetPause( bool OnOff )
{
	if (MP3stream)
		AIL_pause_stream(MP3stream, OnOff);
}