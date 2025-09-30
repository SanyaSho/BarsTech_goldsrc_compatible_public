#ifndef COMMON_GAMEUI_IGAMEUI_H
#define COMMON_GAMEUI_IGAMEUI_H

#include "interface.h"

class IBaseSystem;

typedef struct cl_enginefuncs_s cl_enginefunc_t;

// reasons why the user can't connect to a game server
enum ESteamLoginFailure
{
	STEAMLOGINFAILURE_NONE,
	STEAMLOGINFAILURE_BADTICKET,
	STEAMLOGINFAILURE_NOSTEAMLOGIN,
	STEAMLOGINFAILURE_VACBANNED,
	STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE,
	STEAMLOGINFAILURE_CONNECTIONLOST,
	STEAMLOGINFAILURE_NOCONNECTION
};

//-----------------------------------------------------------------------------
// Purpose: contains all the functions that the GameUI dll exports
//-----------------------------------------------------------------------------
class IGameUI : public IBaseInterface
{
public:
	virtual void Initialize( CreateInterfaceFn* factories, int count ) = 0;
	virtual void Start(struct cl_enginefuncs_s* engineFuncs, int interfaceVersion, IBaseSystem* system) = 0;
	virtual void Shutdown() = 0;

	virtual int	ActivateGameUI() = 0;	// activates the menus, returns 0 if it doesn't want to handle it
	virtual int ActivateDemoUI() = 0;	// activates the demo player, returns 0 if it doesn't want to handle it

	virtual int HasExclusiveInput() = 0;

	virtual void RunFrame() = 0;

	virtual void ConnectToServer(const char* game, int IP, int port) = 0;
	virtual void DisconnectFromServer() = 0;
	virtual void HideGameUI() = 0;

	virtual bool IsGameUIActive() = 0;

	virtual void LoadingStarted(const char* resourceType, const char* resourceName) = 0;
	virtual void LoadingFinished(const char* resourceType, const char* resourceName) = 0;

	virtual void StartProgressBar(const char* progressType, int progressSteps) = 0;
	virtual int ContinueProgressBar(int progressPoint, float progressFraction) = 0;
	virtual void StopProgressBar(bool bError, const char* failureReason, const char* extendedReason) = 0;
	virtual int SetProgressBarStatusText(const char* statusText) = 0;

	virtual void SetSecondaryProgressBar(float progress) = 0;
	virtual void SetSecondaryProgressBarText(const char* statusText) = 0;

	virtual void ValidateCDKey(bool force, bool inConnect) = 0;
	virtual void OnDisconnectFromServer(int eSteamLoginFailure, const char* username) = 0;
};

#define GAMEUI_INTERFACE_VERSION "GameUI007"

#endif //COMMON_GAMEUI_IGAMEUI_H
