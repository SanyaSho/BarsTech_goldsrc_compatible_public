#include "quakedef.h"
#include "hashpak.h"

hash_pack_queue_t *gp_hpak_queue = NULL;
hash_pack_directory_t hash_pack_dir = { 0, NULL };
hash_pack_header_t hash_pack_header = { { 0, 0, 0, 0 }, 0, 0 };

void HPAK_CreatePak(char* pakname, resource_s* pResource, void* pData, FileHandle_t fpSource);
qboolean HPAK_FindResource(hash_pack_directory_t* pDir, unsigned char* hash, resource_s* pResourceEntry);

qboolean HPAK_GetDataPointer(char *pakname, resource_s *pResource, unsigned char **pbuffer, int *bufsize)
{
	qboolean retval = FALSE;
	FileHandle_t fp;
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t *entry;
	char name[MAX_PATH];
	byte *pbuf;

	if (pbuffer)
		*pbuffer = NULL;

	if (bufsize)
		*bufsize = 0;

	if (gp_hpak_queue)
	{
		for (hash_pack_queue_t *p = gp_hpak_queue; p != NULL; p = p->next)
		{
			if (Q_strcasecmp(p->pakname, pakname) != 0 || Q_memcmp(p->resource.rgucMD5_hash, pResource->rgucMD5_hash, sizeof(pResource->rgucMD5_hash)) != 0)
				continue;

			if (pbuffer)
			{
				pbuf = (byte *)Mem_Malloc(p->datasize);
				if (!pbuf)
					Sys_Error("Error allocating %i bytes for hpak!", p->datasize);
				Q_memcpy((void *)pbuf, p->data, p->datasize);
				*pbuffer = pbuf;
			}
			if (bufsize)
				*bufsize = p->datasize;
			return TRUE;
		}
	}

	_snprintf(name, ARRAYSIZE(name), "%s", pakname);

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	fp = FS_Open(name, "rb");
	if (!fp)
	{
		return FALSE;
	}
	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);
	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)) != 0)
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), name);
		FS_Close(fp);
		return FALSE;
	}
	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("HPAK_List:  version mismatch\n"));
		FS_Close(fp);
		return FALSE;
	}
	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, 1, fp);

	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(fp);
		return FALSE;
	}
	directory.p_rgEntries = (hash_pack_entry_t *)Mem_ZeroMalloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, 1, fp);

	for (int i = 0; i < directory.nEntries; i++)
	{
		entry = &directory.p_rgEntries[i];

		if (Q_memcmp(entry->resource.rgucMD5_hash, pResource->rgucMD5_hash, sizeof(pResource->rgucMD5_hash)) != 0)
			continue;

		retval = TRUE;
		FS_Seek(fp, entry->nOffset, FILESYSTEM_SEEK_HEAD);

		if (pbuffer && entry->nFileLength > 0)
		{
			if (bufsize)
				*bufsize = entry->nFileLength;
			pbuf = (byte *)Mem_Malloc(entry->nFileLength);

			if (!pbuf)
			{
				Con_Printf(const_cast<char*>("Couln't allocate %i bytes for HPAK entry\n"), entry->nFileLength);

				if (bufsize)
					*bufsize = 0;
				retval = FALSE;
			}
			else
			{
				FS_Read(pbuf, entry->nFileLength, 1, fp);
				*pbuffer = pbuf;
			}
		}
		break;
	}

	Mem_Free(directory.p_rgEntries);
	FS_Close(fp);

	return retval;
}

void HPAK_AddToQueue(char *pakname, resource_s *pResource, void *pData, FileHandle_t fpSource)
{
	hash_pack_queue_t *n = (hash_pack_queue_t *)Mem_Malloc(sizeof(hash_pack_queue_t));
	if (!n)
		Sys_Error("Unable to allocate %i bytes for hpak queue!", sizeof(hash_pack_queue_t));

	Q_memset(n, 0, sizeof(hash_pack_queue_t));
	n->pakname = Mem_Strdup(pakname);
	Q_memcpy(&n->resource, pResource, sizeof(resource_t));
	n->datasize = pResource->nDownloadSize;
	n->data = Mem_Malloc(pResource->nDownloadSize);
	if (!n->data)
		Sys_Error("Unable to allocate %i bytes for hpak queue!", n->datasize);

	if (pData)
	{
		Q_memcpy(n->data, pData, n->datasize);
		n->next = gp_hpak_queue;
		gp_hpak_queue = n;
	}
	else
	{
		if (!fpSource)
			Sys_Error("Add to Queue called without data or file pointer!");
		FS_Read(n->data, n->datasize, 1, fpSource);
		n->next = gp_hpak_queue;
		gp_hpak_queue = n;
	}
}

void HPAK_FlushHostQueue()
{
	for (hash_pack_queue_t* p = gp_hpak_queue; gp_hpak_queue != NULL; p = gp_hpak_queue)
	{
		gp_hpak_queue = p->next;
		HPAK_AddLump(FALSE, p->pakname, &p->resource, p->data, 0);
		Mem_Free(p->pakname);
		Mem_Free(p->data);
		Mem_Free(p);
	}
}

