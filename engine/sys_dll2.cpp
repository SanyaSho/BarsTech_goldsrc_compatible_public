#include <clocale>
#include <cstdint>
#include <cstring>

#include "winheaders.h"

#ifdef WIN32
#include <Winsock2.h>
#else
#error "Implement me"
#endif

#include "steam/steam_api.h"

#include "quakedef.h"

#include "cdll_int.h"

#include "vgui2/src/vgui_key_translation.h"

#include "dll_state.h"
#include "client.h"
#include "common.h"
#include "buildnum.h"
#include "engine_hlds_api.h"
#include "engine_launcher_api.h"
#include "filesystem.h"
#include "host.h"
#include "host_cmd.h"
#include "idedicatedexports.h"
#include "IEngine.h"
#include "IGame.h"
#include "igameuifuncs.h"
#include "IRegistry.h"
#include "kbutton.h"
#include "keys.h"
#include "modinfo.h"
#include "pr_cmds.h"
#include "qgl.h"
#include "strtools.h"
#include "sv_steam3.h"
#include "sys.h"
#include "sys_getmodes.h"
#include "traceinit.h"
#include "vgui_int.h"
#include "sys_procinfo.h"

#include "d_local.h"

char* g_pPostRestartCmdLineArgs = nullptr;

qboolean g_bIsDedicatedServer = false;

char* szReslistsBaseDir = const_cast<char*>("reslists");
char* szCommonPreloads = const_cast<char*>("multiplayer_preloads");
char* szReslistsExt = const_cast<char*>(".lst");

char* argv[ MAX_NUM_ARGVS ];


// this is only used by software mode
SurfaceCacheForResFn D_SurfaceCacheForRes = NULL;

HINSTANCE g_hinstOpenGlEarly;

qboolean Win32AtLeastV4 = false;
static qboolean g_bIsWin95 = false;
static qboolean g_bIsWin98 = false;

static double g_flLastSteamProgressUpdateTime = 0;
static qboolean g_bPrintingKeepAliveDots = false;

extern qboolean s_bTimeInitialized;
extern double pfreq;
extern int lowshift;
extern CRITICAL_SECTION s_Time_CriticalSection;

void Sys_InitFloatTime();
void Sys_InitArgv( char* lpCmdLine );
void Sys_ShutdownArgv();
vgui2::KeyCode GetVGUI2KeyCodeForBind(const char* bind);

#ifdef WIN32
#define CDKEY_RANDOM_MAX INT16_MAX
#else
#define CDKEY_RANDOM_MAX INT32_MAX
#endif


class CDedicatedServerAPI : IDedicatedServerAPI
{
public:
	bool Init(char* basedir, char* cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory) override;

	int Shutdown() override;

	bool RunFrame() override;

	void AddConsoleText(char* text) override;

	void UpdateStatus(float* fps, int* nActive, int* nMaxPlayers, char* pszMap) override;

private:
	char m_OrigCmd[1024];
};

IDedicatedExports* dedicated = nullptr;

EXPOSE_SINGLE_INTERFACE(CDedicatedServerAPI, IDedicatedServerAPI, ENGINE_HLDS_INTERFACE_VERSION);

class CEngineAPI final : public IEngineAPI
{
public:
	int Run(void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory) override;
};

EXPOSE_SINGLE_INTERFACE(CEngineAPI, IEngineAPI, ENGINE_LAUNCHER_INTERFACE_VERSION);


const char* GetCurrentSteamAppName()
{
	if( !stricmp( com_gamedir, "cstrike" ) ||
		!stricmp( com_gamedir, "cstrike_beta" ) )
	{
		return "Counter-Strike";
	}
	else if( !stricmp( com_gamedir, "valve" ) )
	{
		return "Half-Life";
	}
	else if( !stricmp( com_gamedir, "ricochet" ) )
	{
		return "Ricochet";
	}
	else if( !stricmp( com_gamedir, "dod" ) )
	{
		return "Day of Defeat";
	}
	else if( !stricmp( com_gamedir, "tfc" ) )
	{
		return "Team Fortress Classic";
	}
	else if( !stricmp( com_gamedir, "dmc" ) )
	{
		return "Deathmatch Classic";
	}
	else if( !stricmp( com_gamedir, "czero" ) )
	{
		return "Condition Zero";
	}

	return "Half-Life";
}

