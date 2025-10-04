#include "quakedef.h"
#include "client.h"
#include "cl_parse.h"
#include "cl_tent.h"
#include "protocol.h"
#include "screen.h"
#include "vgui_int.h"
#include "host.h"
#include "delta.h"
#include "cl_main.h"
#include "sound.h"
#include "vid.h"
#include "hashpak.h"
#include "net_ws.h"
#include "decals.h"
#include "r_local.h"
#include "modinfo.h"
#include "r_part.h"
#include "cd.h"
#include "cl_ents.h"
#include "cl_pmove.h"
#include "pm_shared/pm_movevars.h"
#include "net_api_int.h"
#include "voice.h"
#include "cl_parsefn.h"
#include "cl_demo.h"
#include "cl_spectator.h"
#include "host_cmd.h"
#include "hltv.h"
#include "download.h"
#include "r_studio.h"
#include "cl_main.h"
#include "r_trans.h"
#include "pr_cmds.h"

#if defined(GLQUAKE)
#include "glquake.h"
#include "gl_screen.h"
#endif

int CL_UPDATE_BACKUP = 1 << 6;
int CL_UPDATE_MASK = CL_UPDATE_BACKUP - 1;

cvar_t cl_clockreset = { const_cast<char*>("cl_clock"), const_cast<char*>("0.1") };
cvar_t cl_filterstuffcmd = { const_cast<char*>("cl_filterstuffcmd"), const_cast<char*>("0"), FCVAR_ARCHIVE | FCVAR_PRIVILEGED };
cvar_t cl_allow_upload = { const_cast<char*>("cl_allow_upload"), const_cast<char*>("1"), FCVAR_ARCHIVE };
cvar_t cl_allow_download = { const_cast<char*>("cl_allow_download"), const_cast<char*>("1"), FCVAR_ARCHIVE };
cvar_t cl_download_ingame = { const_cast<char*>("cl_download_ingame"), const_cast<char*>("1"), FCVAR_ARCHIVE };
cvar_t cl_showmessages = { const_cast<char*>("cl_showmessages"), const_cast<char*>("0") };
cvar_t cl_weaponlistfix = { const_cast<char*>("cl_weaponlistfix"), const_cast<char*>("0") };
double g_flLatency, g_clockdelta;
double parsecounttime;


void CL_PlayerDropped(int nPlayerNumber)
{
	COM_ClearCustomizationList(&cl.players[nPlayerNumber].customdata, true);
}

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
cl_entity_t* CL_EntityNum(int num)
{
	if (num >= cl.num_entities)
	{
		if (num >= cl.max_edicts)
			Host_Error("CL_EntityNum: %i is an invalid number, cl.max_edicts is %i", num, cl.max_edicts);
		while (cl.num_entities <= num)
		{
			cl_entities[num].index = num;
			cl.num_entities++;
		}
	}

	//Con_SafePrintf(__FUNCTION__ " accesing cl_entities [ %d ] as model %s\r\n", num, cl_entities[num].model->name);
	return &cl_entities[num];
}

void AddNewUserMsg()
{
	UserMsg umsg, *pumsg, *pList;
	int i;
	int fFound;

	umsg.iMsg = MSG_ReadByte();
	umsg.pfn = 0;
	umsg.iSize = MSG_ReadByte();

	if (umsg.iSize == 255)
		umsg.iSize = -1;

	//                                ?
	i = MSG_ReadLong();
	Q_strncpy(umsg.szName, (const char *)&i, 4);
	i = MSG_ReadLong();
	Q_strncpy(&umsg.szName[4], (const char *)&i, 4);
	i = MSG_ReadLong();
	Q_strncpy(&umsg.szName[8], (const char *)&i, 4);
	i = MSG_ReadLong();
	Q_strncpy(&umsg.szName[12], (const char *)&i, 4);
	umsg.szName[15] = 0;

	fFound = 0;
	for (pList = gClientUserMsgs; pList != NULL; pList = pList->next)
	{
		if (pList->iMsg == umsg.iMsg)
			pList->iMsg = 0;

		if (!Q_strcasecmp(pList->szName, umsg.szName))
		{
			fFound = 1;
			pList->iMsg = umsg.iMsg;
			pList->iSize = umsg.iSize;
			// MARK: test;
			//umsg.pfn = pList->pfn;
		}
	}

	if (!fFound)
	{
		pumsg = (UserMsg*)Mem_Malloc(sizeof(UserMsg));
		Q_memcpy(pumsg, &umsg, sizeof(UserMsg));
		pumsg->next = gClientUserMsgs;
		gClientUserMsgs = pumsg;
	}
}

//                           svc`   
//            DispatchMessage              
void DispatchUserMsg(int iMsg)
{
	static char buf[192];
	UserMsg* pList;
	int MsgSize;

	if (iMsg <= SVC_LASTMSG || iMsg >= MAX_USERMESSAGES)
		Host_Error(__FUNCTION__ ":  Illegal User Msg %d\n", iMsg);

	// Con_SafePrintf("trying to dispatch message %d\r\n", iMsg);

	for (pList = gClientUserMsgs; pList != NULL; pList = pList->next)
	{
		if (pList->iMsg == iMsg)
			break;
	}

	if (pList == NULL)
	{
		Host_Error("UserMsg: Not Present on Client %d\n", iMsg);
		return;
	}

	//Con_SafePrintf("detected message %s with pfn %08X\r\n", pList->szName, pList->pfn);

	MsgSize = pList->iSize;

	// размер не задан явно
	if (MsgSize == -1)
		MsgSize = MSG_ReadByte();

	if (cl_showmessages.value != 0.0)
		Con_DPrintf(const_cast<char*>("UserMsg: %s (size %i)\n"), pList->szName, MsgSize);

	if (MsgSize > sizeof(buf))
	{
		Host_Error(__FUNCTION__ ":  User Msg %s/%d sent too much data (%i bytes), %i bytes max.\n", pList->szName, pList->iMsg, MsgSize, sizeof(buf));
		return;
	}

	MSG_ReadBuf(MsgSize, buf);

	if (pList->pfn != NULL)
	{
		if (Q_strcasecmp(pList->szName, "WeaponList")
			|| MsgSize <= 123 //                          ?
			|| (GetGameAppID() == HALF_LIFE_APPID && Q_strcasecmp(com_gamedir, "valve") && cl_weaponlistfix.value <= 0.0)
			)
		{
			pList->pfn(pList->szName, MsgSize, buf);
		}
		else
		{
			Con_Printf(const_cast<char*>("Malformed WeaponList request, ignoring\n"));
		}
	}
	else
	{
		Con_DPrintf(const_cast<char*>("UserMsg: No pfn %s %d\n"), pList->szName, iMsg);
	}
}

void DispatchMsgOld(int iMsg, void* pBuf)
{
	//
}

int DispatchDirectUserMsg(const char *pszName, int iSize, void *pBuf)
{
	UserMsg *pmsg;

	for (pmsg = gClientUserMsgs; pmsg != NULL; pmsg = pmsg->next)
	{
		if (Q_strcasecmp(pszName, pmsg->szName))
			continue;

		if (pmsg->pfn != NULL)
			pmsg->pfn(pmsg->szName, pmsg->iSize == -1 ? iSize : pmsg->iSize, pBuf);
		else
			Con_DPrintf(const_cast<char*>("UserMsg: No pfn %s %d\n"), pmsg->szName, pmsg->iMsg);

		return 1;
	}

	return 0;
}

pfnUserMsgHook HookServerMsg(const char* pszName, pfnUserMsgHook pfn)
{
	UserMsg *pList, *pLastMatch;
	pfnUserMsgHook pfnRet;

	pLastMatch = NULL;

	for (pList = gClientUserMsgs; pList != NULL; pList = pList->next)
	{
		if (!Q_strcasecmp(pszName, pList->szName))
		{
			pLastMatch = pList;
			
			if (pList->pfn == pfn)
			{
				//Con_SafePrintf("found pfn %08X\r\n", pList->pfn);
				pfnRet = pList->pfn;

				return pfnRet;
			}
		}
	}

	pList = (UserMsg*)Mem_ZeroMalloc(sizeof(UserMsg));
	
	if (pLastMatch != NULL)
	{
		Q_memcpy(pList, pLastMatch, sizeof(UserMsg));
	}
	else
	{
		Q_strncpy(pList->szName, pszName, sizeof(pList->szName) - 1);
		pList->szName[sizeof(pList->szName) - 1] = 0;
	}

	pList->pfn = pfn;
	pList->next = gClientUserMsgs;

	gClientUserMsgs = pList;

	//Con_SafePrintf("set pfn to %08X\r\n", pList->pfn);

	pfnRet = NULL;

	return pfnRet;
}

void CL_ShutDownUsrMessages()
{
	UserMsg *p, *pNext;

	for (p = gClientUserMsgs; p != NULL; p = pNext)
	{
		pNext = p->next;
		Mem_Free(p);
	}

	gClientUserMsgs = NULL;
}

