//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// gl_rmisc.cpp

#include "quakedef.h"
#include "sv_main.h"
#include "draw.h"
#include "r_part.h"
#include "r_triangle.h"
#include "screen.h"
#include "common.h"
#include "r_part.h"
#include "sys_getmodes.h"
#include "view.h"

qboolean atismoothing; // GL_RMISC.C:20

extern bool gl_texsort;

void VID_TakeSnapshotRect( const char* pFilename, int x, int y, int w, int h );

cvar_t ati_npatch = { const_cast<char*>("ati_npatch"), const_cast<char*>("1.0"), FCVAR_ARCHIVE }; // GL_RMISC.C:26

cvar_t r_glowshellfreq = { const_cast<char*>("r_glowshellfreq"), const_cast<char*>("2.2") }; // GL_RMISC.C:47
cvar_t gl_affinemodels = { const_cast<char*>("gl_affinemodels"), const_cast<char*>("0") }; // GL_RMISC.C:52
cvar_t gl_spriteblend = { const_cast<char*>("gl_spriteblend"), const_cast<char*>("1"), FCVAR_ARCHIVE }; // GL_RMISC.C:57
cvar_t gl_watersides = { const_cast<char*>("gl_watersides"), const_cast<char*>("0") }; // GL_RMISC.C:63

cvar_t r_norefresh = { const_cast<char*>("r_norefresh"), const_cast<char*>("0") };
cvar_t r_wadtextures = { const_cast<char*>("r_wadtextures"), const_cast<char*>("0") };

cvar_t gl_wateramp = { const_cast<char*>("gl_wateramp"), const_cast<char*>("0.3"), FCVAR_ARCHIVE };
cvar_t gl_zmax = { const_cast<char*>("gl_zmax"), const_cast<char*>("4096") };

cvar_t r_decals = { const_cast<char*>("r_decals"), const_cast<char*>("4096") };
cvar_t sp_decals = { const_cast<char*>("sp_decals"), const_cast<char*>("4096"), FCVAR_ARCHIVE };
cvar_t mp_decals = { const_cast<char*>("mp_decals"), const_cast<char*>("300"), FCVAR_ARCHIVE };

cvar_t gl_dither = { const_cast<char*>("gl_dither"), const_cast<char*>("1"), FCVAR_ARCHIVE };

cvar_t r_traceglow = { const_cast<char*>("r_traceglow"), const_cast<char*>("0") };
cvar_t gl_keeptjunctions = { const_cast<char*>("gl_keeptjunctions"), const_cast<char*>("1"), FCVAR_ARCHIVE };

cvar_t gl_lightholes = { const_cast<char*>("gl_lightholes"), const_cast<char*>("1"), FCVAR_ARCHIVE };

cvar_t r_bmodelinterp = { const_cast<char*>("r_bmodelinterp"), const_cast<char*>("1") };

cvar_t d_spriteskip = { const_cast<char*>("d_spriteskip"), const_cast<char*>("0") };
cvar_t r_mmx = { const_cast<char*>("r_mmx"), const_cast<char*>("0") };

cvar_t	r_drawentities = { const_cast<char*>("r_drawentities"), const_cast<char*>("1") }; // GL_RMISC.C:31
cvar_t	r_drawviewmodel = { const_cast<char*>("r_drawviewmodel"), const_cast<char*>("1") }; // GL_RMISC.C:32
cvar_t	r_speeds = { const_cast<char*>("r_speeds"), const_cast<char*>("0") }; // GL_RMISC.C:33
cvar_t	r_fullbright = { const_cast<char*>("r_fullbright"), const_cast<char*>("0") }; // GL_RMISC.C:34
cvar_t	r_lightmap = { const_cast<char*>("r_lightmap"), const_cast<char*>("0") }; // GL_RMISC.C:38
cvar_t	r_shadows = { const_cast<char*>("r_shadows"), const_cast<char*>("0") }; // GL_RMISC.C:39
cvar_t	r_mirroralpha = { const_cast<char*>("r_mirroralpha"), const_cast<char*>("1") }; // GL_RMISC.C:40
cvar_t	r_wateralpha = { const_cast<char*>("r_wateralpha"), const_cast<char*>("1") }; // GL_RMISC.C:41
cvar_t	r_dynamic = { const_cast<char*>("r_dynamic"), const_cast<char*>("1") }; // GL_RMISC.C:42
cvar_t	r_novis = { const_cast<char*>("r_novis"), const_cast<char*>("0") }; // GL_RMISC.C:43

