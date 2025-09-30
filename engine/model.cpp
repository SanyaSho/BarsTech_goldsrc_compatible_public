//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// model.cpp -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"
#include "r_local.h"
#include "draw.h"
#include "sv_main.h"
#include "decals.h"
#include "textures.h"
#include "model.h"

#include "client.h"
#include "server.h"
#include "host.h"
#include "draw.h"
#include "view.h"

model_t* loadmodel;
char loadname[32];	// for hunk tags
char* wadpath;

void Mod_LoadSpriteModel( model_t* mod, void* buffer );
void Mod_LoadBrushModel( model_t* mod, void* buffer );
void Mod_LoadAliasModel( model_t* mod, void* buffer );

model_t mod_known[MAX_KNOWN_MODELS];
int mod_numknown;

/*struct mod_known_info_t
{
	qboolean		shouldCRC;		// Needs a CRC check?
	qboolean		firstCRCDone;	// True when the first CRC check has been finished

	CRC32_t			initialCRC;
};*/

mod_known_info_t mod_known_info[MAX_KNOWN_MODELS];

extern qboolean gSpriteMipMap;


void SW_Mod_Init( void )
{
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void* Mod_Extradata( model_t* mod )
{
	void* r;

	if (!mod)
		return NULL;

	r = Cache_Check(&mod->cache);
	if (r)
		return r;

	if (mod->type == mod_brush)
		Sys_Error("Mod_ExtraData called with mod_brush!\n");

	Mod_LoadModel(mod, TRUE, FALSE);

	if (!mod->cache.data)
		Sys_Error("Mod_Extradata: caching failed");

	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t* Mod_PointInLeaf( vec_t* p, model_t* model )
{
	mnode_t* node;
	float		d;
	mplane_t* plane;

	if (!model || !model->nodes)
		Sys_Error("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t*)node;

		plane = node->plane;

		if (plane->type >= 3)
			d = DotProduct(p, plane->normal) - plane->dist;
		else
			d = p[plane->type] - plane->dist;

		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll( void )
{
	int		i;
	model_t* mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (mod->needload != (NL_NEEDS_LOADED | NL_UNREFERENCED))
		{
			mod->needload = NL_UNREFERENCED;

//FIX FOR CACHE_ALLOC ERRORS:
			if (mod->type == mod_sprite)
				mod->cache.data = NULL;
		}
	}
}

/*
===============
Mod_FillInCRCInfo
===============
*/
void Mod_FillInCRCInfo( qboolean trackCRC, int model_number )
{
	mod_known_info[model_number].shouldCRC = trackCRC;
	mod_known_info[model_number].firstCRCDone = FALSE;
	mod_known_info[model_number].initialCRC = 0;
}

/*
==================
Mod_FindName

==================
*/
model_t* Mod_FindName( qboolean trackCRC, char* name )
{
	int		i;
	model_t* mod;
	model_t* avail = NULL;

	if (!name[0])
		Sys_Error("Mod_FindName: NULL name");

	//
	// search the currently loaded models
	//
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!Q_strcasecmp(mod->name, name))
			break;

		if (mod->needload == NL_UNREFERENCED)
		{
			if (!avail || mod->type != mod_alias && mod->type != mod_studio)
				avail = mod;
		}
	}

	if (i == mod_numknown)
	{
		if (mod_numknown < MAX_KNOWN_MODELS)
		{
			Mod_FillInCRCInfo(trackCRC, mod_numknown);
			mod_numknown++;
		}
		else
		{
			if (!avail)
				Sys_Error("mod_numknown >= MAX_KNOWN_MODELS");

			mod = avail;
			Mod_FillInCRCInfo(trackCRC, avail - mod_known);
		}

		Q_strncpy(mod->name, name, sizeof(mod->name) - 1);
		mod->name[sizeof(mod->name) - 1] = 0;

		if (mod->needload != (NL_NEEDS_LOADED | NL_UNREFERENCED))
			mod->needload = NL_NEEDS_LOADED;
	}

	return mod;
}

/*
===============
Mod_ValidateCRC

Validates file crc
===============
*/
qboolean Mod_ValidateCRC( char* name, CRC32_t crc )
{
	model_t* mod;
	mod_known_info_t* p;

	mod = Mod_FindName(TRUE, name);

	p = &mod_known_info[mod - mod_known];

	if (p->firstCRCDone)
	{
		if (p->initialCRC != crc)
			return FALSE;
	}

	return TRUE;
}

/*
===============
Mod_NeedCRC
===============
*/
void Mod_NeedCRC( char* name, qboolean needCRC )
{
	model_t* mod;
	mod_known_info_t* p;

	mod = Mod_FindName(FALSE, name);

	p = &mod_known_info[mod - mod_known];

	p->shouldCRC = needCRC;
}

qboolean IsCZPlayerModel( uint32 crc, const char* filename );

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t* Mod_LoadModel( model_t* mod, qboolean crash, qboolean trackCRC )
{
	CRC32_t currentCRC;
	char tmpName[MAX_PATH];

	byte* buf;
	mod_known_info_t* pinfo;

	if (mod->type == mod_alias || mod->type == mod_studio)
	{
		if (Cache_Check(&mod->cache))
		{
			mod->needload = NL_PRESENT;
			return mod;
		}
	}
	else
	{
		if (mod->needload == NL_PRESENT || mod->needload == (NL_NEEDS_LOADED | NL_UNREFERENCED))
			return mod;
	}

	// Check for -steam launch param
	if (COM_CheckParm(const_cast<char*>("-steam")) && mod->name[0] == '/')
	{
		char* p = mod->name;
		while (*(++p) == '/')
			;

		strncpy(tmpName, p, sizeof(tmpName) - 1);
		tmpName[sizeof(tmpName) - 1] = 0;

		strncpy(mod->name, tmpName, sizeof(mod->name) - 1);
		mod->name[sizeof(mod->name) - 1] = 0;
	}

	//
	// load the file
	//
	int length;
	buf = COM_LoadFileForMe(mod->name, &length);

	if (!buf)
	{
		if (crash)
			Sys_Error("Mod_NumForName: %s not found", mod->name);

		return NULL;
	}

	if (trackCRC)
	{
		pinfo = &mod_known_info[mod - mod_known];

		if (pinfo->shouldCRC)
		{
			CRC32_Init(&currentCRC);
			CRC32_ProcessBuffer(&currentCRC, buf, length);
			currentCRC = CRC32_Final(currentCRC);

			if (pinfo->firstCRCDone)
			{
				if (currentCRC != pinfo->initialCRC)
				{
					Sys_Error("%s has been modified since starting the engine.  Consider running system diagnostics to check for faulty hardware.\n", mod->name);
				}
			}
			else
			{
				pinfo->firstCRCDone = TRUE;
				pinfo->initialCRC = currentCRC;

				SetCStrikeFlags();

				if (!IsGameSubscribed("czero") && g_bIsCStrike && IsCZPlayerModel(currentCRC, mod->name) && cls.state != ca_dedicated)
				{
					COM_ExplainDisconnection(TRUE, const_cast<char*>("Cannot continue with altered model %s, disconnecting."), mod->name);
					extern void DbgPrint(FILE*, const char* format, ...);
					extern FILE* m_fMessages;
					DbgPrint(m_fMessages, "disconnecting... <%s#%d>\r\n", __FILE__, __LINE__);
					CL_Disconnect();
					return NULL;
				}
			}
		}
	}

	if (developer.value > 1.0)
		Con_DPrintf(const_cast<char*>("loading %s\n"), mod->name);

	//
	// allocate a new model
	//
#ifdef _2020_PATCH
	if (!COM_FileBase_s(mod->name, loadname, sizeof(loadname)))
	{
		Sys_Error("Mod_LoadModel: Bad model name length: %s\n", mod->name);
	}
#else
	COM_FileBase(mod->name, loadname);
#endif

	loadmodel = mod;

	//
	// fill it in
	//

	mod->needload = NL_PRESENT;

	// call the apropriate loader
	switch (LittleLong(*(unsigned*)buf))
	{
		case IDPOLYHEADER:
			Mod_LoadAliasModel(mod, buf);
			break;

		case IDSPRITEHEADER:
			Mod_LoadSpriteModel(mod, buf);
			break;

		case IDSTUDIOHEADER:
			Mod_LoadStudioModel(mod, buf);
			break;

		default:
			Mod_LoadBrushModel(mod, buf);
			break;
	}

	if (g_modfuncs.m_pfnModelLoad)
		g_modfuncs.m_pfnModelLoad(mod, buf);

	Mem_Free(buf);
	return mod;
}

void Mod_MarkClient( model_t* pModel )
{
	pModel->needload = (NL_NEEDS_LOADED | NL_UNREFERENCED);
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t* Mod_ForName( const char* name, qboolean crash, qboolean trackCRC )
{
	model_t* mod;

	mod = Mod_FindName(trackCRC, (char*)name);

	if (!mod)
		return NULL;

	return Mod_LoadModel(mod, crash, trackCRC);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte* mod_base;

cachewad_t ad_wad;

int tested;
int ad_enabled;

void Mod_AdInit( void )
{
	static char filename[MAX_PATH];

	int iAd;
	const char* arg;

	tested = TRUE;

	iAd = COM_CheckParm(const_cast<char*>("-ad"));
	if (!iAd)
		return;

	arg = com_argv[iAd + 1];
	if (!arg || !*arg)
		return;

	_snprintf(filename, sizeof(filename), "%s", arg);

	// Check if the file exists
	if (FS_FileSize(filename) <= 0)
	{
		Con_Printf(const_cast<char*>("No -ad file specified, skipping\n"));
		return;
	}

	Draw_CacheWadInit(filename, 16, &ad_wad);
#ifndef SWDS
	Draw_CacheWadHandler(&ad_wad, Draw_MiptexTexture, DECAL_EXTRASIZE);
#endif

	ad_enabled = TRUE;
}

void Mod_AdSwap( texture_t* src, int pixels, int entries )
{
	int		i;
	byte* mippal;
	unsigned short*	texpal;

	texture_t* texture;

	if (!tested)
	{
		return;
	}

	texture = (texture_t*)Draw_CacheGet(&ad_wad, Draw_CacheIndex(&ad_wad, const_cast<char*>("img")));
	if (texture)
	{
		Q_memcpy(src + 1, texture + 1, pixels);

		mippal = (byte*)&texture[1] + pixels + sizeof(word);
		texpal = (unsigned short*)((char*)&src[1] + pixels + sizeof(word));

		for (i = 0; i < entries; i++, mippal += sizeof(color24), texpal += 4)
		{
			texpal[0] = mippal[2];
			texpal[1] = mippal[1];
			texpal[2] = mippal[0];
			texpal[3] = 0;
		}
	}
}

/*
===============
Mod_LoadTextures
===============
*/
void Mod_LoadTextures( lump_t* l )
{
	int				i, j, num, max, altmax;
	int				palette, pixels;
	double			starttime;
	miptex_t*		mt;
	texture_t*		tx, * tx2;
	texture_t*		anims[10];
	texture_t*		altanims[10];
	dmiptexlump_t*	m;
	byte*			mippal;
	unsigned short*	texpal;
	qboolean		wads_parsed;
	// for sw: 256x256x85/64x4+768+2+64
	// for hw: 256x256x85/64x4+768+2+72
	// common expr: maxw*maxh*85/64*4 + sizeof(texture_t) + sizeof(word) + MAXCOLORSUSED*3 where word -> colorsused, maxw=maxh=256, 4 -> color channels, 85/64 - mipmaps
	byte			dtexdata[256 * 256 * 85 / 64 * 4 + sizeof(texture_t) + sizeof(word) + 256 * sizeof(color24)];

	starttime = Sys_FloatTime();

	if (!tested)
	{
		Mod_AdInit();
	}

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}
	m = (dmiptexlump_t*)&mod_base[l->fileofs];

	m->nummiptex = LittleLong(m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = (texture_t**)Hunk_AllocName(m->nummiptex * sizeof(texture_t**), loadname);

	wads_parsed = FALSE;

	for (i = 0; i < m->nummiptex; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);

		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t*)((byte*)m + m->dataofs[i]);

		int iLumpSize;
		if (r_wadtextures.value || !LittleLong(mt->offsets[0]))
		{
			if (!wads_parsed)
			{
				TEX_InitFromWad(wadpath);
				TEX_AddAnimatingTextures();

				wads_parsed = TRUE;
			}

			iLumpSize = TEX_LoadLump(mt->name, dtexdata, sizeof(dtexdata));
			if (!iLumpSize)
			{
				m->dataofs[i] = -1;
				continue;
			}

			mt = (miptex_t*)dtexdata;
		}

		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong(mt->offsets[j]);

		mt->width = LittleLong(mt->width);
		mt->height = LittleLong(mt->height);

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error("Texture %s is not 16 aligned", mt->name);

		// total amout of pixels and palette entires
		pixels = 85 * mt->height * mt->width / 64;
		palette = *(unsigned short*)((char*)mt + sizeof(miptex_t) + pixels);

		tx = (texture_t*)Hunk_AllocName(sizeof(word) + sizeof(PackedColorVec) * palette + pixels + sizeof(texture_t), loadname);
		loadmodel->textures[i] = tx;

		// copy data
		Q_memcpy(tx->name, mt->name, sizeof(tx->name));

		if (strchr(tx->name, '~'))
			tx->name[2] = ' ';

		tx->width = mt->width;
		tx->height = mt->height;

		for (j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);

		tx->paloffset = sizeof(texture_t) + pixels + sizeof(word);

		// the pixels immediately follow the structures
		Q_memcpy(&tx[1], &mt[1], pixels + sizeof(word));

		if (!Q_strncmp(mt->name, "sky", 3))
			R_InitSky();

		mippal = (byte*)&mt[1] + pixels + sizeof(word);
		texpal = (unsigned short*)((char*)&tx[1] + pixels + sizeof(word));

		for (j = 0; j < palette; j++, mippal += sizeof(color24), texpal += 4)
		{
			texpal[0] = texgammatable[mippal[2]];
			texpal[1] = texgammatable[mippal[1]];
			texpal[2] = texgammatable[mippal[0]];
			texpal[3] = 0;
		}

		if (ad_enabled && !Q_strcasecmp(tx->name, "DEFAULT"))
		{
			Mod_AdSwap(tx, pixels, palette);
		}
	}

	if (wads_parsed)
	{
		TEX_CleanupWadInfo();
	}

	//
	// sequence the animations
	//
	for (i = 0; i < m->nummiptex; i++)
	{
		tx = loadmodel->textures[i];

		if (!tx)
			continue;

		if (tx->name[0] != '+' && tx->name[0] != '-')
			continue;

		if (tx->anim_next)
			continue; // allready sequenced

		// find the number of frames in the animation
		Q_memset(anims, 0, sizeof(anims));
		Q_memset(altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;

		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';

		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error("Bad animating texture %s", tx->name);

		for (j = i + 1; j < m->nummiptex; j++)
		{
			tx2 = loadmodel->textures[j];

			if (!tx2)
				continue;

			if (tx2->name[0] != '+' && tx2->name[0] != '-')
				continue;

			if (Q_strcmp(tx2->name + 2, tx->name + 2))
				continue;

			num = tx2->name[1];

			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';

			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;

				if (num + 1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;

				if (num + 1 > altmax)
					altmax = num + 1;
			}
			else
				Sys_Error("Bad animating texture %s", tx->name);
		}

//#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];

			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);

			tx2->anim_total = max;
			tx2->anim_min = j;
			tx2->anim_max = (j + 1);
			tx2->anim_next = anims[(j + 1) % max];

			if (altmax)
				tx2->alternate_anims = altanims[0];
		}

		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];

			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);

			tx2->anim_total = altmax;
			tx2->anim_min = j;
			tx2->anim_max = (j + 1);
			tx2->anim_next = altanims[(j + 1) % altmax];

			if (max)
				tx2->alternate_anims = anims[0];
		}
	}

	Con_DPrintf(const_cast<char*>("Texture load: %6.1fms\n"), (Sys_FloatTime() - starttime) * 1000.0);
}