void CL_Parse_Sound()
{
	int field_mask, channel, ent, sound_num, pitch;
	float volume, attenuation;
	vec3_t pos;
	sfx_t sfxsentence, *sfx;

	MSG_StartBitReading(&net_message);
	field_mask = MSG_ReadBits(9);

	volume = 1;
	if (field_mask & SND_VOLUME)
		volume = (float)MSG_ReadBits(8) / 255.f;

	attenuation = 1;
	if (field_mask & SND_ATTENUATION)
		attenuation = (float)MSG_ReadBits(8) / 64.f;

	// MARK: как в HL25 использовать дополнительные каналы при старом протоколе?
	channel = MSG_ReadBits(3);

	// 11 <- Q_log2(MAX_EDICTS)
	ent = MSG_ReadBits(11);

	if (field_mask & SND_NUMBER)
		sound_num = MSG_ReadBits(16);
	else
		sound_num = MSG_ReadBits(8);

	MSG_ReadBitVec3Coord(pos);

	pitch = PITCH_NORM;
	if (field_mask & SND_PITCH)
		pitch = MSG_ReadBits(8);
	
	MSG_EndBitReading(&net_message);

	if (ent >= cl.max_edicts)
	{
		Con_DPrintf(const_cast<char*>("CL_Parse_Sound: ent = %i, cl.max_edicts %i\n"), ent, cl.max_edicts);
		return;
	}

	if (field_mask & SND_SENTENCE)
	{
		if (sound_num >= CVOXFILESENTENCEMAX)
			_snprintf(sfxsentence.name, sizeof(sfxsentence.name), "!#%d", sound_num - CVOXFILESENTENCEMAX);
		else
			_snprintf(sfxsentence.name, sizeof(sfxsentence.name), "!%s", rgpszrawsentence[sound_num]);

		sfx = &sfxsentence;
	}
	else
		sfx = cl.sound_precache[sound_num];
	
	if (channel == CHAN_STATIC)
		S_StartStaticSound(ent, CHAN_STATIC, sfx, pos, volume, attenuation, field_mask, pitch);
	else 
		S_StartDynamicSound(ent, channel, sfx, pos, volume, attenuation, field_mask, pitch);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void CL_UpdateUserinfo
//-----------------------------------------------------------------------------
void CL_UpdateUserinfo(void)
{
	int		slot;
	player_info_t	*player;

	slot = MSG_ReadByte();
	if (slot >= MAX_CLIENTS)
	{
		Host_EndGame(const_cast<char*>("CL_ParseServerMessage: svc_updateuserinfo > MAX_CLIENTS"));
	}

	player = &cl.players[slot];

	player->userid = MSG_ReadLong();
	Q_strncpy(player->userinfo, MSG_ReadString(), sizeof(player->userinfo) - 1);
	player->userinfo[sizeof(player->userinfo) - 1] = 0;
	MSG_ReadBuf(sizeof(player->hashedcdkey), player->hashedcdkey);
	
	Q_strncpy(player->name, Info_ValueForKey(player->userinfo, "name"), sizeof(player->name) - 1);
	player->name[sizeof(player->name) - 1] = 0;

	Q_strncpy(player->model, Info_ValueForKey(player->userinfo, "model"), sizeof(player->model) - 1);
	player->model[sizeof(player->model) - 1] = 0;

	player->topcolor = Q_atoi(Info_ValueForKey(player->userinfo, "topcolor"));
	player->bottomcolor = Q_atoi(Info_ValueForKey(player->userinfo, "bottomcolor"));
	player->spectator = Q_atoi(Info_ValueForKey(player->userinfo, "*hltv"));
#ifdef _WIN32
	player->m_nSteamID = _atoi64(Info_ValueForKey(player->userinfo, "*sid"));
#else
	player->m_nSteamID = strtoll(Info_ValueForKey(player->userinfo, "*sid") , 0, 10);
#endif

	if (!player->userinfo[0] || !player->name[0])
		CL_PlayerDropped(slot);

	// MARK: HL25
#if defined( FEATURE_HL25 )
	Steam_UpdateRichPresence();
#endif // FEATURE_HL25
}

qboolean CL_CheckFile(sizebuf_t *msg, char *filename)
{
	char file[260];

	Q_strncpy(file, filename, sizeof(file) - 1);
	file[sizeof(file) - 1] = 0;

	if (cl_allow_download.value == 0.0)
	{
		Con_DPrintf(const_cast<char*>("Download refused, cl_allow_download is 0\n"));
		return true;
	}
	else if (cls.state == ca_active && cl_download_ingame.value == 0.0)
	{
		Con_DPrintf(const_cast<char*>("In-game download refused...\n"));
		return true;
	}
	else
	{
		if (FS_FileExists(file))
			return true;

		if (cls.passive || cls.demoplayback)
		{
			Con_Printf(const_cast<char*>("Warning! File %s missing during demo playback.\n"), file);
			return true;
		}
		else
		{
			if (CL_CanUseHTTPDownload())
			{
				CL_QueueHTTPDownload(file);
			}
			else
			{
				MSG_WriteByte(msg, clc_stringcmd);
				MSG_WriteString(msg, va(const_cast<char*>("dlfile %s"), file));
			}
			return false;
		}
	}
}

void CL_ClearResourceList(resource_t* pList)
{
	resource_t *pres, *cur;

	pres = pList->pNext;

	if (pList->pNext == NULL)
		return;

	for (cur = pres; cur && cur != pList; cur = pres)
	{
		pres = cur->pNext;
		CL_RemoveFromResourceList(cur);
		Mem_Free(cur);
	}

	pList->pNext = pList;
	pList->pPrev = pList;
}

void CL_ClearResourceLists()
{
	cl.downloadUrl[ 0 ] = '\0';

	CL_ClearResourceList(&cl.resourcesneeded);
	CL_ClearResourceList(&cl.resourcesonhand);
}

qboolean CL_CheckCRCs(char *pszMap)
{
	char szDllName[64];

	if (fs_startup_timings.value != 0)
		AddStartupTiming("begin CL_CheckCRCs()");

	ContinueLoadingProgressBar("ClientConnect", 4, 0.0);
	SetLoadingProgressBarStatusText("#GameUI_CheckCRCs");

	if (Host_IsServerActive() == false)
	{
		CRC32_Init(&cl.mapCRC);
		if (CRC_MapFile(&cl.mapCRC, pszMap))
		{
			if (cl.mapCRC != cl.serverCRC && cls.demoplayback == false)
			{
				COM_ExplainDisconnection(true, const_cast<char*>("Your map [%s] differs from the server's."), pszMap);
				Host_Error("Disconnected");
			}
		}
		else
		{
			if (FS_FileExists(pszMap))
			{
				COM_ExplainDisconnection(true, const_cast<char*>("#GameUI_ConnectionFailed"), pszMap);
				Host_Error("Disconnected");
			}
			if (cl_allow_download.value == 0.0)
			{
				COM_ExplainDisconnection(true, const_cast<char*>("Refusing to download map %s, (cl_allow_download is 0 ) disconnecting.\n"), pszMap);
				Host_Error("Disconnected");
			}
			Con_Printf(const_cast<char*>("Couldn't find map %s, server will download the map\n"), pszMap);
			cl.mapCRC = cl.serverCRC;
		}
	}

	if (gmodinfo.clientcrccheck)
	{
		snprintf(szDllName, sizeof(szDllName), "cl_dlls//client.dll");
		if (!MD5_Hash_File(cls.md5_clientdll, szDllName, 0, 0, 0))
		{
			COM_ExplainDisconnection(true, const_cast<char*>("Couldn't CRC client side dll %s."), szDllName);
			Host_Error("Disconnected");
		}
		if (Q_memcmp(cl.clientdllmd5, cls.md5_clientdll, sizeof(cl.clientdllmd5)))
		{
			if (cls.demoplayback == false && cls.spectator == false)
			{
				COM_ExplainDisconnection(true, const_cast<char*>("Your .dll [%s] differs from the server's."), szDllName);
				Host_Error("Disconnected");
			}
			Con_Printf(const_cast<char*>(
				"Your client side .dll [%s] failed the CRC check.\n"
				"The .dll may be out of date, or the demo may have been recorded using an old version of the .dll.\n"
				"Consider obtaining an updated version of the .dll from the server operator.\n"
				"Demo playback proceeding.\n"),
				szDllName);
		}
	}

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("end   CL_CheckCRCs()");

	return true;
}

void CL_PrecacheBSPModels(char *pfilename)
{
	resource_t *p, *n;

	if (pfilename == NULL)
		return;

	if (Q_strncasecmp(pfilename, "maps/", Q_strlen("maps/")))
		return;

	if (!strstr(pfilename, ".bsp"))
		return;

	ContinueLoadingProgressBar("ClientConnect", 9, 0.0);

	if (cl.resourcesonhand.pNext == &cl.resourcesonhand)
		return;

	for (p = cl.resourcesonhand.pNext; p != &cl.resourcesonhand; p = n)
	{
		n = p->pNext;

		if (p->type != t_model || p->szFileName[0] != '*')
			continue;

		cl.model_precache[p->nIndex] = Mod_ForName(p->szFileName, false, false);

		if (!cl.model_precache[p->nIndex])
		{
			Con_Printf(const_cast<char*>("Model %s not found\n"), p->szFileName);
			if (p->ucFlags & RES_FATALIFMISSING)
			{
				COM_ExplainDisconnection(true, const_cast<char*>("Cannot continue without model %s, disconnecting."), p->szFileName);
				extern void DbgPrint(FILE*, const char* format, ...);
				extern FILE* m_fMessages;
				DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
				CL_Disconnect();
				break;
			}
		}
	}
}

void CL_SendConsistencyInfo(sizebuf_t* msg) 
{
	consistency_t* pc;
	int savepos;
	int iRunningSteam;
	char filename[MAX_OSPATH], szSteamFilename[MAX_OSPATH];
	byte md5[16];
	CRC32_t crcFile;
	qboolean bCountryExistence, user_changed_diskfile;
	int len;
	vec3_t mins, maxs; // SH_CODE: make it zero?

	iRunningSteam = COM_CheckParm(const_cast<char*>("-steam"));
	
	if (!cl.need_force_consistency_response)
		return;

	// Only one consistency response is needed
	cl.need_force_consistency_response = false;

	// Start message with appropriate opcode
	MSG_WriteByte(msg, clc_fileconsistency);

	// Save current message size, so we can later differentiate
	// and determine how much we've written
	savepos = msg->cursize;
	MSG_WriteShort(msg, 0);
	MSG_StartBitWriting(msg);

	for (int i = 0; i < cl.num_consistency; ++i) 
	{
		pc = &cl.consistency_list[i];
		if (!pc)
			continue;

		MSG_WriteBits(1, 1);
		MSG_WriteBits(pc->orig_index, 12);

		if (pc->issound) 
		{
			_snprintf(filename, sizeof(filename), "sound/%s", pc->filename);
		}
		else 
		{
			// Don't copy last char, as we'll overwrite it
			Q_strncpy(filename, pc->filename, sizeof(filename) - 1);
			filename[sizeof(filename) - 1] = '\0';
		}

		bCountryExistence = true;
		if (iRunningSteam) 
		{
			_snprintf(szSteamFilename, sizeof(szSteamFilename), "%s", filename);
			COM_FixSlashes(szSteamFilename);
			bCountryExistence = FS_FileExists(szSteamFilename);
		}

		// Used to detect whether files have been modified since starting the game
		user_changed_diskfile = false;
		if (strstr(filename, "models/") && bCountryExistence) 
		{
			CRC32_Init(&crcFile);
			CRC_File(&crcFile, filename);
			crcFile = CRC32_Final(crcFile);
			user_changed_diskfile = Mod_ValidateCRC(filename, crcFile) == false;
		}

		switch (pc->check_type)
		{
		case force_exactfile:
			MD5_Hash_File(md5, filename, 0, 0, NULL);
			pc->value = *(int*)&md5[0];
			MSG_WriteBits(user_changed_diskfile ? 0 : pc->value, 32);
			break;
		case force_model_samebounds:
		case force_model_specifybounds:
			if (!R_GetStudioBounds(filename, mins, maxs))
				Host_Error("Unable to find %s\n", filename);

			if (user_changed_diskfile)
			{
				maxs[0] = maxs[1] = maxs[2] = 9999.9f;
				mins[0] = mins[1] = mins[2] = -9999.9f;
			}

			MSG_WriteBitData(mins, 12);
			MSG_WriteBitData(maxs, 12);
			break;
		case force_model_specifybounds_if_avail:
			if (!bCountryExistence)
				mins[0] = mins[1] = mins[2] = maxs[0] = maxs[1] = maxs[2] = -1.f;
			else
			{
				if (!R_GetStudioBounds(filename, mins, maxs))
					Host_Error("Unable to find %s\n", filename);

				if (user_changed_diskfile)
				{
					maxs[0] = maxs[1] = maxs[2] = 9999.9f;
					mins[0] = mins[1] = mins[2] = -9999.9f;
				}
			}
			MSG_WriteBitData(mins, 12);
			MSG_WriteBitData(maxs, 12);
			break;
		default:
			return Host_Error("Unknown consistency type %i\n", pc->check_type);
		}
	}

	MSG_WriteBits(0, 1);
	MSG_EndBitWriting(msg);

	len = msg->cursize - savepos - 2;
	*(word*)&msg->data[savepos] = len;

	COM_Munge(&msg->data[savepos + 2], len, CL_GetServerCount());
}

void CL_ResourcesRelated(int arg1, int arg2)
{
	//
}

void CL_RegisterResources(sizebuf_t *msg)
{
	int mungebuffer;

	if (cls.dl.custom || (cls.signon == SIGNONS && cls.state == ca_active))
	{
		cls.dl.custom = false;
	}
	else
	{
		if (fs_startup_timings.value != 0.0)
			AddStartupTiming("begin CL_RegisterResources()");

		ContinueLoadingProgressBar("ClientConnect", 8, 0.0);

		if (cls.demoplayback == false)
			CL_SendConsistencyInfo(msg);

		// cl.model_precache: 0 не используется, 1 всегда игровой мир
		cl.worldmodel = cl.model_precache[1];

		if (cl.worldmodel && cl.maxclients > 0)
		{
			if (cl_entities == NULL)
				CL_ReallocateDynamicData(cl.maxclients);

			// cl_entities <-> &cl_entities[0]
			cl_entities->model = cl.worldmodel;

			CL_PrecacheBSPModels(cl.model_precache[1]->name);

			if (cls.state != ca_disconnected)
			{
				Con_DPrintf(const_cast<char*>("Setting up renderer...\n"));

				// all done
				R_NewMap();
				Hunk_Check();		// make sure nothing is hurt
				noclip_anglehack = false;
				if (cls.passive == false)
				{
					mungebuffer = cl.mapCRC;
					MSG_WriteByte(msg, clc_stringcmd);
					COM_Munge2((byte*)&mungebuffer, sizeof(mungebuffer), ~CL_GetServerCount());
					MSG_WriteString(msg, va(const_cast<char*>("spawn %i %i"), CL_GetServerCount(), mungebuffer));
				}

				CL_InitSpectator();

				if (fs_startup_timings.value != 0.0)
					AddStartupTiming("end   CL_RegisterResources()");
			}
		}
		else
		{
			Con_Printf(const_cast<char*>("Client world model is NULL\n"));
			COM_ExplainDisconnection(true, const_cast<char*>("Client world model is NULL\n"));
			extern void DbgPrint(FILE*, const char* format, ...);
			extern FILE* m_fMessages;
			DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
			CL_Disconnect();
		}
	}
}

void CL_AddToResourceList(resource_t *pResource, resource_t *pList)
{
	if (pResource->pPrev || pResource->pNext)
	{
		Con_Printf(const_cast<char*>("Resource already linked\n"));
	}
	else
	{
		if (!pList->pPrev || !pList->pNext)
			return Sys_Error("Resource list corrupted.\n");

		pResource->pPrev = pList->pPrev;
		pList->pPrev->pNext = pResource;
		pList->pPrev = pResource;
		pResource->pNext = pList;
	}
}

void CL_RemoveFromResourceList( resource_t* pResource )
{
	if( !pResource->pPrev || !pResource->pNext )
		Sys_Error( "Mislinked resource in CL_RemoveFromResourceList" );

	if( pResource == pResource->pNext || pResource->pPrev == pResource )
		Sys_Error( "Attempt to free last entry in list." );

	pResource->pPrev->pNext = pResource->pNext;
	pResource->pNext->pPrev = pResource->pPrev;
	pResource->pPrev = NULL;
	pResource->pNext = NULL;
}

int CL_EstimateNeededResources()
{
	resource_t *p;
	int nTotalSize, nSize;

	nTotalSize = 0;
	
	for (p = cl.resourcesneeded.pNext; p != &cl.resourcesneeded; p = p->pNext)
	{
		switch (p->type)
		{
		case t_sound:
			if (p->szFileName[0] == '*' || (nSize = FS_FileSize(va(const_cast<char*>("sound/%s"), p->szFileName)), nSize != 0 && nSize != -1))
			{
				nSize = 0;
			}
			else
			{
				p->ucFlags |= RES_WASMISSING;
				nSize = p->nDownloadSize;
			}
			break;
		case t_skin:
		case t_generic:
		case t_eventscript:
			if (nSize = FS_FileSize(p->szFileName), nSize != 0 && nSize != -1)
			{
				nSize = 0;
			}
			else
			{
				p->ucFlags |= RES_WASMISSING;
				nSize = p->nDownloadSize;
			}

			break;
		case t_model:
			if (p->szFileName[0] == '*' || !p->ucFlags || (nSize = FS_FileSize(p->szFileName), nSize != 0 && nSize != -1))
			{
				nSize = 0;
			}
			else
			{
				p->ucFlags |= RES_WASMISSING;
				nSize = p->nDownloadSize;
			}
			break;
		case t_decal:
			if (p->ucFlags & RES_CUSTOM)
			{
				p->ucFlags |= RES_WASMISSING;
				nSize = p->nDownloadSize;
			}
			else
				nSize = 0;
			break;
		default:
			nSize = 0;
			break;
		}
		nTotalSize += nSize;
	}

	return nTotalSize;
}

void CL_BatchResourceRequest()
{
	byte data[65536];
	char filename[256];
	sizebuf_t msg;
	resource_t *p, *n;

	Q_memset(&msg, 0, sizeof(msg));
	msg.data = data;
	msg.cursize = 0;
	msg.buffername = "Client to Server Resource Batch";
	msg.maxsize = sizeof(data);

	CL_HTTPStop_f();

	// ...
	for (p = cl.resourcesneeded.pNext; p != NULL && p != &cl.resourcesneeded; p = n)
	{
		n = p->pNext;

		if (p->ucFlags & RES_WASMISSING)
		{
			if (cls.state == ca_active && cl_download_ingame.value == 0.0)
			{
				Con_Printf(const_cast<char*>("Skipping in game download of %s\n"), p->szFileName);
				CL_MoveToOnHandList(p);
			}
			else
			{
				if (IsSafeFileToDownload(p->szFileName))
				{
					switch (p->type)
					{
					case t_sound:
						if (p->szFileName[0] == '*' || CL_CheckFile(&msg, va(const_cast<char*>("sound/%s"), p->szFileName)))
							CL_MoveToOnHandList(p);
						break;
					case t_skin:
						CL_MoveToOnHandList(p);
						break;
					case t_model:
						if (p->szFileName[0] == '*' || CL_CheckFile(&msg, p->szFileName))
							CL_MoveToOnHandList(p);
						break;
					case t_decal:
						if (HPAK_GetDataPointer(const_cast<char*>("custom.hpk"), p, NULL, NULL))
							CL_MoveToOnHandList(p);
						else if (!(p->ucFlags & RES_REQUESTED))
						{
							snprintf(filename, sizeof(filename), "!MD5%s", COM_BinPrintf(p->rgucMD5_hash, sizeof(p->rgucMD5_hash)));
							MSG_WriteByte(&msg, clc_stringcmd);
							MSG_WriteString(&msg, va(const_cast<char*>("dlfile %s"), filename));
							p->ucFlags |= RES_REQUESTED;
						}
						break;
					case t_generic:
					case t_eventscript:
						if (CL_CheckFile(&msg, p->szFileName))
							CL_MoveToOnHandList(p);
						break;
					default:
						break;
					}
				}
				else
				{
					CL_RemoveFromResourceList(p);
					Con_Printf(const_cast<char*>("Invalid file type...skipping download of %s\n"), p->szFileName);
					Mem_Free(p);
				}
			}
		}
		else
		{
			CL_MoveToOnHandList(p);
		}
	}

	if (CL_GetDownloadQueueSize() > 0)
	{
		for(int i = 0; i < sizeof(cl.downloadUrl); i++)
			filename[i] = cl.downloadUrl[i];

		CL_MarkMapAsUsingHTTPDownload();

		extern void DbgPrint(FILE*, const char* format, ...);
		extern FILE* m_fMessages;
		DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);

		CL_Disconnect();
		StartLoadingProgressBar(filename, CL_GetDownloadQueueSize());
		SetLoadingProgressBarStatusText("#GameUI_VerifyingAndDownloading");
	}

	if (cls.state != ca_disconnected)
	{
		if (!msg.cursize && CL_PrecacheResources())
			CL_RegisterResources(&msg);

		if (cls.passive == false)
		{
			Netchan_CreateFragments(false, &cls.netchan, &msg);
			Netchan_FragSend(&cls.netchan);
		}
	}

}

