#include <csetjmp>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <SDL2/SDL.h>

#include "quakedef.h"
#include "client.h"
#include "dll_state.h"
#include "server.h"
#include "host.h"
#include "pr_edict.h"
#include "pr_cmds.h"
#include "Sequence.h"
#include "vgui_int.h"
#include "sv_move.h"
#include "r_studio.h"

#include "FilePaths.h"
#include "qgl.h"
#include "modinfo.h"
#include "sys_getmodes.h"
#include "mathlib.h"

#ifdef WIN32
#include "winheaders.h"
#else
#error
#include <dlfcn.h>
#include <unistd.h>
#endif

BOOL gHasMMXTechnology = FALSE;

FlipScreenFn VID_FlipScreen = nullptr;

qboolean gfBackground = false;

int giActive = DLL_INACTIVE;
int giStateInfo = 0;
int giSubState = 0;

qboolean gfExtendedError = false;
char gszDisconnectReason[ 256 ] = {};
char gszExtendedDisconnectReason[ 256 ] = {};

DLL_FUNCTIONS gEntityInterface = {};
NEW_DLL_FUNCTIONS gNewDLLFunctions = {};

static enginefuncs_t g_engfuncsExportedToDlls =
{
	PF_precache_model_I, PF_precache_sound_I,
	PF_setmodel_I, PF_modelindex,
	ModelFrames, PF_setsize_I,
	PF_changelevel_I, PF_setspawnparms_I,
	SaveSpawnParms, PF_vectoyaw_I,
	PF_vectoangles_I, SV_MoveToOrigin_I,
	PF_changeyaw_I, PF_changepitch_I,
	FindEntityByString, GetEntityIllum,
	FindEntityInSphere, PF_checkclient_I,
	PVSFindEntities, PF_makevectors_I,
	AngleVectors, PF_Spawn_I,
	PF_Remove_I, CreateNamedEntity,
	PF_makestatic_I, PF_checkbottom_I,
	PF_droptofloor_I, PF_walkmove_I,
	PF_setorigin_I, PF_sound_I,
	PF_ambientsound_I, PF_traceline_DLL,
	PF_TraceToss_DLL, TraceMonsterHull,
	TraceHull, TraceModel,
	TraceTexture, TraceSphere,
	PF_aim_I, PF_localcmd_I,
	PF_localexec_I, PF_stuffcmd_I,
	PF_particle_I, PF_lightstyle_I,
	PF_DecalIndex, PF_pointcontents_I,
	PF_MessageBegin_I, PF_MessageEnd_I,
	PF_WriteByte_I, PF_WriteChar_I,
	PF_WriteShort_I, PF_WriteLong_I,
	PF_WriteAngle_I, PF_WriteCoord_I,
	PF_WriteString_I, PF_WriteEntity_I,
	CVarRegister, CVarGetFloat,
	CVarGetString, CVarSetFloat,
	CVarSetString, AlertMessage,
	EngineFprintf, PvAllocEntPrivateData,
	PvEntPrivateData, FreeEntPrivateData,
	SzFromIndex, AllocEngineString,
	GetVarsOfEnt, PEntityOfEntOffset,
	EntOffsetOfPEntity, IndexOfEdict,
	PEntityOfEntIndex, FindEntityByVars,
	GetModelPtr, RegUserMsg,
	AnimationAutomove, GetBonePosition,
	FunctionFromName, NameForFunction,
	ClientPrintf, ServerPrint,
	(decltype(enginefuncs_t::pfnCmd_Args))Cmd_Args, 
	(decltype(enginefuncs_t::pfnCmd_Argv))Cmd_Argv, Cmd_Argc,
	GetAttachment, CRC32_Init,
	CRC32_ProcessBuffer, CRC32_ProcessByte,
	CRC32_Final, RandomLong,
	RandomFloat, PF_setview_I,
	PF_Time, PF_crosshairangle_I,
	COM_LoadFileForMe, COM_FreeFile,
	Host_EndSection, COM_CompareFileTime,
	COM_GetGameDir, Cvar_RegisterVariable,
	PF_FadeVolume, PF_SetClientMaxspeed,
	PF_CreateFakeClient_I,
	PF_RunPlayerMove_I,
	PF_NumberOfEntities_I,
	PF_GetInfoKeyBuffer_I, PF_InfoKeyValue_I,
	PF_SetKeyValue_I, PF_SetClientKeyValue_I,
	PF_IsMapValid_I, PF_StaticDecal,
	PF_precache_generic_I,
	PF_GetPlayerUserId, PF_BuildSoundMsg_I,
	PF_IsDedicatedServer, CVarGetPointer,
	PF_GetPlayerWONId, PF_RemoveKey_I,
	PF_GetPhysicsKeyValue,
	PF_SetPhysicsKeyValue,
	PF_GetPhysicsInfoString, EV_Precache,
	EV_Playback, SV_FatPVS, SV_FatPAS,
	SV_CheckVisibility, DELTA_SetField,
	DELTA_UnsetField, DELTA_AddEncoder,
	PF_GetCurrentPlayer, PF_CanSkipPlayer,
	DELTA_FindFieldIndex,
	DELTA_SetFieldByIndex,
	DELTA_UnsetFieldByIndex, PF_SetGroupMask,
	PF_CreateInstancedBaseline,
	PF_Cvar_DirectSet, PF_ForceUnmodified,
	PF_GetPlayerStats, Cmd_AddGameCommand,
	Voice_GetClientListening,
	Voice_SetClientListening,
	PF_GetPlayerAuthId, SequenceGet,
	SequencePickSentence, COM_FileSize,
	COM_GetApproxWavePlayLength,
	(decltype(enginefuncs_t::pfnIsCareerMatch))VGuiWrap2_IsInCareerMatch,
	VGuiWrap2_GetLocalizedStringLength,
	RegisterTutorMessageShown,
	GetTimesTutorMessageShown,
	ProcessTutorMessageDecayBuffer,
	ConstructTutorMessageDecayBuffer,
	ResetTutorMessageDecayData,
	QueryClientCvarValue, QueryClientCvarValue2,
	EngCheckParm
};