void HPAK_AddLump(qboolean bUseQueue, char *pakname, resource_t *pResource, void *pData, FileHandle_t fpSource)
{
	FileHandle_t iRead;
	FileHandle_t iWrite;
	char name[MAX_PATH];
	char szTempName[MAX_PATH];
	char szOriginalName[MAX_PATH];
	hash_pack_directory_t olddirectory;
	hash_pack_directory_t newdirectory;
	hash_pack_entry_t *pNewEntry = NULL;
	int pos;
	byte md5[16];
	MD5Context_t ctx;
	byte *pDiskData = NULL;

	if (pakname == NULL)
		return Con_Printf(const_cast<char*>(__FUNCTION__ " called with invalid arguments:  no .pak filename\n"));
	
	if (!pResource)
		return Con_Printf(const_cast<char*>(__FUNCTION__ " called with invalid arguments:  no lump to add\n"));
	
	if (!pData && !fpSource)
		return Con_Printf(const_cast<char*>(__FUNCTION__" called with invalid arguments:  no file handle\n"));
	
	if (pResource->nDownloadSize < 1024 || pResource->nDownloadSize > MAX_FILE_SIZE)
		return Con_Printf(const_cast<char*>(__FUNCTION__ " called with bogus lump, size:  %i\n"), pResource->nDownloadSize);
	
	Q_memset(&ctx, 0, sizeof(MD5Context_t));
	MD5Init(&ctx);
	if (pData)
		MD5Update(&ctx, (byte *)pData, pResource->nDownloadSize);
	else
	{
		pos = FS_Tell(fpSource);

		pDiskData = (byte *)Mem_Malloc(pResource->nDownloadSize + 1);
		Q_memset(pDiskData, 0, pResource->nDownloadSize);

		FS_Read(pDiskData, pResource->nDownloadSize, 1, fpSource);
		FS_Seek(fpSource, pos, FILESYSTEM_SEEK_HEAD);

		MD5Update(&ctx, pDiskData, pResource->nDownloadSize);
		Mem_Free(pDiskData);
	}
	MD5Final(md5, &ctx);

	if (Q_memcmp(pResource->rgucMD5_hash, md5, sizeof(md5)) != 0)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ " called with bogus lump, md5 mismatch\n"));
		Con_Printf(const_cast<char*>("Purported:  %s\n"), MD5_Print(pResource->rgucMD5_hash));
		Con_Printf(const_cast<char*>("Actual   :  %s\n"), MD5_Print(md5));
		Con_Printf(const_cast<char*>("Ignoring lump addition\n"));
		return;
	}
	if (bUseQueue)
		return HPAK_AddToQueue(pakname, pResource, pData, fpSource);

	_snprintf(name, ARRAYSIZE(name), "%s", pakname);

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	COM_FixSlashes(name);

	Q_strncpy(szOriginalName, name, ARRAYSIZE(szOriginalName) - 1);
	szOriginalName[ARRAYSIZE(szOriginalName) - 1] = 0;


	iRead = FS_Open(name, "rb");

	if (!iRead)
	{
		HPAK_CreatePak(pakname, pResource, pData, fpSource);
		return;
	}

	COM_StripExtension(name, szTempName);
	COM_DefaultExtension(szTempName, const_cast<char*>(".hp2"));

	iWrite = FS_Open(szTempName, "w+b");
	if (!iWrite)
	{
		FS_Close(iRead);
		Con_Printf(const_cast<char*>("%s: %s -> %d\n"), __FILE__, __FUNCTION__, __LINE__);
		Con_Printf(const_cast<char*>("ERROR: couldn't open %s.\n"), szTempName);
		return;
	}
	FS_Read(&hash_pack_header, sizeof(hash_pack_header_t), 1, iRead);
	if (hash_pack_header.version != HASHPAK_VERSION)
	{
		FS_Close(iRead);
		FS_Close(iWrite);
		FS_Unlink(szTempName);
		Con_Printf(const_cast<char*>("hpk version = %d (def %d) " __FUNCTION__ "\n"), hash_pack_header.version, HASHPAK_VERSION);
		Con_Printf(const_cast<char*>("Invalid .hpk version in " __FUNCTION__ "\n"));
		return;
	}

	FS_Seek(iRead, 0, FILESYSTEM_SEEK_HEAD);
	COM_CopyFileChunk(iWrite, iRead, FS_Size(iRead));
	FS_Seek(iRead, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&olddirectory.nEntries, 4, 1, iRead);

	if (olddirectory.nEntries < 0 || olddirectory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: .hpk had bogus # of directory entries:  %i\n"), olddirectory.nEntries);
		FS_Close(iRead);
		FS_Close(iWrite);
		FS_Unlink(szTempName);
		return;
	}

	olddirectory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * olddirectory.nEntries);

	FS_Read(olddirectory.p_rgEntries, sizeof(hash_pack_entry_t) * olddirectory.nEntries, 1, iRead);
	FS_Close(iRead);

	if (HPAK_FindResource(&olddirectory, pResource->rgucMD5_hash, NULL) != FALSE)
	{
		FS_Close(iWrite);
		FS_Unlink(szTempName);
		Mem_Free(olddirectory.p_rgEntries);
		return;
	}

	newdirectory.nEntries = olddirectory.nEntries + 1;
	newdirectory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * newdirectory.nEntries);

	Q_memset(newdirectory.p_rgEntries, 0, sizeof(hash_pack_entry_t) * newdirectory.nEntries);
	Q_memcpy(newdirectory.p_rgEntries, olddirectory.p_rgEntries, sizeof(hash_pack_entry_t) * olddirectory.nEntries);

	pNewEntry = NULL;

	for (int i = 0; i < olddirectory.nEntries; i++)
	{
		// SH_CODE: check this
		if (Q_memcmp(pResource->rgucMD5_hash, olddirectory.p_rgEntries[i].resource.rgucMD5_hash, sizeof(olddirectory.p_rgEntries[i].resource.rgucMD5_hash)) < 0) //>= 0)
		{
			pNewEntry = &newdirectory.p_rgEntries[i];
			
			for (int j = i; j < olddirectory.nEntries; j++)
				Q_memcpy(&newdirectory.p_rgEntries[j + 1], &olddirectory.p_rgEntries[j], sizeof(hash_pack_entry_t));
			break;
		}
	}

	if (pNewEntry == NULL)
	{
		pNewEntry = &newdirectory.p_rgEntries[newdirectory.nEntries - 1];
	}

	Q_memset(pNewEntry, 0, sizeof(hash_pack_entry_t));
	FS_Seek(iWrite, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);

	pNewEntry->resource = *pResource;

	pNewEntry->nOffset = FS_Tell(iWrite);
	pNewEntry->nFileLength = pResource->nDownloadSize;

	if (pData)
		FS_Write(pData, pResource->nDownloadSize, 1, iWrite);
	else 
		COM_CopyFileChunk(iWrite, fpSource, pResource->nDownloadSize);

	hash_pack_header.nDirectoryOffset = FS_Tell(iWrite);

	FS_Write(&newdirectory.nEntries, 4, 1, iWrite);

	for (int j = 0; j < newdirectory.nEntries; j++)
		FS_Write(&newdirectory.p_rgEntries[j], sizeof(hash_pack_entry_t), 1, iWrite);

	if (newdirectory.p_rgEntries)
		Mem_Free(newdirectory.p_rgEntries);

	if (olddirectory.p_rgEntries)
		Mem_Free(olddirectory.p_rgEntries);

	FS_Seek(iWrite, 0, FILESYSTEM_SEEK_HEAD);
	FS_Write(&hash_pack_header, sizeof(hash_pack_header_t), 1, iWrite);
	FS_Close(iWrite);
	FS_Unlink(szOriginalName);
	FS_Rename(szTempName, szOriginalName);
}