void SetRateRegistrySetting(const char* pchRate)
{
	registry->WriteString("rate", pchRate);
}

const char* GetRateRegistrySetting(const char* pchDef)
{
	return registry->ReadString("rate", pchDef);
}

//Legacy factory function used to acquire the client launcher interface
DLL_EXPORT void F(IEngineAPI** api)
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();

	*api = (IEngineAPI*)factory(ENGINE_LAUNCHER_INTERFACE_VERSION, NULL);
}

void Sys_VID_FlipScreen()
{
#if !defined(GLQUAKE)
	videomode->FlipScreen();
#else
	if (pmainwindow)
		SDL_GL_SwapWindow(pmainwindow);
#endif
}

int Sys_GetSurfaceCacheSize(int width, int height)
{
	int size, pix;

	int iCacheSize = COM_CheckParm(const_cast<char*>("-surfcachesize"));

	if (iCacheSize)
	{
		return atol(com_argv[iCacheSize + 1]) * 1024;
	}

	size = SURFCACHE_SIZE_AT_320X200 * sizeof(color24);

#if !defined(GLQUAKE)
	if (r_pixbytes == 1)
	{
#endif
		pix = width * height * sizeof(WORD);
		if (pix > 64000)
			size += (pix - 64000) * sizeof(color24);
#if !defined(GLQUAKE)
	}
	else
	{
		//size += SURFCACHE_SIZE_AT_320X200;

		pix = width * height * r_pixbytes;
		if (pix > 128000)
			size += (pix - 128000) * sizeof(color24);
	}
#endif

	return size;
}

void Sys_GetCDKey(char* pszCDKey, int* nLength, int* bDedicated)
{
	char hostname[MAX_PATH];
	char key[65];

	if (0 != gethostname(hostname, ARRAYSIZE(hostname)))
	{
		snprintf(key, ARRAYSIZE(key), "%u", RandomLong(0, CDKEY_RANDOM_MAX));
	}
	else
	{
		hostent* pHost = gethostbyname(hostname);

		if (pHost != NULL && pHost->h_length == 4 && pHost->h_addr_list[0] != NULL)
		{
			char* pszAddr = pHost->h_addr_list[0];

			snprintf(key, ARRAYSIZE(key), "%u.%u.%u.%u", (unsigned int)pszAddr[0], (unsigned int)pszAddr[1], (unsigned int)pszAddr[2], (unsigned int)pszAddr[3]);
		}
		else
		{
			CRC32_t crc;
			CRC32_ProcessBuffer(&crc, hostname, strlen(hostname));
			snprintf(key, ARRAYSIZE(key), "%u", (unsigned int)crc);
		}
	}

	key[ARRAYSIZE(key) - 1] = '\0';

	strcpy(pszCDKey, key);

	if (nLength)
		*nLength = strlen(key);

	if (bDedicated)
		*bDedicated = false;
}

void Legacy_ErrorMessage(int nLevel, const char* pszErrorMessage)
{
	//Nothing
}

void Legacy_Sys_Printf(const char* fmt, ...)
{
	char text[1024];
	va_list va;

	va_start(va, fmt);
	vsnprintf(text, ARRAYSIZE(text), fmt, va);
	va_end(va);

	if (dedicated)
		dedicated->Sys_Printf(text);
}

void Legacy_MP3subsys_Suspend_Audio()
{
	//Nothing
}

void Legacy_MP3subsys_Resume_Audio()
{
	//Nothing
}

qboolean Sys_IsWin98()
{
	return g_bIsWin98;
}

