#include "quakedef.h"
#include "decals.h"
#if defined(GLQUAKE)
#include "gl_model.h"
#else
#include "model.h"
#endif
#include "custom.h"
#include "wad.h"
#if defined(GLQUAKE)
#include "gl_draw.h"
#else
#include "draw.h"
#endif
#include "server.h"
#include "host.h"
#include "client.h"

cachewad_t* decal_wad;
cachewad_t* menu_wad;
char decal_names[MAX_DECALNAMES][16];
char szCustName[10];
qboolean gfCustomBuild;
#if !defined(GLQUAKE)
cachewad_t custom_wad;
#endif

void Draw_FreeWad(cachewad_t* pWad);
void Draw_AllocateCacheSpace(cachewad_t* wad, int cacheMax, int tempWad);
#if defined(GLQUAKE)
qboolean Draw_CacheReload(cachewad_t* wad, int i, lumpinfo_t* pLump, cacheentry_t* pic, char* clean, char* path);
#else
qboolean Draw_CacheReload(cachewad_t* wad, int i, lumpinfo_t* pLump, cachepic_t* pic, char* clean, char* path);
#endif
#if defined(GLQUAKE)
qboolean Draw_CacheLoadFromCustom(char* clean, cachewad_t* wad, void* raw, int rawsize, cacheentry_t* pic);
#else
qboolean Draw_CacheLoadFromCustom(char* clean, cachewad_t* wad, void* raw, int rawsize, cachepic_t* pic);
#endif


void Draw_DecalShutdown()
{
	Draw_FreeWad(decal_wad);
	if (decal_wad)
		Mem_Free(decal_wad);
	decal_wad = NULL;

}

void Draw_Shutdown()
{
	if( m_bDrawInitialized )
	{
		m_bDrawInitialized = false;
		Draw_FreeWad(menu_wad);
		if (menu_wad)
			Mem_Free(menu_wad);
		menu_wad = NULL;

	}
}

void Draw_CacheWadInitFromFile(FileHandle_t hFile, int len, char *name, int cacheMax, cachewad_t *wad)
{
	lumpinfo_t *lump_p;
	wadinfo_t header;

	FS_Read(&header, sizeof(wadinfo_t), 1, hFile);
	if (*(uint32 *)header.identification != MAKEID('W', 'A', 'D', '3'))
		Sys_Error("Wad file %s doesn't have WAD3 id\n", name);

	wad->lumps = (lumpinfo_s *)Mem_Malloc(len - header.infotableofs);

	FS_Seek(hFile, header.infotableofs, FILESYSTEM_SEEK_HEAD);
	FS_Read(wad->lumps, len - header.infotableofs, 1, hFile);

	lump_p = wad->lumps;
	for (int i = 0; i < header.numlumps; i++, lump_p++)
		W_CleanupName(lump_p->name, lump_p->name);

	wad->lumpCount = header.numlumps;
	wad->cacheMax = cacheMax;
	wad->cacheCount = 0;
	wad->name = Mem_Strdup(name);
	Draw_AllocateCacheSpace(wad, cacheMax, 0);
	Draw_CacheWadHandler(wad, NULL, 0);
}

void Decal_ReplaceOrAppendLump(lumplist_t **ppList, lumpinfo_t *lump, qboolean bsecondlump)
{
	lumplist_t *p;
	for (p = *ppList; p != NULL; p = p->next)
	{
		if (!stricmp(lump->name, p->lump->name))
		{
			Mem_Free(p->lump);
			p->lump = (lumpinfo_t *)Mem_Malloc(sizeof(lumpinfo_t));
			Q_memcpy(p->lump, lump, sizeof(lumpinfo_t));
			p->breplaced = bsecondlump;
			return;
		}
	}

	p = (lumplist_t *)Mem_Malloc(sizeof(lumplist_t));
	Q_memset(p, 0, sizeof(lumplist_t));
	p->lump = (lumpinfo_t *)Mem_Malloc(sizeof(lumpinfo_t));
	Q_memcpy(p->lump, lump, sizeof(lumpinfo_t));
	p->breplaced = bsecondlump;
	p->next = *ppList;
	*ppList = p;
}