qboolean HPAK_ResourceForHash(char *pakname, unsigned char *hash, resource_t *pResourceEntry)
{
	qboolean bFound;
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	char name[MAX_PATH];
	FileHandle_t fp;

	for (hash_pack_queue_t* p = gp_hpak_queue; p != NULL; p = p->next)
	{
		if (Q_strcasecmp(p->pakname, pakname) != 0 || Q_memcmp(p->resource.rgucMD5_hash, hash, sizeof(p->resource.rgucMD5_hash)) != 0)
			continue;

		if (pResourceEntry)
			Q_memcpy(pResourceEntry, &p->resource, sizeof(resource_t));

		return TRUE;
	}

	_snprintf(name, ARRAYSIZE(name), "%s", pakname);

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	fp = FS_Open(name, "rb");
	if (!fp)
	{
		Con_Printf(const_cast<char*>("%s: %s -> %d\n"), __FILE__, __FUNCTION__, __LINE__);
		Con_Printf(const_cast<char*>("ERROR: couldn't open %s.\n"), name);
		return FALSE;
	}

	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);
	
	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)))
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), name);
		FS_Close(fp);
		return FALSE;
	}
	
	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("HPAK_List:  version mismatch\n"));
		FS_Close(fp);
		return FALSE;
	}
	
	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, 1, fp);
	
	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(fp);
		return FALSE;
	}
	
	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, 1, fp);

	bFound = HPAK_FindResource(&directory, hash, pResourceEntry);

	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);

	return bFound;
}

void HPAK_List_f(void)
{
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t *entry;
	char name[MAX_PATH];
	char szFileName[MAX_PATH];
	char type[32];
	FileHandle_t fp;

	if (cmd_source != src_command)
		return;

	HPAK_FlushHostQueue();

	_snprintf(name, ARRAYSIZE(name), "%s", Cmd_Argv(1));

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	Con_Printf(const_cast<char*>("Contents for %s.\n"), name);

	fp = FS_Open(name, "rb");
	if (!fp)
	{
		Con_Printf(const_cast<char*>("%s: %s -> %d\n"), __FILE__, __FUNCTION__, __LINE__);
		Con_Printf(const_cast<char*>("ERROR: couldn't open %s.\n"), name);
		return;
	}
	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);
	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)))
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), name);
		FS_Close(fp);
		return;
	}
	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("HPAK_List:  version mismatch\n"));
		FS_Close(fp);
		return;
	}
	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, 1, fp);
	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(fp);
		return;
	}
	Con_Printf(const_cast<char*>("# of Entries:  %i\n"), directory.nEntries);
	Con_Printf(const_cast<char*>("# Type Size FileName : MD5 Hash\n"));

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t)* directory.nEntries, 1, fp);
	for (int nCurrent = 0; nCurrent < directory.nEntries; nCurrent++)
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase(entry->resource.szFileName, szFileName);
		switch (entry->resource.type)
		{
		case t_sound:
			Q_strcpy(type, "sound");
			break;
		case t_skin:
			Q_strcpy(type, "skin");
			break;
		case t_model:
			Q_strcpy(type, "model");
			break;
		case t_decal:
			Q_strcpy(type, "decal");
			break;
		case t_generic:
			Q_strcpy(type, "generic");
			break;
		case t_eventscript:
			Q_strcpy(type, "event");
			break;
		default:
			Q_strcpy(type, "?");
			break;
		}
		Con_Printf(const_cast<char*>("%i: %10s %.2fK %s\n  :  %s\n"), nCurrent + 1, type, entry->resource.nDownloadSize / 1024.0f, szFileName, MD5_Print(entry->resource.rgucMD5_hash));
	}
	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);
}

