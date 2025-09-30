#include <cstdio>
#include <cstdlib>

#include "common/SteamCommon.h"
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
#include "steam/steamclientpublic.h"

#include "quakedef.h"

#include "steam/steam_gameserver.h"

#include "server.h"
#include "client.h"
#include "net_ws.h"
#include "host.h"
#include "sv_main.h"
#include "cl_main.h"

#include "common/GameUI/IGameUI.h"
#include "vgui2\BaseUI_Interface.h"

static CSteam3Client s_Steam3Client;
static CSteam3Server *s_Steam3Server;

void CSteam3Server::OnGSPolicyResponse(GSPolicyResponse_t *pPolicyResponse)
{
	if (SteamGameServer()->BSecure())
		Con_Printf(const_cast<char*>("   VAC secure mode is activated.\n"));
	else
		Con_Printf(const_cast<char*>("   VAC secure mode disabled.\n"));
}

void CSteam3Server::OnLogonSuccess(SteamServersConnected_t *pLogonSuccess)
{
	if (m_bLogOnResult)
	{
		if (!m_bLanOnly)
			Con_Printf(const_cast<char*>("Reconnected to Steam servers.\n"));
	}
	else
	{
		m_bLogOnResult = true;
		if (!m_bLanOnly)
			Con_Printf(const_cast<char*>("Connection to Steam servers successful.\n"));
	}

	m_SteamIDGS = SteamGameServer()->GetSteamID();
	CSteam3Server::SendUpdatedServerDetails();
}

uint64 CSteam3Server::GetSteamID()
{
	if (m_bLanOnly)
		return CSteamID(0, k_EUniversePublic, k_EAccountTypeInvalid).ConvertToUint64();
	else
		return m_SteamIDGS.ConvertToUint64();
}

void CSteam3Server::OnLogonFailure(SteamServerConnectFailure_t *pLogonFailure)
{
	if (!m_bLogOnResult)
	{
		if (pLogonFailure->m_eResult == k_EResultServiceUnavailable)
		{
			if (!m_bLanOnly)
			{
				Con_Printf(const_cast<char*>("Connection to Steam servers successful (SU).\n"));
				if (m_bWantToBeSecure)
				{
					Con_Printf(const_cast<char*>("   VAC secure mode not available.\n"));
					m_bLogOnResult = true;
					return;
				}
			}
		}
		else
		{
			if (!m_bLanOnly)
				Con_Printf(const_cast<char*>("Could not establish connection to Steam servers.\n"));
		}
	}

	m_bLogOnResult = true;
}

void CSteam3Server::OnGSClientDeny(GSClientDeny_t *pGSClientDeny)
{
	client_t* cl = CSteam3Server::ClientFindFromSteamID(pGSClientDeny->m_SteamID);
	if (cl)
		OnGSClientDenyHelper(cl, pGSClientDeny->m_eDenyReason, pGSClientDeny->m_rgchOptionalText);
}