void CL_StartResourceDownloading(char *pszMessage, qboolean bCustom)
{
	resourceinfo_t ri;

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("begin CL_StartResourceDownloading()");

	if (pszMessage)
		Con_DPrintf(const_cast<char*>("%s"), pszMessage);

	cls.dl.nTotalSize = COM_SizeofResourceList(&cl.resourcesneeded, &ri);
	cls.dl.nTotalToTransfer = CL_EstimateNeededResources();

	if (bCustom)
	{
		cls.dl.custom = true;
	}
	else
	{
		cls.state = ca_uninitialized;
		cls.dl.custom = false;
		gfExtendedError = false;
	}

	cls.dl.doneregistering = false;
	cls.dl.fLastStatusUpdate = realtime;
	cls.dl.nRemainingToTransfer = cls.dl.nTotalToTransfer;

	Q_memset(cls.dl.rgStats, 0, sizeof(cls.dl.rgStats));

	cls.dl.nCurStat = 0;

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("end   CL_StartResourceDownloading()");

	CL_BatchResourceRequest();
}

void CL_ParseConsistencyInfo()
{
	consistency_t *pc;
	resource_t *skip_crc_change;
	byte nullbuffer[32];
	resource_s *r;
	int hasinfo, i, lastcheck, skip, delta;

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("begin CL_ParseConsistencyInfo()");

	Q_memset(nullbuffer, 0, sizeof(nullbuffer));

	r = cl.resourcesneeded.pNext;
	hasinfo = MSG_ReadBits(1);

	cl.need_force_consistency_response = hasinfo != 0;

	if (!cl.need_force_consistency_response)
		return;
	
	lastcheck = 0;
	skip_crc_change = NULL;

	while (MSG_ReadBits(1))
	{
		int isdelta = MSG_ReadBits(1);

		if (isdelta)
			i = lastcheck + MSG_ReadBits(5);
		else
			i = MSG_ReadBits(10);

		skip = i;

		for (delta = skip - lastcheck; delta > 0; delta--) // SH_CODE: was delta >= 0;
		{
			if (r != skip_crc_change && strstr(r->szFileName, "models/"))
				Mod_NeedCRC(r->szFileName, false);

			r = r->pNext;
		}

		skip_crc_change = r;

		if (strstr(r->szFileName, "models/"))
			Mod_NeedCRC(r->szFileName, true);

		if (cl.num_consistency >= MAX_CONSISTENCY_LIST)
			return Host_Error("Consistency:  server sent too many filenames\n");

		// выбор свободного элемента и его очистка
		pc = &cl.consistency_list[cl.num_consistency++];
		Q_memset(pc, 0, sizeof(consistency_t));

		pc->issound = r->type == t_sound;
		pc->filename = r->szFileName;
		pc->orig_index = i;
		pc->value = 0;

		if (r->type == t_model && Q_memcmp(nullbuffer, r->rguc_reserved, sizeof(r->rguc_reserved)))
		{
			COM_UnMunge(r->rguc_reserved, sizeof(r->rguc_reserved), CL_GetServerCount());
			pc->check_type = r->rguc_reserved[0];
		}

		lastcheck = skip;
	}

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("end   CL_ParseConsistencyInfo()");
}

void CL_ConsistencyRelated(int arg)
{
	//
}

void CL_ParseResourceList()
{
	int total;
	resource_t *resource;

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("begin CL_ParseResourceList()");

	MSG_StartBitReading(&net_message);

	total = MSG_ReadBits(12);

	for (int i = 0; i < total; i++)
	{
		resource = (resource_t*)Mem_ZeroMalloc(sizeof(resource_t));

		resource->type = (resourcetype_t)MSG_ReadBits(4);

		Q_strncpy(resource->szFileName, MSG_ReadBitString(), sizeof(resource->szFileName) - 1);
		resource->szFileName[sizeof(resource->szFileName) - 1] = 0;

		// используется как в качестве id`а декали, так и в качестве индекса кэша ресурсов (звук/модель/событие)
		resource->nIndex = MSG_ReadBits(12);

		resource->nDownloadSize = MSG_ReadBits(24);
		resource->ucFlags = MSG_ReadBits(3) & ~RES_WASMISSING;

		if (resource->ucFlags & RES_CUSTOM)
			MSG_ReadBitData(resource->rgucMD5_hash, sizeof(resource->rgucMD5_hash));

		// дополнительная информация для конкретного типа ресурса
		if (MSG_ReadBits(1))
			MSG_ReadBitData(resource->rguc_reserved, sizeof(resource->rguc_reserved));

		CL_AddToResourceList(resource, &cl.resourcesneeded);
	}

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("end   CL_ParseResourceList()");

	CL_ParseConsistencyInfo();
	MSG_EndBitReading(&net_message);

	SetLoadingProgressBarStatusText("#GameUI_VerifyingResources");

	CL_StartResourceDownloading(const_cast<char*>("Verifying and downloading resources...\n"), false);
}

void CL_ParseResourceLocation()
{
	char* url;
	const char* lastSlash;

	url = MSG_ReadString();

	if (url != NULL && (!Q_strncasecmp("http://", url, 7) || !Q_strncasecmp("https://", url, 8)) )
	{
		lastSlash = strrchr(url, '/');

		if (lastSlash != NULL && !lastSlash[1])
		{
			strncpy(cl.downloadUrl, url, sizeof(cl.downloadUrl));
			cl.downloadUrl[sizeof(cl.downloadUrl) - 1] = 0;
		}
		else
		{
			snprintf(cl.downloadUrl, sizeof(cl.downloadUrl), "%s/", url);
		}

		Con_DPrintf(const_cast<char*>("Using %s as primary download location\n"), cl.downloadUrl);
	}
}

customization_t *CL_PlayerHasCustomization(int nPlayerNum, resourcetype_t type)
{
	customization_t *pList;

	for (pList = cl.players[nPlayerNum].customdata.pNext; pList != NULL; pList = pList->pNext)
	{
		if (pList->resource.type == type)
			return pList;
	}

	return NULL;
}

void CL_RemoveCustomization(int nPlayerNum, customization_t *pRemove)
{
	customization_t *pList, *pNext;
	cachewad_t* pWad;
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif

	for (pList = &cl.players[nPlayerNum].customdata; pList != NULL; pList = pList->pNext)
	{
		if (pList->pNext != pRemove)
			continue;

		pNext = pRemove->pNext;

		if (pRemove->bInUse && pRemove->pBuffer != NULL)
			Mem_Free(pRemove->pBuffer);

		if (pRemove->bInUse && pRemove->pInfo != NULL)
		{
			if (pRemove->resource.type == t_decal)
			{
				if (cls.state == ca_active)
					R_DecalRemoveAll(~nPlayerNum);

				pWad = (cachewad_t*)pRemove->pInfo;

				Mem_Free(pWad->lumps);

				for (int i = 0; i < pWad->cacheCount; i++)
				{
					pic = &pWad->cache[i];
					if (Cache_Check(&pic->cache))
						Cache_Free(&pic->cache);
				}

				Mem_Free(pWad->name);
				Mem_Free(pWad->cache);
			}

			Mem_Free(pRemove->pInfo);
		}

		Mem_Free(pRemove);

		pList->pNext = pNext;
	}
}