qboolean Sys_IsWin95()
{
	return g_bIsWin95;
}

/**
*	Determine Windows OS version, set globals.
*	Information for fields retrieved from: https://www.go4expert.com/articles/os-version-detection-32-64-bit-os-t1472/
*/
void Sys_CheckOSVersion()
{
#ifdef WIN32
	OSVERSIONINFO vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx(&vinfo))
		Sys_Error("Couldn't get OS info");

	Win32AtLeastV4 = vinfo.dwMajorVersion >= 4;

	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
		vinfo.dwMajorVersion == 4)
	{
		if (vinfo.dwMinorVersion == 0)
		{
			g_bIsWin95 = true;
		}
		else
		{
			if (vinfo.dwMinorVersion < 90)
			{
				g_bIsWin98 = true;
			}
		}
	}
#endif
}



static HDC maindc;
static HGLRC baseRC;



void Sys_Init()
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

	Sys_CheckOSVersion();
#endif
	Sys_TruncateFPU();
	Sys_InitFloatTime();
}

void Sys_Shutdown()
{
	Sys_ShutdownFloatTime();
	Steam_ShutdownClient();
#if defined(GLQUAKE)
	GL_Shutdown(pmainwindow, maindc, baseRC);
#endif

	if (s_bTimeInitialized == true)
	{
		DeleteCriticalSection(&s_Time_CriticalSection);
		s_bTimeInitialized = false;
	}
}

void Sys_InitArgv(char* lpCmdLine)
{
	host_parms.argc = 1;
	argv[0] = const_cast<char*>("");

	while (*lpCmdLine && (host_parms.argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= ' ') || (*lpCmdLine > '~')))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[host_parms.argc] = lpCmdLine;
			host_parms.argc++;

			while (*lpCmdLine && ((*lpCmdLine > ' ') && (*lpCmdLine <= '~')))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = '\0';
				lpCmdLine++;
			}

		}
	}

	host_parms.argv = argv;

	COM_InitArgv(host_parms.argc, host_parms.argv);

	host_parms.argc = com_argc;
	host_parms.argv = com_argv;
}

void Sys_ShutdownArgv()
{
	//Nothing
}

void Sys_InitMemory()
{
	// Take at least 14 Mb and no more than 134 Mb, unless they explicitly
	// request otherwise
	 int heapsizeIndex = COM_CheckParm(const_cast<char*>("-heapsize"));

	if (heapsizeIndex && heapsizeIndex < com_argc - 1)
	{
		host_parms.memsize = atol(com_argv[heapsizeIndex + 1]) * 1024;
	}

	if (host_parms.memsize < MINIMUM_MEMORY)
	{
#ifdef _WIN32
		MEMORYSTATUS lpBuffer;
		lpBuffer.dwLength = sizeof(MEMORYSTATUS);
		GlobalMemoryStatus(&lpBuffer);

		if (lpBuffer.dwTotalPhys != 0)
		{
			if (lpBuffer.dwTotalPhys < (15 * 1024 * 1024))
				Sys_Error("Available memory less than 15MB!!! %i", host_parms.memsize);

			host_parms.memsize = (int)(lpBuffer.dwTotalPhys / 2);
			if (host_parms.memsize < MINIMUM_MEMORY)
				host_parms.memsize = MINIMUM_MEMORY;
		}
		else
			host_parms.memsize = MAXIMUM_MEMORY;

		if (g_bIsDedicatedServer)
			host_parms.memsize = DEFAULT_MEMORY;
#else
		host_parms.memsize = DEFAULT_MEMORY;
#endif // _WIN32
	}

	if (host_parms.memsize > MAXIMUM_MEMORY)
	{
		host_parms.memsize = MAXIMUM_MEMORY;
	}

	if (COM_CheckParm(const_cast<char*>("-minmemory")))
	{
		host_parms.memsize = MINIMUM_MEMORY;
	}

#ifdef _WIN32
	host_parms.membase = (void*)GlobalAlloc(GMEM_FIXED, host_parms.memsize);
#else
	host_parms.membase = Mem_Malloc(host_parms.memsize);
#endif // _WIN32

	if (!host_parms.membase)
		Sys_Error("Unable to allocate %.2f MB\n", host_parms.memsize / (1024.0 * 1024.0));
}