qboolean HPAK_ResourceForIndex(char *pakname, int nIndex, resource_s *pResource)
{
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t *entry;
	char name[MAX_PATH];
	FileHandle_t fp;

	if (cmd_source != src_command)
		return FALSE;

	_snprintf(name, ARRAYSIZE(name), "%s", pakname);

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	fp = FS_Open(name, "rb");
	if (!fp)
	{
		Con_Printf(const_cast<char*>("%s: %s -> %d\n"), __FILE__, __FUNCTION__, __LINE__);
		Con_Printf(const_cast<char*>("ERROR: couldn't open %s.\n"), name);
		return FALSE;
	}
	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);
	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)))
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), name);
		FS_Close(fp);
		return FALSE;
	}
	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("HPAK_List:  version mismatch\n"));
		FS_Close(fp);
		return FALSE;
	}
	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, 1, fp);
	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(fp);
		return FALSE;
	}

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);

	if (nIndex < 1 || nIndex > directory.nEntries)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK bogus directory entry request:  %i\n"), nIndex);
		FS_Close(fp);
		return FALSE;
	}

	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, 1, fp);
	entry = &directory.p_rgEntries[nIndex - 1];
	Q_memcpy(pResource, &entry->resource, sizeof(resource_t));
	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);
	return TRUE;
}

void HPAK_RemoveLump(char *pakname, resource_t *pResource)
{
	FileHandle_t fp;
	FileHandle_t tmp;
	char szTempName[MAX_PATH];
	char szOriginalName[MAX_PATH];
	hash_pack_directory_t olddir;
	hash_pack_directory_t newdir;
	hash_pack_entry_t *oldentry = NULL;
	hash_pack_entry_t *newentry = NULL;
	int n;
	int i;

	if (pakname == NULL || *pakname == '\0' || pResource == NULL)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ":  Invalid arguments\n"));
		return;
	}
	HPAK_FlushHostQueue();

	char name[MAX_PATH];
	_snprintf(name, ARRAYSIZE(name), "%s", Cmd_Argv(1));
	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	Q_strncpy(szOriginalName, name, ARRAYSIZE(szOriginalName) - 1);
	szOriginalName[ARRAYSIZE(szOriginalName) - 1] = 0;

	fp = FS_Open(szOriginalName, "rb");
	if (!fp)
		return Con_Printf(const_cast<char*>("Error:  couldn't open HPAK file %s for removal.\n"), szOriginalName);
	
	COM_StripExtension(szOriginalName, szTempName);
	COM_DefaultExtension(szTempName, const_cast<char*>(".hp2"));

	tmp = FS_Open(szTempName, "w+b");
	if (!tmp)
	{
		FS_Close(fp);
		Con_Printf(const_cast<char*>("ERROR: couldn't create %s.\n"), szTempName);
		return;
	}
	FS_Seek(fp, 0, FILESYSTEM_SEEK_HEAD);
	FS_Seek(tmp, 0, FILESYSTEM_SEEK_HEAD);
	FS_Read(&hash_pack_header, sizeof(hash_pack_header_t), 1, fp);
	FS_Write(&hash_pack_header, sizeof(hash_pack_header_t), 1, tmp);
	if (Q_strncmp(hash_pack_header.szFileStamp, "HPAK", sizeof(hash_pack_header.szFileStamp)))
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), szOriginalName);
		FS_Close(fp);
		FS_Close(tmp);
		FS_Unlink(szTempName);
		return;
	}
	if (hash_pack_header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK version outdated\n"));
		FS_Close(fp);
		FS_Close(tmp);
		FS_Unlink(szTempName);
		return;
	}
	
	FS_Seek(fp, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&olddir.nEntries, 4, 1, fp);

	if (olddir.nEntries < 1 || olddir.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), olddir.nEntries);
		FS_Close(fp);
		FS_Close(tmp);
		FS_Unlink(szTempName);
		return;
	}

	if (olddir.nEntries == 1)
	{
		Con_Printf(const_cast<char*>("Removing final lump from HPAK, deleting HPAK:\n  %s\n"), szOriginalName);
		FS_Close(fp);
		FS_Close(tmp);
		FS_Unlink(szOriginalName);
		FS_Unlink(szTempName);
		return;
	}

	olddir.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * olddir.nEntries);
	FS_Read(olddir.p_rgEntries, sizeof(hash_pack_entry_t)* olddir.nEntries, 1, fp);

	newdir.nEntries = olddir.nEntries - 1;
	newdir.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * newdir.nEntries);

	if (!HPAK_FindResource(&olddir, pResource->rgucMD5_hash, NULL))
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK doesn't contain specified lump:  %s\n"), pResource->szFileName);
		FS_Close(fp);
		FS_Close(tmp);
		FS_Unlink(szTempName);
		Mem_Free(olddir.p_rgEntries);
		Mem_Free(newdir.p_rgEntries);

		return;
	}

	Con_Printf(const_cast<char*>("Removing %s from HPAK %s.\n"), pResource->szFileName, szOriginalName);
	
	for (i = 0, n = 0; i < olddir.nEntries; i++)
	{
		oldentry = &olddir.p_rgEntries[i];

		if (Q_memcmp(olddir.p_rgEntries[i].resource.rgucMD5_hash, pResource->rgucMD5_hash, sizeof(pResource->rgucMD5_hash)))
		{
			newentry = &newdir.p_rgEntries[n++];
			Q_memcpy(newentry, oldentry, sizeof(hash_pack_entry_t));
			newentry->nOffset = FS_Tell(tmp);

			FS_Seek(fp, oldentry->nOffset, FILESYSTEM_SEEK_HEAD);
			COM_CopyFileChunk(tmp, fp, newentry->nFileLength);
		}
	}

	hash_pack_header.nDirectoryOffset = FS_Tell(tmp);
	FS_Write(&newdir.nEntries, 4, 1, tmp);
	for (i = 0; i < newdir.nEntries; i++)
		FS_Write(&newdir.p_rgEntries[i], sizeof(hash_pack_entry_t), 1, tmp);

	FS_Seek(tmp, 0, FILESYSTEM_SEEK_HEAD);
	FS_Write(&hash_pack_header, sizeof(hash_pack_header_t), 1, tmp);
	FS_Close(fp);
	FS_Close(tmp);
	FS_Unlink(szOriginalName);
	FS_Rename(szTempName, szOriginalName);
	Mem_Free(olddir.p_rgEntries);
	Mem_Free(newdir.p_rgEntries);
}