void CSteam3Server::OnGSClientDenyHelper(client_t *cl, EDenyReason eDenyReason, const char *pchOptionalText)
{
	switch (eDenyReason)
	{
	case k_EDenyInvalidVersion:
		SV_DropClient(cl, 0, const_cast<char*>("Client version incompatible with server. \nPlease exit and restart"));
		break;

	case k_EDenyNotLoggedOn:
		if (!m_bLanOnly)
			SV_DropClient(cl, 0, const_cast<char*>("No Steam logon\n"));
		break;

	case k_EDenyLoggedInElseWhere:
		if (!m_bLanOnly)
			SV_DropClient(cl, 0, const_cast<char*>("This Steam account is being used in another location\n"));
		break;

	case k_EDenyNoLicense:
		SV_DropClient(cl, 0, const_cast<char*>("This Steam account does not own this game. \nPlease login to the correct Steam account."));
		break;

	case k_EDenyCheater:
		SV_DropClient(cl, 0, const_cast<char*>("VAC banned from secure server\n"));
		break;

	case k_EDenyUnknownText:
		if (pchOptionalText && *pchOptionalText)
			SV_DropClient(cl, 0, (char*)pchOptionalText);
		else
			SV_DropClient(cl, 0, const_cast<char*>("Client dropped by server"));
		break;

	case k_EDenyIncompatibleAnticheat:
		SV_DropClient(cl, 0, const_cast<char*>("You are running an external tool that is incompatible with Secure servers."));
		break;

	case k_EDenyMemoryCorruption:
		SV_DropClient(cl, 0, const_cast<char*>("Memory corruption detected."));
		break;

	case k_EDenyIncompatibleSoftware:
		SV_DropClient(cl, 0, const_cast<char*>("You are running software that is not compatible with Secure servers."));
		break;

	case k_EDenySteamConnectionLost:
		if (!m_bLanOnly)
			SV_DropClient(cl, 0, const_cast<char*>("Steam connection lost\n"));
		break;

	case k_EDenySteamConnectionError:
		if (!m_bLanOnly)
			SV_DropClient(cl, 0, const_cast<char*>("Unable to connect to Steam\n"));
		break;

	case k_EDenySteamResponseTimedOut:
		SV_DropClient(cl, 0, const_cast<char*>("Client timed out while answering challenge.\n---> Please make sure that you have opened the appropriate ports on any firewall you are connected behind.\n---> See http://support.steampowered.com for help with firewall configuration."));
		break;

	case k_EDenySteamValidationStalled:
		if (m_bLanOnly)
			cl->network_userid.m_SteamID = 1;
		break;

	default:
		SV_DropClient(cl, 0, const_cast<char*>("Client dropped by server"));
		break;
	}
}

void CSteam3Server::OnGSClientApprove(GSClientApprove_t *pGSClientSteam2Accept)
{
	client_t* cl = ClientFindFromSteamID(pGSClientSteam2Accept->m_SteamID);
	if (!cl)
		return;

	if (SV_FilterUser(&cl->network_userid))
	{
		char msg[256];
		sprintf(msg, "You have been banned from this server\n");
		SV_RejectConnection(&cl->netchan.remote_address, msg);
		SV_DropClient(cl, 0, const_cast<char*>("STEAM UserID %s is in server ban list\n"), SV_GetClientIDString(cl));
	}
	else if (SV_CheckForDuplicateSteamID(cl) != -1)
	{
		char msg[256];
		sprintf(msg, "Your UserID is already in use on this server.\n");
		SV_RejectConnection(&cl->netchan.remote_address, msg);
		SV_DropClient(cl, 0, const_cast<char*>("STEAM UserID %s is already\nin use on this server\n"), SV_GetClientIDString(cl));
	}
	else
	{
		char msg[512];
		_snprintf(msg, ARRAYSIZE(msg), "\"%s<%i><%s><>\" STEAM USERID validated\n", cl->name, cl->userid, SV_GetClientIDString(cl));
		Con_DPrintf(const_cast<char*>("%s"), msg);
		Log_Printf("%s", msg);
	}
}

void CSteam3Server::OnGSClientKick(GSClientKick_t *pGSClientKick)
{
	client_t* cl = CSteam3Server::ClientFindFromSteamID(pGSClientKick->m_SteamID);
	if (cl)
		CSteam3Server::OnGSClientDenyHelper(cl, pGSClientKick->m_eDenyReason, 0);
}

client_t *CSteam3Server::ClientFindFromSteamID(CSteamID &steamIDFind)
{
	for (int i = 0; i < svs.maxclients; i++)
	{
		auto cl = &svs.clients[i];
		if (!cl->connected && !cl->active && !cl->spawned)
			continue;

		if (cl->network_userid.idtype != AUTH_IDTYPE_STEAM)
			continue;

		if (steamIDFind == cl->network_userid.m_SteamID)
			return cl;
	}

	return NULL;
}

