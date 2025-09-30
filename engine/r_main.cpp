#include "quakedef.h"
#include "r_local.h"
#include "render.h"
#include "client.h"
#include "screen.h"
#include "view.h"
#include "model.h"
#include "color.h"
#include "Random.h"
#include "d_local.h"
#include "server.h"
#include "chase.h"
#include "cl_main.h"
#include "r_trans.h"
#include "r_studio.h"
#include "r_studioint.h"
#include "cmodel.h"
#include "cl_ents.h"
#include "bitmap_win.h"

int currenttexture = -1;	// to avoid unnecessary texture sets

int cnttextures[2] = { -1, -1 };     // cached

//vec3_t r_origin = { 0, 0, 0 };

//refdef_t r_refdef = {};
EXTERN_C
{
void* colormap;
vec3_t		viewlightvec;
//alight_t	r_viewlighting = { 128, 192, viewlightvec };
float		r_time1;
int			r_numallocatededges;
qboolean	r_drawpolys;
qboolean	r_drawculledpolys;
qboolean	r_worldpolysbacktofront;
qboolean	r_recursiveaffinetriangles = true;
int			r_pixbytes = 1;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp, r_dowarpold, r_viewchanged;

int			numbtofpolys;
btofpoly_t* pbtofpolys;
mvertex_t* r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;

byte* r_warpbuffer;

byte* r_stack_start;

qboolean	r_fov_greater_than_90;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//
// screen size info
//
refdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

//int		screenwidth;

float	pixelAspect;
float	screenAspect;
float	verticalFieldOfView;
float	xOrigin, yOrigin;

mplane_t	screenedge[4];

//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

#define		VIEWMODNAME_LENGTH	256
char		viewmodname[VIEWMODNAME_LENGTH + 1];
int			modcount;

int* pfrustum_indexes[4];
int			r_frustum_indexes[4 * 6];

int		reinit_surfcache = 1;	// if 1, surface cache is currently empty and
								// must be reinitialized for current cache size

mleaf_t* r_viewleaf, * r_oldviewleaf;

texture_t* r_notexture_mip;

float		r_aliastransition, r_resfudge;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

// color 64k lookup tables
unsigned short red_64klut[0xffff + 1], green_64klut[0xffff + 1], blue_64klut[0xffff + 1];
// gamma table
unsigned char r_lut[0xffff + 1];

word* r_palette;
};
//extern "C" void Sys_LowFPPrecision(void);
//extern "C" void Sys_HighFPPrecision(void);

cvar_t	r_draworder = { const_cast<char*>("r_draworder"), const_cast<char*>("0") };
cvar_t	r_speeds = { const_cast<char*>("r_speeds"), const_cast<char*>("0") };
cvar_t	r_timegraph = { const_cast<char*>("r_timegraph"), const_cast<char*>("0") };
cvar_t	r_graphheight = { const_cast<char*>("r_graphheight"), const_cast<char*>("10") };
cvar_t	r_waterwarp = { const_cast<char*>("r_waterwarp"), const_cast<char*>("1") };
cvar_t	r_fullbright = { const_cast<char*>("r_fullbright"), const_cast<char*>("0") };
cvar_t	r_drawentities = { const_cast<char*>("r_drawentities"), const_cast<char*>("1") };
cvar_t	r_drawviewmodel = { const_cast<char*>("r_drawviewmodel"), const_cast<char*>("1") };
cvar_t	r_aliasstats = { const_cast<char*>("r_polymodelstats"), const_cast<char*>("0") };
cvar_t	r_dspeeds = { const_cast<char*>("r_dspeeds"), const_cast<char*>("0") };
cvar_t	r_drawflat = { const_cast<char*>("r_drawflat"), const_cast<char*>("0") };
cvar_t r_ambient_r = { const_cast<char*>("r_ambient_r"), const_cast<char*>("0"), FCVAR_SPONLY };
cvar_t r_ambient_g = { const_cast<char*>("r_ambient_g"), const_cast<char*>("0"), FCVAR_SPONLY };
cvar_t r_ambient_b = { const_cast<char*>("r_ambient_b"), const_cast<char*>("0"), FCVAR_SPONLY };
cvar_t	r_reportsurfout = { const_cast<char*>("r_reportsurfout"), const_cast<char*>("0") };
cvar_t	r_maxsurfs = { const_cast<char*>("r_maxsurfs"), const_cast<char*>("0") };
cvar_t	r_numsurfs = { const_cast<char*>("r_numsurfs"), const_cast<char*>("0") };
cvar_t	r_reportedgeout = { const_cast<char*>("r_reportedgeout"), const_cast<char*>("0") };
cvar_t	r_maxedges = { const_cast<char*>("r_maxedges"), const_cast<char*>("0") };
cvar_t	r_numedges = { const_cast<char*>("r_numedges"), const_cast<char*>("0") };
cvar_t	r_aliastransbase = { const_cast<char*>("r_aliastransbase"), const_cast<char*>("200") };
cvar_t	r_aliastransadj = { const_cast<char*>("r_aliastransadj"), const_cast<char*>("100") };

