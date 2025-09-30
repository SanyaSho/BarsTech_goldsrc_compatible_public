#ifndef ENGINE_SV_STEAM3_H
#define ENGINE_SV_STEAM3_H

#include "common/SteamCommon.h"
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
#include "steam/steamclientpublic.h"
#include "server.h"

/**
*	Base class for Steam3 systems.
*/
class CSteam3
{
public:
	CSteam3() = default;
	virtual ~CSteam3() {}

	virtual void Shutdown() = 0;

protected:
	bool m_bLoggedOn = false;
	bool m_bLogOnResult = false;
	HSteamPipe m_hSteamPipe = 0;
};

class CSteam3Client final : public CSteam3
{
public:
	CSteam3Client();

	void InitClient();
	void Shutdown() override;

	int InitiateGameConnection( void *pData, int cbMaxData, uint64 steamID, uint32 unIPServer, uint16 usPortServer, bool bSecure );

	void TerminateConnection( uint32 unIPServer, uint16 usPortServer );

	void RunFrame();

private:
	STEAM_CALLBACK( CSteam3Client, OnClientGameServerDeny, ClientGameServerDeny_t, m_CallbackClientGameServerDeny );
	STEAM_CALLBACK( CSteam3Client, OnGameServerChangeRequested, GameServerChangeRequested_t, m_CallbackGameServerChangeRequested );
	STEAM_CALLBACK( CSteam3Client, OnGameOverlayActivated, GameOverlayActivated_t, m_CallbackGameOverlayActivated );
};

class CSteam3Server : public CSteam3
{
public:

	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnGSClientApprove, GSClientApprove_t, m_CallbackGSClientApprove);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnGSClientDeny, GSClientDeny_t, m_CallbackGSClientDeny);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnGSClientKick, GSClientKick_t, m_CallbackGSClientKick);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnGSPolicyResponse, GSPolicyResponse_t, m_CallbackGSPolicyResponse);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnLogonSuccess, SteamServersConnected_t, m_CallbackLogonSuccess);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnLogonFailure, SteamServerConnectFailure_t, m_CallbackLogonFailure);

protected:
	bool m_bHasActivePlayers;
	bool m_bWantToBeSecure;
	bool m_bLanOnly;
	CSteamID m_SteamIDGS;

public:

	void SetServerType();
	void SetSpawnCount(int count);

	bool BSecure()      const { return m_bWantToBeSecure; }
	bool BLanOnly()     const { return m_bLanOnly; };
	bool BWantsSecure() const { return m_bWantToBeSecure; }
	bool BLoggedOn()    const { return m_bLoggedOn; }

	uint64 GetSteamID();

	void OnGSClientDenyHelper(client_t *cl, EDenyReason eDenyReason, const char *pchOptionalText);
	client_t *ClientFindFromSteamID(class CSteamID &steamIDFind);

	CSteam3Server();

	void Activate();
	virtual void Shutdown();
	bool NotifyClientConnect(client_t *client, const void *pvSteam2Key, uint32 ucbSteam2Key);
	bool NotifyBotConnect(client_t *client);
	void NotifyClientDisconnect(client_t *cl);
	void NotifyOfLevelChange(bool bForce);
	void RunFrame();
	void SendUpdatedServerDetails();
};

extern CSteam3Server *s_Steam3Server;
extern CSteam3Client s_Steam3Client;

uint64 ISteamGameServer_CreateUnauthenticatedUserConnection();
bool ISteamGameServer_BUpdateUserData(uint64 steamid, const char *netname, uint32 score);
bool ISteamApps_BIsSubscribedApp(uint32 appid);
const char *Steam_GetCommunityName();
qboolean Steam_NotifyClientConnect(client_t *cl, const void *pvSteam2Key, unsigned int ucbSteam2Key);
qboolean Steam_NotifyBotConnect(client_t *cl);
void Steam_NotifyClientDisconnect(client_t *cl);
void Steam_NotifyOfLevelChange();
void Steam_Shutdown();
void Steam_Activate();
void Steam_RunFrame();
void Steam_ClientRunFrame();
int Steam_GSInitiateGameConnection(void *pData, int cbMaxData, uint64 steamID, uint32 unIPServer, uint16 usPortServer, qboolean bSecure);
uint64 Steam_GSGetSteamID();
qboolean Steam_GSBSecure();
qboolean Steam_GSBLoggedOn();
qboolean Steam_GSBSecurePreference();
TSteamGlobalUserID Steam_Steam3IDtoSteam2(uint64 unSteamID);
uint64 Steam_StringToSteamID(const char *pStr);
const char *Steam_GetGSUniverse();
CSteam3Server *Steam3Server();
CSteam3Client *Steam3Client();
void Master_SetMaster_f();
void Steam_HandleIncomingPacket(byte *data, int len, int fromip, uint16 port);
const char* Steam_GetCommunityName();

void Steam_InitClient();
void Steam_ShutdownClient();

void Steam_ClientRunFrame();

bool ISteamApps_BIsSubscribedApp( AppId_t appid );

void Steam_SetCVar( const char* pchKey, const char* pchValue );

void Steam_GSTerminateGameConnection(uint32 unIPServer, uint16 usPortServer);

#endif //ENGINE_SV_STEAM3_H