CSteam3Server::CSteam3Server() :
m_CallbackGSClientApprove(this, &CSteam3Server::OnGSClientApprove),
m_CallbackGSClientDeny(this, &CSteam3Server::OnGSClientDeny),
m_CallbackGSClientKick(this, &CSteam3Server::OnGSClientKick),
m_CallbackGSPolicyResponse(this, &CSteam3Server::OnGSPolicyResponse),
m_CallbackLogonSuccess(this, &CSteam3Server::OnLogonSuccess),
m_CallbackLogonFailure(this, &CSteam3Server::OnLogonFailure),
m_SteamIDGS(1, 0, k_EUniverseInvalid, k_EAccountTypeInvalid)
{
	m_bHasActivePlayers = false;
	m_bWantToBeSecure = false;
	m_bLanOnly = false;
}

void CSteam3Server::Activate()
{
	bool bLanOnly;
	int argSteamPort;
	EServerMode eSMode;
	int gamePort;
	char gamedir[MAX_PATH];
	int usSteamPort;
	uint32 unIP;

	if (m_bLoggedOn)
	{
		bLanOnly = sv_lan.value != 0.0;
		if (m_bLanOnly != bLanOnly)
		{
			m_bLanOnly = bLanOnly;
			m_bWantToBeSecure = !COM_CheckParm(const_cast<char*>("-insecure")) && !bLanOnly;
		}
	}
	else
	{
		m_bLoggedOn = true;
		unIP = 0;
		usSteamPort = 26900;
		argSteamPort = COM_CheckParm(const_cast<char*>("-sport"));
		if (argSteamPort > 0)
			usSteamPort = Q_atoi(com_argv[argSteamPort + 1]);
		eSMode = eServerModeAuthenticationAndSecure;
		if (net_local_adr.type == NA_IP)
			unIP = ntohl(*(u_long *)&net_local_adr.ip[0]);

		m_bLanOnly = sv_lan.value > 0.0;
		m_bWantToBeSecure = !COM_CheckParm(const_cast<char*>("-insecure")) && !m_bLanOnly;
		COM_FileBase(com_gamedir, gamedir);

		if (!m_bWantToBeSecure)
			eSMode = eServerModeAuthentication;

		if (m_bLanOnly)
			eSMode = eServerModeNoAuthentication;

		gamePort = (int)iphostport.value;
		if (gamePort == 0)
			gamePort = (int)hostport.value;

		int nAppId = GetGameAppID();
		if (nAppId > 0 && cls.state == ca_dedicated)
		{
			FILE* f = fopen("steam_appid.txt", "w+");
			if (f)
			{
				fprintf(f, "%d\n", nAppId);
				fclose(f);
			}
		}

		if (!SteamGameServer_Init(unIP, usSteamPort, gamePort, 0xFFFFu, eSMode, gpszVersionString))
			Sys_Error("Unable to initialize Steam.");

		SteamGameServer()->SetProduct(gpszProductString);
		SteamGameServer()->SetModDir(gamedir);
		SteamGameServer()->SetDedicatedServer(cls.state == ca_dedicated);
		SteamGameServer()->SetGameDescription(gEntityInterface.pfnGetGameDescription());
		SteamGameServer()->LogOnAnonymous();
		m_bLogOnResult = false;

		if (COM_CheckParm(const_cast<char*>("-nomaster")))
		{
			Con_Printf(const_cast<char*>("Master server communication disabled.\n"));
			gfNoMasterServer = TRUE;
		}
		else
		{
			if (!gfNoMasterServer && svs.maxclients > 1)
			{
				SteamGameServer()->EnableHeartbeats(true);
				double fMasterHeartbeatTimeout = 200.0;
				if (!Q_strcmp(gamedir, "dmc"))
					fMasterHeartbeatTimeout = 150.0;
				if (!Q_strcmp(gamedir, "tfc"))
					fMasterHeartbeatTimeout = 400.0;
				if (!Q_strcmp(gamedir, "cstrike"))
					fMasterHeartbeatTimeout = 400.0;

				SteamGameServer()->SetHeartbeatInterval((int)fMasterHeartbeatTimeout);
				CSteam3Server::NotifyOfLevelChange(true);
			}
		}
	}
}