cvar_t r_luminance = { const_cast<char*>("r_luminance"), const_cast<char*>("0") };
cvar_t r_mmx = { const_cast<char*>("r_mmx"), const_cast<char*>("0") };
cvar_t r_cullsequencebox = { const_cast<char*>("r_cullsequencebox"), const_cast<char*>("0") };
cvar_t r_traceglow = { const_cast<char*>("r_traceglow"), const_cast<char*>("0") };
cvar_t r_decals = { const_cast<char*>("r_decals"), const_cast<char*>("4096") };
cvar_t sp_decals = { const_cast<char*>("sp_decals"), const_cast<char*>("4096") };
cvar_t mp_decals = { const_cast<char*>("mp_decals"), const_cast<char*>("300") };
cvar_t r_lightmap = { const_cast<char*>("r_lightmap"), const_cast<char*>("-1") };
cvar_t r_lightstyle = { const_cast<char*>("r_lightstyle"), const_cast<char*>("-1") };
cvar_t r_wadtextures = { const_cast<char*>("r_wadtextures"), const_cast<char*>("0") };
cvar_t r_glowshellfreq = { const_cast<char*>("r_glowshellfreq"), const_cast<char*>("2.3") };
#ifdef HL25
cvar_t r_prefertexturefiltering = { const_cast<char*>("r_prefertexturefiltering"), const_cast<char*>("1"), FCVAR_ARCHIVE };
cvar_t gl_widescreen_yfov = { const_cast<char*>("gl_widescreen_yfov"), const_cast<char*>("1"), FCVAR_ARCHIVE };
#endif

//extern cvar_t	scr_fov;

/*
==================
R_InitTextures
==================
*/
void R_InitTextures(void)
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

void R_SetVrect(vrect_t* pvrectin, vrect_t* pvrect, int lineadj)
{
	int		h;
	float	size;

	size = scr_viewsize.value > 100 ? 100 : scr_viewsize.value;
	if (cl.intermission)
	{
		size = 100;
		lineadj = 0;
	}
	size /= 100;

	h = pvrectin->height - lineadj;
	pvrect->width = pvrectin->width * size;
	if (pvrect->width < 96)
	{
		size = 96.0 / pvrectin->width;
		pvrect->width = 96;	// min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;
	if (pvrect->height > pvrectin->height - lineadj)
		pvrect->height = pvrectin->height - lineadj;

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width) / 2;
	pvrect->y = (h - pvrect->height) / 2;

	{
		if (lcd_x.value)
		{
			pvrect->y >>= 1;
			pvrect->height >>= 1;
		}
	}
}

void R_ViewChanged(vrect_t* pvrect, int lineadj, float aspect)
{
	int		i;
	float	res_scale;

	r_viewchanged = true;

	R_SetVrect(pvrect, &r_refdef.vrect, lineadj);

	r_refdef.horizontalFieldOfView = 2.0 * tan(scr_fov_value / 360 * M_PI);
	r_refdef.fvrectx = (float)r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float)r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x << 20) + (1 << 19) - 1;
	r_refdef.fvrecty = (float)r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float)r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright << 20) + (1 << 19) - 1;
	r_refdef.fvrectright = (float)r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float)r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float)r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float)r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
		r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
		r_refdef.aliasvrect.height;

	pixelAspect = aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;

	screenAspect = r_refdef.vrect.width * pixelAspect /
		r_refdef.vrect.height;
	// 320*200 1.0 pixelAspect = 1.6 screenAspect
	// 320*240 1.0 pixelAspect = 1.3333 screenAspect
	// proper 320*200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

#ifdef HL25
	if (gl_widescreen_yfov.value != 0 && screenAspect > 1.5)
	{
		r_refdef.horizontalFieldOfView = 4 * (atan(tan(verticalFieldOfView * (M_PI / 360.0f)) * (screenAspect * 0.75f)) * (180.0f / M_PI));
	}
