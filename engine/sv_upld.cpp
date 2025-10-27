#include "quakedef.h"
#include "hashpak.h"
#include "host.h"
#include "sv_upld.h"
#include "sv_main.h"

qboolean SV_CheckFile(sizebuf_t* msg, char* filename)
{
	resource_t p = {};

	if (Q_strlen(filename) == 36 && Q_strncasecmp(filename, "!MD5", 4) == 0)
	{
		// MD5 signature is correct, lets try to find this resource locally
		COM_HexConvert(filename + 4, 32, p.rgucMD5_hash);
		if (HPAK_GetDataPointer(const_cast<char*>("custom.hpk"), &p, 0, 0))
		{
			return TRUE;
		}
	}

	if (sv_allow_upload.value == 0.0f)
	{
		// Downloads are not allowed, continue with the state we have
		return TRUE;
	}

	MSG_WriteByte(msg, svc_stufftext);
	MSG_WriteString(msg, va(const_cast<char*>("upload \"!MD5%s\"\n"), MD5_Print(p.rgucMD5_hash)));

	return FALSE;
}

void SV_ClearResourceLists(client_t* cl)
{
	if (!cl)
		Sys_Error("SV_ClearResourceLists with NULL client!");

	SV_ClearResourceList(&cl->resourcesneeded);
	SV_ClearResourceList(&cl->resourcesonhand);
}

void SV_CreateCustomizationList(client_t* pHost)
{
	resource_t* pResource;

	pHost->customdata.pNext = NULL;

	for (pResource = pHost->resourcesonhand.pNext; pResource != &pHost->resourcesonhand; pResource = pResource->pNext)
	{
		// Search if this resource already in customizations list
		qboolean bFound = FALSE;
		customization_t* pList = pHost->customdata.pNext;
		while (pList && Q_memcmp(pList->resource.rgucMD5_hash, pResource->rgucMD5_hash, 16))
		{
			pList = pList->pNext;
		}
		if (pList != NULL)
		{
			Con_DPrintf(const_cast<char*>("SV_CreateCustomization list, ignoring dup. resource for player %s\n"), pHost->name);
			bFound = TRUE;
		}

		if (!bFound)
		{
			// Try to create customization and add it to the list
			customization_t* pCust;
			qboolean bNoError;
			int nLumps = 0;
			bNoError = COM_CreateCustomization(&pHost->customdata, pResource, -1, FCUST_WIPEDATA | FCUST_FROMHPAK, &pCust, &nLumps);
			if (bNoError)
			{
				pCust->nUserData2 = nLumps;
#ifdef _DEBUG
				int iEdict = pHost->edict - sv.edicts;
				Con_Printf((char*)"trying to register customization of edict %d\n", iEdict);
#endif
				gEntityInterface.pfnPlayerCustomization(pHost->edict, pCust);
			}
			else
			{
				if (sv_allow_upload.value == 0.0f)
					Con_DPrintf(const_cast<char*>("Ignoring custom decal from %s\n"), pHost->name);
				else
					Con_DPrintf(const_cast<char*>("Ignoring invalid custom decal from %s\n"), pHost->name);
			}
		}
	}
}

void SV_Customization(client_t* pPlayer, resource_t* pResource, qboolean bSkipPlayer)
{
	int i;

	host_client = svs.clients;

	for (i = 0; i < svs.maxclients; ++i, ++host_client)
	{
		if (host_client == pPlayer)
			break;
	}

	if (i == svs.maxclients)
		Sys_Error("Couldn't find player index for customization.");

	for (int cl = 0; cl < svs.maxclients; ++cl)
	{
		if ((host_client->active || host_client->spawned) &&
			!host_client->fakeclient &&
			(pPlayer != host_client || !bSkipPlayer))
		{

			MSG_WriteByte(&host_client->netchan.message, svc_customization);
			MSG_WriteByte(&host_client->netchan.message, i);
			MSG_WriteByte(&host_client->netchan.message, pResource->type);
			MSG_WriteString(&host_client->netchan.message, pResource->szFileName);
			MSG_WriteShort(&host_client->netchan.message, pResource->nIndex);
			MSG_WriteLong(&host_client->netchan.message, pResource->nDownloadSize);
			MSG_WriteByte(&host_client->netchan.message, pResource->ucFlags);

			if (pResource->ucFlags & RES_CUSTOM)
				SZ_Write(&host_client->netchan.message, pResource->rgucMD5_hash, 16);

		}
	}
}