PrintfFunc Launcher_ConsolePrintf = nullptr;

// Prototype of an global method function
typedef void (* PFN_GlobalMethod)( edict_t *pEntity );

int g_iextdllMac = 0;
extensiondll_t g_rgextdll[ MAX_EXT_DLLS ] = {};

static FileFindHandle_t g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;

qboolean s_bTimeInitialized = false;
static double curtime = 0.0;
static double lastcurtime = 0.0;

#ifdef WIN32
double pfreq = 0;
int lowshift = 0;
CRITICAL_SECTION s_Time_CriticalSection;

#ifdef _M_IX86
static long ceil_cw = 0;
static long single_cw = 0;
static long full_cw = 0;
static long cw = 0;
static long pushed_cw = 0;
#endif
#endif

void Sys_InitFloatTime();
qboolean _isdigit(char ch);

extern void LoadDllExports(extensiondll_t* extdll, char* pszFileName);
void LoadThisDll(char* szDllFilename);

#ifdef WIN32
#define GIVEFNPTRSTODLL_CALLCONV __stdcall
#else
#define GIVEFNPTRSTODLL_CALLCONV
#endif

#ifdef WIN32
#define GAMEDLL_KEY "gamedll"
#else
#define GAMEDLL_KEY "gamedll_linux"
#endif

typedef void(GIVEFNPTRSTODLL_CALLCONV* GiveFnptrsToDllFn)(enginefuncs_t* pengfuncsFromEngine, globalvars_t* pGlobals);


void Sys_PageIn(void* ptr, int size)
{
	//Obsolete due to vastly increased available memory & I/O speeds
}

const char* Sys_FindFirst(const char* path, char* basename)
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
		Sys_Error("Sys_FindFirst without close");

	const char* result = FS_FindFirst(path, &g_hfind, nullptr);

	if (result && basename)
	{
		COM_FileBase(const_cast<char*>(result), basename);
	}

	return result;
}

const char* Sys_FindFirstPathID(const char* path, char* pathid)
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
		Sys_Error("Sys_FindFirst without close");

	return FS_FindFirst(path, &g_hfind, pathid);
}

const char* Sys_FindNext(char* basename)
{
	const char* result = FS_FindNext(g_hfind);

	if (result && basename)
	{
		COM_FileBase(const_cast<char*>(result), basename);
	}

	return result;
}

void Sys_FindClose()
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		FS_FindClose(g_hfind);
		g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;
	}
}

void Sys_Quit()
{
	giActive = DLL_CLOSE;
}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
#if defined(WIN32) && !defined(_WIN64)
	DWORD  flOldProtect;
	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_EXECUTE_READWRITE, &flOldProtect))
		Sys_Error("Protection change failed\n");
#endif
}

void _Sys_Init()
{
#ifdef WIN32
	if (!s_bTimeInitialized)
	{
		InitializeCriticalSection(&s_Time_CriticalSection);
		s_bTimeInitialized = true;
	}

	MaskExceptions();
	Sys_SetFPCW();

	LARGE_INTEGER	PerformanceFreq;

	if (!QueryPerformanceFrequency(&PerformanceFreq))
		Sys_Error("No hardware timer available");

	// get 32 out of the 64 time bits such that we have around
	// 1 microsecond resolution
	unsigned int lowpart = (unsigned int)PerformanceFreq.LowPart;
	unsigned int highpart = (unsigned int)PerformanceFreq.HighPart;
	lowshift = 0;

	while (highpart || (lowpart > 2000000.0))
	{
		lowshift++;
		lowpart >>= 1;
		lowpart |= (highpart & 1) << 31;
		highpart >>= 1;
	}

	pfreq = 1.0 / (double)lowpart;
#endif
	Sys_InitFloatTime();
}

void Sys_Sleep(int msec)
{
#ifdef WIN32
	Sleep(msec);
#else
	usleep(1000 * msec);
#endif
}

void Sys_DebugOutStraight(const char* pStr)
{
#ifdef WIN32
	OutputDebugString(pStr);
#else
	fprintf(stderr, "%s\n", pStr);
#endif
}