#endif

	// values for perspective projection
	// if math were exact, the values would range from 0.5 to to range+0.5
	// hopefully they wll be in the 0.000001 to range+.999999 and truncate
	// the polygon rasterization will never render in the first row or column
	// but will definately render in the [range] row and column, so adjust the
	// buffer origin to get an exact edge to edge fill
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) +
		r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) +
		r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width - 6) / r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink * pixelAspect;

	// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin * r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;

	// right side clip
	screenedge[1].normal[0] =
		1.0 / ((1.0 - xOrigin) * r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;

	// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin * verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;

	// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0 - yOrigin) * verticalFieldOfView);
	screenedge[3].normal[2] = 1;
	screenedge[3].type = PLANE_ANYZ;

	for (i = 0; i < 4; i++)
		VectorNormalize(screenedge[i].normal);

	res_scale = sqrt((double)(r_refdef.vrect.width * r_refdef.vrect.height) /
		(320.0 * 152.0)) *
		(2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase.value * res_scale;
	r_resfudge = r_aliastransadj.value * res_scale;

	if (scr_fov_value <= 90.0)
		r_fov_greater_than_90 = false;
	else
		r_fov_greater_than_90 = true;

	D_ViewChanged();
}

void R_DrawInitLut()
{
	if (r_lut[0xFFFF]) return;

	for (int i = 0; i < 256; i++)
	{
		for (int j = 0; j < 256; j++)
		{
			int color = (i * j) / 192;
			if (color >= MAXBYTE) color = MAXBYTE;
			r_lut[256 * j + i] = color;

			if (is15bit)
			{
				red_64klut[256 * j + i] = PACKEDRGB555(color, 0, 0);
				green_64klut[256 * j + i] = PACKEDRGB555(0, color, 0);
				blue_64klut[256 * j + i] = PACKEDRGB555(0, 0, color);
			}
			else
			{
				red_64klut[256 * j + i] = PACKEDRGB565(color, 0, 0);
				green_64klut[256 * j + i] = PACKEDRGB565(0, color, 0);
				blue_64klut[256 * j + i] = PACKEDRGB565(0, 0, color);
			}
		}
	}
}

void R_Init()
{
	int		dummy;

	// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte*)&dummy;

	Cmd_AddCommandWithFlags(const_cast<char*>("timerefresh"), R_TimeRefresh_f, CMD_UNSAFE);
	Cmd_AddCommand(const_cast<char*>("pointfile"), R_ReadPointFile_f);

	Cvar_RegisterVariable(&r_luminance);
	Cvar_RegisterVariable(&r_ambient_r);
	Cvar_RegisterVariable(&r_ambient_b);
	Cvar_RegisterVariable(&r_ambient_g);
	Cvar_RegisterVariable(&r_aliastransbase);
	Cvar_RegisterVariable(&r_aliastransadj);
	Cvar_RegisterVariable(&r_mmx);
	Cvar_RegisterVariable(&r_cachestudio);
	Cvar_RegisterVariable(&r_cullsequencebox);
	Cvar_RegisterVariable(&r_traceglow);
	Cvar_RegisterVariable(&r_maxsurfs);
	Cvar_RegisterVariable(&r_numsurfs);
	Cvar_RegisterVariable(&r_maxedges);
	Cvar_RegisterVariable(&r_numedges);
	Cvar_RegisterVariable(&r_bmodelinterp);
	Cvar_RegisterVariable(&r_draworder);
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_decals);
	Cvar_RegisterVariable(&sp_decals);
	Cvar_RegisterVariable(&mp_decals);

	Cvar_SetValue(const_cast<char*>("r_decals"), MAX_DECALS);

	Cvar_RegisterVariable(&r_lightmap);
	Cvar_RegisterVariable(&r_lightstyle);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_timegraph);
	Cvar_RegisterVariable(&r_graphheight);
	Cvar_RegisterVariable(&r_drawflat);
	Cvar_RegisterVariable(&r_aliasstats);
	Cvar_RegisterVariable(&r_dspeeds);
	Cvar_RegisterVariable(&r_reportsurfout);
	Cvar_RegisterVariable(&r_reportedgeout);
	Cvar_RegisterVariable(&r_wadtextures);
	Cvar_RegisterVariable(&r_glowshellfreq);
#ifdef HL25
	Cvar_RegisterVariable(&gl_widescreen_yfov);
	Cvar_RegisterVariable(&r_prefertexturefiltering);
#endif

	Cvar_SetValue(const_cast<char*>("r_mmx"), (float)gHasMMXTechnology);
	Cvar_SetValue(const_cast<char*>("r_maxedges"), (float)NUMSTACKEDGES);
	Cvar_SetValue(const_cast<char*>("r_maxsurfs"), (float)NUMSTACKSURFACES);

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
		view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
		view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	R_InitParticles();

	D_Init();
	R_DrawInitLut();
}