void SV_RegisterResources()
{
	host_client->uploading = false;

	for (resource_t* pResource = host_client->resourcesonhand.pNext; pResource != &host_client->resourcesonhand; pResource = pResource->pNext)
	{
		SV_CreateCustomizationList(host_client);
		SV_Customization(host_client, pResource, true);
	}
}

void SV_MoveToOnHandList( resource_t* pResource )
{
	if( pResource )
	{
		SV_RemoveFromResourceList( pResource );
		SV_AddToResourceList( pResource, &host_client->resourcesonhand );
	}
	else
	{
		Con_DPrintf( const_cast<char*>("Null resource passed to SV_MoveToOnHandList\n") );
	}
}

void SV_AddToResourceList( resource_t* pResource, resource_t* pList )
{
	if( pResource->pPrev || pResource->pNext )
	{
		Con_Printf( const_cast<char*>("Resource already linked\n") );
	}
	else
	{
		pResource->pPrev = pList->pPrev;
		pList->pPrev->pNext = pResource;
		pList->pPrev = pResource;
		pResource->pNext = pList;
	}
}

void SV_ClearResourceList(resource_t* pList)
{
	resource_t* p, * n;

	for (p = pList->pNext; p && p != pList; p = n)
	{
		n = p->pNext;

		SV_RemoveFromResourceList(p);
		Mem_Free(p);
	}


	pList->pPrev = pList;
	pList->pNext = pList;
}

void SV_RemoveFromResourceList( resource_t* pResource )
{
	pResource->pPrev->pNext = pResource->pNext;
	pResource->pNext->pPrev = pResource->pPrev;
	pResource->pPrev = NULL;
	pResource->pNext = NULL;
}

int SV_EstimateNeededResources(void)
{
	resource_t* p;
	int size = 0;

	for (p = host_client->resourcesneeded.pNext; p != &host_client->resourcesneeded; p = p->pNext)
	{
		if (p->type == t_decal)
		{
			if (HPAK_ResourceForHash(const_cast<char*>("custom.hpk"), p->rgucMD5_hash, NULL) == false)
			{
				if (p->nDownloadSize)
				{
					size += p->nDownloadSize;
					p->ucFlags |= RES_WASMISSING;
				}
			}
		}
	}

	return size;
}

void SV_RequestMissingResourcesFromClients()
{
	host_client = svs.clients;

	for (int i = 0; i < svs.maxclients; i++, host_client++)
	{
		if (host_client->active == true || host_client->spawned == true)
		{
			while (SV_RequestMissingResources())
				;
		}
	}
}

qboolean SV_UploadComplete( client_t* cl )
{
	// SH_CODE: TODO: remove this
	if (cl->edict->pvPrivateData == NULL)
		return false;

	if( cl->resourcesneeded.pNext == &cl->resourcesneeded )
	{
		SV_RegisterResources();

		SV_PropagateCustomizations();

		if( sv_allow_upload.value )
			Con_DPrintf( const_cast<char*>("Custom resource propagation complete.\n") );

		cl->uploaddoneregistering = true;

		return true;
	}

	return false;
}

void SV_BatchUploadRequest(client_t *cl)
{
	resource_t *p, *n;
	char filename[MAX_PATH];

	for (p = cl->resourcesneeded.pNext; p != &cl->resourcesneeded; p = n)
	{
		n = p->pNext;

		if ((p->ucFlags & RES_WASMISSING) == 0)
		{
			SV_MoveToOnHandList(p);
		}
		else if (p->type == t_decal)
		{
			if (p->ucFlags & RES_CUSTOM)
			{
				_snprintf(filename, sizeof(filename), "!MD5%s", MD5_Print(p->rgucMD5_hash));
				if (SV_CheckFile(&cl->netchan.message, filename))
				{
					SV_MoveToOnHandList(p);
				}
			}
			else
			{
				Con_Printf(const_cast<char*>("Non customization in upload queue!\n"));
				SV_MoveToOnHandList(p);
			}
		}
	}
}