void HPAK_Remove_f(void)
{
	int nIndex;
	char *pakname;

	resource_t resource;

	if (cmd_source != src_command)
		return;

	HPAK_FlushHostQueue();
	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("Usage:  hpkremove <hpk> <index>\n"));
		return;
	}

	pakname = (char *)Cmd_Argv(1);

	nIndex = Q_atoi((char*)Cmd_Argv(2));
	if (HPAK_ResourceForIndex(pakname, nIndex, &resource))
		HPAK_RemoveLump(pakname, &resource);
	else Con_Printf(const_cast<char*>("Could not locate resource %i in %s\n"), nIndex, pakname);
}

void HPAK_Validate_f(void)
{
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t *entry;
	char name[MAX_PATH];
	char szFileName[MAX_PATH];
	char type[32];
	FileHandle_t fp;

	byte *pData;
	int nDataSize;
	byte md5[16];
	MD5Context_t ctx;

	if (cmd_source != src_command)
		return;

	HPAK_FlushHostQueue();

	if (Cmd_Argc() != 2)
	{
		Con_Printf(const_cast<char*>("Usage:  hpkval hpkname\n"));
		return;
	}

	_snprintf(name, ARRAYSIZE(name), "%s", Cmd_Argv(1));

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	Con_Printf(const_cast<char*>("Validating %s.\n"), name);

	fp = FS_Open(name, "rb");
	if (!fp)
	{
		Con_Printf(const_cast<char*>("%s: %s -> %d\n"), __FILE__, __FUNCTION__, __LINE__);
		Con_Printf(const_cast<char*>("ERROR: couldn't open %s.\n"), name);
		return;
	}

	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);
	
	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)))
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), name);
		FS_Close(fp);
		return;
	}
	
	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("hpkval:  version mismatch\n"));
		FS_Close(fp);
		return;
	}
	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, 1, fp);
	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(fp);
		return;
	}

	Con_Printf(const_cast<char*>("# of Entries:  %i\n"), directory.nEntries);
	Con_Printf(const_cast<char*>("# Type Size FileName : MD5 Hash\n"));

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, 1, fp);
	for (int nCurrent = 0; nCurrent < directory.nEntries; nCurrent++)
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase(entry->resource.szFileName, szFileName);
		switch (entry->resource.type)
		{
			case t_sound:
				Q_strcpy(type, "sound");
				break;
			case t_skin:
				Q_strcpy(type, "skin");
				break;
			case t_model:
				Q_strcpy(type, "model");
				break;
			case t_decal:
				Q_strcpy(type, "decal");
				break;
			case t_generic:
				Q_strcpy(type, "generic");
				break;
			case t_eventscript:
				Q_strcpy(type, "event");
				break;
			default:
				Q_strcpy(type, "?");
				break;
		}

		Con_Printf(const_cast<char*>("%i: %10s %.2fK %s:  "), nCurrent + 1, type, entry->resource.nDownloadSize / 1024.0f, szFileName);

		nDataSize = entry->nFileLength;
		if (nDataSize < 1 || nDataSize >= MAX_FILE_SIZE)
			Con_Printf(const_cast<char*>("Unable to MD5 hash data, size invalid:  %i\n"), nDataSize);
		else
		{
			pData = (byte *)Mem_Malloc(nDataSize + 1);
			Q_memset(pData, 0, nDataSize);
			FS_Seek(fp, entry->nOffset, FILESYSTEM_SEEK_HEAD);
			FS_Read(pData, nDataSize, 1, fp);
			Q_memset(&ctx, 0, sizeof(MD5Context_t));

			MD5Init(&ctx);
			MD5Update(&ctx, pData, nDataSize);
			MD5Final(md5, &ctx);

			if (Q_memcmp(entry->resource.rgucMD5_hash, md5, sizeof(md5)) == 0)
				Con_Printf(const_cast<char*>(" OK\n"));
			else
			{
				Con_Printf(const_cast<char*>(" MISMATCHED\n"));
				Con_Printf(const_cast<char*>("--------------------\n"));
				Con_Printf(const_cast<char*>(" File  :  %s\n"), MD5_Print(entry->resource.rgucMD5_hash));
				Con_Printf(const_cast<char*>(" Actual:  %s\n"), MD5_Print(md5));
				Con_Printf(const_cast<char*>("--------------------\n"));
			}
			if (pData)
				Mem_Free(pData);
		}
	}
	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);
}

