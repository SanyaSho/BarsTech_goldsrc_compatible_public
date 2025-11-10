#ifndef ENGINE_VGUI_INT_H
#define ENGINE_VGUI_INT_H

#include <VGUI_Panel.h>

#include "GameUI/CareerDefs.h"

class ICareerUI;

//VGUI interface

void VGui_ViewportPaintBackground( int* extents );

void VGui_Startup();

void VGui_Shutdown();

void VGui_CallEngineSurfaceAppHandler( void* event, void* userData );

void* VGui_GetPanel();

void VGui_ReleaseMouse();

void VGui_GetMouse();

void VGui_SetVisible( int state );

void VGui_Paint();

int VGui_GameUIKeyPressed();

int VGui_Key_Event( int down, int keynum, const char* pszCurrentBinding );

int VGui_LoadBMP( FileHandle_t file, byte* buffer, int bufsize, int* width, int* height );

//VGUI1 wrappers

void VGuiWrap_SetRootPanelSize();

void VGuiWrap_Startup();

void VGuiWrap_Shutdown();

int VGuiWrap_CallEngineSurfaceAppHandler( void* event, void* userData );

vgui::Panel* VGuiWrap_GetPanel();

void VGuiWrap_ReleaseMouse();

void VGuiWrap_GetMouse();

void VGuiWrap_SetVisible( int state );

void VGuiWrap_Paint( int paintAll );

//VGUI2 wrappers

void VGuiWrap2_Startup();

void VGuiWrap2_Shutdown();

int VGuiWrap2_CallEngineSurfaceAppHandler( void* event, void* userData );

int VGuiWrap2_IsGameUIVisible();

int VGuiWrap2_UseVGUI1();

void* VGuiWrap2_GetPanel();

void VGuiWrap2_ReleaseMouse();

void VGuiWrap2_GetMouse();

void VGuiWrap2_SetVisible( int state );

int VGuiWrap2_GameUIKeyPressed();

int VGuiWrap2_Key_Event( int down, int keynum, const char* pszCurrentBinding );

void VGuiWrap2_Paint();

void VGuiWrap2_NotifyOfServerDisconnect();

void VGuiWrap2_HideGameUI();

int VGuiWrap2_IsConsoleVisible();

void VGuiWrap2_ShowConsole();

void VGuiWrap2_ShowDemoPlayer();

void VGuiWrap2_HideConsole();

void VGuiWrap2_ClearConsole();

void VGuiWrap2_ConPrintf( const char* msg );

void VGuiWrap2_ConDPrintf( const char* msg );

void VGuiWrap2_LoadingStarted( const char* resourceType, const char* resourceName );

void VGuiWrap2_LoadingFinished( const char* resourceType, const char* resourceName );

void VGuiWrap2_NotifyOfServerConnect( const char* game, int IP, int port );

CareerStateType VGuiWrap2_IsInCareerMatch();

ICareerUI* VguiWrap2_GetCareerUI();

int VGuiWrap2_GetLocalizedStringLength( const char* label );

void VguiWrap2_GetMouseDelta( int* x, int* y );

void VGUI2_OnDisconnectFromServer( int eLoginFailure );

void StartLoadingProgressBar( const char* loadingType, int numProgressPoints );

void ContinueLoadingProgressBar( const char* loadingType, int progressPoint, float progressFraction );

void SetLoadingProgressBarStatusText( const char* statusText );

void StopLoadingProgressBar();

void SetSecondaryProgressBar( float progress );

void SetSecondaryProgressBarText( const char *statusText );

void ValidateCDKey( int force, int inConnect );

void RegisterTutorMessageShown( int mid );

int GetTimesTutorMessageShown( int mid );

void ProcessTutorMessageDecayBuffer( int* buffer, int bufferLength );

void ConstructTutorMessageDecayBuffer( int* buffer, int bufferLength );

void ResetTutorMessageDecayData();

#endif //ENGINE_VGUI_INT_H