void CL_ParseCustomization()
{
	int i;
	resource_t *resource;
	customization_t *pList;
	qboolean bFound = false, bNoError;

	i = MSG_ReadByte();

	if (i >= MAX_CLIENTS)
		Host_Error("Bogus player index during customization parsing.\n");

	resource = (resource_t*)Mem_ZeroMalloc(sizeof(resource_t));
	resource->type = (resourcetype_t)MSG_ReadByte();

	Q_strncpy(resource->szFileName, MSG_ReadString(), sizeof(resource->szFileName) - 1);
	resource->szFileName[sizeof(resource->szFileName) - 1] = 0;

	resource->nIndex = MSG_ReadShort();
	resource->nDownloadSize = MSG_ReadLong();
	resource->pNext = NULL;
	resource->pPrev = NULL;
	resource->ucFlags = MSG_ReadByte() & ~RES_WASMISSING;

	if (resource->ucFlags & RES_CUSTOM)
		MSG_ReadBuf(sizeof(resource->rgucMD5_hash), resource->rgucMD5_hash);

	resource->playernum = i;

	if (!cl_allow_download.value)
	{
		Con_DPrintf(const_cast<char*>("Refusing new resource, cl_allow_download set to 0\n"));
		Mem_Free(resource);
	}
	else if (cls.state == ca_active && cl_download_ingame.value == 0.0)
	{
		Con_DPrintf(const_cast<char*>("Refusing new resource, cl_download_ingame set to 0\n"));
		Mem_Free(resource);
	}
	else
	{
		pList = CL_PlayerHasCustomization(i, resource->type);

		if (pList != NULL)
			CL_RemoveCustomization(i, pList);
		
		for (pList = cl.players[resource->playernum].customdata.pNext;
			pList != NULL && Q_memcmp(pList->resource.rgucMD5_hash, resource->rgucMD5_hash, sizeof(resource->rgucMD5_hash));
			pList = pList->pNext)
			;

		if (pList != NULL)
			bFound = true;

		if (HPAK_GetDataPointer(const_cast<char*>("custom.hpk"), resource, 0, 0))
		{
			if (bFound)
				Con_DPrintf(const_cast<char*>("Duplicate resource ignored for local client\n"));
			else if ((bNoError = COM_CreateCustomization(&cl.players[resource->playernum].customdata, resource, resource->playernum, FCUST_FROMHPAK, 0, 0)) == false)
				Con_DPrintf(const_cast<char*>("Error loading customization\n"));

			Mem_Free(resource);
		}
		else
		{
			resource->ucFlags |= RES_WASMISSING;

			CL_AddToResourceList(resource, &cl.resourcesneeded);
			Con_Printf(const_cast<char*>("Requesting %s from server\n"), resource->szFileName);
			CL_StartResourceDownloading(const_cast<char*>("Custom resource propagation...\n"), true);
		}
	}
}

void CL_CustomizationRelated(int arg)
{
	//
}

qboolean CL_RequestMissingResources()
{
	if (cls.dl.doneregistering == false && (cls.dl.custom || cls.state == ca_uninitialized))
	{
		if (cl.resourcesneeded.pNext == &cl.resourcesneeded)
		{
			cls.dl.custom = false;
			cls.dl.doneregistering = true;
			return false;
		}
		else
		{
			if (!(cl.resourcesneeded.pNext->ucFlags & RES_WASMISSING))
			{
				CL_MoveToOnHandList(cl.resourcesneeded.pNext);
				return true;
			}
		}
	}

	return false;
}

void CL_DeallocateDynamicData()
{
	if (cl_entities)
	{
		Mem_Free(cl_entities);
		cl_entities = 0;
	}
	R_DestroyObjects();
}

void CL_ReallocateDynamicData(int clients)
{
	cl.max_edicts = COM_EntsForPlayerSlots(clients);

	if (cl.max_edicts <= 0)
		return Sys_Error(const_cast<char*>("CL_ReallocateDynamicData allocating 0 entities"));

	if (cl_entities != NULL)
		Con_Printf(const_cast<char*>("CL_Reallocate cl_entities"));

	cl_entities = (cl_entity_t*)Mem_ZeroMalloc(sizeof(cl_entity_t) * cl.max_edicts);
	R_AllocObjects(cl.max_edicts);
	Q_memset(cl.frames, 0, sizeof(frame_t) * CL_UPDATE_BACKUP);
}

void CL_CreateCustomizationList()
{
	customization_t *pListHead;

	cl.players[cl.playernum].customdata.pNext = NULL;
	if (cl.num_resources <= 0)
		return;

	pListHead = &cl.players[cl.playernum].customdata;
	for (int i = 0; i < cl.num_resources; i++)
	{
		/*while (COM_CreateCustomization(pListHead, &cl.resourcelist[i], cl.playernum, 0, 0, 0))
		{
			if (cl.num_resources <= ++i)
				return;
		}*/
		if (COM_CreateCustomization(pListHead, &cl.resourcelist[i], cl.playernum, 0, 0, 0) == false)
			Con_DPrintf(const_cast<char*>("Problem with client customization %i, ignoring..."), i);
	}

}

qboolean CL_CheckGameDirectory(char *gamedir)
{
	char szGD[MAX_OSPATH];
	if (gamedir && gamedir[0])
	{
		COM_FileBase(com_gamedir, szGD);
		if (Q_strcasecmp(szGD, gamedir))
		{
			Con_Printf(const_cast<char*>("Server is running game %s.  Restart in that game to connect.\n"), gamedir);
			CL_Disconnect_f();
			return false;
		}
	}
	else
		Con_Printf(const_cast<char*>("Server didn't specify a gamedir, assuming no change\n"));

	return true;
}

int CL_BuildMapCycleListHints(char** hints, char* mapCycleMsg, char* firstMap)
{
	char szMod[260];
	char szMap[524];
	char mapLine[524];

	int length = strlen(mapCycleMsg);

	if (!mapCycleMsg || !length)
		return 1;

	sprintf(szMap, "%s\\%s\\%s%s\r\n%s\\%s\\%s%s\r\n", 
		szReslistsBaseDir, GetCurrentSteamAppName(), szCommonPreloads, szReslistsExt, szReslistsBaseDir, GetCurrentSteamAppName(), firstMap, szReslistsExt);

	*hints = (char*)Mem_ZeroMalloc(strlen(szMap) + 1);

	if (!*hints)
		return 0;

	strcpy(*hints, szMap);

	char* pFileList = mapCycleMsg;

	while (1)
	{
		pFileList = COM_Parse(pFileList);

		if (!com_token[0])
			break;

		strncpy(szMap, com_token, sizeof(szMap) - 1);
		szMap[sizeof(szMap) - 1] = 0;

		if (COM_TokenWaiting(pFileList))
			pFileList = COM_Parse(pFileList);

		if (PF_IsMapValid_I(szMap) && strcmp(firstMap, szMap))
		{
			sprintf(mapLine, "%s\\%s\\%s%s\r\n", szReslistsBaseDir, GetCurrentSteamAppName(), szMap, szReslistsExt);

			*hints = (char*)Mem_Realloc(*hints, strlen(mapLine) + strlen(*hints) + 2);

			if (!*hints)
				return 0;

			strcat(*hints, mapLine);
		}
	}

	sprintf(szMap, "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, GetCurrentSteamAppName());

	*hints = (char*)realloc(*hints, strlen(szMap) + strlen(*hints) + 2);

	strcat(*hints, szMap);
	
	return 1;
}

void CL_ParseServerInfo()
{
	char	*str;
	char    *gamedir;
	int		i, mungebuffer;
	char	szSendres[20];
	char	mapBaseName[4096];

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("begin CL_ParseServerInfo()");

	HPAK_CheckSize(const_cast<char*>("custom"));
	Con_DPrintf(const_cast<char*>("Serverinfo packet received.\n"));
	ContinueLoadingProgressBar("ClientConnect", 3, 0.0);
	SetLoadingProgressBarStatusText("#GameUI_ParseServerInfo");

	if (cls.passive)
		NET_LeaveGroup(cls.netchan.sock, cls.connect_stream);

	CL_ClearState(false);
	ClientDLL_HudVidInit();
	cls.demowaiting = false;

	i = MSG_ReadLong();

	if (i != PROTOCOL_VERSION && !cls.demoplayback)
	{
		Con_Printf(const_cast<char*>("Server returned version %i, not %i\n"), i, PROTOCOL_VERSION);
		return;
	}

	cl.servercount = MSG_ReadLong();
	if (cls.demoplayback)
		cl.servercount = ++gHostSpawnCount;

	mungebuffer = MSG_ReadLong();
	cl.serverCRC = mungebuffer;

	MSG_ReadBuf(16, cl.clientdllmd5);

	cl.maxclients = MSG_ReadByte();

	if (cl.maxclients < 1 || cl.maxclients > MAX_CLIENTS)
	{
		Con_Printf(const_cast<char*>("Bad maxclients (%u) from server\n"), cl.maxclients);
		return;
	}

	Cvar_SetValue(const_cast<char*>("r_decals"), cl.maxclients != 1 ? mp_decals.value : sp_decals.value);
	R_ForceCVars((qboolean)(cl.maxclients > 1));

	CL_DeallocateDynamicData();
	CL_ReallocateDynamicData(cl.maxclients);

	cl.playernum = MSG_ReadByte();
	COM_UnMunge3((byte*)&cl.serverCRC, 4, ~cl.playernum);
	for (i = 0; i < 32; i++)
		COM_ClearCustomizationList(&cl.players[i].customdata, true);

	CL_CreateCustomizationList();

	cl.gametype = MSG_ReadByte();

	gamedir = MSG_ReadString();

	if (!CL_CheckGameDirectory(gamedir))
		return;

	str = MSG_ReadString();
	Con_DPrintf(const_cast<char*>("Remote host:  %s\n"), str);

	Q_strncpy(cl.levelname, MSG_ReadString(), sizeof(cl.levelname) - 1);
	cl.levelname[sizeof(cl.levelname) - 1] = 0;

	// modelname - relative path to bsp
	MSG_SkipString();

	COM_FileBase(cl.levelname, mapBaseName);

	if (cl.maxclients > 1)
	{
		FS_LogLevelLoadStarted(mapBaseName);
		VGuiWrap2_LoadingStarted("level", mapBaseName);
	}

	cls.isVAC2Secure = MSG_ReadByte();

	if (!CL_CheckCRCs(cl.levelname))
		return;

	gHostSpawnCount = cl.servercount;
	cls.state = ca_connected;

	CL_BeginDemoStartup();

	SetSecondaryProgressBar(100.0);
	strcpy(szSendres, "sendres");

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, szSendres);

	if (fs_startup_timings.value != 0.0)
		AddStartupTiming("end   CL_ParseServerInfo()");
}

void CL_ParseBaseline()
{
	delta_t **ppentity, **ppcustom, **ppplayer;
	entity_state_t nullstate;
	cl_entity_t *ent = NULL;
	int index;
	byte type;

	ppentity = DELTA_LookupRegistration("entity_state_t");
	ppplayer = DELTA_LookupRegistration("entity_state_player_t");
	ppcustom = DELTA_LookupRegistration("custom_entity_state_t");

	Q_memset(&nullstate, 0, sizeof(entity_state_t));

	MSG_StartBitReading(&net_message);
	ContinueLoadingProgressBar("ClientConnect", 11, 0.0);
	SetLoadingProgressBarStatusText("#GameUI_ParseBaseline");
	cl.highentity = 0;

	while (MSG_PeekBits(16) != 0xffff)
	{
		index = MSG_ReadBits(11);
		if (index > cl.highentity)
			cl.highentity = index;

		ent = CL_EntityNum(index);
		
		type = MSG_ReadBits(2);

		if (type & ENTITY_BEAM)
		{
			DELTA_ParseDelta((byte*)&nullstate, (byte*)&ent->baseline, *ppcustom, sizeof(entity_state_t));
		}
		else
		{
			if (CL_IsPlayerIndex(index))
				DELTA_ParseDelta((byte*)&nullstate, (byte*)&ent->baseline, *ppplayer, sizeof(entity_state_t));
			else
				DELTA_ParseDelta((byte*)&nullstate, (byte*)&ent->baseline, *ppentity, sizeof(entity_state_t));
		}

		ent->baseline.entityType = type;
	}

	// завершающие 16 бит
	MSG_ReadBits(16);

	Q_memset(cl.instanced_baseline, 0, sizeof(cl.instanced_baseline));
	cl.instanced_baseline_number = MSG_ReadBits(6);

	for (int i = 0; i < cl.instanced_baseline_number; i++)
		DELTA_ParseDelta((byte*)&nullstate, (byte*)&cl.instanced_baseline[i], *ppentity, sizeof(entity_state_t));
	
	MSG_EndBitReading(&net_message);

	ent->curstate = ent->baseline;
	ent->prevstate = ent->baseline;
}