int Decal_CountLumps(lumplist_t *plist)
{
	int c = 0;
	lumplist_t *p = plist;
	while (p != NULL)
	{
		p = p->next;
		c++;
	}
	return c;
}

int Decal_SizeLumps(lumplist_t *plist)
{
	return (sizeof(lumpinfo_t)* Decal_CountLumps(plist));
}

void Decal_MergeInDecals(cachewad_t *pwad, const char *pathID)
{
	int i;
	int lumpcount;
	lumpinfo_t *lump;
	lumplist_t *lumplist;
	lumplist_t *plump;
	lumplist_t *pnext;
	cachewad_t *final;

	if (!pwad)
		Sys_Error(__FUNCTION__ " called with NULL wad\n");

	lumplist = NULL;
	if (!decal_wad)
	{
		pwad->numpaths = 1;
		decal_wad = pwad;

		pwad->basedirs = (char **)Mem_Malloc(sizeof(char *));
		*decal_wad->basedirs = Mem_Strdup(pathID);
		decal_wad->lumppathindices = (int *)Mem_Malloc(sizeof(int)* decal_wad->cacheMax);
		Q_memset(decal_wad->lumppathindices, 0, sizeof(int)* decal_wad->cacheMax);
		return;
	}

	final = (cachewad_t *)Mem_Malloc(sizeof(cachewad_t));
	Q_memset(final, 0, sizeof(cachewad_t));

	for (i = 0, lump = decal_wad->lumps; i < decal_wad->lumpCount; i++, lump++)
		Decal_ReplaceOrAppendLump(&lumplist, lump, FALSE);
	for (i = 0, lump = pwad->lumps; i < pwad->lumpCount; i++, lump++)
		Decal_ReplaceOrAppendLump(&lumplist, lump, TRUE);

	final->lumpCount = Decal_CountLumps(lumplist);
	final->cacheCount = 0;
	final->cacheMax = decal_wad->cacheMax;
	final->name = Mem_Strdup(decal_wad->name);
	Draw_AllocateCacheSpace(final, final->cacheMax, 0);
	final->pfnCacheBuild = decal_wad->pfnCacheBuild;
	final->cacheExtra = decal_wad->cacheExtra;
	final->lumppathindices = (int *)Mem_Malloc(sizeof(int)* final->cacheMax);
	Q_memset(final->lumppathindices, 0, sizeof(int)* final->cacheMax);

	final->numpaths = 2;
	final->basedirs = (char **)Mem_Malloc(sizeof(char*) * 2);
	final->basedirs[0] = Mem_Strdup(*decal_wad->basedirs);
	final->basedirs[1] = Mem_Strdup(pathID);
	final->lumps = (lumpinfo_t *)Mem_Malloc(Decal_SizeLumps(lumplist));

	lumpcount = 0;
	plump = lumplist;
	lump = final->lumps;
	while (plump != NULL)
	{
		pnext = plump->next;
		Q_memcpy(lump, plump->lump, sizeof(lumpinfo_t));
		Mem_Free(plump->lump);
		plump->lump = NULL;

		final->lumppathindices[lumpcount++] = (plump->breplaced != 0);
		Mem_Free(plump);
		plump = pnext;
		lump++;
	}

	Draw_FreeWad(decal_wad);
	if (decal_wad)
		Mem_Free(decal_wad);
	decal_wad = final;
	Draw_FreeWad(pwad);
	Mem_Free(pwad);
}