void HPAK_Extract_f(void)
{
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t *entry;
	char name[MAX_PATH];
	char type[32];
	FileHandle_t fp;
	int nIndex;

	byte *pData;
	int nDataSize;
	FileHandle_t fpOutput;
	char szFileOut[MAX_PATH];

	if (cmd_source != src_command)
		return;

	HPAK_FlushHostQueue();

	if (Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("Usage:  hpkextract hpkname [all | single index]\n"));
		return;
	}
	if (Q_strcasecmp(Cmd_Argv(2), "all") != 0)
	{
		nIndex = Q_atoi(Cmd_Argv(2));

		_snprintf(name, 256, "%s", Cmd_Argv(1));

		if (nIndex != -1)
			Con_Printf(const_cast<char*>("Extracting lump %i from %s\n"), nIndex, name);
	}
	else
	{
		nIndex = -1;

		_snprintf(name, ARRAYSIZE(name), "%s", Cmd_Argv(1));

		COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
		Con_Printf(const_cast<char*>("Extracting all lumps from %s.\n"), name);
	}

	fp = FS_Open(name, "rb");
	if (!fp)
		return Con_Printf(const_cast<char*>("ERROR: couldn't open %s.\n"), name);

	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);

	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)))
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), name);
		FS_Close(fp);
		return;
	}

	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("hpkextract:  version mismatch\n"));
		FS_Close(fp);
		return;
	}

	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, 1, fp);
	
	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(fp);
		return;
	}

	Con_Printf(const_cast<char*>("# of Entries:  %i\n"), directory.nEntries);
	Con_Printf(const_cast<char*>("# Type Size FileName : MD5 Hash\n"));

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, 1, fp);
	for (int nCurrent = 0; nCurrent < directory.nEntries; nCurrent++)
	{
		entry = &directory.p_rgEntries[nCurrent];
		if (nIndex == -1 || nIndex == nCurrent)
		{
			COM_FileBase(entry->resource.szFileName, szFileOut);
			switch (entry->resource.type)
			{
				case t_sound:
					Q_strcpy(type, "sound");
					break;
				case t_skin:
					Q_strcpy(type, "skin");
					break;
				case t_model:
					Q_strcpy(type, "model");
					break;
				case t_decal:
					Q_strcpy(type, "decal");
					break;
				case t_generic:
					Q_strcpy(type, "generic");
					break;
				case t_eventscript:
					Q_strcpy(type, "event");
					break;
				default:
					Q_strcpy(type, "?");
					break;
			}

			Con_Printf(const_cast<char*>("Extracting %i: %10s %.2fK %s\n"), nCurrent, type, entry->resource.nDownloadSize / 1024.0f, szFileOut);
			nDataSize = entry->nFileLength;
			if (nDataSize < 1 || nDataSize >= MAX_FILE_SIZE)
				Con_Printf(const_cast<char*>("Unable to extract data, size invalid:  %s\n"), nDataSize);
			else
			{
				pData = (byte *)Mem_Malloc(nDataSize + 1);
				Q_memset(pData, 0, nDataSize);
				FS_Seek(fp, entry->nOffset, FILESYSTEM_SEEK_HEAD);
				FS_Read(pData, nDataSize, 1, fp);
				_snprintf(szFileOut, sizeof(szFileOut), "hpklmps\\lmp%04i.wad", nCurrent);
				COM_FixSlashes(szFileOut);
				COM_CreatePath(szFileOut);
				fpOutput = FS_Open(szFileOut, "wb");
				if (fpOutput)
				{
					FS_Write(pData, nDataSize, 1, fpOutput);
					FS_Close(fpOutput);
				}
				else Con_Printf(const_cast<char*>("Error creating lump file %s\n"), szFileOut);
				if (pData)
					Mem_Free(pData);
			}
		}
	}

	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);
}

void HPAK_Init()
{
	Cmd_AddCommand(const_cast<char*>("hpklist"), HPAK_List_f);
	Cmd_AddCommand(const_cast<char*>("hpkremove"), HPAK_Remove_f);
	Cmd_AddCommand(const_cast<char*>("hpkval"), HPAK_Validate_f);
	Cmd_AddCommand(const_cast<char*>("hpkextract"), HPAK_Extract_f);
	gp_hpak_queue = NULL;
}