void CL_ParseStatic()
{
	//       !
}

/*
===================
CL_ParseStaticSound

===================
*/
void CL_ParseStaticSound(void)
{
	vec3_t		org;
	int			sound_num;
	float		vol;
	int			atten;
	int			ent;
	int			pitch;
	int			flags;
	sfx_t		sfxsentence, *sfx = NULL;
	float		delay = 0.0f;

	org[0] = MSG_ReadCoord(&net_message);
	org[1] = MSG_ReadCoord(&net_message);
	org[2] = MSG_ReadCoord(&net_message);

	sound_num = MSG_ReadShort();
	vol = (float)MSG_ReadByte() / 255.0;		// reduce back to 0.0 - 1.0 range
	atten = (float)MSG_ReadByte() / 64.0;
	ent = MSG_ReadShort();
	pitch = MSG_ReadByte();
	flags = MSG_ReadByte();

	if (flags & SND_SENTENCE)
	{
		_snprintf(sfxsentence.name, sizeof(sfxsentence.name), "!%s", rgpszrawsentence[sound_num]);
		sfx = &sfxsentence;
	}
	else
	{
		sfx = cl.sound_precache[sound_num];
	}

	if (sfx)
	{
		S_StartStaticSound(ent, CHAN_STATIC, sfx, org, vol, atten, flags, pitch);
	}
}

void CL_ParseMovevars()
{
	movevars.gravity = MSG_ReadFloat();
	movevars.stopspeed = MSG_ReadFloat();
	movevars.maxspeed = MSG_ReadFloat();
	movevars.spectatormaxspeed = MSG_ReadFloat();
	movevars.accelerate = MSG_ReadFloat();
	movevars.airaccelerate = MSG_ReadFloat();
	movevars.wateraccelerate = MSG_ReadFloat();
	movevars.friction = MSG_ReadFloat();
	movevars.edgefriction = MSG_ReadFloat();
	movevars.waterfriction = MSG_ReadFloat();
	movevars.entgravity = MSG_ReadFloat();
	movevars.bounce = MSG_ReadFloat();
	movevars.stepsize = MSG_ReadFloat();
	movevars.maxvelocity = MSG_ReadFloat();
	movevars.zmax = MSG_ReadFloat();
	movevars.waveHeight = MSG_ReadFloat();
	movevars.footsteps = MSG_ReadByte();
	movevars.rollangle = MSG_ReadFloat();
	movevars.rollspeed = MSG_ReadFloat();
	movevars.skycolor_r = MSG_ReadFloat();
	movevars.skycolor_g = MSG_ReadFloat();
	movevars.skycolor_b = MSG_ReadFloat();
	movevars.skyvec_x = MSG_ReadFloat();
	movevars.skyvec_y = MSG_ReadFloat();
	movevars.skyvec_z = MSG_ReadFloat();

	Q_strncpy(movevars.skyName, MSG_ReadString(), sizeof(movevars.skyName) - 1);
	movevars.skyName[sizeof(movevars.skyName) - 1] = 0;
	//               
#ifdef GLQUAKE
	if (movevars.zmax != gl_zmax.value)
		Cvar_SetValue(const_cast<char*>("gl_zmax"), movevars.zmax);
	if (movevars.waveHeight != gl_wateramp.value)
		Cvar_SetValue(const_cast<char*>("gl_wateramp"), movevars.waveHeight);
	// cl_entities[0] = world
	if (cl_entities)
		cl_entities->curstate.scale = gl_wateramp.value;
#endif
}

void CL_ParseSoundFade()
{
	cls.soundfade.nStartPercent = MSG_ReadByte();
	cls.soundfade.soundFadeHoldTime = MSG_ReadByte();
	cls.soundfade.soundFadeInTime = MSG_ReadByte();
	cls.soundfade.soundFadeOutTime = MSG_ReadByte();
	cls.soundfade.soundFadeStartTime = realtime;
}

void CL_Parse_DeltaDescription()
{
	delta_t **ppdelta;
	delta_description_t nulldesc, *pdesc;
	char *name, szDesc[256];
	int c;

	Q_memset(&nulldesc, 0, sizeof(delta_description_t));

	name = MSG_ReadString();
	if (!name || !name[0])
		Host_Error(__FUNCTION__ ":  Illegible description name\n");

	Q_strncpy(szDesc, name, sizeof(szDesc) - 1);
	szDesc[sizeof(szDesc) - 1] = 0;

	ppdelta = DELTA_LookupRegistration(szDesc);

	if (ppdelta && *ppdelta)
		DELTA_FreeDescription(ppdelta);

	MSG_StartBitReading(&net_message);

	c = MSG_ReadBits(16);

	*ppdelta = (delta_t*)Mem_ZeroMalloc(sizeof(delta_t));
	pdesc = (delta_description_t *)Mem_ZeroMalloc(sizeof(delta_description_t) * c);

	(*ppdelta)->dynamic = 1;
	(*ppdelta)->fieldCount = c;

	for (int i = 0; i < c; i++)
		DELTA_ParseDelta((byte*)&nulldesc, (byte*)&pdesc[i], g_MetaDelta, sizeof(delta_description_t));

	(*ppdelta)->pdd = pdesc;
	MSG_EndBitReading(&net_message);
}

void CL_ParseUndefined()
{
	//
}

/*
===============
CL_Restore

Restores a saved game.
===============
*/
void CL_Restore(const char *fileName)
{
	int				i, decalCount, mapCount;
	DWORD sign;
	FileHandle_t pFile;
	char name[16];
	DECALLIST decalList;
	char *pMapName;
	
	pFile = FS_Open(fileName, "rb");
	if (pFile)
	{
		FS_SetVBuf(pFile, NULL, 0, 4096);
		FS_Read(&sign, sizeof(sign), 1, pFile);
		//                                      ,                           
		FS_Read(&i, sizeof(i), 1, pFile);

		//                   
		if (sign == 'VLAV')
		{
			FS_Read(&decalCount, sizeof(decalCount), 1, pFile);
			for (i = 0; i < decalCount; i++)
			{
				FS_Read(name, sizeof(name), 1, pFile);
				FS_Read(&decalList.entityIndex, sizeof(decalList.entityIndex), 1, pFile);
				FS_Read(&decalList.depth, sizeof(decalList.depth), 1, pFile);
				FS_Read(&decalList.flags, sizeof(decalList.flags), 1, pFile);
				FS_Read(&decalList.position, sizeof(decalList.position), 1, pFile);
				if (r_decals.value != 0.0)
				{
					R_DecalShoot(Draw_DecalIndex(Draw_DecalIndexFromName(name)), decalList.entityIndex, 0, decalList.position, decalList.flags);
				}
			}
		}
		FS_Close(pFile);
	}

	mapCount = MSG_ReadByte();
	for (i = 0; i < mapCount; i++)
	{
		pMapName = MSG_ReadString();

		// JAY UNDONE:  Actually load decals that transferred through the transition!!!
		Con_Printf(const_cast<char*>("Loading decals from %s\n"), pMapName );
	}
}

void CL_DumpMessageLoad_f()
{
	int total = 0;

	Con_Printf(const_cast<char*>("-------- Message Load ---------\n"));

	for (int i = 0; i < (MAX_PACKETACCUM - 1); i++)
	{
		if (!msg_buckets[i])
			continue;

		Con_Printf(const_cast<char*>("%i:%s: %i msgs:%.2fK\n"), i, i > (ARRAYSIZE(cl_parsefuncs) - 2) ? "bogus #" : cl_parsefuncs[i].pszname, msg_buckets[i], (float)total_data[i] / 1024.f);
		total += msg_buckets[i];
	}

	Con_Printf(const_cast<char*>("User messages:  %i:%.2fK\n"), msg_buckets[MAX_PACKETACCUM - 1], (float)total_data[MAX_PACKETACCUM - 1] / 1024.f);
	Con_Printf(const_cast<char*>("------ End:  %i Total----\n"), msg_buckets[MAX_PACKETACCUM - 1] + total);
}

void CL_TransferMessageData()
{
	for (int i = 0; i < MAX_PACKETACCUM; i++)
		total_data[i] += last_data[i];
}

void CL_ResetFrame(frame_t* frame)
{
	frame->clientbytes = 0;
	frame->receivedtime = realtime;
	frame->packetentitybytes = 0;
	frame->tentitybytes = 0;
	frame->playerinfobytes = 0;
	frame->soundbytes = 0;
	frame->usrbytes = 0;
	frame->invalid = false;
	frame->choked = false;
	frame->latency = 0.0;
	frame->eventbytes = 0;
	frame->msgbytes = 0;
	frame->time = cl.mtime[0];
}

char *CL_MsgInfo(int cmd)
{
	static char sz[256];
	UserMsg *pmsg;

	Q_strcpy(sz, "???");
	if (cmd <= SVC_LASTMSG)
	{
		Q_strncpy(sz, cl_parsefuncs[cmd].pszname, sizeof(sz)-1);
		sz[sizeof(sz)-1] = 0;
	}
	else
	{
		for (pmsg = gClientUserMsgs; pmsg != NULL; pmsg = pmsg->next)
		{
			if (pmsg->iMsg == cmd)
			{
				Q_strncpy(sz, pmsg->szName, sizeof(sz)-1);
				sz[sizeof(sz) - 1] = 0;
				break;
			}
		}
	}

	return sz;
}

void CL_WriteErrorMessage(int starting_count, int current_count, sizebuf_t *msg)
{
	FileHandle_t file;
	char name[MAX_PATH];

	snprintf(name, sizeof(name), "%s", "buffer.dat");
	COM_FixSlashes(name);
	COM_CreatePath(name);
	file = FS_Open(name, "wb");

	if (file == NULL)
		return;

	FS_Write(&starting_count, sizeof(starting_count), 1, file);
	FS_Write(&current_count, sizeof(current_count), 1, file);
	FS_Write(msg->data, msg->cursize, 1, file);
	FS_Close(file);
	Con_Printf(const_cast<char*>("Wrote erroneous message to %s\n"), "buffer.dat");
}

void CL_WriteMessageHistory(int starting_count, int cmd)
{
	int command, idx;

	Con_Printf(const_cast<char*>("Last %i messages parsed.\n"), CMD_COUNT);

	for (int i = ( CMD_COUNT - 1 ); i >= 1; i--)
	{
		idx = (currentcmd + ~i) & CMD_MASK;
		command = oldcmd[idx].command;
		Con_Printf(const_cast<char*>("%i %04i %s\n"), oldcmd[idx].frame_number, oldcmd[idx].starting_offset, CL_MsgInfo(command));
	}

	Con_Printf(const_cast<char*>("BAD:  %3i:%s\n"), msg_readcount - 1, CL_MsgInfo(cmd));

	if (developer.value)
		CL_WriteErrorMessage(starting_count, msg_readcount - 1, &net_message);
}

FILE* m_fMessages = fopen("wmessages.log", "wb");