void Decal_Init()
{
	int i;
	int filesize;
	FileHandle_t hfile;
	cachewad_t* decal_wad_temp;
	char pszPathID[2][15] = { "DEFAULTGAME", "GAME" };

	Draw_DecalShutdown();
	for (i = 0; i < ARRAYSIZE(pszPathID); i++)
	{
		hfile = FS_OpenPathID("decals.wad", "rb", pszPathID[i]);
		if (i == 0 && !hfile)
			Sys_Error("Couldn't find '%s' in \"%s\" search path\n", "decals.wad", pszPathID[i]);

		filesize = FS_Size(hfile);
		decal_wad_temp = (cachewad_t*)Mem_Malloc(sizeof(cachewad_t));
		Q_memset(decal_wad_temp, 0, sizeof(cachewad_t));

		Draw_CacheWadInitFromFile(hfile, filesize, const_cast<char*>("decals.wad"), MAX_DECALS, decal_wad_temp);
		Draw_CacheWadHandler(decal_wad_temp, Draw_MiptexTexture, DECAL_EXTRASIZE);
		Decal_MergeInDecals(decal_wad_temp, pszPathID[i]);
		FS_Close(hfile);
	}

	sv_decalnamecount = Draw_DecalCount();
	if (sv_decalnamecount > MAX_DECALS)
		Sys_Error("Too many decals: %d / %d\n", sv_decalnamecount, MAX_DECALS);

	for (i = 0; i < sv_decalnamecount; i++)
	{
		Q_memset(&sv_decalnames[i], 0, sizeof(decalname_t));
		Q_strncpy(sv_decalnames[i].name, (char*)Draw_DecalName(i), sizeof sv_decalnames[i].name - 1);
		sv_decalnames[i].name[sizeof sv_decalnames[i].name - 1] = 0;
	}

}

int Draw_DecalCount(void)
{
	if (decal_wad)
		return decal_wad->lumpCount;
	return 0;
}

int Draw_DecalSize(int number)
{
	if (decal_wad)
	{
		if (decal_wad->lumpCount > number)
			return decal_wad->lumps[number].size;
	}
	return 0;
}

const char *Draw_DecalName(int number)
{
	if (decal_wad)
	{
		if (decal_wad->lumpCount > number)
			return decal_wad->lumps[number].name;
	}
	return NULL;
}

int Draw_DecalIndex( int id )
{
	char tmp[32];
	char *pName;
	if (!decal_names[id][0])
		Sys_Error("Used decal #%d without a name\n", id);

	pName = decal_names[id];
	if (!Host_IsServerActive() && violence_hblood.value == 0.0f && !Q_strncmp(pName, "{blood", 6))
	{
		_snprintf(tmp, sizeof(tmp), "{yblood%s", pName + 6);
		pName = tmp;
	}
	return Draw_CacheIndex(decal_wad, pName);
}

int Draw_DecalIndexFromName( char* name )
{
	char tmpName[16];
	Q_strncpy(tmpName, name, sizeof(tmpName)-1);
	tmpName[sizeof(tmpName)-1] = 0;

	if (tmpName[0] == '}')
		tmpName[0] = '{';

	for (int i = 0; i < MAX_DECALS; i++)
	{
		if (*decal_names[i] && !Q_strcmp(tmpName, decal_names[i]))
			return i;
	}

	return 0;
}

void Draw_AllocateCacheSpace(cachewad_t* wad, int cacheMax, int tempWad)
{
#if defined(GLQUAKE)
	int len = sizeof(cacheentry_t) * cacheMax;
	wad->cache = (cacheentry_t*)Mem_Malloc(len);
#else
	int len = sizeof(cachepic_t) * cacheMax;
	wad->cache = (cachepic_t*)Mem_Malloc(len);
#endif
	Q_memset(wad->cache, 0, len);
#if defined (GLQUAKE)
	wad->tempWad = tempWad;
#endif
}

void Draw_CacheWadHandler(cachewad_t* wad, PFNCACHE fn, int extraDataSize)
{
	wad->pfnCacheBuild = fn;
	wad->cacheExtra = extraDataSize;
}

