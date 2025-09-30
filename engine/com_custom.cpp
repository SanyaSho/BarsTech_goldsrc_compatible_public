#include "quakedef.h"
#include "com_custom.h"
#include "client.h"
#if defined(GLQUAKE)
#include "gl_draw.h"
#else
#include "draw.h"
#endif
#include "decals.h"
#include "hashpak.h"
#include "custom.h"

void COM_ClearCustomizationList(customization_t* pHead, qboolean bCleanDecals)
{
	customization_s* pCurrent, * pNext;
	cachewad_t* pWad;
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif

	pCurrent = pHead->pNext;

	if (pCurrent == NULL)
		return;

	while (pCurrent)
	{
		pNext = pCurrent->pNext;

		if (pCurrent->bInUse)
		{
			if (pCurrent->pBuffer)
				Mem_Free(pCurrent->pBuffer);

			if (pCurrent->pInfo)
			{
				if (pCurrent->resource.type == t_decal)
				{
					if (bCleanDecals && cls.state == ca_active)
						R_DecalRemoveAll(~pCurrent->resource.playernum);

					pWad = (cachewad_t*)pCurrent->pInfo;

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

				Mem_Free(pCurrent->pInfo);
			}
		}

		Mem_Free(pCurrent);
		pCurrent = pNext;
	}

	pHead->pNext = NULL;
}

qboolean COM_CreateCustomization(customization_t* pListHead, resource_t* pResource, int playernumber, int flags, customization_t** pCustomization, int* nLumps)
{
	customization_t* pCust;
	qboolean bError;

	bError = FALSE;
	if (pCustomization)
		*pCustomization = NULL;
	pCust = (customization_t*)Mem_ZeroMalloc(sizeof(customization_t));

	pCust->resource = *pResource;

	if (pResource->nDownloadSize <= 0)
	{
		bError = TRUE;
		goto CustomizationError;
	}

	pCust->bInUse = TRUE;

	if (flags & FCUST_FROMHPAK)
	{
		if (!HPAK_GetDataPointer(const_cast<char*>("custom.hpk"), pResource, (uint8**)&pCust->pBuffer, NULL))
		{
			bError = TRUE;
			goto CustomizationError;
		}
	}
	else
	{
		pCust->pBuffer = COM_LoadFile(pResource->szFileName, 5, NULL);
	}

	if ((pCust->resource.ucFlags & RES_CUSTOM) && pCust->resource.type == t_decal)
	{
		pCust->resource.playernum = playernumber;

		if (!(flags & FCUST_VALIDATED) && // Don't validate twice
			!CustomDecal_Validate(pCust->pBuffer, pResource->nDownloadSize))
		{
			bError = TRUE;
			goto CustomizationError;
		}

		if (!(flags & RES_CUSTOM))
		{
			cachewad_t* pWad = (cachewad_t*)Mem_ZeroMalloc(sizeof(cachewad_t));
			pCust->pInfo = pWad;
			if (pResource->nDownloadSize >= 1024 && pResource->nDownloadSize <= 20480)
			{
				bError = CustomDecal_Init(pWad, pCust->pBuffer, pResource->nDownloadSize, playernumber) == false;
				if (bError)
					goto CustomizationError;

				if (pWad->lumpCount > 0)
				{
					if (nLumps)
						*nLumps = pWad->lumpCount;

					pCust->bTranslated = TRUE;
					pCust->nUserData1 = 0;
					pCust->nUserData2 = pWad->lumpCount;
					if (flags & FCUST_WIPEDATA)
					{
						Mem_Free(pWad->name);
						Mem_Free(pWad->cache);
						Mem_Free(pWad->lumps);
						Mem_Free(pCust->pInfo);
						pCust->pInfo = NULL;
					}
				}
			}
		}
	}

CustomizationError:
	if (bError)
	{
		if (pCust->pBuffer)
			Mem_Free(pCust->pBuffer);
		if (pCust->pInfo)
			Mem_Free(pCust->pInfo);
		Mem_Free(pCust);
	}
	else
	{
		if (pCustomization)
			*pCustomization = pCust;
		pCust->pNext = pListHead->pNext;
		pListHead->pNext = pCust;
	}
	return bError == FALSE;
}

int COM_SizeofResourceList(resource_t* pList, resourceinfo_t* ri)
{
	resource_t* p;
	int nSize;

	nSize = 0;
	Q_memset(ri, 0, sizeof(*ri));
	for (p = pList->pNext; p != pList; p = p->pNext)
	{
		nSize += p->nDownloadSize;

		if (p->type == t_model && p->nIndex == 1)
			ri->info[t_world].size += p->nDownloadSize;
		else if (p->type < t_end)
			ri->info[p->type].size += p->nDownloadSize;
	}
	return nSize;
}

