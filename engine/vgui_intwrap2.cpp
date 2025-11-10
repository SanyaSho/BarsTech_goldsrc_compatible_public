#include <UtlVector.h>

#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>

#include "quakedef.h"
#include "client.h"
#include "host_cmd.h"

#include "cdll_int.h"
#include "cl_main.h"
#include "APIProxy.h"
#include "BaseUI/IBaseUI.h"
#include "GameUI/IClientVGUI.h"
#include "GameUI/IGameConsole.h"
#include "GameUI/IGameUI.h"
#if defined(GLQUAKE)
#include "gl_screen.h"
#else
#include "screen.h"
#endif
#include "r_shared.h"
#include "host.h"
#include "modinfo.h"
#include "sys_getmodes.h"
#include "vgui_int.h"
#include "vgui2/BaseUI_Interface.h"
#include "vgui2/BaseUISurface.h"

static CUtlVector<char> g_TempConsoleBuffer;

static IBaseUI* staticUIFuncs = NULL;

static qboolean staticExclusiveInputShadow = false;

static int s_tutorMessageDecayData[ 256 ] = {};

void VGuiWrap2_Startup()
{
	if( staticUIFuncs )
		return;

	CreateInterfaceFn engineFactory = Sys_GetFactoryThis();

	CreateInterfaceFn factories[ 2 ] = 
	{
		engineFactory,
		GetFileSystemFactory()
	};

	staticUIFuncs = (IBaseUI*)(engineFactory(BASEUI_INTERFACE_VERSION, NULL));

	staticUIFuncs->Initialize(factories, ARRAYSIZE(factories));
	staticUIFuncs->Start(&cl_enginefuncs, CLDLL_INTERFACE_VERSION);

	//Flush temporary buffer
	g_TempConsoleBuffer.AddToTail( '\0' );
	VGuiWrap2_ConPrintf( g_TempConsoleBuffer.Base() );
	g_TempConsoleBuffer.Purge();
}

void VGuiWrap2_Shutdown()
{
	if( staticUIFuncs )
	{
		staticUIFuncs->Shutdown();
		staticUIFuncs = NULL;
	}
}

int VGuiWrap2_CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if( staticUIFuncs )
		staticUIFuncs->CallEngineSurfaceAppHandler( event, userData );

	return FALSE;
}

int VGuiWrap2_IsGameUIVisible()
{
	if( !staticGameUIFuncs )
		return FALSE;

	return staticGameUIFuncs->HasExclusiveInput() != 0;
}

int VGuiWrap2_UseVGUI1()
{
	if( !staticClient )
		return TRUE;

	return staticClient->UseVGUI1() != 0;
}

void* VGuiWrap2_GetPanel()
{
	//Nothing
	return NULL;
}

void VGuiWrap2_ReleaseMouse()
{
	//Nothing
}

void VGuiWrap2_GetMouse()
{
	//Nothing
}

void VGuiWrap2_SetVisible( int state )
{
	//Nothing
}

int VGuiWrap2_GameUIKeyPressed()
{
	if (!staticGameUIFuncs)
		return FALSE;

	if( staticGameUIFuncs->HasExclusiveInput() )
	{
		if( cl.levelname[ 0 ] )
		{
			staticUIFuncs->HideGameUI();
		}
	}
	else
	{
		staticUIFuncs->ActivateGameUI();
	}

	return TRUE;
}

int VGuiWrap2_Key_Event( int down, int keynum, const char* pszCurrentBinding )
{
	if( !staticUIFuncs )
		return TRUE;

	return staticUIFuncs->Key_Event( down, keynum, pszCurrentBinding ) == 0;
}