//-----------------------------------------------------------------------------
// Purpose: Parse incoming message from server.
// Input  : normal_message - 
// Output : void CL_ParseServerMessage
//-----------------------------------------------------------------------------
void CL_ParseServerMessage(qboolean normal_message /* = true */)
{
	// Index of svc_ or user command to issue.
	int			cmd;
	// For determining data parse sizes
	int bufStart, bufEnd, bufDefault;
	frame_t *frame;
	int bytes;

	bufDefault = msg_readcount;

	extern void DbgPrint(FILE*, const char* format, ...);

	if (cl_shownet.value == 1)
	{
		Con_Printf(const_cast<char*>("%i "), net_message.cursize);
	}
	else if (cl_shownet.value == 2)
	{
		Con_Printf(const_cast<char*>("------------------\n"));
	}

	Q_memset(last_data, 0, sizeof(last_data));
	frame = &cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK];

	//
	// parse the message
	//
	if (normal_message)
	{
		// Assume no entity/player update this packet
		if (cls.state == ca_active)
		{
			frame->invalid = true;
			frame->choked = false;
		}
		else
		{
			CL_ResetFrame(frame);
		}
	}

	while (1)
	{
		if (msg_badread)
			Host_Error("CL_ParseServerMessage: Bad server message");

		// Mark start position
		bufStart = msg_readcount;

		cmd = MSG_ReadByte();

		if (cmd == -1)
			break;

		//DbgPrint(m_fMessages, "received %d message from server\r\n", cmd);

		/*if (cmd == svc_packetentities)
		{
			void LogArray(FILE * out, PBYTE data, INT size);
			extern FILE* m_fMessages;
			DbgPrint(m_fMessages, "got svc_packetentities\n");
			LogArray(m_fMessages, net_message.data + msg_readcount - 1, net_message.cursize - (msg_readcount - 1));
		}*/

		if (cmd != svc_nop)
		{
			int idx = currentcmd++ & CMD_MASK;
			oldcmd[idx].command = cmd;
			oldcmd[idx].starting_offset = bufStart;
			oldcmd[idx].frame_number = host_framecount;
		}

		if (cmd > SVC_LASTMSG)
		{
			msg_buckets[MAX_PACKETACCUM - 1]++;
			//DbgPrint(m_fMessages, "not an engine clientside message, dispatching...\r\n", cmd);
			DispatchUserMsg(cmd);
			bufEnd = msg_readcount;
			last_data[MAX_PACKETACCUM - 1] += bufEnd - bufStart;
			cl.frames[cl.parsecountmod].usrbytes += bufEnd - bufStart;
			continue;
		}

		if (cl_shownet.value == 2)
		{
			if (Q_strlen(cl_parsefuncs[cmd].pszname) > 1)
				Con_Printf(const_cast<char*>("%3i:%s\n"), msg_readcount - 1, cl_parsefuncs[cmd].pszname);
		}

		if (cmd >= 0) //< MAX_PACKETACCUM)
		{
			msg_buckets[cmd]++;
			if (cmd == svc_bad)
			{
				CL_WriteMessageHistory(bufDefault, 0);
				// MARK:: return здесь даёт зайти на сервер несмотря на все сетевые ошибки,
				// но сам мир при этом не отобразится, звуки не будут проигрываться
				// только регулярные clc_move и по возможности clc_setpause
				// return;
				Host_Error("CL_ParseServerMessage: Illegible server message - %s\n", cl_parsefuncs[cmd].pszname);
			}
		}

		if (cl_parsefuncs[cmd].pfnParse)
		{
			if (cl_showmessages.value)
				Con_DPrintf(const_cast<char*>("Msg: %s\n"), cl_parsefuncs[cmd].pszname);

			//DbgPrint(m_fMessages, "calling %s\r\n", cl_parsefuncs[cmd].pszname);
			cl_parsefuncs[cmd].pfnParse();
		}

		bufEnd = msg_readcount;

		last_data[cmd] += bufEnd - bufStart;

		//DbgPrint(m_fMessages, "parsed %d (%d) bytes of %d received\r\n", bufEnd - bufStart, msg_readcount, net_message.cursize);

		//                               net_graph
		switch (cmd)
		{
		case svc_packetentities:
		case svc_deltapacketentities:
			bytes = cls.playerbits / 8;
			cl.frames[cl.parsecountmod].packetentitybytes += bufEnd - bufStart;
			cl.frames[cl.parsecountmod].packetentitybytes -= bytes;
			cl.frames[cl.parsecountmod].playerinfobytes += bytes;
			break;
		case svc_tempentity:
			cl.frames[cl.parsecountmod].tentitybytes += bufEnd - bufStart;
			break;
		case svc_sound:
			cl.frames[cl.parsecountmod].soundbytes += bufEnd - bufStart;
			break;
		case svc_clientdata:
			cl.frames[cl.parsecountmod].clientbytes += bufEnd - bufStart;
			break;
		case svc_event:
		case svc_event_reliable:
			cl.frames[cl.parsecountmod].eventbytes += bufEnd - bufStart;
			break;
		}
	}

	if (cl_shownet.value == 2 && Q_strlen("END OF MESSAGE") > 1)
		Con_Printf(const_cast<char*>("%3i:%s\n"), msg_readcount - 1, "END OF MESSAGE");

	// Latch in total message size
	if (cls.state == ca_active)
	{
		frame->msgbytes += net_message.cursize;
	}

	CL_TransferMessageData();

	if (!cls.demoplayback)
	{
		if (cls.state != ca_active)
			CL_WriteDemoStartup(bufDefault, &net_message);
		if (cls.demorecording && !cls.demowaiting)
			CL_WriteDemoMessage(bufDefault, &net_message);
	}
}

void CL_ParseHLTV()
{
	int cmd;

	cmd = MSG_ReadByte();

	if (cmd == HLTV_ACTIVE)
	{
		cls.spectator = true;
		g_bRedirectedToProxy = 0;
	}
	else if (cmd == HLTV_STATUS)
	{
		MSG_ReadLong();

		MSG_ReadShort();

		// number of proxies
		MSG_ReadWord();
		// number of slots
		MSG_ReadLong();
		// number of spectators
		MSG_ReadLong();

		MSG_ReadWord();
	}
	else if (cmd == HLTV_LISTEN)
	{
		cls.signon = SIGNONS;
		Net_StringToAdr(MSG_ReadString(), &cls.game_stream);
		NET_JoinGroup(cls.netchan.sock, cls.game_stream);
		SCR_EndLoadingPlaque();
	}
	else
	{
		Con_Printf(const_cast<char*>(" CL_Parse_HLTV: unknown HLTV command.\n"));
	}
}

void CL_AdjustClock()
{
	if (!g_clockdelta)
		return;

	if (!cl_fixtimerate.value)
		return;

	float fDeltaMod = fabs(g_clockdelta);

	if (fDeltaMod < 0.001f)
		return;

	if (cl_fixtimerate.value < 0.0f)
		cl_fixtimerate.value = 7.5f;

	float msec = g_clockdelta * 1000.0f;
	float multiplier = msec < 0.0f ? 1.0f : -1.0f;

	float sign = fabs(msec);

	float adjust = multiplier * ((cl_fixtimerate.value > sign ? sign : cl_fixtimerate.value) / 1000.0);

	if (fDeltaMod > fabs(adjust))
	{
		g_clockdelta += adjust;
		cl.time += adjust;
	}

	if (cl.oldtime > cl.time)
		cl.oldtime = cl.time;
}

void CL_Parse_Time()
{
	float time, dt, ticks = MSG_ReadFloat();

	// Get server time stamp
	cl.mtime[1] = cl.mtime[0];
	cl.mtime[0] = ticks;

	// Single player == keep clocks in sync
	time = ticks;
	if (cl.maxclients == 1)
		cl.time = ticks;
	else
		time = cl.time;

	dt = time - ticks;

	if (fabs(dt) <= cl_clockreset.value)
	{
		ticks = time;
		if (dt != 0)
			g_clockdelta = dt;
	}
	else
	{
		cl.time = ticks;
		g_clockdelta = 0;
	}

	// Don't let client compute a "negative" frame time based on cl.tickcount lagging cl.oldtickcount
	if (ticks < cl.oldtime)
	{
		cl.oldtime = ticks;
	}
}

void CL_Parse_ClientData()
{
	CL_ParseClientdata();
}

void CL_Parse_Disconnect()
{
	char *msg;

	msg = MSG_ReadString();
	SCR_EndLoadingPlaque();

	if (msg && msg[0])
	{
		COM_ExplainDisconnection(true, const_cast<char*>("#GameUI_DisconnectedFromServerExtended"));
		COM_ExtendedExplainDisconnection(true, msg);

		if (!Q_strncasecmp(msg, "Invalid VALVE CD Key\n", 21))
			ValidateCDKey(TRUE, TRUE);
		else if (!Q_strncasecmp(msg, "Invalid STEAM UserID Ticket\n", 28))
			VGUI2_OnDisconnectFromServer(1);
		else if (!Q_strcasecmp(msg, "No Steam logon\n"))
			VGUI2_OnDisconnectFromServer(2);
		else if (!Q_strcasecmp(msg, "Steam connection lost\n"))
			VGUI2_OnDisconnectFromServer(5);
		else if (!Q_strcasecmp(msg, "Unable to connect to Steam\n"))
			VGUI2_OnDisconnectFromServer(6);
		else if (!Q_strcasecmp(msg, "This Steam account is being used in another location\n"))
			VGUI2_OnDisconnectFromServer(4);
		else if (!Q_strcasecmp(msg, "VAC banned from secure server\n"))
			VGUI2_OnDisconnectFromServer(3);
		else
			VGUI2_OnDisconnectFromServer(0);
	}
	else
	{
		COM_ExplainDisconnection(true, const_cast<char*>("#GameUI_DisconnectedFromServer"));
	}

	Host_EndGame(const_cast<char*>("Server disconnected\n"));
}

void CL_Parse_QuakeLegacy()
{
	//
}

void CL_Parse_Version()
{
	int i;

	i = MSG_ReadLong();

	if (i != PROTOCOL_VERSION)
		Host_Error(__FUNCTION__ ": Server is protocol %i instead of %i\n", i, PROTOCOL_VERSION);
}

void CL_Set_ServerExtraInfo()
{
	char *dir; 
	char val[32]; 

	dir = MSG_ReadString();

	snprintf(val, sizeof(val), "%d", MSG_ReadByte() != 0);
	Cvar_DirectSet(&sv_cheats, val);

	COM_AddDefaultDir(dir);
}

void CL_Parse_SetView()
{
	cl.viewentity = MSG_ReadShort();
}

void CL_Parse_Print(void)
{
	Con_Printf(const_cast<char*>("%s"), MSG_ReadString());
}

void CL_Parse_CenterPrint()
{
	SCR_CenterPrint(MSG_ReadString());
}

qboolean AllowedCommand(const char *pCmdString, const char *pBannedString)
{
	const char *pFound;

	pFound = Q_stristr(pCmdString, pBannedString);

	if (!pFound)
		return true;

	if (pFound == pCmdString)
		return false;

	return pFound[-1] != ';' && pFound[-1] != ' ' && pFound[-1] != '\n' && pFound[-1] != '\r';
}

qboolean ValidStuffText(const char *cmd)
{
	const char* pchTokCmd;
	const char* pos;
	const char* pos1;
	int strLen;
	qboolean bGoodCommand;

	if (Host_IsSinglePlayerGame())
	{
		if (cls.demoplayback == false)
			return true;
	}

	Cmd_TokenizeString(const_cast<char*>(cmd));

	pchTokCmd = Cmd_Argv(0);

	if (pchTokCmd != NULL)
	{
		bGoodCommand = Q_stristr(pchTokCmd, "connect") != pchTokCmd;
		bGoodCommand &= Q_stristr(pchTokCmd, "bind") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "_set") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "unbind") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "retry") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "quit") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "_restart") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "motd_write") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "motdfile") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "kill") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "exit") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "writecfg") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "cl_filterstuff") == NULL;
		bGoodCommand &= Q_stristr(pchTokCmd, "unbindall") == NULL;

		if (bGoodCommand == false)
			return false;
	}

	bGoodCommand = _strnicmp(cmd, "alias", 6) != 0;

	pos = Q_stristr(cmd, "reconnect");
	pos1 = Q_stristr(cmd, "connect");

	if (pos != NULL && pos1 != NULL && pos1 == &pos[2])
	{
		// MARK: при индексации от 0 первый аргумент "onnect..."
		pos1 = Q_stristr(&pos[3], "connect ");
	}

	if (pos == NULL || (pos1 != NULL && pos1 != &pos[2]))
	{
		if (Q_stristr(pos1, "connect ") != NULL)
			bGoodCommand &= AllowedCommand(pos1, "connect ");
	}

	bGoodCommand &= AllowedCommand(cmd, "motd_write");
	bGoodCommand &= AllowedCommand(cmd, "motdfile");
	bGoodCommand &= AllowedCommand(cmd, "retry");
	bGoodCommand &= AllowedCommand(cmd, "_set");
	bGoodCommand &= Q_stristr(cmd, "bind ") == NULL;
	bGoodCommand &= Q_stristr(cmd, "unbind ") == NULL;
	bGoodCommand &= AllowedCommand(cmd, "quit");
	bGoodCommand &= Q_stristr(cmd, "_restart") == NULL;
	bGoodCommand &= AllowedCommand(cmd, "kill");
	bGoodCommand &= Q_stristr(cmd, "exit") == NULL;

	if (!g_bIsTFC)
		bGoodCommand &= Q_stristr(cmd, "exec") == NULL;

	bGoodCommand &= Q_stristr(cmd, "writecfg") == NULL;
	bGoodCommand &= Q_stristr(cmd, "cl_filterstuffcmd") == NULL;
	bGoodCommand &= Q_stristr(cmd, "unbindall") == NULL;

	if (cl_filterstuffcmd.value <= 0)
		return bGoodCommand;

	bGoodCommand &= Q_stristr(cmd, "ex_interp ") == NULL;
	bGoodCommand &= Q_stristr(cmd, "say ") == NULL;
	bGoodCommand &= Q_stristr(cmd, "developer") == NULL;
	bGoodCommand &= Q_stristr(cmd, "timerefresh") == NULL;
	bGoodCommand &= Q_stristr(cmd, "rate") == NULL;
	bGoodCommand &= Q_stristr(cmd, "cd ") == NULL;
	bGoodCommand &= Q_stristr(cmd, "fps_max") == NULL;
	bGoodCommand &= Q_stristr(cmd, "speak_enabled") == NULL;
	bGoodCommand &= Q_stristr(cmd, "voice_enable") == NULL;
	bGoodCommand &= Q_stristr(cmd, "sensitivity") == NULL;
	bGoodCommand &= Q_stristr(cmd, "sys_ticrate") == NULL;
	bGoodCommand &= Q_stristr(cmd, "setinfo") == NULL;
	bGoodCommand &= Q_stristr(cmd, "removedemo") == NULL;
	bGoodCommand &= Q_stristr(cmd, "volume") == NULL;
	bGoodCommand &= Q_stristr(cmd, "mp3volume") == NULL;

	// поиск остальных всевозможных команд, связанных с клиентом, рендером, HUD или вводом

	pos = Q_stristr(cmd, "cl_");
	if (pos != NULL)
	{
		bGoodCommand &= pos == cmd || pos[-1] == ' ' || pos[-1] == ';';
	}

	pos = Q_stristr(cmd, "gl_");
	if (pos != NULL)
	{
		bGoodCommand &= pos == cmd || pos[-1] == ' ' || pos[-1] == ';';
	}

	pos = Q_stristr(cmd, "m_");
	if (pos != NULL)
	{
		bGoodCommand &= pos == cmd || pos[-1] == ' ' || pos[-1] == ';';
	}

	pos = Q_stristr(cmd, "r_");
	if (pos != NULL)
	{
		bGoodCommand &= pos == cmd || pos[-1] == ' ' || pos[-1] == ';';
	}

	pos = Q_stristr(cmd, "hud_");
	if (pos != NULL)
	{
		bGoodCommand &= pos == cmd || pos[-1] == ' ' || pos[-1] == ';';
	}

	strLen = strlen(cmd);

	// поиск шеллкода

	for (int i = 0; i < strLen; i++)
	{
		bGoodCommand &= cmd[i] < ' ' && cmd[i] != '\n';
	}

	return bGoodCommand;
}