void Draw_FreeWad(cachewad_t* pWad)
{
	int i;
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif;

	if (!pWad)
		return;

	if (pWad->lumps)
		Mem_Free(pWad->lumps);

	pWad->lumps = NULL;
	Mem_Free(pWad->name);
	if (pWad->numpaths)
	{
		for (i = 0; i < pWad->numpaths; i++)
		{
			Mem_Free(pWad->basedirs[i]);
			pWad->basedirs[i] = NULL;
		}
		Mem_Free(pWad->basedirs);
		pWad->basedirs = NULL;
	}
	if (pWad->lumppathindices)
	{
		Mem_Free(pWad->lumppathindices);
		pWad->lumppathindices = NULL;
	}
	if (pWad->cache)
	{
#if defined (GLQUAKE)
		if (pWad->tempWad)
		{
			Mem_Free(pWad->cache);
			pWad->cache = NULL;
			return;
		}
#endif
		for (i = 0, pic = pWad->cache; i < pWad->cacheCount; i++, pic++)
		{
			if (Cache_Check(&pic->cache))
				Cache_Free(&pic->cache);
		}
		Mem_Free(pWad->cache);
		pWad->cache = NULL;
	}
}


qboolean Draw_CustomCacheWadInit(int cacheMax, cachewad_t* wad, void* raw, int nFileSize)
{
	lumpinfo_t* lump_p;
	wadinfo_t header;

	header = *(wadinfo_t*)raw;

	if (*(uint32*)header.identification != MAKEID('W', 'A', 'D', '3'))
	{
		Con_Printf(const_cast<char*>("Custom file doesn't have WAD3 id\n"));
		return FALSE;
	}

	if (header.numlumps != 1)
	{
		Con_Printf(const_cast<char*>("Custom file has wrong number of lumps %i\n"), header.numlumps);
		return FALSE;
	}

	if (header.infotableofs < 1)
	{
		Con_Printf(const_cast<char*>("Custom file has bogus infotableofs %i\n"), header.infotableofs);
		return FALSE;
	}

	if (header.infotableofs + sizeof(lumpinfo_t) != nFileSize)
	{
		Con_Printf(const_cast<char*>("Custom file has bogus infotableofs ( %i > %i )\n"), header.infotableofs + sizeof(lumpinfo_t), nFileSize);
		return FALSE;
	}

	lump_p = (lumpinfo_t*)Mem_Malloc(sizeof(lumpinfo_t));
	wad->lumps = lump_p;
	Q_memcpy(lump_p, (char*)raw + header.infotableofs, sizeof(lumpinfo_t));
	W_CleanupName(lump_p->name, lump_p->name);

	if (lump_p->size != lump_p->disksize)
	{
		Con_Printf(const_cast<char*>("Custom file has mismatched lump size ( %i vs. %i )\n"), lump_p->size, lump_p->disksize);
		return FALSE;
	}

	if (lump_p->size < 1)
	{
		Con_Printf(const_cast<char*>("Custom file has bogus lump size %i\n"), lump_p->size);
		return FALSE;
	}

	if (lump_p->filepos < sizeof(wadinfo_t))
	{
		Con_Printf(const_cast<char*>("Custom file has bogus lump offset %i\n"), lump_p->filepos);
		return FALSE;
	}

	if (lump_p->size + lump_p->filepos > header.infotableofs)
	{
		Con_Printf(const_cast<char*>("Custom file has bogus lump %i\n"), 0);
		return FALSE;
	}

	wad->cacheMax = cacheMax;
	wad->lumpCount = 1;
	wad->cacheCount = 0;
	wad->name = Mem_Strdup("tempdecal.wad");
	Draw_AllocateCacheSpace(wad, cacheMax, 0);
	Draw_CacheWadHandler(wad, NULL, 0);

	return TRUE;
}