void Sys_Error(const char* error, ...)
{
	static qboolean bReentry = false;

	char text[1024];

	va_list va;

	va_start(va, error);
	vsnprintf(text, sizeof(text), error, va);
	va_end(va);

	if (bReentry == false)
	{
		bReentry = true;

		if (svs.dll_initialized)
		{
			if (gEntityInterface.pfnSys_Error)
				gEntityInterface.pfnSys_Error(text);
		}

		Log_Printf("FATAL ERROR (shutting down): %s\n", text);

		if (g_bIsDedicatedServer)
		{
			if (Launcher_ConsolePrintf)
				Launcher_ConsolePrintf("FATAL ERROR (shutting down): %s\n", text);
			else
				printf("FATAL ERROR (shutting down): %s\n", text);
		}
		else
		{
			Sys_Printf(text);
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", text, pmainwindow);
			VideoMode_IsWindowed();
		}
		exit(-1);
	}

	fprintf(stderr, "%s\n", text);

	longjmp(host_abortserver, 2);
}

void Sys_Warning(const char* pszWarning, ...)
{
	char text[1024];
	va_list va;

	va_start(va, pszWarning);
	vsnprintf(text, ARRAYSIZE(text), pszWarning, va);
	va_end(va);

	Sys_Printf(text);
}

void Sys_Printf(const char* fmt, ...)
{
	char text[1024];
	va_list va;

	va_start(va, fmt);
	vsnprintf(text, sizeof(text), fmt, va);
	va_end(va);

	if (g_bIsDedicatedServer && Launcher_ConsolePrintf)
	{
		Launcher_ConsolePrintf("%s", text);
	}

	fprintf(stderr, "%s\n", text);
}

double Sys_FloatTime()
{
#ifdef WIN32
	static int			sametimecount;
	static unsigned int	oldtime;
	static qboolean		first = true;
	LARGE_INTEGER		PerformanceCount;
	unsigned int		temp;

	if (!s_bTimeInitialized)
		return 1.0;

	EnterCriticalSection(&s_Time_CriticalSection);

	Sys_PushFPCW_SetHigh();

	QueryPerformanceCounter(&PerformanceCount);

	if (lowshift)
	{
		temp = ((unsigned int)PerformanceCount.LowPart >> lowshift) |
			((unsigned int)PerformanceCount.HighPart << (32 - lowshift));
	}
	else
	{
		temp = (unsigned int)PerformanceCount.LowPart;
	}

	if (first)
	{
		oldtime = temp;
		first = false;
	}
	else
	{
		// check for turnover or backward time
		if ((temp <= oldtime) && ((oldtime - temp) < 0x10000000))
		{
			oldtime = temp;	// so we can't get stuck
		}
		else
		{
			unsigned int t2 = temp - oldtime;

			double time = (double)t2 * pfreq;
			oldtime = temp;

			curtime += time;

			if (curtime == lastcurtime)
			{
				sametimecount++;

				if (sametimecount > 100000)
				{
					curtime += 1.0;
					sametimecount = 0;
				}
			}
			else
			{
				sametimecount = 0;
			}

			lastcurtime = curtime;
		}
	}

	Sys_PopFPCW();

	LeaveCriticalSection(&s_Time_CriticalSection);

	return curtime;
#else
	static timespec start_time;

	timespec now;

	if (!s_bTimeInitialized)
	{
		s_bTimeInitialized = true;
		clock_gettime(1, &start_time);
	}

	clock_gettime(1, &now);

	return (double)(now.tv_sec - start_time.tv_sec) + now.tv_nsec / 1000000.0;
#endif
}

void Sys_InitFloatTime()
{
	Sys_FloatTime();

	int j = COM_CheckParm(const_cast<char*>("-starttime"));

	if (j)
	{
		curtime = Q_atof((char*)com_argv[j + 1]);
	}
	else
	{
		curtime = 0;
	}

	lastcurtime = curtime;
}

void Sys_ShutdownFloatTime()
{
	lastcurtime = 0.0;
	curtime = 0.0;
}

void Dispatch_Substate(int iSubState)
{
	giSubState = iSubState;
}

void GameSetSubState(int iSubState)
{
	if (iSubState & 2)
	{
		Dispatch_Substate(ENG_NORMAL);
	}
	else if (iSubState != ENG_NORMAL)
	{
		Dispatch_Substate(iSubState);
	}
}

void GameSetState(int iState)
{
	giActive = iState;
}

void GameSetBackground(bool bNewSetting)
{
	gfBackground = bNewSetting;
}