void VGuiWrap2_Paint()
{
	RECT rect;
	POINT pnt;

	if( !staticGameUIFuncs )
	{
		return;
	}

	pnt.x = 0;
	pnt.y = 0;
	rect.top = 0;

	if( VideoMode_IsWindowed() )
	{
		SDL_GetWindowPosition( pmainwindow, (int*)&pnt.x, (int*)&pnt.y);
		SDL_GetWindowSize( pmainwindow, (int*)&rect.right, (int*)&rect.bottom );
	}
	else
	{
		pnt.x = 0;
		pnt.y = 0;
		VideoMode_GetCurrentVideoMode((int*)&rect.right, (int*)&rect.bottom, NULL );
	}

	rect.bottom += rect.top;

	AllowFog( FALSE );

	staticUIFuncs->Paint( pnt.x, pnt.y, rect.right, rect.bottom );

	if (VGuiWrap2_UseVGUI1())
	{
		qboolean excl = staticGameUIFuncs->HasExclusiveInput() != 0;

		if( excl != staticExclusiveInputShadow )
		{
			if( excl )
			{
				VGuiWrap_ReleaseMouse();
			}
			else
			{
				VGuiWrap_GetMouse();
				ClearIOStates();
			}
		}

		staticExclusiveInputShadow = excl;
	}

	AllowFog( TRUE );
}

void VGuiWrap2_NotifyOfServerDisconnect()
{
	if( staticGameUIFuncs )
		staticGameUIFuncs->DisconnectFromServer();
}

void VGuiWrap2_HideGameUI()
{
	if( staticUIFuncs )
		staticUIFuncs->HideGameUI();
}

int VGuiWrap2_IsConsoleVisible()
{
	if( !staticGameConsole )
		return FALSE;

	return staticGameConsole->IsConsoleVisible();
}

void VGuiWrap2_ShowConsole()
{
	if (staticUIFuncs)
	{
		staticUIFuncs->ActivateGameUI();

		if (staticUIFuncs)
			staticUIFuncs->ShowConsole();
	}
}

void VGuiWrap2_ShowDemoPlayer()
{
	if( staticUIFuncs )
		staticUIFuncs->ActivateGameUI();

	if( staticGameUIFuncs )
		staticGameUIFuncs->ActivateDemoUI();
}

void VGuiWrap2_HideConsole()
{
	if( staticUIFuncs )
		staticUIFuncs->HideConsole();
}

void VGuiWrap2_ClearConsole()
{
	if( staticGameConsole )
		staticGameConsole->Clear();
}

void VGuiWrap2_ConPrintf( const char* msg )
{
	if( staticGameConsole )
	{
		staticGameConsole->Printf( "%s", msg );
		return;
	}

	int len = strlen( msg );

	g_TempConsoleBuffer.InsertMultipleBefore( g_TempConsoleBuffer.Count(), len, msg );
}

void VGuiWrap2_ConDPrintf( const char* msg )
{
	if( staticGameConsole )
	{
		staticGameConsole->DPrintf( "%s", msg );
		return;
	}

	int len = strlen( msg );

	g_TempConsoleBuffer.InsertMultipleBefore( g_TempConsoleBuffer.Count(), len, msg );
}

void VGuiWrap2_LoadingStarted( const char* resourceType, const char* resourceName )
{
	if( staticGameUIFuncs )
		staticGameUIFuncs->LoadingStarted( resourceType, resourceName );
}

void VGuiWrap2_LoadingFinished( const char* resourceType, const char* resourceName )
{
	if( staticGameUIFuncs )
		staticGameUIFuncs->LoadingFinished( resourceType, resourceName );
}

void StartLoadingProgressBar(const char* loadingType, int numProgressPoints)
{
	//Display the bar only if we're playing a multiplayer game or are connected to a server
	if (!Host_IsSinglePlayerGame() && (!UserIsConnectedOnLoopback() || gmodinfo.type != SINGLEPLAYER_ONLY))
	{
		if (staticUIFuncs)
			staticUIFuncs->ActivateGameUI();

		if (staticGameUIFuncs)
		{
			staticGameUIFuncs->StartProgressBar(loadingType, numProgressPoints);
			SCR_UpdateScreen();
		}
	}
}

void ContinueLoadingProgressBar(const char* loadingType, int progressPoint, float progressFraction)
{
	if (staticGameUIFuncs)
	{
		if (staticGameUIFuncs->ContinueProgressBar(progressPoint, progressFraction))
			SCR_UpdateScreen();
	}
}