void CSteam3Server::Shutdown()
{
	if (m_bLoggedOn)
	{
		SteamGameServer()->EnableHeartbeats(0);
		SteamGameServer()->LogOff();

		SteamGameServer_Shutdown();
		m_bLoggedOn = false;
	}
}

bool CSteam3Server::NotifyClientConnect(client_t *client, const void *pvSteam2Key, uint32 ucbSteam2Key)
{
	class CSteamID steamIDClient;
	bool bRet = false;

	if (client == NULL || !m_bLoggedOn)
		return false;

	client->network_userid.idtype = AUTH_IDTYPE_STEAM;

	bRet = SteamGameServer()->SendUserConnectAndAuthenticate(htonl(client->network_userid.clientip), pvSteam2Key, ucbSteam2Key, &steamIDClient);
	client->network_userid.m_SteamID = steamIDClient.ConvertToUint64();

	return bRet;
}

bool CSteam3Server::NotifyBotConnect(client_t *client)
{
	if (client == NULL || !m_bLoggedOn)
		return false;

	client->network_userid.idtype = AUTH_IDTYPE_LOCAL;
	CSteamID steamId = SteamGameServer()->CreateUnauthenticatedUserConnection();
	client->network_userid.m_SteamID = steamId.ConvertToUint64();
	return true;
}

void CSteam3Server::NotifyClientDisconnect(client_t *cl)
{
	if (!cl || !m_bLoggedOn)
		return;

	if (cl->network_userid.idtype == AUTH_IDTYPE_STEAM || cl->network_userid.idtype == AUTH_IDTYPE_LOCAL)
	{
		SteamGameServer()->SendUserDisconnect(cl->network_userid.m_SteamID);
	}
}

void CSteam3Server::NotifyOfLevelChange(bool bForce)
{
	SendUpdatedServerDetails();
	bool iHasPW = (sv_password.string[0] && Q_strcasecmp(sv_password.string, "none"));
	SteamGameServer()->SetPasswordProtected(iHasPW);
	SteamGameServer()->ClearAllKeyValues();

	for (cvar_t *var = cvar_vars; var; var = var->next)
	{
		if (!(var->flags & FCVAR_SERVER))
			continue;

		const char *szVal;
		if (var->flags & FCVAR_PROTECTED)
		{
			if (Q_strlen(var->string) > 0 && Q_strcasecmp(var->string, "none"))
				szVal = "1";
			else
				szVal = "0";
		}
		else
		{
			szVal = var->string;
		}
		SteamGameServer()->SetKeyValue(var->name, szVal);
	}
}