qboolean SV_RequestMissingResources()
{
	if (host_client->uploading == true && host_client->uploaddoneregistering == false)
	{
		SV_UploadComplete(host_client);
	}

	return false;
}

void SV_ParseResourceList(client_t *pSenderClient)
{
	int i, total;
	int totalsize;
	resource_t *resource;
	resourceinfo_t ri;

	total = MSG_ReadShort();

	SV_ClearResourceList(&host_client->resourcesneeded);
	SV_ClearResourceList(&host_client->resourcesonhand);

	for (i = 0; i < total; i++)
	{
		resource = (resource_t *)Mem_ZeroMalloc(sizeof(resource_t));
		Q_strncpy(resource->szFileName, MSG_ReadString(), sizeof(resource->szFileName) - 1);
		resource->szFileName[sizeof(resource->szFileName) - 1] = 0;
		resource->type = (resourcetype_t)MSG_ReadByte();
		resource->nIndex = MSG_ReadShort();
		resource->nDownloadSize = MSG_ReadLong();
		resource->ucFlags = MSG_ReadByte() & (~RES_WASMISSING);
		if (resource->ucFlags & RES_CUSTOM)
			MSG_ReadBuf(16, resource->rgucMD5_hash);
		resource->pNext = NULL;
		resource->pPrev = NULL;

		if (msg_badread || resource->type > t_world ||
			resource->nDownloadSize > 1024 * 1024 * 1024)	// FIXME: Are they gone crazy??!
		{
			SV_ClearResourceList(&host_client->resourcesneeded);
			SV_ClearResourceList(&host_client->resourcesonhand);
			return;
		}

		SV_AddToResourceList(resource, &host_client->resourcesneeded);
	}

	if (sv_allow_upload.value != 0.0f)
	{
		Con_DPrintf(const_cast<char*>("Verifying and uploading resources...\n"));
		totalsize = COM_SizeofResourceList(&host_client->resourcesneeded, &ri);
		if (totalsize != 0)
		{
			Con_DPrintf(const_cast<char*>("Custom resources total %.2fK\n"), total / 1024.0f);
			if (ri.info[t_model].size)
			{
				total = ri.info[t_model].size;
				Con_DPrintf(const_cast<char*>("  Models:  %.2fK\n"), total / 1024.0f);
			}
			if (ri.info[t_sound].size)
			{
				total = ri.info[t_sound].size;
				Con_DPrintf(const_cast<char*>("  Sounds:  %.2fK\n"), total / 1024.0f);
			}
			if (ri.info[t_decal].size)
			{
				// this check is useless, because presence of decals was checked before.
				total = ri.info[t_decal].size;
				Con_DPrintf(const_cast<char*>("  Decals:  %.2fK\n"), total / 1024.0f);
			}
			if (ri.info[t_skin].size)
			{
				total = ri.info[t_skin].size;
				Con_DPrintf(const_cast<char*>("  Skins :  %.2fK\n"), total / 1024.0f);
			}
			if (ri.info[t_generic].size)
			{
				total = ri.info[t_generic].size;
				Con_DPrintf(const_cast<char*>("  Generic :  %.2fK\n"), total / 1024.0f);
			}
			if (ri.info[t_eventscript].size)
			{
				total = ri.info[t_eventscript].size;
				Con_DPrintf(const_cast<char*>("  Events  :  %.2fK\n"), total / 1024.0f);
			}
			Con_DPrintf(const_cast<char*>("----------------------\n"));

			int bytestodownload = SV_EstimateNeededResources();
			if (bytestodownload > sv_max_upload.value * 1024 * 1024)
			{
				SV_ClearResourceList(&host_client->resourcesneeded);
				SV_ClearResourceList(&host_client->resourcesonhand);
				return;
			}

			if (bytestodownload > 1024)
				Con_DPrintf(const_cast<char*>("Resources to request: %.2fK\n"), bytestodownload / 1024.0f);
			else
				Con_DPrintf(const_cast<char*>("Resources to request: %i bytes\n"), bytestodownload);
		}
	}

	host_client->uploading = TRUE;
	host_client->uploaddoneregistering = FALSE;

	SV_BatchUploadRequest(host_client);
}