int Draw_CacheByIndex(cachewad_t* wad, int nIndex, int playernum)
{
	int i;
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif
	char szTestName[32];
	_snprintf(szTestName, sizeof(szTestName), "%03i%02i", playernum, nIndex);
	for (i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++)
	{
		if (!Q_strcmp(szTestName, pic->name))
			break;
	}
	if (i == wad->cacheCount)
	{
		if (i == wad->cacheMax)
			Sys_Error("Cache wad (%s) out of %d entries", wad->name, i);
		wad->cacheCount++;
		_snprintf(pic->name, sizeof(pic->name), "%s", szTestName);
	}
	return i;
}

qboolean CustomDecal_Validate(void* raw, int nFileSize)
{
	qboolean bret = FALSE;
	cachewad_t* fakewad = NULL;
	fakewad = (cachewad_t*)Mem_ZeroMalloc(sizeof(cachewad_t));
	if (fakewad)
	{
		bret = CustomDecal_Init(fakewad, raw, nFileSize, -2);
		if (bret)
			bret = (Draw_CustomCacheGet(fakewad, raw, nFileSize, 0) != NULL);

		Draw_FreeWad(fakewad);
		Mem_Free(fakewad);
	}
	return bret;
}

qboolean CustomDecal_Init(struct cachewad_s* wad, void* raw, int nFileSize, int playernum)
{
	qboolean bret = Draw_CustomCacheWadInit(16, wad, raw, nFileSize);
	if (bret)
	{
		Draw_CacheWadHandler(wad, Draw_MiptexTexture, DECAL_EXTRASIZE);
		for (int i = 0; i < wad->lumpCount; i++)
			Draw_CacheByIndex(wad, i, playernum);
	}
	return bret;
}

void* Draw_CacheGet(cachewad_t* wad, int index)
{
	int i;
	void* dat;
	char* path;
	char name[16];
	char clean[16];
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif
	lumpinfo_t* pLump;

	if (index >= wad->cacheCount)
		Sys_Error("Cache wad indexed before load %s: %d", wad->name, index);

	pic = wad->cache;

	//for (int i = 0; i < wad->cacheCount; i++)
	//	Con_SafePrintf("decal %d: name %s\n", i, pic[i].name);

	dat = Cache_Check(&pic[index].cache);
	path = pic[index].name;

	if (
#if defined(GLQUAKE)
		wad->tempWad || 
#endif
		dat == NULL)
	{
		COM_FileBase(path, name);
		W_CleanupName(name, clean);
		for (i = 0, pLump = wad->lumps; i < wad->lumpCount; i++, pLump++)
		{
			if (!Q_strcmp(clean, pLump->name))
				break;
		}
		if (i >= wad->lumpCount)
			return NULL;

		if (Draw_CacheReload(wad, i, pLump, pic + index, clean, path))
		{
			if (pic->cache.data == NULL)
				Sys_Error(__FUNCTION__ ": failed to load %s", path);
			dat = pic->cache.data;
		}
	}
	return dat;
}

void* Draw_CustomCacheGet(cachewad_t* wad, void* raw, int rawsize, int index)
{
	void* pdata;
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif
	char name[16];
	char clean[16];

	if (wad->cacheCount <= index)
		Sys_Error("Cache wad indexed before load %s: %d", wad->name, index);

	pic = &wad->cache[index];
	pdata = Cache_Check(&pic->cache);
	if (!pdata)
	{
		COM_FileBase(pic->name, name);
		W_CleanupName(name, clean);

		if (Draw_CacheLoadFromCustom(clean, wad, raw, rawsize, pic))
		{
			if (!pic->cache.data)
				Sys_Error(__FUNCTION__ ": failed to load %s", pic->name);
			pdata = pic->cache.data;
		}
	}
	return pdata;
}