void CSteam3Server::RunFrame()
{
	bool bHasPlayers;
	char szOutBuf[4096];
	double fCurTime;

	static double s_fLastRunFragsUpdate;
	static double s_fLastRunCallback;
	static double s_fLastRunSendPackets;

	if (svs.maxclients <= 1)
		return;

	fCurTime = Sys_FloatTime();
	if (fCurTime - s_fLastRunFragsUpdate > 1.0)
	{
		s_fLastRunFragsUpdate = fCurTime;
		bHasPlayers = false;
		for (int i = 0; i < svs.maxclients; i++)
		{
			client_t* cl = &svs.clients[i];
			if (cl->active || cl->spawned || cl->connected)
			{
				bHasPlayers = true;
				break;
			}
		}

		m_bHasActivePlayers = bHasPlayers;
		SendUpdatedServerDetails();
		bool iHasPW = (sv_password.string[0] && Q_strcasecmp(sv_password.string, "none"));
		SteamGameServer()->SetPasswordProtected(iHasPW);

		for (int i = 0; i < svs.maxclients; i++)
		{
			client_t* cl = &svs.clients[i];
			if (!cl->active)
				continue;

			SteamGameServer()->BUpdateUserData(cl->network_userid.m_SteamID, cl->name, cl->edict->v.frags);
		}

		if (SteamGameServer()->WasRestartRequested())
		{
			Con_Printf(const_cast<char*>("%cMasterRequestRestart\n"), 3);
			if (COM_CheckParm(const_cast<char*>("-steam")))
			{
				Con_Printf(const_cast<char*>("Your server needs to be restarted in order to receive the latest update.\n"));
				Log_Printf("Your server needs to be restarted in order to receive the latest update.\n");
			}
			else
			{
				Con_Printf(const_cast<char*>("Your server is out of date.  Please update and restart.\n"));
			}
		}
	}

	if (fCurTime - s_fLastRunCallback > 0.1)
	{
		SteamGameServer_RunCallbacks();
		s_fLastRunCallback = fCurTime;
	}

	if (fCurTime - s_fLastRunSendPackets > 0.01)
	{
		s_fLastRunSendPackets = fCurTime;

		uint16 port;
		uint32 ip;
		int iLen = SteamGameServer()->GetNextOutgoingPacket(szOutBuf, sizeof(szOutBuf), &ip, &port);
		while (iLen > 0)
		{
			netadr_t netAdr;
			*((uint32*)&netAdr.ip[0]) = htonl(ip);
			netAdr.port = htons(port);
			netAdr.type = NA_IP;

			NET_SendPacket(NS_SERVER, iLen, szOutBuf, netAdr);

			iLen = SteamGameServer()->GetNextOutgoingPacket(szOutBuf, sizeof(szOutBuf), &ip, &port);
		}
	}
}

void CSteam3Server::SendUpdatedServerDetails()
{
	int botCount = 0;
	if (svs.maxclients > 0)
	{

		for (int i = 0; i < svs.maxclients; i++)
		{
			auto cl = &svs.clients[i];
			if ((cl->active || cl->spawned || cl->connected) && cl->fakeclient)
				++botCount;
		}
	}

	int maxPlayers = sv_visiblemaxplayers.value;
	if (maxPlayers < 0)
		maxPlayers = svs.maxclients;

	SteamGameServer()->SetMaxPlayerCount(maxPlayers);
	SteamGameServer()->SetBotPlayerCount(botCount);
	SteamGameServer()->SetServerName(Cvar_VariableString(const_cast<char*>("hostname")));
	SteamGameServer()->SetMapName(sv.name);
}

CSteam3Client::CSteam3Client()
	: m_CallbackClientGameServerDeny( this, &CSteam3Client::OnClientGameServerDeny )
	, m_CallbackGameServerChangeRequested( this, &CSteam3Client::OnGameServerChangeRequested )
	, m_CallbackGameOverlayActivated( this, &CSteam3Client::OnGameOverlayActivated )
{
}

void CSteam3Client::InitClient()
{
	if( !m_bLoggedOn )
	{
		m_bLoggedOn = true;
		_unlink( "steam_appid.txt" );
		if( !getenv( "SteamAppId" ) )
		{
			AppId_t appId = GetGameAppID();
			if( appId > 0 )
			{
				FILE* pFile = fopen( "steam_appid.txt", "w+" );
				if( pFile )
				{
					fprintf( pFile, "%d\n", static_cast<int>( appId ) );
					fclose( pFile );
				}
			}
		}
		if( !SteamAPI_Init() )
			Sys_Error( "Failed to initalize authentication interface. Exiting...\n" );
		m_bLogOnResult = false;
	}
}

void CSteam3Client::Shutdown()
{
	if( m_bLoggedOn )
	{
		SteamAPI_Shutdown();
		m_bLoggedOn = false;
	}
}

int CSteam3Client::InitiateGameConnection( void *pData, int cbMaxData, 
										   uint64 steamID, 
										   uint32 unIPServer, uint16 usPortServer, bool bSecure )
{
	return SteamUser()->InitiateGameConnection( pData, cbMaxData, 
												steamID, 
												unIPServer, usPortServer, bSecure );
}