void CL_Parse_StuffText(void)
{
	char *s;

	s = MSG_ReadString();
	
	if (ValidStuffText(s))
		Cbuf_AddFilteredText(s);
	else
		Con_Printf(const_cast<char*>("Server tried to send invalid command:\"%s\"\n"), s);
}

void CL_Parse_ServerInfo()
{
	CL_ParseServerInfo();
	vid.recalc_refdef = 1;
	sys_timescale.value = 1.0;
}

void CL_Parse_SetAngle()
{
	cl.viewangles[PITCH] = MSG_ReadHiresAngle();
	cl.viewangles[YAW] = MSG_ReadHiresAngle();
	cl.viewangles[ROLL] = MSG_ReadHiresAngle();
}

void CL_Parse_AddAngle()
{
	cl.viewangles[YAW] += MSG_ReadHiresAngle();
}

void CL_Parse_CrosshairAngle()
{
	cl.crosshairangle[PITCH] = (float)MSG_ReadChar() / 5.0f;
	cl.crosshairangle[YAW] = (float)MSG_ReadChar() / 5.0f;
}

void CL_Parse_LightStyle(void)
{
	int i = MSG_ReadByte();
	if (i >= MAX_LIGHTSTYLES)
	{
		Sys_Error("svc_lightstyle > MAX_LIGHTSTYLES");
	}

	Q_strncpy(cl_lightstyle[i].map, MSG_ReadString(), MAX_STYLESTRING - 1);
	cl_lightstyle[i].map[MAX_STYLESTRING - 1] = 0;

	cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
}

void CL_Parse_StopSound(void)
{
	int i;
	i = MSG_ReadShort();
	S_StopSound(i >> 3, i & 7);
}

void CL_Parse_Particle()
{
	R_ParseParticleEffect();
}

void CL_Parse_SpawnBaseline()
{
	CL_ParseBaseline();
}

void CL_Parse_SpawnStatic()
{
	CL_ParseStatic();
}

void CL_Parse_TempEntity()
{
	CL_ParseTEnt();
}

void CL_Parse_SetPause(void)
{
	cl.paused = MSG_ReadByte();

	if (!VGuiWrap2_IsInCareerMatch())
	{
		if (cl.paused)
			CDAudio_Pause();
		else
			CDAudio_Resume();
	}
}

void CL_Parse_SignonNum(void)
{
	int i;
	i = MSG_ReadByte();
	if (i <= cls.signon)
	{
		Con_Printf(const_cast<char*>("Received signon %i when at %i\n"), i, cls.signon);
		CL_Disconnect_f();
		return;
	}
	cls.signon = i;
	CL_SignonReply();
}

void CL_Parse_SpawnStaticSound()
{
	CL_ParseStaticSound();
}

void CL_Parse_CDTrack(void)
{
	cl.cdtrack = MSG_ReadByte();
	cl.looptrack = MSG_ReadByte();

	if (COM_CheckParm(const_cast<char*>("-nocdaudio")))
	{
		return;
	}

	if (cls.demoplayback)
	{
		if (cls.forcetrack == -1)
		{
			CDAudio_Play(cl.cdtrack, true);
			return;
		}
	}
	else
	{
		if (cls.demorecording == false || cls.forcetrack == -1)
		{
			CDAudio_Play(cl.cdtrack, true);
			return;
		}
	}

	CDAudio_Play(cls.forcetrack, true);
}

void CL_Parse_Restore(void)
{
	CL_Restore(MSG_ReadString());
}

void CL_Parse_WeaponAnim()
{
	cl.weaponstarttime = 0.0f;
	cl.weaponsequence = MSG_ReadByte();
	cl.viewent.curstate.body = MSG_ReadByte();
}

void CL_Parse_DecalName()
{
	int i;

	i = MSG_ReadByte();
	Draw_DecalSetName(i, MSG_ReadString());
}

void CL_Parse_RoomType()
{
	Cvar_SetValue(const_cast<char*>("room_type"), MSG_ReadShort());
}

void CL_Parse_NewUserMsg()
{
	AddNewUserMsg();
}

void CL_Parse_UpdateUserInfo()
{
	CL_UpdateUserinfo();
}

void CL_Parse_Damage()
{
	//                               
}

// obsolete quake code
void CL_Parse_KilledMonster()
{
	//                                                         
	//                                           
}

void CL_Parse_FoundSecret()
{

}

void CL_Parse_Intermission()
{
	cl.intermission = 1;
	vid.recalc_refdef = true;
}

void CL_Parse_Finale()
{
	cl.intermission = 2;
	vid.recalc_refdef = true;
	SCR_CenterPrint(MSG_ReadString());
}


void CL_Parse_CutScene()
{
	vid.recalc_refdef = true;
	cl.intermission = 3;
	SCR_CenterPrint(MSG_ReadString());
}

void CL_Parse_PacketEntities()
{
	cls.playerbits = 0;
	CL_ParsePacketEntities(false, &cls.playerbits);
	CL_SetSolidEntities();
}

void CL_Parse_DeltaPacketEntities()
{
	cls.playerbits = 0;
	CL_ParsePacketEntities(true, &cls.playerbits);
	CL_SetSolidEntities();
}

void CL_Parse_Choke()
{
	cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK].choked = true;
}

void CL_Parse_ResourceList()
{
	CL_ParseResourceList();
}

void CL_Parse_ResourceLocation()
{
	CL_ParseResourceLocation();
}

void CL_Parse_NewMoveVars()
{
	CL_ParseMovevars();
}

void CL_Parse_ResourceRequest()
{
	CL_SendResourceListBlock();
}

void CL_Parse_Customization()
{
	CL_ParseCustomization();
}

void CL_Parse_SoundFade()
{
	CL_ParseSoundFade();
}

void CL_ResetEvent( event_info_t* ei )
{
	ei->index = 0;
	Q_memset( &ei->args, 0, sizeof( ei->args ) );
	ei->fire_time = 0;
	ei->flags = 0;
}

void CL_CalcPlayerVelocity(int idx, vec_t *velocity)
{
	vec3_t delta;
	float dt;

	velocity[0] = velocity[1] = velocity[2] = 0;
	
	if (!CL_IsPlayerIndex(idx))
		return;

	if (idx - 1 == cl.playernum)
	{
		VectorCopy(cl.frames[cl.parsecountmod].clientdata.velocity, velocity);
	}
	else
	{
		// Con_SafePrintf(__FUNCTION__ " accesing cl_entities [ %d ] as model %s\r\n", idx, cl_entities[idx].model->name);
		VectorSubtract(cl_entities[idx].curstate.velocity, cl_entities[idx].prevstate.velocity, delta);
		dt = cl_entities[idx].curstate.animtime - cl_entities[idx].prevstate.animtime;
		if (dt == 0.0f)
		{
			VectorCopy(cl_entities[idx].curstate.velocity, velocity)
		}
		else
		{
			VectorScale(delta, 1.0f / dt, velocity);
		}
	}
}

void CL_DescribeEvent(int slot, char *eventname)
{
	if (!cl_showevents.value)
		return;

	if (eventname)
		Con_NPrintf(slot & (CON_MAX_DEBUG_AREAS - 1), const_cast<char*>("%i %f %s"), slot, cl.time, eventname);
}

void CL_FireEvents()
{
	event_info_t *pev;
	event_hook_t *phook;

	for (int i = 0; i < MAX_EVENT_QUEUE; i++)
	{
		pev = &cl.events.ei[i];

		if (pev->index && (!pev->fire_time || pev->fire_time <= cl.time))
		{
			phook = CL_FindEventHook((char*)cl.event_precache[pev->index].filename);

			if (phook)
			{
				CL_DescribeEvent(i, (char*)cl.event_precache[pev->index].filename);
				phook->pfnEvent(&pev->args);
			}
			else
			{
				Con_DPrintf(const_cast<char*>("CL_FireEvents:  Unknown event %i:%s\n"), pev->index, cl.event_precache[pev->index].filename);
			}
			CL_ResetEvent(pev);
		}
	}
}

event_info_t *CL_FindEmptyEvent()
{
	int i;
	event_state_t *es;
	event_info_t *ei;

	es = &cl.events;

	// Look for first slot where index is != 0
	for (i = 0; i < MAX_EVENT_QUEUE; i++)
	{
		ei = &es->ei[i];
		if (ei->index != 0)
			continue;

		return ei;
	}

	// No slots available
	return NULL;
}

event_info_t *CL_FindUnreliableEvent()
{
	int i;
	event_state_t *es;
	event_info_t *ei;

	es = &cl.events;
	for (i = 0; i < MAX_EVENT_QUEUE; i++)
	{
		ei = &es->ei[i];
		if (ei->index != 0)
		{
			// It's reliable, so skip it
			if (ei->flags & FEV_RELIABLE)
			{
				continue;
			}
		}

		return ei;
	}

	// This should never happen
	return NULL;
}

// копия CL_QueueEvent, реализация применения client_index на откуп мододелам
void CL_ClientQueueEvent(int flags, int index, int client_index, float delay, event_args_t *pargs)
{
	event_info_t *p_events;

	p_events = CL_FindEmptyEvent();

	if (p_events == NULL)
	{
		if (flags & FEV_RELIABLE)
			p_events = CL_FindUnreliableEvent();

		if (p_events == NULL)
			return;
	}

	p_events->packet_index = 0;
	p_events->index = index;
	p_events->args = *pargs;
	p_events->fire_time = delay == 0 ? 0 : delay + cl.time;
	p_events->flags = flags;
}

void CL_QueueEvent(int flags, int index, float delay, event_args_t *pargs)
{
	event_info_t *ei;

	ei = CL_FindEmptyEvent();

	if (ei == NULL)
	{
		if (flags & FEV_RELIABLE)
			ei = CL_FindUnreliableEvent();
		
		if (ei == NULL)
			return;
	}

	ei->packet_index = 0;
	ei->index = index;
	ei->args = *pargs;
	ei->fire_time = delay == 0.f ? 0.f : delay + cl.time;
	ei->flags = flags;

}