/*
===============
Mod_LoadLighting
===============
*/
void Mod_LoadLighting( lump_t* l )
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = (color24*)Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility( lump_t* l )
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}

	loadmodel->visdata = (byte*)Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities( lump_t* l )
{
	char* pszInputStream;

	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = (char*)Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->entities, mod_base + l->fileofs, l->filelen);

	if (loadmodel->entities)
	{
		pszInputStream = COM_Parse(loadmodel->entities);

		while (*pszInputStream && com_token[0] != '}')
		{
			if (!Q_strcmp(com_token, "wad"))
			{
				COM_Parse(pszInputStream);

				if (wadpath)
					Mem_Free(wadpath);

				wadpath = Mem_Strdup(com_token);
				break;
			}

			pszInputStream = COM_Parse(pszInputStream);
		}
	}
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes( lump_t* l )
{
	dvertex_t* in;
	mvertex_t* out;
	int			i, count;

	in = (dvertex_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mvertex_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels( lump_t* l )
{
	dmodel_t* in;
	dmodel_t* out;
	int			i, j, count;

	in = (dmodel_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (dmodel_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}

		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);

		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges( lump_t* l )
{
	dedge_t* in;
	medge_t* out;
	int 	i, count;

	in = (dedge_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (medge_t*)Hunk_AllocName((count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo( lump_t* l )
{
	texinfo_t* in;
	mtexinfo_t* out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (texinfo_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);

		len1 = Length(out->vecs[0]);
		len2 = Length(out->vecs[1]);
		len1 = (len1 + len2) / 2;

		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error("miptex >= loadmodel->numtextures");

			out->texture = loadmodel->textures[miptex];

			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents( msurface_t* s )
{
	float	mins[2], maxs[2], val;
	int		i, j, e;
	mvertex_t* v = NULL;
	mtexinfo_t* tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++)
	{
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j = 0; j < 2; j++)
		{
			// FIXED: loss of floating point
			val = v->position[0] * (double)tex->vecs[j][0] +
				v->position[1] * (double)tex->vecs[j][1] +
				v->position[2] * (double)tex->vecs[j][2] +
				(double)tex->vecs[j][3];

			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++)
	{
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 256)
		{
			Sys_Error("Bad surface extents %d/%d at position (%d,%d,%d)", s->extents[0], s->extents[1],
				(int)v->position[0], (int)v->position[1], (int)v->position[2]);
		}
	}
}


/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces( lump_t* l )
{
	dface_t* in;
	msurface_t* out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (dface_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);

	out = (msurface_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;
		out->pdecals = NULL; // the surface has no decals by default

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);

		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort(in->texinfo);

		CalcSurfaceExtents(out);

		// lighting info

		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];

		i = LittleLong(in->lightofs);

		if (i == -1)
			out->samples = NULL;
		else
			out->samples = (color24*)((byte*)loadmodel->lightdata + i);

		// set the drawing flags flag

		if (!Q_strncmp(out->texinfo->texture->name, "sky", 3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			continue;
		}

		if (!(out->flags & (SURF_DRAWSKY | SURF_DRAWTILED)) && out->samples == 0)
			OutputDebugString("");

		if (!Q_strncmp(out->texinfo->texture->name, "scroll", 6))	// scroll
		{
			out->flags |= SURF_DRAWTILED;
			out->extents[0] = out->texinfo->texture->width;
			out->extents[1] = out->texinfo->texture->height;
			continue;
		}

		if (out->texinfo->texture->name[0] == '!' ||
			!Q_strncasecmp(out->texinfo->texture->name, "laser", 5) ||
			!Q_strncasecmp(out->texinfo->texture->name, "water", 5))	// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
				out->texinfo->flags |= TEX_SPECIAL;
			}
			continue;
		}

		if (out->texinfo->flags & TEX_SPECIAL)
		{
			out->flags |= SURF_DRAWTILED;
			out->extents[0] = out->texinfo->texture->width;
			out->extents[1] = out->texinfo->texture->height;
			continue;
		}
	}
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent( mnode_t* node, mnode_t* parent )
{
	node->parent = parent;

	if (node->contents < 0)
		return;

	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes( lump_t* l )
{
	int			i, j, count, p;
	dnode_t* in;
	mnode_t* out;

	in = (dnode_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(dnode_t))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j = 0; j < 2; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t*)(loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent(loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs( lump_t* l )
{
	dleaf_t* in;
	mleaf_t* out;
	int			i, j, count, p;

	in = (dleaf_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mleaf_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;

		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes( lump_t* l )
{
	dclipnode_t* in, * out;
	int			i, count;
	hull_t* hull;

	in = (dclipnode_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (dclipnode_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -36;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 36;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -32;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -18;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 18;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0( void )
{
	mnode_t* in, * child;
	dclipnode_t* out;
	int			i, j, count;
	hull_t* hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = (dclipnode_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;

		for (j = 0; j < 2; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces( lump_t* l )
{
	int		i, j, count;
	short* in;
	msurface_t** out;

	in = (short*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (msurface_t**)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++)
	{
		j = LittleShort(in[i]);

		if (j >= loadmodel->numsurfaces)
			Sys_Error("Mod_ParseMarksurfaces: bad surface number");

		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges( lump_t* l )
{
	int		i, count;
	int* in, * out;

	in = (int*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (int*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes( lump_t* l )
{
	int			i, j;
	mplane_t* out;
	dplane_t* in;
	int			count;
	int			bits;

	in = (dplane_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mplane_t*)Hunk_AllocName(count * 2 * sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;
		for (j = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds( vec_t* mins, vec_t* maxs )
{
	int		i;
	vec3_t	corner;

	for (i = 0; i < 3; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length(corner);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel( model_t* mod, void* buffer )
{
	int			i, j, k;
	dheader_t* header;
	dmodel_t* bm;

	loadmodel->type = mod_brush;

	header = (dheader_t*)buffer;

	i = LittleLong(header->version);
	if (i != Q1BSP_VERSION && i != BSPVERSION)
		Sys_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

	// swap all the lumps
	mod_base = (byte*)header;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int*)header)[i] = LittleLong(((int*)header)[i]);

	// load into heap

	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);

	if (!stricmp(com_gamedir, "bshift"))
	{
		Mod_LoadEntities(&header->lumps[LUMP_ENTITIES + 1]);
		Mod_LoadTextures(&header->lumps[LUMP_TEXTURES]);
		Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
		Mod_LoadPlanes(&header->lumps[LUMP_PLANES - 1]);
	}
	else
	{
		Mod_LoadEntities(&header->lumps[LUMP_ENTITIES]);
		Mod_LoadTextures(&header->lumps[LUMP_TEXTURES]);
		Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
		Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	}

	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);

	for (k = 0; k < mod->numsurfaces; k++)
	{
		if (mod->surfaces[k].numedges + mod->surfaces[k].firstedge > mod->numsurfedges)
			Sys_Error("MOD_LoadBrushModel: more surfedges referenced than loaded in %s", mod->name);
	}

	for (k = 0; k < mod->numleafs; k++)
	{
		if (mod->leafs[k].nummarksurfaces + mod->leafs[k].firstmarksurface - mod->marksurfaces > mod->nummarksurfaces)
			Sys_Error("MOD_LoadBrushModel: more marksurfaces referenced than loaded in %s", mod->name);
	}

	for (k = 0; k < mod->numnodes; k++)
	{
		if (mod->nodes[k].firstsurface + mod->nodes[k].numsurfaces > mod->numsurfaces)
			Sys_Error("MOD_LoadBrushModel: more surfaces referenced than loaded in %s", mod->name);
	}

	for (k = 0; k < mod->numsubmodels; k++)
	{
		if (mod->submodels[k].firstface + mod->submodels[k].numfaces > mod->numsurfaces)
			Sys_Error("MOD_LoadBrushModel: more faces referenced than loaded in %s", mod->name);
	}

	Mod_MakeHull0();

	mod->numframes = 2;		// regular and alternate animation
	mod->flags = 0;

	//
	// set up the submodels (FIXME: this is confusing)
	//
	for (i = 0; i < mod->numsubmodels; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1; j < MAX_MAP_HULLS; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);
		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1)
		{
			// duplicate the basic information
			char name[10];

			_snprintf(name, sizeof(name), "*%i", i + 1);
			loadmodel = Mod_FindName(FALSE, name);
			*loadmodel = *mod;
			Q_strncpy(loadmodel->name, name, sizeof(loadmodel->name) - 1);
			loadmodel->name[sizeof(loadmodel->name) - 1] = 0;
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasFrame
=================
*/
void* Mod_LoadAliasFrame( void* pin, int* pframeindex, int numv,
	trivertx_t* pbboxmin, trivertx_t* pbboxmax, aliashdr_t* pheader, char* name )
{
	trivertx_t* pframe, * pinframe;
	int				i, j;
	daliasframe_t* pdaliasframe;

	pdaliasframe = (daliasframe_t*)pin;

	Q_strcpy(name, pdaliasframe->name);

	for (i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about
		// endianness
		pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
		pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t*)(pdaliasframe + 1);
	pframe = (trivertx_t*)Hunk_AllocName(numv * sizeof(*pframe), loadname);

	*pframeindex = (byte*)pframe - (byte*)pheader;

	for (j = 0; j < numv; j++)
	{
		int		k;

		// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k = 0; k < 3; k++)
		{
			pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	pinframe += numv;

	return (void*)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void* Mod_LoadAliasGroup( void* pin, int* pframeindex, int numv,
	trivertx_t* pbboxmin, trivertx_t* pbboxmax, aliashdr_t* pheader, char* name )
{
	daliasgroup_t* pingroup;
	maliasgroup_t* paliasgroup;
	int					i, numframes;
	daliasinterval_t* pin_intervals;
	float* poutintervals;
	void* ptemp;

	pingroup = (daliasgroup_t*)pin;

	numframes = LittleLong(pingroup->numframes);

	paliasgroup = (maliasgroup_t*)Hunk_AllocName(sizeof(paliasgroup->frames[0]) * numframes + 8, loadname);

	paliasgroup->numframes = numframes;

	for (i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = pingroup->bboxmin.v[i];
		pbboxmax->v[i] = pingroup->bboxmax.v[i];
	}

	*pframeindex = (byte*)paliasgroup - (byte*)pheader;

	pin_intervals = (daliasinterval_t*)(pingroup + 1);

	poutintervals = (float*)Hunk_AllocName(numframes * sizeof(float), loadname);

	paliasgroup->intervals = (byte*)poutintervals - (byte*)pheader;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat(pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error("Mod_LoadAliasGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void*)pin_intervals;

	for (i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadAliasFrame(ptemp,
			&paliasgroup->frames[i].frame,
			numv,
			&paliasgroup->frames[i].bboxmin,
			&paliasgroup->frames[i].bboxmax,
			pheader, name);
	}

	return ptemp;
}


/*
=================
Mod_LoadAliasSkin
=================
*/
void* Mod_LoadAliasSkin( void* pin, int* pskinindex, int skinsize,
	aliashdr_t* pheader )
{
	byte* pskin, * pinskin;

	pskin = (byte*)Hunk_AllocName(skinsize * r_pixbytes, loadname);
	pinskin = (byte*)pin;
	*pskinindex = (byte*)pskin - (byte*)pheader;

	if (r_pixbytes == 1)
	{
		Q_memcpy(pskin, pinskin, skinsize);
	}
	else if (r_pixbytes == 2)
	{
	}
	else
	{
		Sys_Error("Mod_LoadAliasSkin: driver set invalid r_pixbytes: %d\n",
			r_pixbytes);
	}

	pinskin += skinsize;

	return ((void*)pinskin);
}


/*
=================
Mod_LoadAliasSkinGroup
=================
*/
void* Mod_LoadAliasSkinGroup( void* pin, int* pskinindex, int skinsize,
	aliashdr_t* pheader )
{
	daliasskingroup_t* pinskingroup;
	maliasskingroup_t* paliasskingroup;
	int						i, numskins;
	daliasskininterval_t* pinskinintervals;
	float* poutskinintervals;
	void* ptemp;

	pinskingroup = (daliasskingroup_t*)pin;

	numskins = LittleLong(pinskingroup->numskins);

	paliasskingroup = (maliasskingroup_t*)Hunk_AllocName(sizeof(paliasskingroup->skindescs[0]) * numskins + 8, loadname);

	paliasskingroup->numskins = numskins;

	*pskinindex = (byte*)paliasskingroup - (byte*)pheader;

	pinskinintervals = (daliasskininterval_t*)(pinskingroup + 1);

	poutskinintervals = (float*)Hunk_AllocName(numskins * sizeof(float), loadname);

	paliasskingroup->intervals = (byte*)poutskinintervals - (byte*)pheader;

	for (i = 0; i < numskins; i++)
	{
		*poutskinintervals = LittleFloat(pinskinintervals->interval);
		if (*poutskinintervals <= 0)
			Sys_Error("Mod_LoadAliasSkinGroup: interval<=0");

		poutskinintervals++;
		pinskinintervals++;
	}

	ptemp = (void*)pinskinintervals;

	for (i = 0; i < numskins; i++)
	{
		ptemp = Mod_LoadAliasSkin(ptemp,
			&paliasskingroup->skindescs[i].skin, skinsize, pheader);
	}

	return ptemp;
}


/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel( model_t* mod, void* buffer )
{
	int					i;
	mdl_t* pmodel, * pinmodel;
	stvert_t* pstverts, * pinstverts;
	aliashdr_t* pheader;
	mtriangle_t* ptri;
	dtriangle_t* pintriangles;
	int					version, numframes, numskins;
	int					size;
	daliasframetype_t* pframetype;
	daliasskintype_t* pskintype;
	maliasskindesc_t* pskindesc;
	int					skinsize;
	int					start, end, total;

	color24* pSource;
	PackedColorVec* pPalette;

	start = Hunk_LowMark();

	pinmodel = (mdl_t*)buffer;

	version = LittleLong(pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error("Mod_LoadAliasModel: Model %s has wrong version number (%i should be %i)",
			mod->name, version, ALIAS_VERSION);

	//
	// allocate space for a working header, plus all the data except the frames,
	// skin and group info
	//
	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mdl_t) +
			LittleLong (pinmodel->numverts) * sizeof (stvert_t) +
			LittleLong (pinmodel->numtris) * sizeof (mtriangle_t);

	pheader = (aliashdr_t*)Hunk_AllocName(size, loadname);
	pmodel = (mdl_t*)&pheader->frames[LittleLong(pinmodel->numframes)];

	//	mod->cache.data = pheader;
	mod->flags = LittleLong(pinmodel->flags);

	//
	// endian-adjust and copy the data, starting with the alias model header
	//
	pmodel->boundingradius = LittleFloat(pinmodel->boundingradius);
	pmodel->numskins = LittleLong(pinmodel->numskins);
	pmodel->skinwidth = LittleLong(pinmodel->skinwidth);
	pmodel->skinheight = LittleLong(pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		Sys_Error("Mod_LoadAliasModel: Model %s has a skin taller than %d", mod->name,
			MAX_LBM_HEIGHT);

	pmodel->numverts = LittleLong(pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Sys_Error("Mod_LoadAliasModel: Model %s has no vertices: %d", mod->name, pmodel->numverts);

	if (pmodel->numverts > MAXALIASVERTS)
		Sys_Error("Mod_LoadAliasModel: Model %s has too many vertices: %d", mod->name, pmodel->numverts);

	pmodel->numtris = LittleLong(pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Sys_Error("Mod_LoadAliasModel: Model %s has no triangles: %d", mod->name, pmodel->numtris);

	pmodel->numframes = LittleLong(pinmodel->numframes);

	if (pmodel->numframes <= 0)
		Sys_Error("Mod_LoadAliasModel: Model %s has no frames: %d", mod->name, pmodel->numframes);

	pmodel->size = LittleFloat(pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = (synctype_t)LittleLong(pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i = 0; i < 3; i++)
	{
		pmodel->scale[i] = LittleFloat(pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat(pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat(pinmodel->eyeposition[i]);
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;

	if (pmodel->skinwidth & 0x03)
		Sys_Error("Mod_LoadAliasModel: skinwidth not multiple of 4");

	pheader->model = (byte*)pmodel - (byte*)pheader;

	//
	// load the skins
	//
	skinsize = pmodel->skinheight * pmodel->skinwidth;

	if (numskins < 1)
		Sys_Error("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	pskintype = (daliasskintype_t*)&pinmodel[1];

	pskindesc = (maliasskindesc_t*)Hunk_AllocName(numskins * sizeof(maliasskindesc_t),
		loadname);

	pheader->skindesc = (byte*)pskindesc - (byte*)pheader;

	for (i = 0; i < numskins; i++)
	{
		aliasskintype_t	skintype;

		skintype = (aliasskintype_t)LittleLong(pskintype->type);
		pskindesc[i].type = skintype;

		if (skintype == ALIAS_SKIN_SINGLE)
		{
			pskintype = (daliasskintype_t*)
				Mod_LoadAliasSkin(pskintype + 1,
					&pskindesc[i].skin,
					skinsize, pheader);
		}
		else
		{
			pskintype = (daliasskintype_t*)
				Mod_LoadAliasSkinGroup(pskintype + 1,
					&pskindesc[i].skin,
					skinsize, pheader);
		}
	}

	//
	// set base s and t vertices
	//
	pstverts = (stvert_t*)&pmodel[1];
	pinstverts = (stvert_t*)pskintype;

	pheader->stverts = (byte*)pstverts - (byte*)pheader;

	for (i = 0; i < pmodel->numverts; i++)
	{
		pstverts[i].onseam = LittleLong(pinstverts[i].onseam);
		// put s and t in 16.16 format
		pstverts[i].s = LittleLong(pinstverts[i].s) << 16;
		pstverts[i].t = LittleLong(pinstverts[i].t) << 16;
	}

	//
	// set up the triangles
	//
	ptri = (mtriangle_t*)&pstverts[pmodel->numverts];
	pintriangles = (dtriangle_t*)&pinstverts[pmodel->numverts];

	pheader->triangles = (byte*)ptri - (byte*)pheader;

	for (i = 0; i < pmodel->numtris; i++)
	{
		int		j;

		ptri[i].facesfront = LittleLong(pintriangles[i].facesfront);

		for (j = 0; j < 3; j++)
		{
			ptri[i].vertindex[j] =
				LittleLong(pintriangles[i].vertindex[j]);
		}
	}

	//
	// load the frames
	//
	if (numframes < 1)
		Sys_Error("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pframetype = (daliasframetype_t*)&pintriangles[pmodel->numtris];

	for (i = 0; i < numframes; i++)
	{
		aliasframetype_t	frametype;

		frametype = (aliasframetype_t)LittleLong(pframetype->type);
		pheader->frames[i].type = frametype;

		if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t*)
				Mod_LoadAliasFrame(pframetype + 1,
					&pheader->frames[i].frame,
					pmodel->numverts,
					&pheader->frames[i].bboxmin,
					&pheader->frames[i].bboxmax,
					pheader, pheader->frames[i].name);
		}
		else
		{
			pframetype = (daliasframetype_t*)
				Mod_LoadAliasGroup(pframetype + 1,
					&pheader->frames[i].frame,
					pmodel->numverts,
					&pheader->frames[i].bboxmin,
					&pheader->frames[i].bboxmax,
					pheader, pheader->frames[i].name);
		}
	}

	mod->type = mod_alias;

	// FIXME: do this right
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;

	// 256 - max colorsused
	pPalette = (PackedColorVec*)Hunk_AllocName(sizeof(PackedColorVec) * 256, loadname);
	pSource = (color24*)pframetype;

	for (i = 0; i < 256; i++)
	{
		pPalette[i].b = pSource->r;
		pPalette[i].g = pSource->g;
		pPalette[i].r = pSource->b;
		pPalette[i].a = 0;

		pSource++;
	}

	/*pheader->palette*/*(int*)((byte*)pheader + 16) = (byte*)pPalette - (byte*)pheader;

	//
	// move the complete, relocatable alias model to the cache
	//	
	end = Hunk_LowMark();
	total = end - start;

	Cache_Alloc(&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	Q_memcpy(mod->cache.data, pheader, total);

	Hunk_FreeToLowMark(start);
}

//=============================================================================

/*
===============
Mod_LoadSpriteFrame
===============
*/
void* Mod_LoadSpriteFrame( void* pin, mspriteframe_t** ppframe )
{
	dspriteframe_t* pinframe;
	mspriteframe_t* pspriteframe;
	int					width, height, size, origin[2];

	pinframe = (dspriteframe_t*)pin;

	width = LittleLong(pinframe->width);
	height = LittleLong(pinframe->height);
	size = width * height;

	pspriteframe = (mspriteframe_t*)Hunk_AllocName(sizeof(mspriteframe_t) + size * r_pixbytes,
		loadname);

	Q_memset(pspriteframe, 0, sizeof(mspriteframe_t) + size * r_pixbytes);
	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong(pinframe->origin[0]);
	origin[1] = LittleLong(pinframe->origin[1]);

	pspriteframe->up = (float)origin[1];
	pspriteframe->down = (float)(origin[1] - height);
	pspriteframe->left = (float)origin[0];
	pspriteframe->right = (float)(width + origin[0]);

	if (r_pixbytes == 1)
	{
		Q_memcpy(&pspriteframe->pixels[0], (byte*)(pinframe + 1), size);
	}
	else if (r_pixbytes == 2)
	{
	}
	else
	{
		Sys_Error("Mod_LoadSpriteFrame: driver set invalid r_pixbytes: %d\n",
			r_pixbytes);
	}

	return (void*)((byte*)pinframe + sizeof(dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void* Mod_LoadSpriteGroup( void* pin, mspriteframe_t** ppframe )
{
	dspritegroup_t* pingroup;
	mspritegroup_t* pspritegroup;
	int					i, numframes;
	dspriteinterval_t* pin_intervals;
	float* poutintervals;
	void* ptemp;

	pingroup = (dspritegroup_t*)pin;
	numframes = LittleLong(pingroup->numframes);

	pspritegroup = (mspritegroup_t*)Hunk_AllocName(sizeof(mspritegroup_t) + (numframes - 1) * sizeof(pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t*)pspritegroup;

	pin_intervals = (dspriteinterval_t*)(pingroup + 1);

	poutintervals = (float*)Hunk_AllocName(numframes * sizeof(float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat(pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void*)pin_intervals;

	for (i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadSpriteFrame(ptemp, &pspritegroup->frames[i]);
	}

	return ptemp;
}


/*
===============
Mod_LoadSpriteModel
===============
*/
void Mod_LoadSpriteModel( model_t* mod, void* buffer )
{
	int					i;
	int					version;
	dsprite_t* pin;
	msprite_t* psprite;
	int					numframes;
	int					size;
	int					palsize;
	dspriteframetype_t* pframetype;

	unsigned char* palsrc;
	unsigned short* paldest;

	pin = (dsprite_t*)buffer;

	version = LittleLong(pin->version);

	if (version != SPRITE_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong(pin->numframes);

	palsize = *(unsigned short*)&pin[1];

	// word - colorsused
	size = sizeof(PackedColorVec) * palsize + sizeof(word) + sizeof(mspriteframedesc_t) * numframes + sizeof(msprite_t) - sizeof(mspriteframedesc_t);

	psprite = (msprite_t*)Hunk_AllocName(size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong(pin->type);

	psprite->texFormat = LittleLong(pin->texFormat);

	psprite->maxwidth = LittleLong(pin->width);
	psprite->maxheight = LittleLong(pin->height);
	psprite->beamlength = LittleFloat(pin->beamlength);

	mod->synctype = (synctype_t)LittleLong(pin->synctype);

	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

	// word - colorsused
	psprite->paloffset = sizeof(mspriteframedesc_t) * numframes + sizeof(msprite_t) - sizeof(mspriteframedesc_t) + sizeof(word);

	palsrc = (unsigned char*)(&pin[1]) + sizeof(word);
	paldest = (unsigned short*)((char*)(psprite + 1) + sizeof(psprite->frames) * (numframes - 1));
	*(paldest++) = palsize;

	for (i = 0; i < palsize; i++, paldest += 4, palsrc += sizeof(color24))
	{
		paldest[0] = palsrc[0];
		paldest[1] = palsrc[1];
		paldest[2] = palsrc[2];
		paldest[3] = 0;
	}

	//
	// load the frames
	//
	if (numframes < 1)
		Sys_Error("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;
	mod->flags = 0;

	pframetype = (dspriteframetype_t*)((char*)(pin + 1) + sizeof(word) + sizeof(color24) * palsize);

	for (i = 0; i < numframes; i++)
	{
		spriteframetype_t	frametype;

		frametype = (spriteframetype_t)LittleLong(pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t*)Mod_LoadSpriteFrame(pframetype + 1, &psprite->frames[i].frameptr);
		}
		else
		{
			pframetype = (dspriteframetype_t*)Mod_LoadSpriteGroup(pframetype + 1, &psprite->frames[i].frameptr);
		}
	}

	mod->type = mod_sprite;
}

/*
===============
Mod_UnloadSpriteTextures
===============
*/
void Mod_UnloadSpriteTextures( model_t* pModel )
{
	if (!pModel)
		return;

	if (pModel->type == mod_sprite)
	{
		pModel->needload = NL_NEEDS_LOADED;
	}
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print( void )
{
	int		i;
	model_t* mod;

	Con_Printf(const_cast<char*>("Cached models:\n"));

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		Con_Printf(const_cast<char*>("%8p : %s"), mod->cache.data, mod->name);

		if (mod->needload & NL_UNREFERENCED)
			Con_Printf(const_cast<char*>(" (!R)"));
		if (mod->needload & NL_NEEDS_LOADED)
			Con_Printf(const_cast<char*>(" (!P)"));

		Con_Printf(const_cast<char*>("\n"));
	}
}

/*
===============
Mod_ChangeGame
===============
*/
void Mod_ChangeGame( void )
{
	int i;

	model_t* mod;
	mod_known_info_t* p;

	for (i = 0; i < mod_numknown; i++)
	{
		mod = &mod_known[i];

		if (mod->type == mod_studio)
		{
			if (Cache_Check(&mod->cache))
				Cache_Free(&mod->cache);
		}

		p = &mod_known_info[i];

		p->firstCRCDone = FALSE;
		p->initialCRC = 0;
	}
}