void CSteam3Client::TerminateConnection( uint32 unIPServer, uint16 usPortServer )
{
	SteamUser()->TerminateGameConnection( unIPServer, usPortServer );
}

void CSteam3Client::RunFrame()
{
	SteamAPI_RunCallbacks();
}

void CSteam3Client::OnClientGameServerDeny( ClientGameServerDeny_t* pParam )
{
	
	COM_ExplainDisconnection( true, const_cast<char*>("Invalid server version, unable to connect.") );
	extern void DbgPrint(FILE*, const char* format, ...);
	extern FILE* m_fMessages;
	DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
	CL_Disconnect();
	
}

void CSteam3Client::OnGameServerChangeRequested( GameServerChangeRequested_t *pGameServerChangeRequested )
{
	char buf[128];
	
	Cvar_DirectSet( &password, pGameServerChangeRequested->m_rgchPassword );
	Con_Printf( const_cast<char*>("Connecting to %s\n"), pGameServerChangeRequested->m_rgchServer );
	Q_strncpy_s(buf, pGameServerChangeRequested->m_rgchServer, 128);
	buf[strcspn(buf, ";\n")] = 0;
	Cbuf_AddText( va( const_cast<char*>("connect %s\n"), buf ) );
	
}

void CSteam3Client::OnGameOverlayActivated( GameOverlayActivated_t *pGameOverlayActivated )
{
	
	
	if( Host_IsSinglePlayerGame() )
	{
		if( pGameOverlayActivated->m_bActive )
		{
			Cbuf_AddText( const_cast<char*>("setpause;") );
		}
		else if( !staticGameUIFuncs->IsGameUIActive())
		{
			Cbuf_AddText( const_cast<char*>("unpause;") );
		}
	}
	
}

void Steam_InitClient()
{
	s_Steam3Client.InitClient();
}

void Steam_ShutdownClient()
{
	s_Steam3Client.Shutdown();
}

void Steam_ClientRunFrame()
{
	s_Steam3Client.RunFrame();
}

bool ISteamApps_BIsSubscribedApp( AppId_t appid )
{
	if( !SteamApps() )
		return false;

	return SteamApps()->BIsSubscribedApp( appid );
}

void Steam_GSTerminateGameConnection(uint32 unIPServer, uint16 usPortServer)
{
	SteamUser()->TerminateGameConnection(ntohl(unIPServer), ntohs(usPortServer));
}

void Steam_Activate()
{
	if (!Steam3Server())
	{
		s_Steam3Server = new CSteam3Server();
		if (s_Steam3Server == NULL)
			return;
	}

	Steam3Server()->Activate();
}

void Steam_RunFrame()
{
	if (Steam3Server())
	{
		Steam3Server()->RunFrame();
	}
}

void Steam_SetCVar(const char *pchKey, const char *pchValue)
{
	if (Steam3Server())
	{
		SteamGameServer()->SetKeyValue(pchKey, pchValue);
	}
}

int Steam_GSInitiateGameConnection(void *pData, int cbMaxData, uint64 steamID, uint32 unIPServer, uint16 usPortServer, qboolean bSecure)
{
	return Steam3Client()->InitiateGameConnection(pData, cbMaxData, steamID, unIPServer, usPortServer, bSecure != 0);
}

uint64 Steam_GSGetSteamID()
{
	return Steam3Server()->GetSteamID();
}

qboolean Steam_GSBSecure()
{
	//useless call
	//Steam3Server();
	return SteamGameServer()->BSecure();
}

qboolean Steam_GSBLoggedOn()
{
	return Steam3Server()->BLoggedOn() && SteamGameServer()->BLoggedOn();
}

qboolean Steam_GSBSecurePreference()
{
	return Steam3Server()->BWantsSecure();
}

TSteamGlobalUserID Steam_Steam3IDtoSteam2(uint64 unSteamID)
{
	class CSteamID steamID = unSteamID;
	TSteamGlobalUserID steam2ID;
	steamID.ConvertToSteam2(&steam2ID);
	return steam2ID;
}