cvar_t	gl_clear = { const_cast<char*>("gl_clear"), const_cast<char*>("0") }; // GL_RMISC.C:50
cvar_t	gl_cull = { const_cast<char*>("gl_cull"), const_cast<char*>("1") }; // GL_RMISC.C:51

cvar_t	gl_flashblend = { const_cast<char*>("gl_flashblend"), const_cast<char*>("0") }; // GL_RMISC.C:53

cvar_t	gl_polyoffset = { const_cast<char*>("gl_polyoffset"), const_cast<char*>("4"), FCVAR_ARCHIVE }; // GL_RMISC.C:58

cvar_t	gl_alphamin = { const_cast<char*>("gl_alphamin"),  const_cast<char*>("0.25") }; // GL_RMISC.C:61

cvar_t	gl_overbright = { const_cast<char*>("gl_overbright"), const_cast<char*>("1"), FCVAR_ARCHIVE }; // GL_RMISC.C:64
cvar_t	gl_envmapsize = { const_cast<char*>("gl_envmapsize"), const_cast<char*>("256") }; // GL_RMISC.C:65
cvar_t	gl_flipmatrix = { const_cast<char*>("gl_flipmatrix"), const_cast<char*>("0"), FCVAR_ARCHIVE }; // GL_RMISC.C:66
cvar_t	gl_monolights = { const_cast<char*>("gl_monolights"), const_cast<char*>("0"), FCVAR_ARCHIVE }; // GL_RMISC.C:67

cvar_t	gl_fog = { const_cast<char*>("gl_fog"), const_cast<char*>("1"), FCVAR_ARCHIVE }; // GL_RMISC.C:67

cvar_t r_cullsequencebox = { const_cast<char*>("r_cullsequencebox"), const_cast<char*>("0") };

bool gl_texsort = true; // GL_RMISC.C:82

qboolean	filterMode; // GL_RMISC.C:85
float		filterColorRed = 1.0f; // GL_RMISC.C:86
float		filterColorGreen = 1.0f; // GL_RMISC.C:87
float		filterColorBlue = 1.0f; // GL_RMISC.C:88
float		filterBrightness = 1.0f; // GL_RMISC.C:89

extern cvar_t r_cachestudio;
extern cvar_t r_glowshellfreq;
extern cvar_t ati_npatch;
extern cvar_t gl_affinemodels;

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f( void )
{
	if (!cl.worldmodel)
	{
		Con_Printf(const_cast<char*>("No map loaded\n"));
		return;
	}

	if (sv_cheats.value == 0.0)
	{
		Con_Printf(const_cast<char*>("sv_cheats not enabled\n"));
		return;
	}

	qglDrawBuffer(GL_FRONT);
	qglFinish();

	float start = Sys_FloatTime();

	for (int i = 0; i < 128; i++)
	{
		r_refdef.viewangles[1] = i / 128.0 * 360.0;
		R_RenderView();
	}

	qglFinish();

	float end = Sys_FloatTime();

	Con_Printf(const_cast<char*>("%f seconds (%f fps)\n"), end - start, (double)(128.0 / (end - start)));
	qglDrawBuffer(GL_BACK);
	GL_EndRendering();
}

/*
====================
GL_Dump_f

Dump renderer info
====================
*/
void GL_Dump_f( void )
{
	if (gl_vendor)
		Con_Printf(const_cast<char*>("GL Vendor: %s\n"), gl_vendor);
	if (gl_renderer)
		Con_Printf(const_cast<char*>("GL Renderer: %s\n"), gl_renderer);
	if (gl_version)
		Con_Printf(const_cast<char*>("GL Version: %s\n"), gl_version);
	if (gl_extensions)
		Con_Printf(const_cast<char*>("GL Extensions: %s\n"), gl_extensions);
}