void Sys_ShutdownMemory()
{
#ifdef _WIN32
	GlobalFree(host_parms.membase);
#else
	free(host_parms.membase);
#endif
	host_parms.membase = 0;
	host_parms.memsize = 0;
}

void Sys_SetupLegacyAPIs()
{
	VID_FlipScreen = &Sys_VID_FlipScreen;
	D_SurfaceCacheForRes = &Sys_GetSurfaceCacheSize;
	Launcher_ConsolePrintf = &Legacy_Sys_Printf;
}

void Sys_InitLauncherInterface()
{
#ifdef _WIN32
	gHasMMXTechnology = PROC_IsMMX();
	g_hinstOpenGlEarly = GetModuleHandle("opengl32.dll");
#else
	gHasMMXTechnology = TRUE;
#endif
	Sys_SetupLegacyAPIs();
}

void Sys_ShutdownLauncherInterface()
{
	//Nothing
}

void Sys_InitAuthentication()
{
	Sys_Printf("STEAM Auth Server\r\n");
}

void Sys_ShutdownAuthentication()
{
	//Nothing
}

void Sys_ShowProgressTicks()
{
	static bool recursionGuard = false;
	static int32 numTics = 0;

	if (!recursionGuard)
	{
		recursionGuard = true;

		if (COM_CheckParm(const_cast<char*>("-steam")))
		{
			float flTime = Sys_FloatTime();
			if (g_flLastSteamProgressUpdateTime + 2.0 <= flTime)
			{
				g_flLastSteamProgressUpdateTime = flTime;

				numTics++;

				if (g_bIsDedicatedServer)
				{
					if (g_bMajorMapChange)
					{
						g_bPrintingKeepAliveDots = true;
						Sys_Printf(".");
					}
				}
				else
				{
					char msg[128] = "Updating game resources";
					int count = numTics % 5 + 1;

					for (int i = 0; i < count; i++)
					{
						msg[strlen(msg)] = '.';
						msg[strlen(msg) + 1] = '\0';
					}

					SetLoadingProgressBarStatusText(msg);
				}
			}
		}

		recursionGuard = false;
	}
}

int Sys_InitGame(char* lpOrgCmdLine, char* pBaseDir, void* pwnd, int bIsDedicated)
{
	host_initialized = false;

	if (!bIsDedicated)
	{
		pmainwindow = reinterpret_cast<SDL_Window*>(pwnd);
		videomode->UpdateWindowPosition();
	}

	g_bIsDedicatedServer = bIsDedicated;

	Q_memset(&gmodinfo, 0, sizeof(gmodinfo));

	int bSuccess = false;

	SV_ResetModInfo();

	TraceInit("Sys_Init()", "Sys_Shutdown()", 0);

	Sys_Init();

	FS_LogLevelLoadStarted("Launcher");

	SeedRandomNumberGenerator();

	TraceInit("Sys_InitMemory()", "Sys_ShutdownMemory()", 0);

	Sys_InitMemory();

	TraceInit("Sys_InitLauncherInterface()", "Sys_ShutdownLauncherInterface()", 0);

	Sys_InitLauncherInterface();

#if defined(GLQUAKE)
	if (GL_SetMode(pmainwindow, &maindc, &baseRC, 0, "opengl32.dll", lpOrgCmdLine))
#endif
	{
		TraceInit("Host_Init( &host_parms )", "Host_Shutdown()", 0);

		Host_Init(&host_parms);

		if (host_initialized)
		{
			TraceInit("Sys_InitAuthentication()", "Sys_ShutdownAuthentication()", 0);

			Sys_InitAuthentication();

			if (g_bIsDedicatedServer)
			{
				Host_InitializeGameDLL();
				NET_Config(true);
			}
			else
			{
				ClientDLL_ActivateMouse();
			}

			bSuccess = true;

#ifndef WIN32
			char en_US[12];

			strcpy(en_US, "en_US.UTF-8");

			const char* pszLocale = setlocale(6, nullptr);

			if (!pszLocale)
				pszLocale = "c";

			if (stricmp(pszLocale, en_US))
			{
				char MessageText[512];

				snprintf(
					MessageText,
					sizeof(MessageText),
					"SetLocale('%s') failed. Using '%s'.\nYou may have limited glyph support.\nPlease install '%s' locale.",
					en_US,
					pszLocale,
					en_US);

				SDL_ShowSimpleMessageBox(0, "Warning", MessageText, pmainwindow);
			}

#endif
		}
	}

	return bSuccess;
}