void HPAK_ValidatePak(char *fullpakname)
{
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t *entry;
	char szFileName[MAX_PATH];
	FileHandle_t fp;

	byte *pData;
	byte md5[16];

	MD5Context_t ctx;

	HPAK_FlushHostQueue();
	fp = FS_Open(fullpakname, "rb");

	if (!fp)
		return;

	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);

	if (header.version != HASHPAK_VERSION || Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)) != 0)
	{
		Con_Printf(const_cast<char*>("%s is not a PAK file, deleting\n"), fullpakname);
		FS_Close(fp);
		FS_RemoveFile(fullpakname, 0);
		return;
	}

	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory, 4, 1, fp);

	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK %s had bogus # of directory entries:  %i, deleting\n"), fullpakname, directory.nEntries);
		FS_Close(fp);
		FS_RemoveFile(fullpakname, 0);
		return;
	}

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, 1, fp);
	for (int nCurrent = 0; nCurrent < directory.nEntries; nCurrent++)
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase(entry->resource.szFileName, szFileName);

		if ((unsigned int)entry->nFileLength >= MAX_FILE_SIZE)
		{
			Con_Printf(const_cast<char*>("Mismatched data in HPAK file %s, deleting\n"), fullpakname);
			Con_Printf(const_cast<char*>("Unable to MD5 hash data lump %i, size invalid:  %i\n"), nCurrent + 1, entry->nFileLength);

			FS_Close(fp);
			FS_RemoveFile(fullpakname, 0);
			Mem_Free(directory.p_rgEntries);
			return;
		}

		pData = (byte *)Mem_Malloc(entry->nFileLength + 1);

		Q_memset(pData, 0, entry->nFileLength);
		FS_Seek(fp, entry->nOffset, FILESYSTEM_SEEK_HEAD);
		FS_Read(pData, entry->nFileLength, 1, fp);
		Q_memset(&ctx, 0, sizeof(MD5Context_t));

		MD5Init(&ctx);
		MD5Update(&ctx, pData, entry->nFileLength);
		MD5Final(md5, &ctx);

		if (pData)
			Mem_Free(pData);

		if (Q_memcmp(entry->resource.rgucMD5_hash, md5, sizeof(md5)) != 0)
		{
			Con_Printf(const_cast<char*>("Mismatched data in HPAK file %s, deleting\n"), fullpakname);
			FS_Close(fp);
			FS_RemoveFile(fullpakname, 0);
			Mem_Free(directory.p_rgEntries);
			return;
		}
	}
	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);
}

char *HPAK_GetItem(int item)
{
	int nCurrent;
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	hash_pack_entry_t *entry;
	static char name[MAX_PATH];
	char szFileName[MAX_PATH];
	FileHandle_t fp;

	HPAK_FlushHostQueue();

	_snprintf(name, ARRAYSIZE(name), "%s", "custom");

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	fp = FS_Open(name, "rb");

	if (!fp)
		return const_cast<char*>("");

	FS_Read(&header, sizeof(hash_pack_header_t), 1, fp);
	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)))
	{
		Con_Printf(const_cast<char*>("%s is not an HPAK file\n"), name);
		FS_Close(fp);
		return const_cast<char*>("");
	}
	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf(const_cast<char*>("HPAK_List:  version mismatch\n"));
		FS_Close(fp);
		return const_cast<char*>("");
	}
	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, 1, fp);
	if (directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: HPAK had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(fp);
		return const_cast<char*>("");
	}
	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, 1, fp);
	nCurrent = directory.nEntries - 1;
	if (nCurrent > item)
		nCurrent = item;
	entry = &directory.p_rgEntries[nCurrent];
	COM_FileBase(entry->resource.szFileName, szFileName);
	_snprintf(name, sizeof(name), "!MD5%s", MD5_Print(entry->resource.rgucMD5_hash));
	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);
	return name;
}

void HPAK_CheckSize(char *pakname)
{
	char fullname[MAX_PATH];
	float maxSize;
	float actualSize;
	FileHandle_t hfile;

	maxSize = hpk_maxsize.value;
	if (!maxSize || pakname == NULL)
		return;

	if (maxSize < 0.0f)
	{
		Con_Printf(const_cast<char*>("hpk_maxsize < 0, setting to 0\n"));
		Cvar_DirectSet(&hpk_maxsize, const_cast<char*>("0"));
		return;
	}

	_snprintf(fullname, ARRAYSIZE(fullname), "%s", pakname);

	COM_DefaultExtension(fullname, const_cast<char*>(HASHPAK_EXTENSION));
	COM_FixSlashes(fullname);

	actualSize = 0.0f;
	maxSize *= 1000000.0f;

	hfile = FS_Open(fullname, "rb");
	if (hfile)
	{
		actualSize = (float)FS_Size(hfile);
		FS_Close(hfile);
	}

	if (actualSize >= maxSize)
	{
		Con_Printf(const_cast<char*>("Server: Size of %s > %f MB, deleting.\n"), fullname, hpk_maxsize.value);
		Log_Printf("Server: Size of %s > %f MB, deleting.\n", fullname, hpk_maxsize.value);
		FS_RemoveFile(fullname, 0);
	}
}