void SetLoadingProgressBarStatusText(const char* statusText)
{
	if (staticGameUIFuncs)
	{
		if (staticGameUIFuncs->SetProgressBarStatusText(statusText))
			SCR_UpdateScreen();
	}
}

void StopLoadingProgressBar()
{
	if (cls.state == ca_active)
	{
		if (staticUIFuncs)
			staticUIFuncs->HideGameUI();
	}
	else if (staticUIFuncs)
	{
		if (staticClient)
		{
			staticClient->HideAllVGUIMenu();
		}

		staticUIFuncs->ActivateGameUI();
	}
	if (staticGameUIFuncs)
		staticGameUIFuncs->StopProgressBar(gfExtendedError != false, gszDisconnectReason, gszExtendedDisconnectReason);

	gfExtendedError = false;
	gszDisconnectReason[0] = '\0';
	gszExtendedDisconnectReason[0] = '\0';
}

void VGuiWrap2_NotifyOfServerConnect(const char* game, int IP, int port)
{
	if (staticGameUIFuncs)
	{
		gfExtendedError = false;
		gszDisconnectReason[0] = '\0';
		gszExtendedDisconnectReason[0] = '\0';
		StopLoadingProgressBar();

		staticGameUIFuncs->ConnectToServer(game, IP, port);
	}
}

void SetSecondaryProgressBar(float progress)
{
	if (staticGameUIFuncs)
		staticGameUIFuncs->SetSecondaryProgressBar(progress);
}

void SetSecondaryProgressBarText(const char* statusText)
{
	if (staticGameUIFuncs)
		staticGameUIFuncs->SetSecondaryProgressBarText(statusText);
}

void ValidateCDKey(int force, int inConnect)
{
	if (staticGameUIFuncs)
		staticGameUIFuncs->ValidateCDKey(force != 0, inConnect != 0);
}

void VGUI2_OnDisconnectFromServer(int eLoginFailure)
{
	if (staticGameUIFuncs)
		staticGameUIFuncs->OnDisconnectFromServer(eLoginFailure, "");
}

CareerStateType VGuiWrap2_IsInCareerMatch()
{
	if( !staticCareerUI )
		return CAREER_NONE;

	return g_careerState;
}

ICareerUI* VguiWrap2_GetCareerUI()
{
	return staticCareerUI;
}

int VGuiWrap2_GetLocalizedStringLength( const char* label )
{
	if( !label || !vgui2::localize() )
		return 0;

	const wchar_t* pszLocalized = vgui2::localize()->Find( label );

	if( !pszLocalized )
		return 0;

	return wcslen( pszLocalized );
}

void RegisterTutorMessageShown( int mid )
{
	if( mid >= 0 && mid < ARRAYSIZE( s_tutorMessageDecayData ) )
		s_tutorMessageDecayData[ mid ]++;
}

int GetTimesTutorMessageShown( int mid )
{
	if( mid >= 0 && mid < ARRAYSIZE( s_tutorMessageDecayData ) )
		return s_tutorMessageDecayData[ mid ];

	return -1;
}

void ProcessTutorMessageDecayBuffer(int* buffer, int bufferLength)
{
	ResetTutorMessageDecayData();

	int amountToCopy = min((int)ARRAYSIZE(s_tutorMessageDecayData), bufferLength);

	if (amountToCopy > 0)
		memcpy(s_tutorMessageDecayData, buffer, sizeof(int) * amountToCopy);
}

void ConstructTutorMessageDecayBuffer(int* buffer, int bufferLength)
{
	if (!buffer)
		return;

	memset(buffer, 0, sizeof(int) * bufferLength);

	int amountToCopy = min((int)ARRAYSIZE(s_tutorMessageDecayData), bufferLength);

	if (amountToCopy > 0)
		memcpy(buffer, s_tutorMessageDecayData, sizeof(int) * amountToCopy);
}

void ResetTutorMessageDecayData()
{
	memset( s_tutorMessageDecayData, 0, sizeof( s_tutorMessageDecayData ) );
}

void VguiWrap2_GetMouseDelta(int* x, int* y)
{
	g_BaseUISurface.GetMouseDelta(x, y);
}