void Sys_ShutdownGame()
{
	if (!g_bIsDedicatedServer)
	{
		ClientDLL_DeactivateMouse();
	}

	TraceShutdown("Host_Shutdown()", 0);
	Host_Shutdown();

	if (g_bIsDedicatedServer)
	{
		NET_Config(false);
	}

	TraceShutdown("Sys_ShutdownLauncherInterface()", 0);
	Sys_ShutdownLauncherInterface();

	TraceShutdown("Sys_ShutdownAuthentication()", 0);
	Sys_ShutdownAuthentication();

	TraceShutdown("Sys_ShutdownMemory()", 0);
	Sys_ShutdownMemory();

	TraceShutdown("Sys_Shutdown()", 0);
	Sys_Shutdown();
}

void ClearIOStates()
{
	for (int key = 0; key < 256; ++key)
	{
		Key_Event(key, false);
	}

	Key_ClearStates();
	ClientDLL_ClearStates();
}

int RunListenServer(void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory)
{
	static char OrigCmd[1024];

	g_pPostRestartCmdLineArgs = postRestartCmdLineArgs;
	strcpy(OrigCmd, cmdline);

	TraceInit("Sys_InitArgv( OrigCmd )", "Sys_ShutdownArgv()", 0);
	Sys_InitArgv(OrigCmd);

#if !defined(GLQUAKE)
	int is32bit = COM_CheckParm(const_cast<char*>("-32bit"));

	if (is32bit != 0)
		r_pixbytes = 4;
#endif

	eng->SetQuitting(IEngine::QUIT_NOTQUITTING);

	registry->Init();

	Steam_InitClient();
	
	int result = ENGRUN_QUITTING;

	TraceInit("FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0);

	if (FileSystem_Init(basedir, filesystemFactory))
	{
		VideoMode_Create();

		result = ENGRUN_UNSUPPORTED_VIDEOMODE;
		registry->WriteInt("CrashInitializingVideoMode", 1);

		if (videomode->Init(instance))
		{
			result = ENGRUN_CHANGED_VIDEOMODE;
			registry->WriteInt("CrashInitializingVideoMode", 0);

			if (game->Init(instance))
			{
				result = ENGRUN_UNSUPPORTED_VIDEOMODE;

				if (eng->Load(false, basedir, cmdline))
				{
					videomode->PlayStartupSequence();

					while (true)
					{
						game->SleepUntilInput(0);

						if (eng->GetQuitting() != IEngine::QUIT_NOTQUITTING)
							break;

						eng->Frame();
					}

					result = eng->GetQuitting() != IEngine::QUIT_TODESKTOP ? ENGRUN_CHANGED_VIDEOMODE : ENGRUN_QUITTING;

					eng->Unload();
				}

				game->Shutdown();
			}

			videomode->Shutdown();
		}

		TraceShutdown("FileSystem_Shutdown()", 0);
		FileSystem_Shutdown();

		registry->Shutdown();

		TraceShutdown("Sys_ShutdownArgv()", 0);
		Sys_ShutdownArgv();
	}

	return result;
}

void Sys_CreateCrashDump(uint32 _exception_code, void* exception_info)
{
	char szComment[256];

	_snprintf(szComment, sizeof(szComment), "hl: %s\n", com_gamedir);

	SteamAPI_SetMiniDumpComment(szComment);

	SteamAPI_WriteMiniDump(_exception_code, exception_info, build_number());
}

int CEngineAPI::Run( void* instance, char* basedir, char* cmdline, char* postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
	if( !strstr( cmdline, "-nobreakpad" ) )
	{
		SteamAPI_UseBreakpadCrashHandler( va( const_cast<char*>("%d"), build_number() ), __DATE__, __TIME__, 0, 0, 0 );
	}

	return RunListenServer( instance, basedir, cmdline, postRestartCmdLineArgs, launcherFactory, filesystemFactory );
}

int BuildMapCycleListHints(char** hints)
{
	char szMod[MAX_PATH];
	COM_FileBase(com_gamedir, szMod);

	char cszMapCycleTxtFile[MAX_PATH];
	sprintf(cszMapCycleTxtFile, "%s/%s", szMod, mapcyclefile.string);

	FileHandle_t pFile = FS_Open(cszMapCycleTxtFile, "rb");

	if (FILESYSTEM_INVALID_HANDLE == pFile)
	{
		Con_Printf(const_cast<char*>("Unable to open %s\n"), cszMapCycleTxtFile);
		return false;
	}

	char szMap[MAX_PATH + 2];
	sprintf(
		szMap, "%s\\%s\\%s%s\r\n",
		szReslistsBaseDir,
		GetCurrentSteamAppName(),
		szCommonPreloads,
		szReslistsExt
	);

	*hints = (char*)malloc(strlen(szMap) + 1);

	if (!*hints)
	{
		Con_Printf(const_cast<char*>("Unable to allocate memory for map cycle hints list"));
		return false;
	}

	strcpy(*hints, szMap);

	unsigned int uiSize = FS_Size(pFile);

	if (uiSize)
	{
		char* pszData = (char*)malloc(uiSize);

		if (pszData)
		{
			char* pszParseToken = pszData;

			if (FS_Read(pszData, uiSize, 1, pFile) == 1)
			{
				char mapLine[MAX_PATH + 2];

				while (true)
				{
					pszParseToken = COM_Parse(pszParseToken);

					if (!com_token[0])
					{
						break;
					}

					strncpy(szMap, com_token, ARRAYSIZE(szMap) - 1);
					szMap[ARRAYSIZE(szMap) - 1] = '\0';

					if (COM_TokenWaiting(pszParseToken))
						pszParseToken = COM_Parse(pszParseToken);

					snprintf(
						mapLine, ARRAYSIZE(mapLine),
						"%s\\%s\\%s%s\r\n",
						szReslistsBaseDir,
						GetCurrentSteamAppName(),
						szMap,
						szReslistsExt
					);

					*hints = (char*)realloc(*hints, strlen(*hints) + 1 + strlen(mapLine) + 1);

					if (!*hints)
					{
						Con_Printf(const_cast<char*>("Unable to reallocate memory for map cycle hints list"));
						return 0;
					}

					strcat(*hints, mapLine);
				}
			}

			free(pszParseToken);
		}
	}

	FS_Close(pFile);

	sprintf(szMap, "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, GetCurrentSteamAppName());

	*hints = (char*)realloc(*hints, strlen(*hints) + 1 + strlen(szMap) + 1);
	strcat(*hints, szMap);

	return true;
}

bool CDedicatedServerAPI::Init( char* basedir, char* cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
	dedicated = (IDedicatedExports *)launcherFactory( "VENGINE_DEDICATEDEXPORTS_API_VERSION001", NULL );
	
	if( dedicated )
	{
		strcpy( m_OrigCmd, cmdline );

		if( !strstr( cmdline, "-nobreakpad" ) )
		{
			SteamAPI_UseBreakpadCrashHandler( va( const_cast<char*>("%d"), build_number() ), __DATE__, __TIME__, false, nullptr, nullptr );
		}

		TraceInit( "Sys_InitArgv( m_OrigCmd )", "Sys_ShutdownArgv()", 0 );
		Sys_InitArgv( m_OrigCmd );

		eng->SetQuitting( IEngine::QUIT_NOTQUITTING );

		registry->Init();

		g_bIsDedicatedServer = true;

		TraceInit( "FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0 );
		if( FileSystem_Init( basedir, filesystemFactory ) &&
			game->Init( NULL ) &&
			eng->Load( true, basedir, cmdline ) )
		{
			char szCommand[ 256 ];
			snprintf( szCommand, ARRAYSIZE( szCommand ), "exec %s\n", servercfgfile.string );
			szCommand[ ARRAYSIZE( szCommand ) - 1 ] = '\0';
			Cbuf_InsertText( szCommand );

			return true;
		}
	}

	return false;
}

int CDedicatedServerAPI::Shutdown()
{
	eng->Unload();
	game->Shutdown();

	TraceShutdown( "FileSystem_Shutdown()", 0 );
	FileSystem_Shutdown();

	registry->Shutdown();

	TraceShutdown( "Sys_ShutdownArgv()", 0 );
	Sys_ShutdownArgv();

	dedicated = NULL;

	return giActive;
}

bool CDedicatedServerAPI::RunFrame()
{
	if( !eng->GetQuitting() )
	{
		eng->Frame();
		return true;
	}

	return false;
}

void CDedicatedServerAPI::AddConsoleText( char* text )
{
	Cbuf_AddText( text );
}

void CDedicatedServerAPI::UpdateStatus( float* fps, int* nActive, int* nMaxPlayers, char* pszMap )
{
	Host_GetHostInfo( fps, nActive, NULL, nMaxPlayers, pszMap );
}

class CGameUIFuncs : public IGameUIFuncs
{
public:
	CGameUIFuncs() = default;
	~CGameUIFuncs() = default;

	bool IsKeyDown( const char* keyname, bool& isdown ) override
	{
		kbutton_t* pButton = ClientDLL_FindKey( keyname );

		if( !pButton )
			return false;

		isdown = ( pButton->state & 1 ) != 0;

		return true;
	}

	const char* Key_NameForKey( int keynum ) override
	{
		return Key_KeynumToString( keynum );
	}

	const char* Key_BindingForKey( int keynum ) override
	{
		return ::Key_BindingForKey( keynum );
	}

	vgui2::KeyCode GetVGUI2KeyCodeForBind( const char* bind ) override
	{
		return ::GetVGUI2KeyCodeForBind( bind );
	}

	void GetVideoModes( vmode_t** liststart, int* count ) override
	{
		VideoMode_GetVideoModes( liststart, count );
	}

	void GetCurrentVideoMode( int* width, int* height, int* bpp ) override
	{
		VideoMode_GetCurrentVideoMode( width, height, bpp );
	}

	void GetCurrentRenderer( char* name, int namelen,
							 int* windowed, int* hdmodels,
							 int* addons_folder, int* vid_level ) override
	{
		VideoMode_GetCurrentRenderer(
			name, namelen,
			windowed,
			hdmodels, addons_folder,
			vid_level
		);
	}

	bool IsConnectedToVACSecureServer() override
	{
		if( cls.state == ca_active || cls.state == ca_connected )
			return cls.isVAC2Secure;

		return false;
	}

	int Key_KeyStringToKeyNum( const char* keyname ) override
	{
		return Key_StringToKeynum( keyname );
	}

private:
	CGameUIFuncs( const CGameUIFuncs& ) = delete;
	CGameUIFuncs& operator=( const CGameUIFuncs& ) = delete;
};

EXPOSE_SINGLE_INTERFACE( CGameUIFuncs, IGameUIFuncs, GAMEUIFUNCS_INTERFACE_VERSION );