void CL_ParseEvent()
{
	event_args_t newargs{}, nullargs{};
	delta_t **ppevent;
	int num_events, event_index, packet_index;
	float delay = 0.0f;
	entity_state_t *state;
	int referenced;

	ppevent = DELTA_LookupRegistration("event_t");

	MSG_StartBitReading(&net_message);

	/*****************************
	* формат:
	* бит наличия
	* атрибут (если бит наличия равен 1)
	******************************/
	num_events = MSG_ReadBits(5);
	for (int i = 0; i < num_events; i++)
	{
		event_index = MSG_ReadBits(10);

		// packet_index
		packet_index = -1;
		referenced = MSG_ReadBits(1);
		if (referenced)
		{
			packet_index = MSG_ReadBits(11);

#ifdef _DEBUG
			if (!referenced)
				Con_SafePrintf("event %d has no packet index\r\n", event_index);
#endif
			// 22.09.25 - дельта в оригинале считывается даже при нулевом бите packet_index
			// MARK: нужна ли здесь проверка на packet_index?
			// args
			referenced = MSG_ReadBits(1);
			if (referenced)
				DELTA_ParseDelta((byte*)&nullargs, (byte*)&newargs, *ppevent, sizeof(event_args_t));
		}

		// fire_time
		referenced = MSG_ReadBits(1);
		if (referenced)
			delay = (float)MSG_ReadBits(16) / 100.0f;

		if (packet_index == -1)
			continue;

		if (packet_index >= cl.frames[cl.parsecountmod].packet_entities.num_entities)
		{
			if (newargs.entindex)
			{
				if (newargs.entindex > 0 && newargs.entindex <= cl.maxclients)
					newargs.angles[PITCH] *= -3.0;
			}
			else
				Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  Received non-packet entity index 0 for event\n"));
		}
		else
		{
			state = &cl.frames[cl.parsecountmod].packet_entities.entities[packet_index];
			newargs.entindex = state->number;

			if (VectorCompare(newargs.origin, vec3_origin))
			{
				VectorCopy(state->origin, newargs.origin);
			}

			if (VectorCompare(newargs.angles, vec3_origin))
			{
				VectorCopy(state->angles, newargs.angles);
			}

			COM_NormalizeAngles(newargs.angles);

			if (state->number > 0 && state->number <= cl.maxclients)
			{
				newargs.angles[PITCH] *= -3.0;
				CL_CalcPlayerVelocity(state->number, newargs.velocity);
				newargs.ducking = state->usehull == 1;
			}
		}
		CL_QueueEvent(0, event_index, delay, &newargs);
	}
	MSG_EndBitReading(&net_message);
}

// date 11.10.2022
// time 16:32
void CL_Parse_ReliableEvent()
{
	event_args_t newargs, nullargs;
	delta_t **ppevent;
	int event_index;
	float delay;

	ppevent = DELTA_LookupRegistration("event_t");

	Q_memset(&nullargs, 0, sizeof(event_args_t));
	Q_memset(&newargs, 0, sizeof(event_args_t));

	MSG_StartBitReading(&net_message);

	event_index = MSG_ReadBits(10);
	DELTA_ParseDelta((byte*)&nullargs, (byte*)&newargs, *ppevent, sizeof(event_args_t));

	delay = 0.0f;
	if (MSG_ReadBits(1))
		delay = (float)MSG_ReadBits(16) / 100.f;

	if (newargs.entindex > 0 && newargs.entindex <= cl.maxclients)
		newargs.angles[PITCH] *= -3.0;

	CL_QueueEvent(FEV_RELIABLE, event_index, delay, &newargs);
	MSG_EndBitReading(&net_message);
}

void CL_Parse_Pings()
{
	int i, ping, packet_loss;
	qboolean hasping;

	MSG_StartBitReading(&net_message);
	while (true)
	{
		hasping = MSG_ReadBits(1);
		if (hasping == false)
			break;

		i = MSG_ReadBits(5);
		ping = MSG_ReadBits(12);
		packet_loss = MSG_ReadBits(7);

		cl.players[i].ping = ping;
		cl.players[i].packet_loss = packet_loss;
	}
	MSG_EndBitReading(&net_message);
}

void CL_Parse_HLTV()
{
	CL_ParseHLTV();
}

void CL_Parse_Director()
{
	int length;

	// структура: 1 байт [длина] + [длина] байт [данные]

	length = MSG_ReadByte();

	ClientDLL_DirectorMessage(length, &net_message.data[msg_readcount]);

	// по-хорошему эту работу должен делать MSG_ReadBuf, но обработка происходит на стороне пользовательского клиента, поэтому добавляем длину к прочитанным байтам
	msg_readcount += length;
}

void CL_Parse_Timescale()
{
	sys_timescale.value = MSG_ReadFloat();

	if (cls.spectator == false && cls.demoplayback == false || sys_timescale.value < 0.1)
		sys_timescale.value = 1.0;
}

void CL_Parse_VoiceInit()
{
	int quality;

	// с обновления steampipe у всех клиентов единый кодек - steam silk
	MSG_ReadString();

	quality = MSG_ReadByte();
#if defined( FEATURE_HL25 ) // SanyaSho: What's the difference?
	Voice_Init(empty_string, quality);
#else
	Voice_Init("", quality);
#endif // FEATURE_HL25
}

void CL_Parse_VoiceData()
{
	short startReadCount = msg_readcount;
	int iEntity;
	int nDataLength;
	int nChannel;
	char chReceived[8192];

	iEntity = MSG_ReadByte() + 1;

	nDataLength = min(MSG_ReadShort(), (int)sizeof(chReceived));

	MSG_ReadBuf(nDataLength, chReceived);

	cl.frames[cl.parsecountmod].voicebytes += msg_readcount - startReadCount;

	if (iEntity < 0 || iEntity >= cl.num_entities)
		return;

	if (cl.playernum + 1 == iEntity)
		Voice_LocalPlayerTalkingAck();

	if (!nDataLength)
		return;

	nChannel = Voice_GetChannel(iEntity);
	if (nChannel == VOICE_CHANNEL_ERROR && (nChannel = Voice_AssignChannel(iEntity), nChannel == VOICE_CHANNEL_ERROR))
		Con_DPrintf(const_cast<char*>("CL_ParseVoiceData: Voice_AssignChannel failed for client %d!\n"), iEntity - 1);
	else
		Voice_AddIncomingData(nChannel, chReceived, nDataLength, cls.netchan.incoming_sequence);
}

svc_func_t cl_parsefuncs[] =
{
	{ svc_bad, const_cast<char*>("svc_bad"), NULL },
	{ svc_nop, const_cast<char*>("svc_nop"), NULL },
	{ svc_disconnect, const_cast<char*>("svc_disconnect"), CL_Parse_Disconnect },
	{ svc_event, const_cast<char*>("svc_event"), CL_ParseEvent },
	{ svc_version, const_cast<char*>("svc_version"), CL_Parse_Version },
	{ svc_setview, const_cast<char*>("svc_setview"), CL_Parse_SetView },
	{ svc_sound, const_cast<char*>("svc_sound"), CL_Parse_Sound },
	{ svc_time, const_cast<char*>("svc_time"), CL_Parse_Time },
	{ svc_print, const_cast<char*>("svc_print"), CL_Parse_Print },
	{ svc_stufftext, const_cast<char*>("svc_stufftext"), CL_Parse_StuffText },
	{ svc_setangle, const_cast<char*>("svc_setangle"), CL_Parse_SetAngle },
	{ svc_serverinfo, const_cast<char*>("svc_serverinfo"), CL_Parse_ServerInfo },
	{ svc_lightstyle, const_cast<char*>("svc_lightstyle"), CL_Parse_LightStyle },
	{ svc_updateuserinfo, const_cast<char*>("svc_updateuserinfo"), CL_Parse_UpdateUserInfo },
	{ svc_deltadescription, const_cast<char*>("svc_deltadescription"), CL_Parse_DeltaDescription },
	{ svc_clientdata, const_cast<char*>("svc_clientdata"), CL_Parse_ClientData },
	{ svc_stopsound, const_cast<char*>("svc_stopsound"), CL_Parse_StopSound },
	{ svc_pings, const_cast<char*>("svc_pings"), CL_Parse_Pings },
	{ svc_particle, const_cast<char*>("svc_particle"), CL_Parse_Particle },
	{ svc_damage, const_cast<char*>("svc_damage"), CL_Parse_Damage },
	{ svc_spawnstatic, const_cast<char*>("svc_spawnstatic"), CL_Parse_SpawnStatic },
	{ svc_event_reliable, const_cast<char*>("svc_event_reliable"), CL_Parse_ReliableEvent },
	{ svc_spawnbaseline, const_cast<char*>("svc_spawnbaseline"), CL_Parse_SpawnBaseline },
	{ svc_tempentity, const_cast<char*>("svc_tempentity"), CL_Parse_TempEntity },
	{ svc_setpause, const_cast<char*>("svc_setpause"), CL_Parse_SetPause },
	{ svc_signonnum, const_cast<char*>("svc_signonnum"), CL_Parse_SignonNum },
	{ svc_centerprint, const_cast<char*>("svc_centerprint"), CL_Parse_CenterPrint },
	{ svc_killedmonster, const_cast<char*>("svc_killedmonster"), CL_Parse_KilledMonster },
	{ svc_foundsecret, const_cast<char*>("svc_foundsecret"), CL_Parse_FoundSecret },
	{ svc_spawnstaticsound, const_cast<char*>("svc_spawnstaticsound"), CL_Parse_SpawnStaticSound },
	{ svc_intermission, const_cast<char*>("svc_intermission"), CL_Parse_Intermission },
	{ svc_finale, const_cast<char*>("svc_finale"), CL_Parse_Finale },
	{ svc_cdtrack, const_cast<char*>("svc_cdtrack"), CL_Parse_CDTrack },
	{ svc_restore, const_cast<char*>("svc_restore"), CL_Parse_Restore },
	{ svc_cutscene, const_cast<char*>("svc_cutscene"), CL_Parse_CutScene },
	{ svc_weaponanim, const_cast<char*>("svc_weaponanim"), CL_Parse_WeaponAnim },
	{ svc_decalname, const_cast<char*>("svc_decalname"), CL_Parse_DecalName },
	{ svc_roomtype, const_cast<char*>("svc_roomtype"), CL_Parse_RoomType },
	{ svc_addangle, const_cast<char*>("svc_addangle"), CL_Parse_AddAngle },
	{ svc_newusermsg, const_cast<char*>("svc_newusermsg"), CL_Parse_NewUserMsg },
	{ svc_packetentities, const_cast<char*>("svc_packetentities"), CL_Parse_PacketEntities },
	{ svc_deltapacketentities, const_cast<char*>("svc_deltapacketentities"), CL_Parse_DeltaPacketEntities },
	{ svc_choke, const_cast<char*>("svc_choke"), CL_Parse_Choke },
	{ svc_resourcelist, const_cast<char*>("svc_resourcelist"), CL_Parse_ResourceList },
	{ svc_newmovevars, const_cast<char*>("svc_newmovevars"), CL_Parse_NewMoveVars },
	{ svc_resourcerequest, const_cast<char*>("svc_resourcerequest"), CL_Parse_ResourceRequest },
	{ svc_customization, const_cast<char*>("svc_customization"), CL_Parse_Customization },
	{ svc_crosshairangle, const_cast<char*>("svc_crosshairangle"), CL_Parse_CrosshairAngle },
	{ svc_soundfade, const_cast<char*>("svc_soundfade"), CL_Parse_SoundFade },
	{ svc_filetxferfailed, const_cast<char*>("svc_filetxferfailed"), CL_ParseFileTxferFailed },
	{ svc_hltv, const_cast<char*>("svc_hltv"), CL_Parse_HLTV },
	{ svc_director, const_cast<char*>("svc_director"), CL_Parse_Director },
	{ svc_voiceinit, const_cast<char*>("svc_voiceinit"), CL_Parse_VoiceInit },
	{ svc_voicedata, const_cast<char*>("svc_voicedata"), CL_Parse_VoiceData },
	{ svc_sendextrainfo, const_cast<char*>("svc_sendextrainfo"), CL_Set_ServerExtraInfo },
	{ svc_timescale, const_cast<char*>("svc_timescale"), CL_Parse_Timescale },
	{ svc_resourcelocation, const_cast<char*>("svc_resourcelocation"), CL_Parse_ResourceLocation },
	{ svc_sendcvarvalue, const_cast<char*>("svc_sendcvarvalue"), CL_Send_CvarValue },
	{ svc_sendcvarvalue2, const_cast<char*>("svc_sendcvarvalue2"), CL_Send_CvarValue2 },
	{ svc_exec, const_cast<char*>("svc_exec"), CL_Exec },
	{ (unsigned char)-1, const_cast<char*>("End of List"), NULL }
};