void ForceReloadProfile()
{
	Cbuf_AddText( const_cast<char*>("exec config.cfg\n") );
	Cbuf_AddText( const_cast<char*>("+mlook\n") );
	Cbuf_Execute();

	if( COM_CheckParm( const_cast<char*>("-nomousegrab") ) )
		Cvar_Set( const_cast<char*>("cl_mousegrab"), const_cast<char*>("0") );

	Key_SetBinding( '~', "toggleconsole" );
	Key_SetBinding( '`', "toggleconsole" );
	Key_SetBinding( K_ESCAPE, "cancelselect" );

	SDL_GL_SetSwapInterval( ( gl_vsync.value <= 0.0 ) - 1 );

	if( cls.state != ca_dedicated )
	{
		char szRate[ 32 ];
		Sys_GetRegKeyValue("Software\\Valve\\Steam", "Rate", szRate, sizeof(szRate), rate.string);
		Cvar_DirectSet( &rate, szRate );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves a value from the registry
// Input  : *pszSubKey - 
//			*pszElement - 
//			*pszReturnString - 
//			nReturnLength - 
//			*pszDefaultValue - 
//-----------------------------------------------------------------------------
#if defined(_WIN32)
void Sys_GetRegKeyValue(const char* pszKey, const char* pszElement, char* pszReturnString, int nReturnLength, const char* pszDefaultValue)
{
	LONG lResult;           // Registry function result code
	HKEY hKey;              // Handle of opened/created key
	char szBuff[128];       // Temp. rgba
	DWORD dwDisposition;    // Type of key opening event
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data
	HKEY rootKey;
	const char* pszSubKey;

	// Assume the worst
	_snprintf(pszReturnString, nReturnLength, "%s", pszDefaultValue);

	if (nReturnLength > 1024) return;

	pszSubKey = pszKey;

	if (!_strnicmp(pszKey, "HKEY_LOCAL_MACHINE", 18))
	{
		pszSubKey = &pszKey[19];
		rootKey = HKEY_LOCAL_MACHINE;
	}
	else
	{
		rootKey = HKEY_CURRENT_USER;
	}

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	lResult = RegCreateKeyEx(
		rootKey,	// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0,					// DWORD ulOptions,	  // reserved 
		const_cast<char*>("String"),			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation

	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		// Just Set the Values according to the defaults
		lResult = RegSetValueEx(hKey, pszElement, 0, REG_SZ, (CONST BYTE*)pszDefaultValue, Q_strlen(pszDefaultValue) + 1);
	}
	else
	{
		// We opened the existing key. Now go ahead and find out how big the key is.
		dwSize = nReturnLength;
		lResult = RegQueryValueEx(hKey, pszElement, 0, &dwType, (unsigned char*)szBuff, &dwSize);

		// Success?
		if (lResult == ERROR_SUCCESS)
		{
			// Only copy strings, and only copy as much data as requested.
			if (dwType == REG_SZ)
			{
				Q_strncpy(pszReturnString, szBuff, nReturnLength);
				pszReturnString[nReturnLength - 1] = '\0';
			}
		}
		else
			// Didn't find it, so write out new value
		{
			// Just Set the Values according to the defaults
			lResult = RegSetValueEx(hKey, pszElement, 0, REG_SZ, (CONST BYTE*)pszDefaultValue, Q_strlen(pszDefaultValue) + 1);
		}
	};

	// Always close this key before exiting.
	RegCloseKey(hKey);

}

qboolean Sys_QueryRegKeyValue(char* pszKey, char* pszPath)
{
	LONG lResult;           // Registry function result code
	HKEY hKey;              // Handle of opened/created key
	DWORD dwDisposition;    // Type of key opening event
	DWORD dwSize;

	HKEY hk = HKEY_CURRENT_USER;

	if (!Q_strcasecmp(pszKey, "HKEY_LOCAL_MACHINE"))
	{
		hk = HKEY_LOCAL_MACHINE;
		pszKey += 19;
	}

	lResult = RegOpenKeyEx(hk, pszKey, 0, KEY_ALL_ACCESS, &hKey);

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	lResult = RegQueryValueExA(hKey, pszPath, NULL, &dwDisposition, NULL, &dwSize);

	RegCloseKey(hKey);

	return (lResult != ERROR_SUCCESS) ? false : true;
}

void Sys_SetRegKeyValue(const char* pszSubKey, const char* pszElement, const char* pszDefaultValue)
{
	LONG lResult;           // Registry function result code
	HKEY hKey;              // Handle of opened/created key
	DWORD dwDisposition;    // Type of key opening event

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	lResult = RegCreateKeyEx(
		HKEY_CURRENT_USER,	// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0,					// DWORD ulOptions,	  // reserved 
		const_cast<char*>("String"),			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation

	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// Just Set the Values according to the defaults
	RegSetValueEx(hKey, pszElement, 0, REG_SZ, (CONST BYTE*)pszDefaultValue, Q_strlen(pszDefaultValue) + 1);

	// Always close this key before exiting.
	RegCloseKey(hKey);
}
#endif

qboolean Voice_GetClientListening(int iReceiver, int iSender)
{
	int iSenderIdx = iSender - 1;
	int iReceiverIdx = iReceiver - 1;

	if (iSenderIdx/*iReceiverIdx*/ < 0 || iSenderIdx >= svs.maxclients || iReceiverIdx < 0 || iReceiverIdx >= svs.maxclients)
		return false;
	else
		return (svs.clients[iReceiverIdx].m_VoiceStreams[iSenderIdx / MAX_CLIENTS] != 0) & (1 << (iSenderIdx & (MAX_CLIENTS - 1)));
}

qboolean Voice_SetClientListening(int iReceiver, int iSender, qboolean bListen)
{
	int iSenderIdx = iSender - 1;
	int iReceiverIdx = iReceiver - 1;

	if (iReceiverIdx < 0 || iReceiverIdx >= svs.maxclients || iSenderIdx < 0 || iSenderIdx >= svs.maxclients)
		return false;

	uint32* pStreams = &svs.clients[iSenderIdx].m_VoiceStreams[iReceiverIdx / MAX_CLIENTS];
	if (bListen)
		*pStreams |= 1 << (iReceiverIdx & (MAX_CLIENTS - 1));
	else
		*pStreams &= ~(1 << (iReceiverIdx & (MAX_CLIENTS - 1)));

	return true;
}

DISPATCHFUNCTION GetDispatch(char* pname)
{
	int i;
	DISPATCHFUNCTION pDispatch;

	for (i = 0; i < g_iextdllMac; i++)
	{
#ifdef _WIN32
		pDispatch = (DISPATCHFUNCTION)GetProcAddress((HMODULE)g_rgextdll[i].pDLLHandle, pname);
#else
		pDispatch = (DISPATCHFUNCTION)Sys_GetProcAddress((HMODULE)g_rgextdll[i].pDLLHandle, pname);
#endif
		if (pDispatch)
			return pDispatch;
	}

	return NULL;
}

char* FindAddressInTable(extensiondll_t* pDll, uint32 function)
{
#ifdef WIN32
	for (int i = 0; i < pDll->functionCount; ++i)
	{
		if (pDll->functionTable[i].pFunction == function)
			return pDll->functionTable[i].pFunctionName;
	}
#else
	Dl_info addrInfo;

	if (dladdr(function, &addrInfo))
		return addrInfo.dli_sname;
#endif

	return NULL;
}

uint32 FindNameInTable(extensiondll_t* pDll, const char* pName)
{
#ifdef WIN32
	for (int i = 0; i < pDll->functionCount; ++i)
	{
		if (!Q_strcmp((char*)pName, pDll->functionTable[i].pFunctionName))
			return pDll->functionTable[i].pFunction;
	}

	return 0;
#else
	return (uint32)(dlsym(pDll->pDLLHandle, pName));
#endif
}

//Used to fix up function names for saved games saved on another platform
const char* ConvertNameToLocalPlatform(const char* pchInName)
{
	static char s_szNewName[512];

	char szTempName[512];

#ifdef WIN32
	if (strstr(pchInName, "@"))
		return pchInName;

	if (*pchInName != '_' || pchInName[1] != 'Z')
	{
		return "unknown";
	}

	strncpy(szTempName, pchInName + 3, ARRAYSIZE(szTempName));
	szTempName[ARRAYSIZE(szTempName) - 1] = '\0';

	char* pszEnd = &szTempName[strlen(szTempName)];

	int iParamBytes = atol(szTempName);

	char* pszParams = szTempName;

	while (pszParams < pszEnd && _isdigit(*pszParams))
		pszParams++;

	char* pszParamEnd = &pszParams[iParamBytes];

	int iNameBytes = atoi(pszParamEnd);

	*pszParamEnd = '\0';

	char* pszName = &pszParamEnd[1];

	if (_isdigit(*pszName))
	{
		while (pszName < pszEnd && _isdigit(*pszName))
			pszName++;
	}

	pszName[iNameBytes] = '\0';

	snprintf(s_szNewName, ARRAYSIZE(s_szNewName), "%s@%s", pszName, pszParams);

	return s_szNewName;
#else
	if (*pchInName == '_' && pchInName[1] == 'Z')
		return pchInName;

	if (strchr(pchInName, '@'))
	{
		strncpy(szTempName, pchInName, ARRAYSIZE(szTempName));
		szTempName[ARRAYSIZE(szTempName) - 1] = '\0';

		char* pszAt = strchr(szTempName, '@');
		char* pszParams = pszAt + 1;
		*pszAt = '0';

		//Think functions
		snprintf(s_szNewName, ARRAYSIZE(s_szNewName), "_ZN%d%s%d%sEv", strlen(pszParams), pszParams, strlen(szTempName), szTempName);
		if (Sys_GetProcAddress(g_rgextdll[0].pDLLHandle, s_szNewName))
			return s_szNewName;

		//Touch/Blocked functions
		snprintf(s_szNewName, ARRAYSIZE(s_szNewName), "_ZN%d%s%d%sEP11CBaseEntity", strlen(pszParams), pszParams, strlen(szTempName), szTempName);
		if (Sys_GetProcAddress(g_rgextdll[0].pDLLHandle, s_szNewName))
			return s_szNewName;

		//Use functions
		snprintf(s_szNewName, ARRAYSIZE(s_szNewName), "_ZN%d%s%d%sEP11CBaseEntityS1_8USE_TYPEf", strlen(pszParams), pszParams, strlen(szTempName), szTempName);
		if (Sys_GetProcAddress(g_rgextdll[0].pDLLHandle, s_szNewName))
			return s_szNewName;
	}

	return "unknown";
#endif
}

#ifdef _WIN32
qboolean _isdigit(char ch)
{
	return (ch >= '0' && ch <= '9') ? true : false;
}
#endif

uint32 FunctionFromName(const char* pName)
{
	const char* pszName = ConvertNameToLocalPlatform(pName);

	uint32 function = 0;

	for (int i = 0; i < g_iextdllMac; ++i)
	{
		function = FindNameInTable(&g_rgextdll[i], pszName);

		if (function)
			return function;
	}

	Con_Printf(const_cast<char*>("Can't find proc: %s\n"), pszName);

	return 0;
}

const char* NameForFunction(uint32 function)
{
	const char* pszName = NULL;

	for (int i = 0; i < g_iextdllMac; ++i)
	{
		pszName = FindAddressInTable(&g_rgextdll[i], function);

		if (pszName)
			return pszName;
	}

	Con_Printf(const_cast<char*>("Can't find address: %08lx\n"), function);

	return NULL;
}

ENTITYINIT GetEntityInit(char* pClassName)
{
	return (ENTITYINIT)GetDispatch(pClassName);
}

FIELDIOFUNCTION GetIOFunction(char* pName)
{
	return (FIELDIOFUNCTION)GetDispatch(pName);
}

void DLL_SetModKey( modinfo_t *pinfo, char *pkey, char *pvalue )
{
	if( !stricmp( pkey, "url_info" ) )
	{
		pinfo->bIsMod = true;
		Q_strncpy( pinfo->szInfo, pvalue, sizeof( pinfo->szInfo ) - 1 );
		pinfo->szInfo[ sizeof( pinfo->szInfo ) - 1 ] = '\0';
	}
	else if( !stricmp( pkey, "url_dl" ) )
	{
		pinfo->bIsMod = true;
		Q_strncpy( pinfo->szDL, pvalue, sizeof( pinfo->szDL ) - 1 );
		pinfo->szDL[ sizeof( pinfo->szDL ) - 1 ] = 0;
	}
	else if( !stricmp( pkey, "version" ) )
	{
		pinfo->bIsMod = true;
		pinfo->version = atol(pvalue) != 0;
	}
	else if( !stricmp( pkey, "size" ) )
	{
		pinfo->bIsMod = true;
		pinfo->size = atol(pvalue) != 0;
	}
	else if( !stricmp( pkey, "svonly" ) )
	{
		pinfo->bIsMod = true;
		pinfo->svonly = atol(pvalue) != 0;
	}
	else if( !stricmp( pkey, "cldll" ) )
	{
		pinfo->bIsMod = true;
		pinfo->cldll = atol(pvalue) != 0;
	}
	else if( !stricmp( pkey, "secure" ) )
	{
		pinfo->bIsMod = true;
		pinfo->secure = atol(pvalue) != 0;
	}
	else if( !stricmp( pkey, "hlversion" ) )
	{
		Q_strncpy( pinfo->szHLVersion, pvalue, sizeof( pinfo->szHLVersion ) - 1 );
		pinfo->szHLVersion[ sizeof( pinfo->szHLVersion ) - 1 ] = '\0';
	}
	else if( !stricmp( pkey, "edicts" ) )
	{
		int iEdicts = atol(pvalue);

		if( iEdicts < MAX_EDICTS )
			iEdicts = MAX_EDICTS;

		pinfo->num_edicts = iEdicts;
	}
	else if( !stricmp( pkey, "fallback_dir" ) )
	{
		COM_AddDefaultDir( pvalue );
	}
	else if( !stricmp( pkey, "crcclientdll" ) )
	{
		pinfo->bIsMod = true;
		pinfo->clientcrccheck = atol(pvalue) != 0;
	}
	if( !stricmp( pkey, "type" ) )
	{
		if( !stricmp( pvalue, "singleplayer_only" ) )
		{
			pinfo->type = SINGLEPLAYER_ONLY;
		}
		else if( !stricmp( pvalue, "multiplayer_only" ) )
		{
			pinfo->type = MULTIPLAYER_ONLY;
		}
		else
		{
			pinfo->type = BOTH;
		}
	}
}

void LoadEntityDLLs(char* szBaseDir)
{
	SV_ResetModInfo();

	g_iextdllMac = 0;
	Q_memset(g_rgextdll, 0, sizeof(g_rgextdll));

	char szGameDir[64];
	Q_strncpy(szGameDir, com_gamedir, ARRAYSIZE(szGameDir));

	if (stricmp(szGameDir, "valve"))
		gmodinfo.bIsMod = true;

	char szDllFilename[8192];

	char szDllListFile[FILENAME_MAX];
	snprintf(szDllListFile, ARRAYSIZE(szDllListFile), "%s", "liblist.gam");

	FileHandle_t hLibListFile = FS_Open(szDllListFile, "rb");

	if (hLibListFile)
	{
		char szKey[64];
		char szValue[256];

		int iSize = FS_Size(hLibListFile);

		if (iSize > (512 * 512) || !iSize)
			Sys_Error("Game listing file size is bogus [%s: size %i]", "liblist.gam", iSize);

		byte* pFileData = (byte*)Mem_Malloc(iSize + 1);

		if (!pFileData)
			Sys_Error("Could not allocate space for game listing file of %i bytes", iSize + 1);

		int iRead = FS_Read(pFileData, iSize, 1, hLibListFile);

		if (iRead != iSize)
			Sys_Error("Error reading in game listing file, expected %i bytes, read %i", iSize, iRead);

		com_ignorecolons = true;

		pFileData[iSize] = '\0';

		char* pBuffer = (char*)pFileData;

		while (true)
		{
			pBuffer = COM_Parse(pBuffer);

			if (Q_strlen(com_token) <= 0)
				break;

			Q_strncpy(szKey, com_token, ARRAYSIZE(szKey) - 1);
			szKey[ARRAYSIZE(szKey) - 1] = 0;

			pBuffer = COM_Parse(pBuffer);

			Q_strncpy(szValue, com_token, ARRAYSIZE(szValue) - 1);
			szValue[ARRAYSIZE(szValue) - 1] = 0;

			if (Q_stricmp(szKey, GAMEDLL_KEY))
			{
				DLL_SetModKey(&gmodinfo, szKey, szValue);
			}
			else
			{
				int iDllOverride = COM_CheckParm(const_cast<char*>("-dll"));

				if (iDllOverride && iDllOverride < com_argc - 1)
				{
					Q_strncpy(szValue, (char*)com_argv[iDllOverride + 1], ARRAYSIZE(szValue) - 1);
					szValue[ARRAYSIZE(szValue) - 1] = 0;
				}

#ifndef WIN32
				//Find and remove architecture extensions from the filename.
				char* pszUnderscore = strchr(szValue, '_');

				if (pszUnderscore)
				{
					*pszUnderscore = '\0';

					//Append correct extension.
					Q_strcat(szValue, DEFAULT_SO_EXT, ARRAYSIZE(szValue));
				}
#endif
				char* updir = Q_strstr(szValue, "..");

				if (updir != NULL && Q_strstr(&updir[2], ".."))
				{
					Con_DPrintf(const_cast<char*>("Skipping library with illegal characters in path: %s\n"), szValue);
				}
				else
				{
					char* pext = COM_LastFileExtension(szValue);
					if (!Q_stricmp(pext, &DEFAULT_SO_EXT[1]))
					{
						FS_GetLocalPath(szValue, szDllFilename, ARRAYSIZE(szDllFilename));
						Con_DPrintf(const_cast<char*>("\nAdding:  %s/%s\n"), szGameDir, szValue);
						LoadThisDll(szDllFilename);
					}
					else
					{
						Con_DPrintf(const_cast<char*>("Skipping non-shared library:  %s\n"), szValue);
					}
				}
			}
		}
		com_ignorecolons = false;
		Mem_Free(pFileData);
		FS_Close(hLibListFile);
	}
	else
	{
		char szDllWildcard[FILENAME_MAX];
		
#ifdef _WIN32
		snprintf(szDllWildcard, ARRAYSIZE(szDllFilename), "%s\\*" DEFAULT_SO_EXT, "valve\\dlls");

		for (const char* i = Sys_FindFirst(szDllWildcard, NULL); i; i = Sys_FindNext(NULL))
		{
			snprintf(szDllFilename, ARRAYSIZE(szDllFilename), "%s/%s/%s", szBaseDir, "valve\\dlls", i);
			LoadThisDll(szDllFilename);
		}
#else
		snprintf(szDllWildcard, ARRAYSIZE(szDllFilename), "%s/*" DEFAULT_SO_EXT, "valve/dlls");

		for (const char* i = Sys_FindFirst(szDllWildcard, NULL); i; i = Sys_FindNext(NULL))
		{
			snprintf(szDllFilename, ARRAYSIZE(szDllFilename), "%s/%s/%s", szBaseDir, "valve/dlls", i);
			LoadThisDll(szDllFilename);
		}
#endif

		Sys_FindClose();
	}

	//Initialize the newest functions in case the dll doesn't provide it.
	gNewDLLFunctions.pfnGameShutdown = NULL;
	gNewDLLFunctions.pfnShouldCollide = NULL;
	gNewDLLFunctions.pfnCvarValue = NULL;
	gNewDLLFunctions.pfnCvarValue2 = NULL;
	gNewDLLFunctions.pfnOnFreeEntPrivateData = NULL;

	int interface_version;

	//First check if the dll supports the newest functions.
	NEW_DLL_FUNCTIONS_FN pGetNewDLLFunctions = (NEW_DLL_FUNCTIONS_FN)GetIOFunction("GetNewDLLFunctions");

	if (pGetNewDLLFunctions)
	{
		interface_version = NEW_DLL_FUNCTIONS_VERSION;

		pGetNewDLLFunctions(&gNewDLLFunctions, &interface_version);
	}

	//Check if it supports the newer dll functions.
	APIFUNCTION2 pGetEntityAPI2 = (APIFUNCTION2)GetIOFunction("GetEntityAPI2");

	if (pGetEntityAPI2)
	{
		interface_version = INTERFACE_VERSION;

		if (!pGetEntityAPI2(&gEntityInterface, &interface_version))
		{
			Con_Printf(const_cast<char*>("==================\n"));
			Con_Printf(const_cast<char*>("Game DLL version mismatch\n"));
			Con_Printf(const_cast<char*>("DLL version is %i, engine version is %i\n"), interface_version, INTERFACE_VERSION);

			if (interface_version > INTERFACE_VERSION)
				Con_Printf(const_cast<char*>("Engine appears to be outdated, check for updates\n"));
			else
				Con_Printf(const_cast<char*>("The game DLL for %s appears to be outdated, check for updates\n"), szGameDir);

			Con_Printf(const_cast<char*>("==================\n"));
			Host_Error("\n");
		}
	}
	else
	{
		//Check if it provides the original dll functions.
		APIFUNCTION pGetEntityAPI = (APIFUNCTION)GetIOFunction("GetEntityAPI");

		if (!pGetEntityAPI)
			Host_Error("Couldn't get DLL API from %s!", szDllFilename);

		interface_version = INTERFACE_VERSION;

		if (!pGetEntityAPI(&gEntityInterface, interface_version))
		{
			Con_Printf(const_cast<char*>("==================\n"));
			Con_Printf(const_cast<char*>("Game DLL version mismatch\n"));
			Con_Printf(const_cast<char*>("The game DLL for %s appears to be outdated, check for updates\n"), szGameDir);
			Con_Printf(const_cast<char*>("==================\n"));
			Host_Error("\n");
		}
	}

	const char* pszDescription = gEntityInterface.pfnGetGameDescription();

	const char* pszType = "mod";

	if (!gmodinfo.bIsMod)
		pszType = "game";

	Con_DPrintf(const_cast<char*>("Dll loaded for %s %s\n"), pszType, pszDescription);
}

/**
*	Loads an entity DLL and passes the engine functions and global variables to it.
*/
static void LoadThisDll( char* szDllFilename )
{
	CSysModule* pModule = FS_LoadLibrary( szDllFilename );

	if( pModule )
	{
#ifdef _WIN32
		GiveFnptrsToDllFn pFn = (GiveFnptrsToDllFn)GetProcAddress( (HMODULE)pModule, "GiveFnptrsToDll" );
#else
		GiveFnptrsToDllFn pFn = (GiveFnptrsToDllFn)Sys_GetProcAddress( pModule, "GiveFnptrsToDll" );
#endif

		if( pFn )
		{
			pFn( &g_engfuncsExportedToDlls, &gGlobalVariables );

			if( g_iextdllMac == MAX_EXT_DLLS )
			{
				Con_Printf( const_cast<char*>("Too many DLLs, ignoring remainder\n") );
#ifdef _WIN32
				FreeLibrary((HMODULE)pModule);
#else
				Sys_UnloadModule( pModule );
#endif
			}
			else
			{
				extensiondll_t* extdll = &g_rgextdll[ g_iextdllMac++ ];

				Q_memset( extdll, 0, sizeof( *extdll ) );

				extdll->pDLLHandle = pModule;

				LoadDllExports(extdll, (char*)szDllFilename);
			}
		}
		else
		{
			Con_Printf( const_cast<char*>("Couldn't get GiveFnptrsToDll in %s\n"), szDllFilename );
#ifdef _WIN32
			FreeLibrary((HMODULE)pModule);
#else
			Sys_UnloadModule(pModule);
#endif
		}
	}
	else
	{
#ifdef WIN32
		Con_Printf( const_cast<char*>("LoadLibrary failed on %s (%d)\n"), szDllFilename, GetLastError() );
#else
		Con_Printf( const_cast<char*>("LoadLibrary failed on %s: %s\n"), szDllFilename, dlerror() );
#endif
	}
}

void ReleaseEntityDlls()
{
	if( svs.dll_initialized )
	{
		FreeAllEntPrivateData();

		if( gNewDLLFunctions.pfnGameShutdown )
			gNewDLLFunctions.pfnGameShutdown();

		Cvar_UnlinkExternals();

		for( int i = 0; i < g_iextdllMac; ++i )
		{
			extensiondll_t* extdll = &g_rgextdll[ i ];

#ifdef _WIN32
			FreeLibrary((HMODULE)extdll->pDLLHandle);
#else
			Sys_UnloadModule(extdll->pDLLHandle);
#endif
			extdll->pDLLHandle = NULL;

			if( extdll->functionTable )
				Mem_Free( extdll->functionTable );

			extdll->functionTable = NULL;
		}

		
		svs.dll_initialized = false;
	}
}

void EngineFprintf(void* pfile, char* szFmt, ...)
{
	AlertMessage(at_console, const_cast<char*>("EngineFprintf:  Obsolete API\n"));
}

void AlertMessage(ALERT_TYPE atype, char* szFmt, ...)
{
	static char szOut[1024];

	va_list va;

	va_start(va, szFmt);

	if (atype != at_logged || svs.maxclients <= 1)
	{
		if (developer.value != 0.0)
		{
			if (at_notice <= atype && atype <= at_error)
			{
				switch (atype)
				{
				case at_notice:
					Q_strcpy(szOut, "NOTE:  ");
					break;

				case at_console:
					szOut[0] = '\0';
					break;

				case at_aiconsole:
					if (developer.value < 2.0)
					{
						va_end(va);
						return;
					}

				case at_warning:
					Q_strcpy(szOut, "WARNING:  ");
					break;

				case at_error:
					Q_strcpy(szOut, "ERROR:  ");
					break;

				default:
					break;
				}
			}

			vsnprintf(&szOut[Q_strlen(szOut)], ARRAYSIZE(szOut) - Q_strlen(szOut), szFmt, va);
			Con_Printf(const_cast<char*>("%s"), szOut);
		}
	}
	else
	{
		vsnprintf(szOut, ARRAYSIZE(szOut), szFmt, va);
		Log_Printf("%s", szOut);
	}

	va_end(va);
}

void IN_DeactivateMouse()
{
	ClientDLL_DeactivateMouse();
}

void IN_ActivateMouse()
{
	ClientDLL_ActivateMouse();
}

void IN_MouseEvent(int mstate)
{
	ClientDLL_MouseEvent(mstate);
}

void IN_ClearStates()
{
	ClientDLL_ClearStates();
}

#ifdef _WIN32
void Sys_ReadClipboard(char** out)
{
	HANDLE	th;
	int iSize;

	if (!out)
		return;

	if (OpenClipboard(NULL))
	{
		th = GetClipboardData(CF_TEXT);
		if (th != NULL)
		{
			char* pszBuffer = (char*)GlobalLock(th);
			if (pszBuffer)
			{
				iSize = GlobalSize(th);
				*out = (char*)Mem_Malloc(iSize + 1);
				Q_strcpy(*out, pszBuffer);
			}
			GlobalUnlock(th);
		}
		CloseClipboard();
	}
}

qboolean Sys_IsEscapeKeyPressed()
{
	return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}
#endif

void Sys_SplitPath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
	_splitpath(path, drive, dir, fname, ext);
}