void HPAK_CheckIntegrity( const char* pakname )
{
	char name[MAX_PATH];
	_snprintf(name, sizeof(name), "%s", pakname);

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	COM_FixSlashes(name);
	HPAK_ValidatePak(name);
}

qboolean HPAK_FindResource(hash_pack_directory_t *pDir, unsigned char *hash, resource_s *pResourceEntry)
{
	for (int i = 0; i < pDir->nEntries; i++)
	{
		if (Q_memcmp(hash, pDir->p_rgEntries[i].resource.rgucMD5_hash, sizeof(pDir->p_rgEntries[i].resource.rgucMD5_hash)) == 0)
		{
			if (pResourceEntry)
				*pResourceEntry = pDir->p_rgEntries[i].resource;

			return TRUE;
		}
	}
	return FALSE;
}

void HPAK_CreatePak(char *pakname, resource_s *pResource, void *pData, FileHandle_t fpSource)
{
	char name[MAX_PATH];
	int32 curpos;
	FileHandle_t fp;
	hash_pack_entry_t *pCurrentEntry = NULL;

	byte md5[16];
	MD5Context_t ctx;
	byte *pDiskData = NULL;

	if ((!fpSource && !pData) || (fpSource && pData))
	{
		Con_Printf(const_cast<char*>("HPAK_CreatePak, must specify one of pData or fpSource\n"));
		return;
	}

	_snprintf(name, ARRAYSIZE(name), "%s", pakname);

	COM_DefaultExtension(name, const_cast<char*>(HASHPAK_EXTENSION));
	Con_Printf(const_cast<char*>("Creating HPAK %s.\n"), name);


	fp = FS_Open(name, "wb");
	if (!fp)
		return Con_Printf(const_cast<char*>("ERROR: couldn't open new .hpk, check access rights to %s.\n"), name);

	Q_memset(&ctx, 0, sizeof(MD5Context_t));
	MD5Init(&ctx);

	if (pData)
		MD5Update(&ctx, (byte *)pData, pResource->nDownloadSize);
	else
	{
		curpos = FS_Tell(fpSource);
		pDiskData = (byte *)Mem_Malloc(pResource->nDownloadSize + 1);
		Q_memset(pDiskData, 0, pResource->nDownloadSize);
		FS_Read(pDiskData, pResource->nDownloadSize, 1, fpSource);
		FS_Seek(fpSource, curpos, FILESYSTEM_SEEK_HEAD);
		MD5Update(&ctx, pDiskData, pResource->nDownloadSize);
		Mem_Free(pDiskData);
	}


	MD5Final(md5, &ctx);

	if (Q_memcmp(pResource->rgucMD5_hash, md5, sizeof(md5)) != 0)
	{
		Con_Printf(const_cast<char*>("HPAK_CreatePak called with bogus lump, md5 mismatch\n"));
		Con_Printf(const_cast<char*>("Purported:  %s\n"), MD5_Print(pResource->rgucMD5_hash));
		Con_Printf(const_cast<char*>("Actual   :  %s\n"), MD5_Print(md5));
		Con_Printf(const_cast<char*>("Ignoring lump addition\n"));
		return;
	}

	Q_memset(&hash_pack_header, 0, sizeof(hash_pack_header_t));
	Q_memcpy(hash_pack_header.szFileStamp, "HPAK", sizeof(hash_pack_header.szFileStamp));

	hash_pack_header.version = HASHPAK_VERSION;
	hash_pack_header.nDirectoryOffset = 0;

	FS_Write(&hash_pack_header, sizeof(hash_pack_header_t), 1, fp);
	Q_memset(&hash_pack_dir, 0, sizeof(hash_pack_directory_t));

	hash_pack_dir.nEntries = 1;
	hash_pack_dir.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc(sizeof(hash_pack_entry_t));
	Q_memset(hash_pack_dir.p_rgEntries, 0, sizeof(hash_pack_entry_t) * hash_pack_dir.nEntries);

	pCurrentEntry = &hash_pack_dir.p_rgEntries[0];
	pCurrentEntry->resource = *pResource;

	pCurrentEntry->nOffset = FS_Tell(fp);
	pCurrentEntry->nFileLength = pResource->nDownloadSize;

	if (pData)
		FS_Write(pData, pResource->nDownloadSize, 1, fp);
	else 
		COM_CopyFileChunk(fp, fpSource, pResource->nDownloadSize);

	curpos = FS_Tell(fp);
	FS_Write(&hash_pack_dir.nEntries, 4, 1, fp);

	for (int i = 0; i < hash_pack_dir.nEntries; i++)
		FS_Write(&hash_pack_dir.p_rgEntries[i], sizeof(hash_pack_entry_t), 1, fp);

	if (hash_pack_dir.p_rgEntries)
		Mem_Free(hash_pack_dir.p_rgEntries);

	hash_pack_dir.p_rgEntries = NULL;

	hash_pack_dir.nEntries = 0;
	hash_pack_header.nDirectoryOffset = curpos;

	FS_Seek(fp, 0, FILESYSTEM_SEEK_HEAD);
	FS_Write(&hash_pack_header, sizeof(hash_pack_header_t), 1, fp);
	FS_Close(fp);
}