void R_LoadOverviewSpriteModel(model_t* mod, byte* pdata, byte* packPalette, int XTiles, int yTiles)
{
	mspriteframe_t* pspriteframe;
	msprite_t* psprite;
	word* spr_palette;
	int                    i, numframes;

	numframes = XTiles * yTiles;

	// 2078 = ???
	const int palsize = 256 * 4 * sizeof(word);
	// assume that this model has no frames
	// 28 = offsetof(msprite_t, frames); 2 = colorsused
	const int headersize = offsetof(msprite_t, frames) + sizeof(word);
	psprite = (msprite_t*)Hunk_AllocName(sizeof(mspriteframedesc_t) * numframes + palsize + headersize, mod->name);
	mod->cache.data = psprite;

	psprite->maxwidth = OVERVIEW_FRAME_WIDTH;
	psprite->maxheight = OVERVIEW_FRAME_HEIGHT;
	psprite->beamlength = 0;

	// use all 256 colors, why?
	const word colorsused = 1 << 8;

	mod->synctype = ST_SYNC;

	mod->mins[0] = mod->mins[1] = mod->mins[2] = -64;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 64;

	psprite->type = SPR_VP_PARALLEL_ORIENTED;
	psprite->texFormat = SPR_ALPHTEST;
	psprite->numframes = numframes;
	// word - colorsused
	psprite->paloffset = sizeof(mspriteframedesc_t) * numframes + headersize;

	mod->cache.data = NULL;

	// set colorsused
	*(WORD*)((BYTE*)psprite + psprite->paloffset - sizeof(WORD)) = colorsused;
	spr_palette = (WORD*)((BYTE*)psprite + psprite->paloffset);

	// copy palette
	for (int i = 0; i < colorsused; i++)
	{
		spr_palette[i * 4] = packPalette[i * 3 + 0];
		spr_palette[i * 4 + 1] = packPalette[i * 3 + 1];
		spr_palette[i * 4 + 2] = packPalette[i * 3 + 2];
		spr_palette[i * 4 + 3] = 0;
	}

	mod->numframes = numframes;
	mod->flags = 0;

	for (int i = 0; i < numframes; i++)
	{
		psprite->frames[i].type = SPR_SINGLE;

		pspriteframe = (mspriteframe_t*)Hunk_AllocName((r_pixbytes << 14) + sizeof(mspriteframe_t), mod->name);
		Q_memset(pspriteframe, 0, (r_pixbytes << 14) + sizeof(mspriteframe_t));

		psprite->frames[i].frameptr = pspriteframe;

		pspriteframe->width = OVERVIEW_FRAME_WIDTH;
		pspriteframe->height = OVERVIEW_FRAME_HEIGHT;
		pspriteframe->up = 0.0;
		pspriteframe->down = -OVERVIEW_FRAME_HEIGHT;
		pspriteframe->left = 0.0;
		pspriteframe->right = OVERVIEW_FRAME_WIDTH;

		if (r_pixbytes != 1)
		{
			Sys_Error("R_LoadOverviewSpriteModel: driver set invalid r_pixbytes: %d\n", r_pixbytes);
		}

		Q_memcpy(pspriteframe->pixels, pdata, OVERVIEW_FRAME_WIDTH * OVERVIEW_FRAME_HEIGHT);

		pdata += OVERVIEW_FRAME_WIDTH * OVERVIEW_FRAME_HEIGHT;
	}

	mod->type = mod_sprite;
}

void RotatedBBox(vec_t *mins, vec_t *maxs, vec_t *angles, vec_t *tmins, vec_t *tmaxs)
{
	if (angles[0] == 0.0 && angles[1] == 0.0 && angles[2] == 0.0)
	{
		VectorCopy(mins, tmins);
		VectorCopy(maxs, tmaxs);
	}
	else
	{
		vec3_t modmins, modmaxs;

		modmins[0] = max(fabs(mins[0]), 0.0f);
		modmaxs[0] = max(fabs(maxs[0]), modmins[0]);

		modmins[1] = max(fabs(mins[1]), modmaxs[0]);
		modmaxs[1] = max(fabs(maxs[1]), modmins[1]);

		modmins[2] = max(fabs(mins[2]), modmaxs[1]);
		modmaxs[2] = max(fabs(maxs[2]), modmins[2]);

		tmins[0] = -modmaxs[2];
		tmaxs[0] = modmaxs[2];
		tmins[1] = -modmaxs[2];
		tmaxs[1] = modmaxs[2];
		tmins[2] = -modmaxs[2];
		tmaxs[2] = modmaxs[2];
	}
}

/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox(model_t *clmodel, float *minmaxs)
{
	int			i, *pindex, clipflags;
	vec3_t		acceptpt, rejectpt;
	double		d;

	clipflags = 0;

	if (currententity->angles[0] || currententity->angles[1]
		|| currententity->angles[2])
	{
		for (i = 0; i<4; i++)
		{
			d = DotProduct(currententity->origin, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1 << i);
		}
	}
	else
	{
		for (i = 0; i<4; i++)
		{
			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the sign bit
			// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct(rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3 + 0]];
			acceptpt[1] = minmaxs[pindex[3 + 1]];
			acceptpt[2] = minmaxs[pindex[3 + 2]];

			d = DotProduct(acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1 << i);
		}
	}

	return clipflags;
}