/*
====================
R_InitTextures
====================
*/
void R_InitTextures( void )
{
	int		x, y, m;
	byte* dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t*)Hunk_AllocName(sizeof(texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2, const_cast<char*>("notexture"));

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m = 0; m < 4; m++)
	{
		dest = (byte*)r_notexture_mip + r_notexture_mip->offsets[m];

		for (y = 0; y < (16 >> m); y++)
		{
			for (x = 0; x < (16 >> m); x++)
			{
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xFF;
			}
		}
	}
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f( void )
{
	char base[64];
	char name[1024];
	vrect_t saveVrect;

	if (!cl.worldmodel)
	{
		Con_Printf(const_cast<char*>("No map loaded\n"));
		return;
	}

	if (!gl_envmapsize.value)
	{
		Con_Printf(const_cast<char*>("Invalid gl_envmapsize value\n"));
		return;
	}

	model_t* in = NULL;

	in = cl_entities->model;
	if (cl.num_entities && in)
		COM_FileBase(in->name, base);
	else
		Q_strcpy(base, "Env");

	qglDrawBuffer(GL_FRONT);
	qglReadBuffer(GL_FRONT);
	envmap = true;

	Q_memcpy(&saveVrect, &r_refdef, sizeof(saveVrect));

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = gl_envmapsize.value;
	r_refdef.vrect.height = gl_envmapsize.value;

	VectorClear(r_refdef.viewangles);

	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	snprintf(name, sizeof(name), "%srt.bmp", base);
	VID_TakeSnapshotRect(name, 0, 0, (int)gl_envmapsize.value, (int)gl_envmapsize.value);

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	snprintf(name, sizeof(name), "%sbk.bmp", base);
	VID_TakeSnapshotRect(name, 0, 0, (int)gl_envmapsize.value, (int)gl_envmapsize.value);

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	snprintf(name, sizeof(name), "%slf.bmp", base);
	VID_TakeSnapshotRect(name, 0, 0, (int)gl_envmapsize.value, (int)gl_envmapsize.value);

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	snprintf(name, sizeof(name), "%sft.bmp", base);
	VID_TakeSnapshotRect(name, 0, 0, (int)gl_envmapsize.value, (int)gl_envmapsize.value);

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	snprintf(name, sizeof(name), "%sup.bmp", base);
	VID_TakeSnapshotRect(name, 0, 0, (int)gl_envmapsize.value, (int)gl_envmapsize.value);

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	snprintf(name, sizeof(name), "%sdn.bmp", base);
	VID_TakeSnapshotRect(name, 0, 0, (int)gl_envmapsize.value, (int)gl_envmapsize.value);

	Q_memcpy(&r_refdef, &saveVrect, sizeof(saveVrect));

	envmap = false;

	qglDrawBuffer(GL_BACK);
	qglReadBuffer(GL_BACK);

	GL_EndRendering();
}

byte dottexture[8][8] =
{
	{0,1,1,0,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{0,1,1,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};
void R_InitParticleTexture( void )
{
	int		x, y;
	byte	data[8][8][4];

	//
	// particle texture
	//
	particletexture = GL_GenTexture();
	GL_Bind(particletexture);

	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}

	int w, h, bpp;
	VideoMode_GetCurrentVideoMode(&w, &h, &bpp);

	if (bpp == 32)
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	else
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);
}

/*
====================
R_UploadEmptyTex
====================
*/
void R_UploadEmptyTex( void )
{
	byte pPal[768];
	Q_memset(pPal, 0, sizeof(pPal));

	pPal[765] = -1;
	pPal[766] = 0;
	pPal[767] = -1;

	r_notexture_mip->gl_texturenum = GL_LoadTexture(const_cast<char*>("**empty**"), GLT_SYSTEM,
		r_notexture_mip->width, r_notexture_mip->height, (byte*)&r_notexture_mip[1], true, TEX_TYPE_NONE, pPal);
}