uint64 Steam_StringToSteamID(const char *pStr)
{
	CSteamID steamID;
	if (Steam3Server())
	{
		CSteamID serverSteamId(Steam3Server()->GetSteamID());
		steamID.SetFromSteam2String(pStr, serverSteamId.GetEUniverse());
	}
	else
	{
		steamID.SetFromSteam2String(pStr, k_EUniversePublic);
	}

	return steamID.ConvertToUint64();
}

const char *Steam_GetGSUniverse()
{
	CSteamID steamID(Steam3Server()->GetSteamID());
	switch (steamID.GetEUniverse())
	{
	case k_EUniversePublic:
		return "";

	case k_EUniverseBeta:
		return "(beta)";

	case k_EUniverseInternal:
		return "(internal)";

	default:
		return "(u)";
	}
}

CSteam3Server *Steam3Server()
{
	return s_Steam3Server;
}

CSteam3Client *Steam3Client()
{
	return &s_Steam3Client;
}

void Master_SetMaster_f()
{
	int i;
	const char * pszCmd;

	i = Cmd_Argc();
	if (!Steam3Server())
	{
		Con_Printf(const_cast<char*>("Usage:\nSetmaster unavailable, start a server first.\n"));
		return;
	}

	if (i < 2 || i > 5)
	{
		Con_Printf(const_cast<char*>("Usage:\nSetmaster <enable | disable>\n"));
		return;
	}

	pszCmd = Cmd_Argv(1);
	if (!pszCmd || !pszCmd[0])
		return;

	if (Q_strcasecmp(pszCmd, "disable") || gfNoMasterServer)
	{
		if (!Q_strcasecmp(pszCmd, "enable"))
		{
			if (gfNoMasterServer)
			{
				gfNoMasterServer = FALSE;
				SteamGameServer()->EnableHeartbeats(gfNoMasterServer != 0);
			}
		}
	}
	else
	{
		gfNoMasterServer = TRUE;
		SteamGameServer()->EnableHeartbeats(gfNoMasterServer != 0);
	}
}

void Steam_HandleIncomingPacket(byte *data, int len, int fromip, uint16 port)
{
	SteamGameServer()->HandleIncomingPacket(data, len, fromip, port);
}

const char* Steam_GetCommunityName()
{
	if (SteamFriends())
		return SteamFriends()->GetPersonaName();

	return NULL;
}

qboolean Steam_NotifyClientConnect(client_t* cl, const void* pvSteam2Key, unsigned int ucbSteam2Key)
{
	if (Steam3Server())
	{
		return Steam3Server()->NotifyClientConnect(cl, pvSteam2Key, ucbSteam2Key);
	}
	return FALSE;
}

qboolean Steam_NotifyBotConnect(client_t* cl)
{
	if (Steam3Server())
	{
		return Steam3Server()->NotifyBotConnect(cl);
	}
	return FALSE;
}

void Steam_NotifyClientDisconnect(client_t* cl)
{
	if (Steam3Server())
	{
		Steam3Server()->NotifyClientDisconnect(cl);
	}
}

void Steam_NotifyOfLevelChange()
{
	if (Steam3Server())
	{
		Steam3Server()->NotifyOfLevelChange(false);
	}
}

void Steam_Shutdown()
{
	if (Steam3Server())
	{
		Steam3Server()->Shutdown();
		delete s_Steam3Server;
		s_Steam3Server = NULL;
	}
}

uint64 ISteamGameServer_CreateUnauthenticatedUserConnection()
{
	if (!SteamGameServer())
	{
		return 0L;
	}

	return SteamGameServer()->CreateUnauthenticatedUserConnection().ConvertToUint64();
}

bool ISteamGameServer_BUpdateUserData(uint64 steamid, const char* netname, uint32 score)
{
	if (!SteamGameServer())
	{
		return false;
	}

	return SteamGameServer()->BUpdateUserData(steamid, netname, score);
}