// JAY: Setup frustum
/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox(vec_t* mins, vec_t* maxs)
{
	int i;
	float d1, d2, d3;

	for (i = 0; i < 4; i++)
	{
		d1 = pfrustum_indexes[i][0] <= 2 ? mins[pfrustum_indexes[i][0]] : maxs[pfrustum_indexes[i][0] - 3];
		d2 = pfrustum_indexes[i][1] <= 2 ? mins[pfrustum_indexes[i][1]] : maxs[pfrustum_indexes[i][1] - 3];
		d3 = pfrustum_indexes[i][2] <= 2 ? mins[pfrustum_indexes[i][2]] : maxs[pfrustum_indexes[i][2] - 3];

		if ((d1 * view_clipplanes[i].normal[0] + d2 * view_clipplanes[i].normal[1] + d3 * view_clipplanes[i].normal[2])
			- view_clipplanes[i].dist <= 0.0f)
		{
			return TRUE;
		}
	}

	return FALSE;
}

void R_ForceCVars(qboolean multiplayer)
{
	if (!multiplayer || g_bIsDedicatedServer)
		return;

	if (r_lightmap.value != -1)
		Cvar_DirectSet(&r_lightmap, const_cast<char*>("-1"));
	if (r_draworder.value != 0)
		Cvar_DirectSet(&r_draworder, const_cast<char*>("0"));
	if (r_fullbright.value != 0)
		Cvar_DirectSet(&r_fullbright, const_cast<char*>("0"));
	if (r_ambient_r.value != 0)
		Cvar_DirectSet(&r_ambient_r, const_cast<char*>("0"));
	if (r_ambient_g.value != 0)
		Cvar_DirectSet(&r_ambient_g, const_cast<char*>("0"));
	if (r_ambient_b.value != 0)
		Cvar_DirectSet(&r_ambient_b, const_cast<char*>("0"));
	if (r_drawflat.value != 0)
		Cvar_DirectSet(&r_drawflat, const_cast<char*>("0"));
	if (r_aliasstats.value != 0)
		Cvar_DirectSet(&r_aliasstats, const_cast<char*>("0"));
	if (snd_show.value != 0)
		Cvar_DirectSet(&snd_show, const_cast<char*>("0"));
	if (chase_active.value != 0)
		Cvar_DirectSet(&chase_active, const_cast<char*>("0"));
	if (v_lambert.value != 1.5)
		Cvar_DirectSet(&v_lambert, const_cast<char*>("1.5"));
	if (r_luminance.value != 0)
		Cvar_DirectSet(&r_luminance, const_cast<char*>("0"));
	if (d_spriteskip.value != 0.0)
		Cvar_DirectSet(&d_spriteskip, const_cast<char*>("0"));
	if (sv_cheats.value == 0.0 && r_drawentities.value != 1.0)
		Cvar_DirectSet(&r_drawentities, const_cast<char*>("1.0"));
}

void R_NewMap(void)
{
	int		i;
	vrect_t vrect;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles();

	R_DecalInit();
	V_InitLevel();

	r_cnumsurfs = r_maxsurfs.value;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES)
	{
		surfaces = (surf_t*)Hunk_AllocName(r_cnumsurfs * sizeof(surf_t), const_cast<char*>("surfaces"));
		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = FALSE;
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
	}
	else
	{
		r_surfsonstack = TRUE;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = r_maxedges.value;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES)
	{
		auxedges = NULL;
	}
	else
	{
		auxedges = (edge_t*)Hunk_AllocName(r_numallocatededges * sizeof(edge_t),
			const_cast<char*>("edges"));
	}

	r_dowarpold = FALSE;
	r_viewchanged = FALSE;

#ifdef PASSAGES
	CreatePassages();
#endif

	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	R_ViewChanged(&vrect, sb_lines, vid.aspect);

	R_LoadSkys();
}

void AllowFog(int allowed)
{

}