/*
====================
R_Init

====================
*/
void R_Init( void )
{
	// is CMD_UNSAFE are FCMD_RESTRICTED_COMMAND??
	Cmd_AddCommandWithFlags(const_cast<char*>("timerefresh"), R_TimeRefresh_f, CMD_UNSAFE);
	Cmd_AddCommand( const_cast<char*>("envmap"), R_Envmap_f);
	Cmd_AddCommand( const_cast<char*>("pointfile"), R_ReadPointFile_f);
	Cmd_AddCommand( const_cast<char*>("gl_dump"), GL_Dump_f);

	Cvar_RegisterVariable(&ati_npatch);
	Cvar_RegisterVariable(&gl_wireframe);
	Cvar_RegisterVariable(&r_cachestudio);
	Cvar_RegisterVariable(&r_cullsequencebox);
	Cvar_RegisterVariable(&r_bmodelinterp);
	Cvar_RegisterVariable(&r_norefresh);
	Cvar_RegisterVariable(&r_lightmap);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_decals);
	Cvar_RegisterVariable(&sp_decals);
	Cvar_RegisterVariable(&mp_decals);

	Cvar_SetValue(const_cast<char*>("r_decals"), 4096.0);

	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_mirroralpha);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&d_spriteskip);
	Cvar_RegisterVariable(&r_wadtextures);
	Cvar_RegisterVariable(&r_mmx);
	Cvar_RegisterVariable(&r_traceglow);
	Cvar_RegisterVariable(&r_glowshellfreq);
	Cvar_RegisterVariable(&gl_clear);
	Cvar_RegisterVariable(&gl_cull);
	Cvar_RegisterVariable(&gl_affinemodels);
	Cvar_RegisterVariable(&gl_dither);
	Cvar_RegisterVariable(&gl_spriteblend);
	Cvar_RegisterVariable(&gl_polyoffset);
	Cvar_RegisterVariable(&gl_lightholes);
	Cvar_RegisterVariable(&gl_keeptjunctions);
	Cvar_RegisterVariable(&gl_wateramp);
	Cvar_RegisterVariable(&gl_overbright);
	Cvar_RegisterVariable(&gl_zmax);
	Cvar_RegisterVariable(&gl_alphamin);
	Cvar_RegisterVariable(&gl_flipmatrix);
	Cvar_RegisterVariable(&gl_monolights);
	Cvar_RegisterVariable(&gl_fog);

	if (gl_mtexable)
	{
	    gl_texsort = false;
	    Cvar_SetValue(const_cast<char*>("gl_overbright"), 0.0);
	}

	R_InitParticles();
	R_InitParticleTexture();
	R_UploadEmptyTex();

	for (int i = 0; i < 16; i++)
	{
		playertextures[i] = GL_GenTexture();
	}
}

/*
============
R_CheckVariables
============
*/
void R_CheckVariables( void )
{
	static float oldFilterMode;
	static float oldFilterColorRed;
	static float oldFilterColorGreen;
	static float oldFilterColorBlue;
	static float oldFilterBrightness;

	if (filterMode != oldFilterMode
		|| filterColorRed != oldFilterColorRed
		|| filterColorGreen != oldFilterColorGreen
		|| filterColorBlue != oldFilterColorBlue
		|| filterBrightness != oldFilterBrightness)
	{
		oldFilterMode = filterMode;
		oldFilterColorRed = filterColorRed;
		oldFilterColorGreen = filterColorGreen;
		oldFilterColorBlue = filterColorBlue;
		oldFilterBrightness = filterBrightness;
		GL_LoadFilterTexture(filterColorRed, filterColorGreen, filterColorBlue, filterBrightness);
	}
}

/*
==================
D_FlushCaches
==================
*/
void D_FlushCaches( void )
{
	//GL_BuildLightmaps();
}

/*
===============
R_NewMap
===============
*/
void R_NewMap( void )
{
	int		i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	Q_memset(&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles();

	R_DecalInit();
	V_InitLevel();
	GL_BuildLightmaps();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;

	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;

		if (!Q_strncmp(cl.worldmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;

		// Would you look at that, GoldSrc has same Quake mirrors code
		//  and it's not only here -Enko
		if (!Q_strncmp(cl.worldmodel->textures[i]->name, "window02_1", 10))
			mirrortexturenum = i;

		cl.worldmodel->textures[i]->texturechain = NULL;
	}

	R_LoadSkys();
	cl_entities->curstate.scale = gl_wateramp.value;
	GL_UnloadTextures();
}