#if defined(GLQUAKE)
qboolean Draw_CacheReload(cachewad_t* wad, int i, lumpinfo_t* pLump, cacheentry_t* pic, char* clean, char* path)
#else
qboolean Draw_CacheReload(cachewad_t* wad, int i, lumpinfo_t* pLump, cachepic_t* pic, char* clean, char* path)
#endif
{
	int len;
	byte* buf;
	FileHandle_t hFile;
	//const char *pathID;//unused?//used!

	if (wad->numpaths < 1)
		hFile = FS_Open(wad->name, "rb");
	else
		hFile = FS_OpenPathID(wad->name, "rb", wad->basedirs[wad->lumppathindices[i]]);
	if (!hFile)
		return FALSE;
	len = FS_Size(hFile);
#if defined (GLQUAKE)
	if (wad->tempWad)
	{
		buf = (byte*)Hunk_TempAlloc(wad->cacheExtra + pLump->size + 1);
		pic->cache.data = buf;
	}
	else
#endif
		buf = (byte*)Cache_Alloc(&pic->cache, wad->cacheExtra + pLump->size + 1, clean);
	if (!buf)
		Sys_Error(__FUNCTION__ ": not enough space for %s in %s", path, wad->name);
	buf[pLump->size + wad->cacheExtra] = 0;

	FS_Seek(hFile, pLump->filepos, FILESYSTEM_SEEK_HEAD);
	FS_Read((char*)&buf[wad->cacheExtra], pLump->size, 1, hFile);
	FS_Close(hFile);

	if (wad->pfnCacheBuild)
		wad->pfnCacheBuild(wad, buf);

	return TRUE;
}

qboolean Draw_ValidateCustomLogo(cachewad_t* wad, unsigned char* data, lumpinfo_t* lump)
{
	texture_t tex;
	miptex_t* mip;
	miptex_t tmp;
	int pix;
	int pixoffset;
	int mip2;
	int mip3;
	int nPalleteCount;
	int nSize;

	if (wad->cacheExtra != DECAL_EXTRASIZE)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ": Bad cached wad %s\n"), wad->name);
		return FALSE;
	}

	tex = *(texture_t*)data;
	mip = (miptex_t*)(data + wad->cacheExtra);
	tmp = *mip;

	tex.width = LittleLong(tmp.width);
	tex.height = LittleLong(tmp.height);
	tex.anim_max = 0;
	tex.anim_min = 0;
	tex.anim_total = 0;
	tex.alternate_anims = NULL;
	tex.anim_next = NULL;

	if (!tex.width || tex.width > 256 || tex.height > 256)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ": Bad cached wad %s\n"), wad->name);
		return FALSE;
	}

	for (int i = 0; i < MIPLEVELS; i++)
		tex.offsets[i] = wad->cacheExtra + LittleLong(tmp.offsets[i]);

	pix = tex.width * tex.height;
	pixoffset = pix + (pix >> 2) + (pix >> 4) + (pix >> 6);

	mip2 = (pix >> 2) + tmp.offsets[0] + pix;
	mip3 = (pix >> 4) + mip2;

	if ((tmp.offsets[0] + pix != tmp.offsets[1])
		|| mip2 != tmp.offsets[2]
		|| mip3 != tmp.offsets[3])
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ": Bad cached wad %s\n"), wad->name);
		return FALSE;
	}

	nPalleteCount = *(u_short*)(data + pixoffset + sizeof(texture_t));
	if (nPalleteCount > 256)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ": Bad cached wad palette size %i on %s\n"), nPalleteCount, wad->name);
		return FALSE;
	}

	nSize = pixoffset + LittleLong(tmp.offsets[0]) + 3 * nPalleteCount + 2;
	if (nSize > lump->disksize)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ": Bad cached wad %s\n"), wad->name);
		return FALSE;
	}

	return TRUE;
}