void R_DrawBEntitiesOnList(void)
{
	int			i, j, k, clipflags;
	vec3_t		oldorigin, oldlightorg;
	model_t* clmodel;
	float		minmaxs[6];

	if (!r_drawentities.value)
		return;

	VectorCopy(modelorg, oldorigin);
	insubmodel = TRUE;
	r_dlightframecount = r_framecount;

	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity->curstate.rendermode != kRenderNormal)
		{
			AddTEntity(currententity);
			continue;
		}

		switch (currententity->model->type)
		{
		case mod_brush:

			clmodel = currententity->model;

			RotatedBBox(clmodel->mins, clmodel->maxs, currententity->angles, &minmaxs[0], &minmaxs[3]);

			// see if the bounding box lets us trivially reject, also sets
			// trivial accept status
			for (j = 0; j < 3; j++)
			{
				minmaxs[j] += currententity->origin[j];
				minmaxs[3 + j] += currententity->origin[j];
			}

			clipflags = R_BmodelCheckBBox(clmodel, minmaxs);

			if (clipflags != BMODEL_FULLY_CLIPPED)
			{
				VectorCopy(currententity->origin, r_entorigin);
				VectorSubtract(r_origin, r_entorigin, modelorg);
				// FIXME: is this needed?
				VectorCopy(modelorg, r_worldmodelorg);

				r_pcurrentvertbase = clmodel->vertexes;

				// FIXME: stop transforming twice
				R_RotateBmodel();

				// calculate dynamic lighting for bmodel if it's not an
				// instanced model
				if (clmodel->firstmodelsurface != 0)
				{
					for (k = 0; k < MAX_DLIGHTS; k++)
					{
						dlight_t* dl = &cl_dlights[k];

						if ((dl->die < cl.time) ||
							(!dl->radius))
						{
							continue;
						}

						VectorCopy(dl->origin, oldlightorg);
						VectorSubtract(dl->origin, currententity->origin, dl->origin);

						R_MarkLights(dl, 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode);

						VectorCopy(oldlightorg, dl->origin);
					}
				}

				// if the driver wants polygons, deliver those. Z-buffering is on
				// at this point, so no clipping to the world tree is needed, just
				// frustum clipping
				if (r_drawpolys | r_drawculledpolys)
				{
					R_ZDrawSubmodelPolys(clmodel);
				}
				else
				{
					r_pefragtopnode = NULL;

					for (j = 0; j < 3; j++)
					{
						r_emins[j] = minmaxs[j];
						r_emaxs[j] = minmaxs[3 + j];
					}

					R_SplitEntityOnNode2(cl.worldmodel->nodes);

					if (r_pefragtopnode)
					{
						currententity->topnode = r_pefragtopnode;

						if (r_pefragtopnode->contents >= 0)
						{
							// not a leaf; has to be clipped to the world BSP
							r_clipflags = clipflags;
							R_DrawSolidClippedSubmodelPolygons(clmodel);
						}
						else
						{
							// falls entirely in one leaf, so we just put all the
							// edges in the edge list and let 1/z sorting handle
							// drawing order
							R_DrawSubmodelPolygons(clmodel, clipflags);
						}

						currententity->topnode = NULL;
					}
				}

				// put back world rotation and frustum clipping		
				// FIXME: R_RotateBmodel should just work off base_vxx
				VectorCopy(base_vpn, vpn);
				VectorCopy(base_vup, vup);
				VectorCopy(base_vright, vright);
				VectorCopy(oldorigin, modelorg);
				R_TransformFrustum();
			}

			break;

		default:
			break;
		}
	}

	insubmodel = FALSE;
}