#if defined(GLQUAKE)
qboolean Draw_CacheLoadFromCustom(char* clean, cachewad_t* wad, void* raw, int rawsize, cacheentry_t* pic)
#else
qboolean Draw_CacheLoadFromCustom(char* clean, cachewad_t* wad, void* raw, int rawsize, cachepic_t* pic)
#endif
{
	int idx;
	byte* buf;
	lumpinfo_t* pLump;

	idx = 0;
	if (Q_strlen(clean) > 4)
	{
		idx = Q_atoi(clean + 3);
		if (idx < 0 || idx >= wad->lumpCount)
			return FALSE;

	}
	pLump = &wad->lumps[idx];
	buf = (byte*)Cache_Alloc(&pic->cache, wad->cacheExtra + pLump->size + 1, clean);
	if (!buf)
		Sys_Error(__FUNCTION__ ": not enough space for %s in %s", clean, wad->name);

	buf[wad->cacheExtra + pLump->size] = 0;
	Q_memcpy((char*)buf + wad->cacheExtra, (char*)raw + pLump->filepos, pLump->size);

	if (Draw_ValidateCustomLogo(wad, buf, pLump))
	{
		gfCustomBuild = TRUE;
		Q_strcpy(szCustName, "T");
		Q_memcpy(&szCustName[1], clean, 5);
		szCustName[6] = 0;

		if (wad->pfnCacheBuild)
			wad->pfnCacheBuild(wad, buf);
		gfCustomBuild = FALSE;
		return TRUE;
	}
	return FALSE;
}

int Draw_CacheIndex(cachewad_t* wad, char* path)
{
	int i;
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif
	for (i = 0, pic = wad->cache; i < wad->cacheCount; i++, pic++)
	{
		if (!Q_strcmp(path, pic->name))
			break;
	}
	if (i == wad->cacheCount)
	{
		if (wad->cacheMax == wad->cacheCount)
			Sys_Error("Cache wad (%s) out of %d entries", wad->name, wad->cacheCount);

		wad->cacheCount++;
		Q_strncpy(pic->name, path, 63);
		pic->name[63] = 0;
	}
	return i;
}

int Draw_CacheFindIndex(cachewad_t* wad, char* path)
{
#if defined(GLQUAKE)
	cacheentry_t* pic;
#else
	cachepic_t* pic;
#endif
	pic = wad->cache;
	for (int i = 0; i < wad->cacheCount; i++, pic++)
	{
		if (!Q_strcmp(path, pic->name))
			return i;
	}
	return -1;
}

texture_t *Draw_DecalTexture(int index)
{
	texture_t *retval;
	customization_t *pCust;

	if (index >= 0)
		retval = (texture_t *)Draw_CacheGet(decal_wad, index);
	else
	{
		pCust = cl.players[~index].customdata.pNext;
		if (!pCust || !pCust->bInUse || !pCust->pInfo || !pCust->pBuffer)
			Sys_Error("Failed to load custom decal for player #%i:%s using default decal 0.\n", ~index, cl.players[~index].name);

		retval = (texture_t *)Draw_CustomCacheGet((cachewad_t *)pCust->pInfo, pCust->pBuffer, pCust->resource.nDownloadSize, pCust->nUserData1);
		if (!retval)
			retval = (texture_t *)Draw_CacheGet(decal_wad, 0);
	}
	return retval;
}

void Draw_CacheWadInit(char *name, int cacheMax, cachewad_t *wad)
{
	int len;
	FileHandle_t hFile = FS_Open(name, "rb");
	if (!hFile)
		Sys_Error(__FUNCTION__ ": Couldn't open %s\n", name);
	len = FS_Size(hFile);
	Draw_CacheWadInitFromFile(hFile, len, name, cacheMax, wad);
	FS_Close(hFile);
}

void Draw_DecalSetName(int decal, char *name)
{
	extern void DbgPrint(FILE*, const char* format, ...);
	extern FILE* m_fMessages;
	DbgPrint(m_fMessages, "trying to set name %s to decal %d <%s#%d>\r\n", name, decal, __FILE__, __LINE__);

	if (decal >= MAX_DECALNAMES)
		return;

	int len = sizeof(decal_names[0]) - 1;
	Q_strncpy(decal_names[decal], name, len);
	decal_names[decal][len] = 0;
}