/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing(void)
{
	edge_t	ledges[NUMSTACKEDGES +
		((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
	surf_t	lsurfs[NUMSTACKSURFACES +
		((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];

	if (auxedges)
	{
		r_edges = auxedges;
	}
	else
	{
		r_edges = (edge_t*)
			(((uintptr_t)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack)
	{
		surfaces = (surf_t*)
			(((uintptr_t)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
	}

	R_BeginEdgeFrame();

	if (r_dspeeds.value)
	{
		rw_time1 = Sys_FloatTime();
	}

	if (!r_refdef.onlyClientDraws)
	{
		R_RenderWorld();
	}

	if (r_drawculledpolys)
		R_ScanEdges();

	// only the world can be drawn back to front with no z reads or compares, just
	// z writes, so have the driver turn z compares on now
	D_TurnZOn();

	if (r_dspeeds.value)
	{
		rw_time2 = Sys_FloatTime();
		db_time1 = rw_time2;
	}

	if (!r_refdef.onlyClientDraws)
	{
		R_DrawBEntitiesOnList();
	}

	if (r_dspeeds.value)
	{
		db_time2 = Sys_FloatTime();
		se_time1 = db_time2;
	}

	if (!r_dspeeds.value)
	{
		S_ExtraUpdate();	// don't let sound get messed up if going slow
	}

	if (!(r_drawpolys | r_drawculledpolys))
		R_ScanEdges();
}

void R_PreDrawViewModel(void)
{
	currententity = &cl.viewent;

	if (!r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value || !r_drawentities.value || cl.stats[STAT_HEALTH] <= 0 ||
		!currententity->model || r_fov_greater_than_90 || cl.viewentity > cl.maxclients || currententity->model->type != mod_studio)
		return;

	if (cl.weaponstarttime == 0.0)
		cl.weaponstarttime = cl.time;

	currententity->curstate.sequence = cl.weaponsequence;
	currententity->curstate.frame = 0.0;
	currententity->curstate.framerate = 1.0;
	currententity->curstate.animtime = cl.weaponstarttime;

	// Begin model drawing via r_studio routines
	pStudioAPI->StudioDrawModel(STUDIO_EVENTS);
}

void R_DrawEntitiesOnList(void)
{
	int		i, j;
	vec3_t	origin;

	if (!r_drawentities.value)
		return;

	VectorCopy(modelorg, origin);

	spritedraw = D_SpriteDrawSpans;

	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity->curstate.rendermode != kRenderNormal)
			continue;

		switch (currententity->model->type)
		{
		case mod_sprite:
			VectorCopy(currententity->origin, r_entorigin);
			VectorSubtract(r_origin, r_entorigin, modelorg);
			R_DrawSprite();
			break;
		case mod_studio:
			if (currententity->player)
				pStudioAPI->StudioDrawPlayer(STUDIO_RENDER | STUDIO_EVENTS, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1]);
			else if (currententity->curstate.movetype == MOVETYPE_FOLLOW)
			{
				for (j = 0; j < cl_numvisedicts; j++)
				{
					if (cl_visedicts[j]->index == currententity->curstate.aiment)
					{
						currententity = cl_visedicts[j];

						if (currententity->player)
							pStudioAPI->StudioDrawPlayer(0, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1]);
						else
							pStudioAPI->StudioDrawModel(0);

						currententity = cl_visedicts[i];
						pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
					}
				}
			}
			else
				pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
			break;
		default:
			break;
		}
	}

	VectorCopy(origin, modelorg);
}

void R_DrawViewModel(void)
{
	int			i, j;
	colorVec	lightvec;

	currententity = &cl.viewent;

	if (!r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value != 0.0 || r_drawentities.value == 0.0 ||
		cl.stats[STAT_HEALTH] <= 0 || !currententity->model || r_fov_greater_than_90 || cl.viewentity > cl.maxclients)
	{
		lightvec = R_LightPoint(currententity->origin);
		cl.light_level = (lightvec.r + lightvec.g + lightvec.b) / 3;

		for (i = 0; i < 4; i++)
		{
			VectorCopy(currententity->origin, cl_entities[cl.viewentity].attachment[i]);
		}
		return;
	}

	switch (currententity->model->type)
	{
	case mod_studio:
		if (cl.weaponstarttime == 0.0)
			cl.weaponstarttime = cl.time;

		currententity->curstate.sequence = cl.weaponsequence;
		currententity->curstate.frame = 0.0;
		currententity->curstate.framerate = 1.0;
		currententity->curstate.animtime = cl.weaponstarttime;

		currententity->curstate.colormap = cl.players[cl.playernum].topcolor | cl.players[cl.playernum].bottomcolor << 8;

		if (currententity->curstate.rendermode == kRenderTransAlpha)
			polysetdraw = D_PolysetDrawHole;

		pStudioAPI->StudioDrawModel(STUDIO_RENDER);
		break;
	}
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves(void)
{
	byte* vis;
	mnode_t* node;
	int		i;

	if (r_oldviewleaf == r_viewleaf)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			node = (mnode_t*)&cl.worldmodel->leafs[i + 1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView_ (void)
{
	byte	warpbuffer[WARP_WIDTH * WARP_HEIGHT * 2];

	r_warpbuffer = warpbuffer;

	if (r_timegraph.value || r_speeds.value || r_dspeeds.value)
		r_time1 = Sys_FloatTime ();

	R_SetupFrame ();

#ifdef PASSAGES
SetVisibilityByPassages ();
#else
	R_MarkLeaves ();	// done here so we know if we're in water
#endif

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	Sys_LowFPPrecision ();

	if (!cl_entities[0].model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");
		
	if (!r_dspeeds.value)
	{
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
	}
	
	R_EdgeDrawing ();

	if (!r_dspeeds.value)
	{
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
	}
	
	if (r_dspeeds.value)
	{
		se_time2 = Sys_FloatTime ();
		de_time1 = se_time2;
	}

	if (!r_refdef.onlyClientDraws)
	{
		R_PreDrawViewModel();
		R_DrawEntitiesOnList();

		if (!r_dspeeds.value)
		{
			S_ExtraUpdate();	// don't let sound get messed up if going slow
		}
	}

	ClientDLL_DrawNormalTriangles();
	R_DrawTEntitiesOnList(r_refdef.onlyClientDraws);

	if (!r_dspeeds.value)
	{
		S_ExtraUpdate();	// don't let sound get messed up if going slow
	}

	if (r_dspeeds.value)
	{
		de_time2 = Sys_FloatTime ();
		dv_time1 = de_time2;
	}

	if (!r_refdef.onlyClientDraws)
		R_DrawViewModel ();

	if (!r_dspeeds.value)
	{
		S_ExtraUpdate();	// don't let sound get messed up if going slow
	}

	if (r_dspeeds.value)
	{
		dv_time2 = Sys_FloatTime ();
		dp_time1 = Sys_FloatTime ();
	}

	if (!r_refdef.onlyClientDraws)
	{
		R_DrawParticles();

		if (r_dspeeds.value)
			dp_time2 = Sys_FloatTime ();

		if (r_dowarp)
			D_WarpScreen ();
	}

	if (r_timegraph.value)
		R_TimeGraph ();

	if (r_aliasstats.value)
		R_PrintAliasStats ();
		
	if (r_speeds.value)
		R_PrintTimes ();

	if (r_dspeeds.value)
		R_PrintDSpeeds ();

	if (r_luminance.value)
		R_ScreenLuminance ();

	if (r_reportsurfout.value && r_outofsurfaces)
		Con_Printf (const_cast<char*>("Short %d surfaces\n"), r_outofsurfaces);

	if (r_reportedgeout.value && r_outofedges)
		Con_Printf (const_cast<char*>("Short roughly %d edges\n"), r_outofedges * 2 / 3);

// back to high floating-point precision
	Sys_HighFPPrecision ();

	currententity = NULL;
	r_warpbuffer = NULL;
}

void R_RenderView (void)
{
	int		dummy;
	int		delta;
	
	delta = (byte *)&dummy - r_stack_start;
	if (delta < -10000 || delta > 10000)
		Sys_Error ("R_RenderView: called without enough stack");

	if ( Hunk_LowMark() & 3 )
		Sys_Error ("Hunk is missaligned");

	if ( (uintptr_t)(&dummy) & 3 )
		Sys_Error ("Stack is missaligned");

	if ( (uintptr_t)(&r_warpbuffer) & 3 )
		Sys_Error ("Globals are missaligned");

	R_RenderView_ ();
}

void R_SetStackBase(void)
{
	int		dummy;

	// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte*)&dummy;
}

model_t* R_LoadMapSprite(const char* szFilename)
{
	int					palCount;
	int					i, j, k, size;
	int					width, height;
	char				filename[MAX_PATH];
	FileHandle_t file;

	unsigned char* buffer, * buffer2;
	unsigned char* packPalette = NULL;

	model_t* mod;

	buffer = NULL;

	// 128 x 128 - frame size
	// 48 - max frames
	buffer2 = (unsigned char*)Mem_Malloc(OVERVIEW_FRAME_WIDTH * OVERVIEW_FRAME_HEIGHT * OVERVIEW_SPRITE_MAX_FRAMES);

	Q_strncpy(filename, szFilename, sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = 0;

	Q_strlwr(filename);

	if (Q_strcmp("bmp", &filename[Q_strlen(filename) - 3]))
	{
		Con_Printf(const_cast<char*>("Software Mode only accepts 8 Bit BMP files as map images\n"), filename);
		Mem_Free(buffer2);
		return NULL;
	}

	file = FS_Open(filename, "rb");

	if (!file)
	{
		Con_Printf(const_cast<char*>("Couldn't open %s\n"), filename);
		Mem_Free(buffer2);
		return NULL;
	}

	mod = Mod_FindName(FALSE, filename);
	mod->needload = NL_PRESENT;

	LoadBMP8(file, &packPalette, &palCount, &buffer, &width, &height);

	size = height * width;

	if (size == 0 || width % OVERVIEW_FRAME_WIDTH || height % OVERVIEW_FRAME_HEIGHT)
	{
		Con_Printf(const_cast<char*>("Wrong image dimesions/color depth in %s\n"), filename);
		Mem_Free(packPalette);
		Mem_Free(buffer);
		Mem_Free(buffer2);
		return NULL;
	}

	for (i = 0; i < 768; i += 3)
	{
		// зелёный цвет - прозрачный на растровых видах карт
		if (packPalette[i + 0] == 0 &&
			packPalette[i + 1] == 0xFF &&
			packPalette[i + 2] == 0)
		{
			packPalette[i + 0] = 0;
			packPalette[i + 1] = 0;
			packPalette[i + 2] = 0;
		}
	}

	unsigned char* buf;
	unsigned char* buf2;

	buf = buffer2;
	for (i = 0; i < height; i += OVERVIEW_FRAME_HEIGHT)
	{
		for (j = 0; j < width; j += OVERVIEW_FRAME_WIDTH)
		{
			buf2 = &buffer[j + width * i];
			for (k = OVERVIEW_FRAME_HEIGHT; k != 0; k--)
			{
				memcpy(buf, buf2, OVERVIEW_FRAME_WIDTH);
				buf += OVERVIEW_FRAME_WIDTH;
				buf2 += width;
			}
		}
	}

	R_LoadOverviewSpriteModel(mod, buffer2, packPalette, width / OVERVIEW_FRAME_WIDTH, height / OVERVIEW_FRAME_HEIGHT);

	Mem_Free(packPalette);
	Mem_Free(buffer);
	Mem_Free(buffer2);

	return mod;
}
