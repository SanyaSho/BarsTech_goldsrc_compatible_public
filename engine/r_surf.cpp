// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_local.h"
#include "client.h"
#include "cl_main.h"
#include "render.h"
#include "view.h"
#include "vid.h"
#include "decals.h"
#include "color.h"
#include "host.h"
#include "d_local.h"
#include <mmintrin.h>

#define MAX_TILERAND		20
#define MAX_DECALSURFS		500
#define DECAL_DISTANCE		4
#define MAX_OVERLAP_DECALS	4

drawsurf_t	r_drawsurf;

EXTERN_C
{
int				lightleft;
int				lightdelta, lightdeltastep;
int				lightright, lightleftstep, lightrightstep;

int				sourcesstep, blocksize, sourcetstep;
int				blockdivshift;
unsigned		blockdivmask;
void			*prowdestbase;
unsigned char	*pbasesource;
int				surfrowbytes;	// used by ASM files
colorVec		*r_lightptr;
int				r_stepback;
int				r_lightwidth;
int				r_numhblocks, r_numvblocks;
//unsigned char* r_source;
unsigned char *r_sourcemax;
};
int				sourcevstep;
texture_t*		r_basetexture[18];
int				r_offset;
int				r_deltav; // номер текстуры из цикла, которая будет загружена в первую очередь
word			my_cw = 0x37F;
word			fpu_cw;
double 			lut_8byte_aligner;
word 			lut_realigner1[256];
word 			lut_realigner2[256];
__m64			MMXBUF0, MMXBUF1;
decal_t			gDecalPool[MAX_DECALS];
int				gDecalCount;
int				gDecalFlags;
vec3_t			gDecalPos;
int				gDecalSize;
model_t			*gDecalModel;
texture_t		*gDecalTexture;
int				gDecalIndex;
int				gDecalEntity;
float			gDecalAppliedScale;

void 			R_DrawSurfaceBlock16Holes(void);
void			R_DrawSurfaceBlock16Filtered(void);
word* 			R_DecalFilteredSurface(byte* psource, word* prowdest);
void 			R_DrawSurfaceBlock16Fullbright1(void);
word* 			R_DecalFullbrightSurface(byte* psource, word* prowdest);
void 			R_DrawSurfaceBlock16Fullbright2(void);
void 			R_DrawSurfaceBlock16Fullbright3(void);
void 			R_DrawSurfaceBlock16Fullbright4(void);
void 			R_DrawSurfaceBlock16Fullbright5(void);
void 			R_DrawSurfaceBlock16MMX(void);
void 			R_DrawSurfaceBlock16(void);
word* 			R_DecalLightSurface(byte* psource, word* prowdest);

void 			R_DecalPickRow(byte* data, PackedColorVec* pal, byte* row);
void 			R_DecalResolution(decal_t* pdecals, int* scaleinfo, int miplevel);
void 			R_DecalSurface(decal_t* pdecal, byte* row, int width, int height);
void 			R_DecalSurfaceScaled(decal_t* current, byte* row, int width, int height);

colorVec 		blocklights[18 * 18];

const int 		gBits[8] = { 0, 0, 1, 1, 2, 2, 2, 3 };
const double	r_blockdivmul[] = { 1.0, 0.5, 0.25, 0.125, 0.0625 };
const int 		rtable[MAX_TILERAND][MAX_TILERAND] =
{
	1630, 2723, 2341, 227, 534, 916, 2865, 356, 1445, 2401, 780, 2919, 3136, 2817, 770, 496, 338, 2106, 2607, 2420,
	951, 2377, 3087, 2028, 595, 444, 3128, 1635, 2979, 3341, 1707, 1580, 2947, 299, 88, 433, 2364, 73, 774, 1361,
	418, 1919, 430, 3347, 2211, 1829, 1942, 118, 2595, 2530, 1669, 2043, 3326, 637, 2126, 1487, 2005, 1086, 13, 1734,
	2407, 1413, 3095, 2829, 2314, 1470, 536, 207, 604, 2233, 1398, 679, 1950, 1951, 603, 2686, 297, 2195, 9, 728,
	318, 2777, 2214, 2611, 3282, 1256, 1422, 3031, 3225, 404, 955, 641, 751, 2885, 1468, 2589, 2375, 522, 587, 2365,
	3257, 1240, 1531, 2298, 1876, 2893, 2132, 841, 260, 254, 3132, 2026, 929, 2756, 2739, 68, 3206, 2833, 1647, 2421,
	1494, 1831, 77, 2103, 522, 14, 3145, 39, 2828, 736, 473, 1874, 1225, 234, 775, 1842, 1396, 669, 2693, 2566,
	2225, 1424, 2026, 2315, 1669, 732, 1419, 2645, 2670, 1707, 3175, 1457, 154, 890, 237, 2528, 1942, 3124, 815, 3268,
	1730, 1330, 817, 1521, 590, 1553, 1987, 2254, 1385, 3176, 1134, 2284, 227, 2775, 1372, 367, 1569, 437, 2100, 3233,
	2373, 1126, 738, 2245, 316, 963, 2273, 860, 1459, 1242, 2176, 1097, 1080, 1208, 2491, 2052, 2610, 1964, 151, 1856,
	704, 2625, 275, 1074, 407, 2265, 2551, 330, 312, 811, 375, 246, 83, 1665, 1314, 808, 1811, 766, 1053, 1360,
	456, 2259, 1010, 3007, 3341, 2599, 3153, 2824, 1931, 2255, 468, 2647, 1674, 1027, 2662, 2393, 1558, 497, 2539, 2057,
	2029, 2030, 139, 233, 1600, 224, 1665, 2150, 2233, 702, 2921, 2574, 327, 2393, 156, 1266, 2614, 2393, 722, 2325,
	651, 3022, 1815, 1783, 1796, 3111, 842, 2068, 2717, 2888, 2587, 1272, 3041, 1050, 1565, 1783, 105, 2659, 773, 2396,
	2291, 2313, 814, 2385, 1011, 1730, 2685, 456, 2234, 117, 2423, 1648, 2175, 3245, 423, 933, 1210, 1221, 2182, 2446,
	2020, 1646, 1698, 437, 3240, 1092, 2303, 1054, 1377, 59, 285, 977, 874, 2432, 1089, 515, 2388, 2667, 2465, 467,
	856, 579, 776, 2459, 3097, 1458, 301, 1127, 1268, 3016, 1262, 2904, 2735, 978, 2111, 2782, 2594, 1633, 2724, 1305,
	2559, 3056, 1993, 1852, 708, 721, 1280, 2830, 1701, 2537, 678, 1481, 880, 3310, 2316, 2549, 732, 1452, 3106, 2287,
	3106, 729, 1324, 2862, 2996, 2823, 886, 867, 3074, 2968, 1399, 2778, 1744, 1767, 103, 705, 22, 341, 2328, 260,
	3336, 467, 2052, 1564, 2234, 2026, 1583, 1928, 9, 3084, 2093, 256, 1262, 1483, 974, 3311, 3295, 1658, 2164, 249
};

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights(void)
{
	msurface_t* surf;
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t* tex;

	surf = r_drawsurf.surf;
	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue;		// not lit by this light

		VectorSubtract(cl_dlights[lnum].origin, currententity->origin, impact);

		rad = cl_dlights[lnum].radius;
		dist = DotProduct(cl_dlights[lnum].origin, surf->plane->normal) -
			surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (t = 0; t < tmax; t++)
		{
			td = local[1] - t * 16;
			if (td < 0)
				td = -td;
			for (s = 0; s < smax; s++)
			{
				sd = local[0] - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);
				if (dist < minlight)
				{
					blocklights[t * smax + s].r += ((int)((rad - dist) * 256) * cl_dlights[lnum].color.r) >> 8;
					blocklights[t * smax + s].g += ((int)((rad - dist) * 256) * cl_dlights[lnum].color.g) >> 8;
					blocklights[t * smax + s].b += ((int)((rad - dist) * 256) * cl_dlights[lnum].color.b) >> 8;
				}
			}
		}
	}

}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap(void)
{
	int			smax, tmax;
	int			t;
	int			i, size;
	color24* lightmap;
	unsigned	scale;
	int			maps;
	msurface_t* surf;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

	if (filterMode)
	{
		for (i = 0; i < size; i++)
		{
			blocklights[i].r = (filterColorRed * 255.0f) * (filterBrightness * 255.0f);
			blocklights[i].g = (filterColorGreen * 255.0f) * (filterBrightness * 255.0f);
			blocklights[i].b = (filterColorBlue * 255.0f) * (filterBrightness * 255.0f);
		}

		return;
	}

	if (r_fullbright.value == 1 || !cl.worldmodel->lightdata)
	{
		for (i = 0; i < size; i++)
			blocklights[i].r = blocklights[i].g = blocklights[i].b = 0xFFFF;

		return;
	}

	// clear to ambient
	for (i = 0; i < size; i++)
	{
		blocklights[i].r = r_refdef.ambientlight.r << 8;
		blocklights[i].g = r_refdef.ambientlight.g << 8;
		blocklights[i].b = r_refdef.ambientlight.b << 8;
	}

	// add all the lightmaps
	if (lightmap)
	{
		if (r_lightmap.value == -1 && r_lightstyle.value == -1)
		{
			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
				 maps++)
			{
				scale = r_drawsurf.lightadj[maps];	// 8.8 fraction		
				for (i=0 ; i<size ; i++)
				{
					blocklights[i].r += lightmap[i].r * scale;
					blocklights[i].g += lightmap[i].g * scale;
					blocklights[i].b += lightmap[i].b * scale;

				}
				lightmap += size;	// skip to next lightmap
			}
		}
		else
		{
			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
				 maps++)
			{
				if (maps != r_lightmap.value && surf->styles[maps] != r_lightstyle.value)
					continue;

				for (i=0 ; i<size ; i++)
				{
					blocklights[i].r += (lightmap[i].r + lightmap[i].r << 5) << 3;
					blocklights[i].g += (lightmap[i].g + lightmap[i].g << 5) << 3;
					blocklights[i].b += (lightmap[i].b + lightmap[i].b << 5) << 3;
				}
				lightmap += size;	// skip to next lightmap
			}
		}
	}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights();

	// bound, invert, and shift
	for (i=0 ; i<size ; i++)
	{
		if (blocklights[i].r < 255 * 256)
			blocklights[i].r = (lightgammatable[blocklights[i].r >> VID_CBITS] & ((255 * 256) >> VID_CBITS)) << VID_CBITS;
		else
			blocklights[i].r = 255 * 256;

		if (blocklights[i].g < 255 * 256)
			blocklights[i].g = (lightgammatable[blocklights[i].g >> VID_CBITS] & ((255 * 256) >> VID_CBITS)) << VID_CBITS;
		else
			blocklights[i].g = 255 * 256;

		if (blocklights[i].b < 255 * 256)
			blocklights[i].b = (lightgammatable[blocklights[i].b >> VID_CBITS] & ((255 * 256) >> VID_CBITS)) << VID_CBITS;
		else
			blocklights[i].b = 255 * 256;
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t* R_TextureAnimation(texture_t* base)
{
	int		relative;
	int		count;

	if (currententity->curstate.frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total || base->name[0] == '-')
		return base;

	relative = (int)(cl.time * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error("R_TextureAnimation: infinite cycle");
	}

	return base;
}

void R_SetupTextureIndex(int u)
{
	int tu, tv, relative, count, v;
	texture_t* tm = r_drawsurf.texture;
	texture_t* tm2;
	if (r_drawsurf.texture->name[0] == '-' && r_drawsurf.texture->anim_total != 0)
	{
		tu = ((r_drawsurf.texture->width << 16) + r_drawsurf.surf->texturemins[0] + u * 16) / r_drawsurf.texture->width;

		for (v = 0; v < r_numvblocks; v++)
		{
			tv = ((r_drawsurf.texture->height << 16) + r_drawsurf.surf->texturemins[1] + v * 16) / r_drawsurf.texture->height;

			relative = rtable[tu % MAX_TILERAND][tv % MAX_TILERAND] % r_drawsurf.texture->anim_total;

			tm2 = r_drawsurf.texture;
			count = 0;
			while (tm2->anim_min > relative || tm2->anim_max <= relative)
			{
				tm2 = tm2->anim_next;
				if (!tm2)
					Sys_Error("R_TextureAnimation: broken cycle");
				if (++count > 100)
					Sys_Error("R_TextureAnimation: infinite cycle");
			}

			r_basetexture[v] = tm2;
		}
	}
	else if (r_basetexture[0] != tm)
	{
		for (int i = 0; i < ARRAYSIZE(r_basetexture); i++)
			r_basetexture[i] = tm;
	}
}

void R_DrawSurface(void)
{
	// r_ds vars there
	decal_t		*pdecals;
	int rendermode;
	void(*pblockdrawer)(void);
	word*(*pdecaldrawer)(byte* psource, word* prowdest);
	int smax, tmax, soffset, basetoffset;
	int scaleinfo[4]; // размеры декали
	int u, j, localvblocks;
	word* pcolumndest;

	static byte decalbuffer[1024];

	R_BuildLightMap();

	surfrowbytes = r_drawsurf.rowbytes;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;

	r_lightwidth = (r_drawsurf.surf->extents[0] >> 4) + 1;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

	pdecals = r_drawsurf.surf->pdecals;
	rendermode = currententity->curstate.rendermode;

	if (rendermode == kRenderTransAlpha)
	{
		pblockdrawer = R_DrawSurfaceBlock16Holes;
		pdecaldrawer = NULL;
		pdecals = NULL;
	}
	else if (rendermode != kRenderNormal || (r_drawsurf.surf->flags & SURF_DRAWTILED))
	{
		if (filterMode)
		{
			pblockdrawer = R_DrawSurfaceBlock16Filtered;
			pdecaldrawer = R_DecalFilteredSurface;
		}
		else
		{
			pblockdrawer = R_DrawSurfaceBlock16Fullbright1;
			pdecaldrawer = R_DecalFullbrightSurface;
		}
	}
	else if (filterMode)
	{
		pblockdrawer = R_DrawSurfaceBlock16Filtered;
		pdecaldrawer = R_DecalFilteredSurface;
	}
	else
	{
		switch ((int)r_fullbright.value)
		{
		case 1:
			pblockdrawer = R_DrawSurfaceBlock16Fullbright1;
			pdecaldrawer = R_DecalFullbrightSurface;
			break;
		case 2:
			pblockdrawer = R_DrawSurfaceBlock16Fullbright2;
			pdecaldrawer = R_DecalFullbrightSurface;
			break;
		case 3:
			pblockdrawer = R_DrawSurfaceBlock16Fullbright3;
			pdecaldrawer = NULL;
			pdecals = NULL;
			break;
		case 4:
			pblockdrawer = R_DrawSurfaceBlock16Fullbright4;
 			pdecaldrawer = NULL;
 			pdecals = NULL;
			break;
		case 5:
			pblockdrawer = R_DrawSurfaceBlock16Fullbright5;
			pdecaldrawer = NULL;
			pdecals = NULL;
			break;
		default:
#if defined(_WIN32) || defined(_WIN64)
			if (r_mmx.value)
				pblockdrawer = R_DrawSurfaceBlock16MMX;
			else
#endif
				pblockdrawer = R_DrawSurfaceBlock16;
			pdecaldrawer = R_DecalLightSurface;
		}
	}

	smax = r_drawsurf.texture->width >> r_drawsurf.surfmip;
	tmax = r_drawsurf.texture->height>> r_drawsurf.surfmip;

	r_stepback = smax * tmax;
	sourcevstep = smax * blocksize;

	sourcetstep = smax;

	soffset = ((r_drawsurf.surf->texturemins[0] >> r_drawsurf.surfmip) + (smax << 16)) % smax;
	basetoffset = (((r_drawsurf.surf->texturemins[1] >> r_drawsurf.surfmip) + (tmax << 16)) % tmax) * smax;

	R_SetupTextureIndex(0);

	pcolumndest = (word*)r_drawsurf.surfdat;

	R_DrawInitLut();

	if (pdecals != NULL)
	{
		word* decalsurfbase;

		R_DecalResolution(pdecals, scaleinfo, r_drawsurf.surfmip);
		scaleinfo[0] >>= blockdivshift;
		scaleinfo[1] >>= blockdivshift;
		scaleinfo[2] = (blocksize + scaleinfo[2] - 1) >> blockdivshift;
		scaleinfo[3] = (blocksize + scaleinfo[3] - 1) >> blockdivshift;
		localvblocks = r_numvblocks;

		for (int i = 0; i < r_numhblocks; i++)
		{
			r_lightptr = &blocklights[i];
			r_offset = soffset + basetoffset;
			r_deltav = 0;
			prowdestbase = pcolumndest;
			decalsurfbase = pcolumndest;

			if (i < scaleinfo[0] || i > scaleinfo[2])
			{
				pblockdrawer();
			}
			else
			{
				r_numvblocks = scaleinfo[1];

				if (scaleinfo[1] != 0)
				{
					pblockdrawer();
					decalsurfbase = &pcolumndest[blocksize * r_numvblocks * (surfrowbytes >> 1)];
				}

				// парс по вертикали
				for (j = scaleinfo[1]; j < scaleinfo[3]; j++)
				{
					R_DecalPickRow((byte*)r_basetexture[j] + r_basetexture[j]->offsets[r_drawsurf.surfmip] + r_offset,
						(PackedColorVec*)((byte*)r_basetexture[j] + r_basetexture[j]->paloffset), decalbuffer);
					R_DecalSurface(pdecals, decalbuffer, i << blockdivshift, j << blockdivshift);
					decalsurfbase = pdecaldrawer(decalbuffer, decalsurfbase);
				}

				r_numvblocks = localvblocks - scaleinfo[3];
				if (r_numvblocks > 0)
				{
					r_deltav = j;
					prowdestbase = decalsurfbase;
					pblockdrawer();
				}
			}

			r_numvblocks = localvblocks;
			soffset += blocksize;
			if (soffset >= smax)
			{
				R_SetupTextureIndex(i + 1);
				soffset = 0;
			}

			pcolumndest += blocksize;
		}
	}
	else
	{
		r_deltav = 0;
		
		for (u = 0; u<r_numhblocks; u++)
		{
			r_lightptr = &blocklights[u];

			prowdestbase = pcolumndest;

			r_offset = basetoffset + soffset;

			(*pblockdrawer)();

			soffset = soffset + blocksize;
			if (soffset >= smax)
			{
				R_SetupTextureIndex(u + 1);
				soffset = 0;
			}

			pcolumndest += blocksize;
		}
	}
}

void R_DrawSurfaceBlock16(void)
{
	int				v, i, b;
	byte* psource;
	word* prowdest;
	PackedColorVec* palette, pix;
	texture_t* texture;
	int redlightleft, redlightright, redlightleftstep, redlightrightstep;
	int greenlightleft, greenlightright, greenlightleftstep, greenlightrightstep;
	int bluelightleft, bluelightright, bluelightleftstep, bluelightrightstep;
	int redfrac, greenfrac, bluefrac;
	int redfracstep, greenfracstep, bluefracstep;

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		redlightleft = r_lightptr[0].r;
		greenlightleft = r_lightptr[0].g;
		bluelightleft = r_lightptr[0].b;

		redlightright = r_lightptr[1].r;
		greenlightright = r_lightptr[1].g;
		bluelightright = r_lightptr[1].b;

		r_lightptr += r_lightwidth;

		redlightleftstep = ((int)r_lightptr[0].r - redlightleft) >> blockdivshift;
		greenlightleftstep = ((int)r_lightptr[0].g - greenlightleft) >> blockdivshift;
		bluelightleftstep = ((int)r_lightptr[0].b - bluelightleft) >> blockdivshift;
		redlightrightstep = ((int)r_lightptr[1].r - redlightright) >> blockdivshift;
		greenlightrightstep = ((int)r_lightptr[1].g - greenlightright) >> blockdivshift;
		bluelightrightstep = ((int)r_lightptr[1].b - bluelightright) >> blockdivshift;

		texture = r_basetexture[r_deltav + v];
		psource = r_offset + (byte*)texture + texture->offsets[r_drawsurf.surfmip];
		palette = (PackedColorVec*)((byte*)texture + texture->paloffset);
		//bool bIsFullbrightPalette = texture->name[2] == '~';
		//bool bIsFullbrightPalette2 = strstr(texture->name, "lab_crt");

		for (i = 0; i < blocksize; i++)
		{
			redfracstep = (redlightright - redlightleft) >> blockdivshift;
			redfrac = redlightleft;

			greenfracstep = (greenlightright - greenlightleft) >> blockdivshift;
			greenfrac = greenlightleft;

			bluefracstep = (bluelightright - bluelightleft) >> blockdivshift;
			bluefrac = bluelightleft;

			for (b = 0; b < blocksize; b++)
			{
				pix = palette[psource[b]];

				/*if (psource[b] >= 224 && (bIsFullbrightPalette || bIsFullbrightPalette2))
				{
					if (is15bit)
						prowdest[b] = PACKEDRGB555(pix.b, pix.g, pix.r);
					else
						prowdest[b] = PACKEDRGB565(pix.b, pix.g, pix.r);
				}
				else*/
				{
					if (is15bit)
						prowdest[b] = PACKEDRGB555(r_lut[(redfrac & 0xFF00) + pix.b], r_lut[(greenfrac & 0xFF00) + pix.g], r_lut[(bluefrac & 0xFF00) + pix.r]);
					else
						prowdest[b] = PACKEDRGB565(r_lut[(redfrac & 0xFF00) + pix.b], r_lut[(greenfrac & 0xFF00) + pix.g], r_lut[(bluefrac & 0xFF00) + pix.r]);
				}

				redfrac += redfracstep;
				greenfrac += greenfracstep;
				bluefrac += bluefracstep;
			}

			redlightright += redlightrightstep;
			redlightleft += redlightleftstep;
			greenlightright += greenlightrightstep;
			greenlightleft += greenlightleftstep;
			bluelightright += bluelightrightstep;
			bluelightleft += bluelightleftstep;

			prowdest += surfrowbytes >> 1;
			psource += sourcetstep;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

#if defined(_WIN32) || defined(_WIN64)
/*
 REFERENCE: INTEL APPNOTE #553
*/
void R_DrawSurfaceBlock16MMX(void)
{
	static __m64 MMXMULTIPLIER = { 0x555555555555 };
	static __m64 MMX1615REDBLUE = { 0x00f800f800f800f8 };	// masks out all but the 5MSBits of red and blue
	static __m64 MMX16GREEN = { 0x0000fC000000fC00 }, MMX15GREEN = { 0x0000f8000000f800 };	// masks out all but the 5MSBits of green
	static __m64 MMX16MULFACT = { 0x2000000420000004 }, MMX15MULFACT = { 0x2000000820000008 };	// multiply each word by 2**13, 2**3, 2**13, 2**3 and add results

	__m64 lightleft, lightright;
	__m64 lightleftstep, lightrightstep;
	__m64 lightfrac, lightfracstep;
	__m64 litBlock1, litBlock2, mergedBlocks, lightfrac2, finalColor;
	texture_t* texture;
	PackedColorVec* palette;
	word* prowdest;
	byte* psource;
	int i, b, v;

	if (gHasMMXTechnology)
	{
		prowdest = (word*)prowdestbase;

		lightleft = _m_pmulhw(_m_psrlwi(_m_punpckldq(_m_punpcklwd(_mm_cvtsi32_si64(r_lightptr[0].b), _mm_cvtsi32_si64(r_lightptr[0].g)), _mm_cvtsi32_si64(r_lightptr[0].r)), 1u), MMXMULTIPLIER);
		lightright = _m_pmulhw(_m_psrlwi(_m_punpckldq(_m_punpcklwd(_mm_cvtsi32_si64(r_lightptr[1].b), _mm_cvtsi32_si64(r_lightptr[1].g)), _mm_cvtsi32_si64(r_lightptr[1].r)), 1u), MMXMULTIPLIER);

		if (is15bit)
		{
			for (v = 0; v < r_numvblocks; v++)
			{
				texture = r_basetexture[v + r_deltav];
				psource = r_offset + (byte*)texture + texture->offsets[r_drawsurf.surfmip];
				palette = (PackedColorVec*)((byte*)texture + texture->paloffset);

				r_lightptr += r_lightwidth;

				MMXBUF0 = _m_pmulhw(_m_psrlwi(_m_punpckldq(_m_punpcklwd(_mm_cvtsi32_si64(r_lightptr[0].b), _mm_cvtsi32_si64(r_lightptr[0].g)), _mm_cvtsi32_si64(r_lightptr[0].r)), 1u), MMXMULTIPLIER);
				MMXBUF1 = _m_pmulhw(_m_psrlwi(_m_punpckldq(_m_punpcklwd(_mm_cvtsi32_si64(r_lightptr[1].b), _mm_cvtsi32_si64(r_lightptr[1].g)), _mm_cvtsi32_si64(r_lightptr[1].r)), 1u), MMXMULTIPLIER);
				lightleftstep = _m_psraw(_m_psubw(MMXBUF0, lightleft), _mm_cvtsi32_si64(blockdivshift));
				lightrightstep = _m_psraw(_m_psubw(MMXBUF1, lightright), _mm_cvtsi32_si64(blockdivshift));

				for (i = 0; i < blocksize; i++)
				{
					lightfracstep = _m_psrawi(_m_psubw(lightright, lightleft), blockdivshift);
					lightfrac = lightleft;

					for (b = 0; b < blocksize; b += 2)
					{
						litBlock1 = _m_pmulhw(_m_psllwi(*(__m64*)&palette[psource[b]], 3), lightfrac);
						lightfrac2 = _m_paddw(lightfrac, lightfracstep);
						litBlock2 = _m_pmulhw(_m_psllwi(*(__m64*)&palette[psource[b + 1]], 3), lightfrac2);
						lightfrac = _m_paddw(lightfrac2, lightfracstep);
						mergedBlocks = _m_packuswb(litBlock1, litBlock2);
						finalColor = _m_psrldi(_m_por(_m_pmaddwd(_m_pand(mergedBlocks, MMX1615REDBLUE), MMX15MULFACT), _m_pand(mergedBlocks, MMX15GREEN)), 6);
						*(unsigned int*)&prowdest[b] = _mm_cvtsi64_si32(_m_packssdw(finalColor, finalColor));
					}

					lightleft = _m_paddw(lightleft, lightleftstep);
					lightright = _m_paddw(lightright, lightrightstep);

					psource += sourcetstep;
					prowdest += surfrowbytes >> 1;
				}

				lightleft = MMXBUF0;
				lightright = MMXBUF1;

				r_offset += sourcevstep;
				if (r_offset >= r_stepback)
					r_offset -= r_stepback;
			}
		}
		else
		{
			for (v = 0; v < r_numvblocks; v++)
			{
				texture = r_basetexture[v + r_deltav];
				psource = r_offset + (byte*)texture + texture->offsets[r_drawsurf.surfmip];
				palette = (PackedColorVec*)((byte*)texture + texture->paloffset);

				r_lightptr += r_lightwidth;

				MMXBUF0 = _m_pmulhw(_m_psrlwi(_m_punpckldq(_m_punpcklwd(_mm_cvtsi32_si64(r_lightptr[0].b), _mm_cvtsi32_si64(r_lightptr[0].g)), _mm_cvtsi32_si64(r_lightptr[0].r)), 1u), MMXMULTIPLIER);
				MMXBUF1 = _m_pmulhw(_m_psrlwi(_m_punpckldq(_m_punpcklwd(_mm_cvtsi32_si64(r_lightptr[1].b), _mm_cvtsi32_si64(r_lightptr[1].g)), _mm_cvtsi32_si64(r_lightptr[1].r)), 1u), MMXMULTIPLIER);
				lightleftstep = _m_psraw(_m_psubw(MMXBUF0, lightleft), _mm_cvtsi32_si64(blockdivshift));
				lightrightstep = _m_psraw(_m_psubw(MMXBUF1, lightright), _mm_cvtsi32_si64(blockdivshift));

				for (i = 0; i < blocksize; i++)
				{
					lightfracstep = _m_psrawi(_m_psubw(lightright, lightleft), blockdivshift);
					lightfrac = lightleft;

					for (b = 0; b < blocksize; b += 2)
					{
						litBlock1 = _m_pmulhw(_m_psllwi(*(__m64*)&palette[psource[b]], 3), lightfrac);
						lightfrac2 = _m_paddw(lightfrac, lightfracstep);
						litBlock2 = _m_pmulhw(_m_psllwi(*(__m64*)&palette[psource[b + 1]], 3), lightfrac2);
						lightfrac = _m_paddw(lightfrac2, lightfracstep);
						mergedBlocks = _m_packuswb(litBlock1, litBlock2);
						finalColor = _m_psradi(_m_pslldi(_m_por(_m_pmaddwd(_m_pand(mergedBlocks, MMX1615REDBLUE), MMX16MULFACT), _m_pand(mergedBlocks, MMX16GREEN)), 11), 16);
						*(unsigned int*)&prowdest[b] = _mm_cvtsi64_si32(_m_packssdw(finalColor, finalColor));
					}

					lightleft = _m_paddw(lightleft, lightleftstep);
					lightright = _m_paddw(lightright, lightrightstep);

					psource += sourcetstep;
					prowdest += surfrowbytes >> 1;
				}

				lightleft = MMXBUF0;
				lightright = MMXBUF1;

				r_offset += sourcevstep;
				if (r_offset >= r_stepback)
					r_offset -= r_stepback;
			}

		}
		_m_empty();
	}
}
#endif

void R_DrawSurfaceBlock16Filtered(void)
{
	int				v, i, b;
	byte			*psource;
	word			*prowdest;
	PackedColorVec	*palette, pix;
	texture_t*		texture;
	int				red, green, blue;

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		texture = r_basetexture[r_deltav + v];
		psource = r_offset + (byte*)texture + texture->offsets[r_drawsurf.surfmip];
		palette = (PackedColorVec*)((byte*)texture + texture->paloffset);

		for (i = 0; i < blocksize; i++)
		{
			for (b = 0; b < blocksize; b++)
			{
				pix = palette[psource[b]];

				red = pix.b * filterBrightness * filterColorRed;
				green = pix.g * filterBrightness * filterColorGreen;
				blue = pix.r * filterBrightness * filterColorBlue;

				if (is15bit)
					prowdest[b] = PACKEDRGB555(red, green, blue);
				else
					prowdest[b] = PACKEDRGB565(red, green, blue);
			}

			prowdest += surfrowbytes >> 1;
			psource += sourcetstep;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright1(void)
{
	int				v, i, b;
	byte			*psource;
	word			*prowdest;
	PackedColorVec	*palette, pix;
	texture_t*		texture;

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		texture = r_basetexture[r_deltav + v];
		psource = r_offset + (byte*)texture + texture->offsets[r_drawsurf.surfmip];
		palette = (PackedColorVec*)((byte*)texture + texture->paloffset);

		for (i = 0; i < blocksize; i++)
		{
			for (b = 0; b < blocksize; b++)
			{
				pix = palette[psource[b]];
				if (is15bit)
					prowdest[b] = PACKEDRGB555(pix.b, pix.g, pix.r);
				else
					prowdest[b] = PACKEDRGB565(pix.b, pix.g, pix.r);
			}

			prowdest += surfrowbytes >> 1;
			psource += sourcetstep;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright2(void)
{
	int				v, i, b;
	word			*prowdest, pix;
	int redlightleft, redlightright, redlightleftstep, redlightrightstep;
	int greenlightleft, greenlightright, greenlightleftstep, greenlightrightstep;
	int bluelightleft, bluelightright, bluelightleftstep, bluelightrightstep;
	int redfrac, greenfrac, bluefrac;
	int redfracstep, greenfracstep, bluefracstep;

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		redlightleft = r_lightptr[0].r;
		greenlightleft = r_lightptr[0].g;
		bluelightleft = r_lightptr[0].b;

		redlightright = r_lightptr[1].r;
		greenlightright = r_lightptr[1].g;
		bluelightright = r_lightptr[1].b;

		r_lightptr += r_lightwidth;

		redlightleftstep = ((int)r_lightptr[0].r - redlightleft) >> blockdivshift;
		greenlightleftstep = ((int)r_lightptr[0].g - greenlightleft) >> blockdivshift;
		bluelightleftstep = ((int)r_lightptr[0].b - bluelightleft) >> blockdivshift;
		redlightrightstep = ((int)r_lightptr[1].r - redlightright) >> blockdivshift;
		greenlightrightstep = ((int)r_lightptr[1].g - greenlightright) >> blockdivshift;
		bluelightrightstep = ((int)r_lightptr[1].b - bluelightright) >> blockdivshift;

		for (i = 0; i < blocksize; i++)
		{
			redfracstep = (redlightright - redlightleft) >> blockdivshift;
			redfrac = redlightleft;

			greenfracstep = (greenlightright - greenlightleft) >> blockdivshift;
			greenfrac = greenlightleft;

			bluefracstep = (bluelightright - bluelightleft) >> blockdivshift;
			bluefrac = bluelightleft;

			for (b = 0; b < blocksize; b++)
			{
				if (is15bit)
					prowdest[b] = (redfrac >> 1) & 0x7C00 | (greenfrac >> 6) & 0x3E0 | bluefrac >> 11;
				else
					prowdest[b] = (redfrac >> 0) & 0xFFFFF800 | (greenfrac >> 5) & 0x7E0 | bluefrac >> 11;

				redfrac += redfracstep;
				greenfrac += greenfracstep;
				bluefrac += bluefracstep;
			}

			redlightright += redlightrightstep;
			redlightleft += redlightleftstep;
			greenlightright += greenlightrightstep;
			greenlightleft += greenlightleftstep;
			bluelightright += bluelightrightstep;
			bluelightleft += bluelightleftstep;

			prowdest += surfrowbytes >> 1;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright3(void)
{
	int				v, i, b;
	word			*prowdest, pix;
	int redlightleft, redlightright, redlightleftstep, redlightrightstep;
	int greenlightleft, greenlightright, greenlightleftstep, greenlightrightstep;
	int bluelightleft, bluelightright, bluelightleftstep, bluelightrightstep;
	int redfrac, greenfrac, bluefrac;
	int redfracstep, greenfracstep, bluefracstep;

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		redlightleft = r_lightptr[0].r;
		greenlightleft = r_lightptr[0].g;
		bluelightleft = r_lightptr[0].b;

		redlightright = r_lightptr[1].r;
		greenlightright = r_lightptr[1].g;
		bluelightright = r_lightptr[1].b;

		r_lightptr += r_lightwidth;

		redlightleftstep = ((int)r_lightptr[0].r - redlightleft) >> blockdivshift;
		greenlightleftstep = ((int)r_lightptr[0].g - greenlightleft) >> blockdivshift;
		bluelightleftstep = ((int)r_lightptr[0].b - bluelightleft) >> blockdivshift;
		redlightrightstep = ((int)r_lightptr[1].r - redlightright) >> blockdivshift;
		greenlightrightstep = ((int)r_lightptr[1].g - greenlightright) >> blockdivshift;
		bluelightrightstep = ((int)r_lightptr[1].b - bluelightright) >> blockdivshift;

		for (i = 0; i < blocksize; i++)
		{
			redfracstep = (redlightright - redlightleft) >> blockdivshift;
			redfrac = redlightleft;

			greenfracstep = (greenlightright - greenlightleft) >> blockdivshift;
			greenfrac = greenlightleft;

			bluefracstep = (bluelightright - bluelightleft) >> blockdivshift;
			bluefrac = bluelightleft;

			for (b = 0; b < blocksize; b++)
			{
				if (is15bit)
				{
					pix = (redfrac >> 1) & 0x7C00 | (greenfrac >> 6) & 0x3E0 | bluefrac >> 11;
					prowdest[b] = pix;
					if (!i || !b)
						prowdest[b] = pix & 0x1F																	// blue
						| ((pix & WORD(~0x3FF)) + ((~0xFE * (pix & 0x7C00)) >> r_drawsurf.surfmip >> 8)) & 0x7C00	// red
						| ((pix & WORD(~0x1F)) + ((~0xFE * (pix & 0x3E0)) >> r_drawsurf.surfmip >> 8)) & 0x3E0;		// green
				}
				else
				{
					pix = (redfrac >> 0) & 0xF800 | (greenfrac >> 5) & 0x7E0 | bluefrac >> 11;
					prowdest[b] = pix;
					if (!i || !b)
						prowdest[b] = pix & 0x1F																	// blue
						| ((pix & WORD(~0x3FF)) + ((~0xFE * (pix & 0xF800)) >> r_drawsurf.surfmip >> 8)) & 0xF800	// red
						| ((pix & WORD(~0x1F)) + ((~0xFE * (pix & 0x7E0)) >> r_drawsurf.surfmip >> 8)) & 0x7E0;		// green
				}

				redfrac += redfracstep;
				greenfrac += greenfracstep;
				bluefrac += bluefracstep;
			}

			redlightright += redlightrightstep;
			redlightleft += redlightleftstep;
			greenlightright += greenlightrightstep;
			greenlightleft += greenlightleftstep;
			bluelightright += bluelightrightstep;
			bluelightleft += bluelightleftstep;

			prowdest += surfrowbytes >> 1;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright4(void)
{
	int				v, i, b;
	word			*prowdest;
	int redlightleft, redlightright, redlightleftstep, redlightrightstep;
	int greenlightleft, greenlightright, greenlightleftstep, greenlightrightstep;
	int bluelightleft, bluelightright, bluelightleftstep, bluelightrightstep;
	int redfrac, greenfrac, bluefrac;
	int redfracstep, greenfracstep, bluefracstep;

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		redlightleft = r_lightptr[0].r;
		greenlightleft = r_lightptr[0].g;
		bluelightleft = r_lightptr[0].b;

		redlightright = r_lightptr[1].r;
		greenlightright = r_lightptr[1].g;
		bluelightright = r_lightptr[1].b;

		r_lightptr += r_lightwidth;

		redlightleftstep = ((int)r_lightptr[0].r - redlightleft) >> blockdivshift;
		greenlightleftstep = ((int)r_lightptr[0].g - greenlightleft) >> blockdivshift;
		bluelightleftstep = ((int)r_lightptr[0].b - bluelightleft) >> blockdivshift;
		redlightrightstep = ((int)r_lightptr[1].r - redlightright) >> blockdivshift;
		greenlightrightstep = ((int)r_lightptr[1].g - greenlightright) >> blockdivshift;
		bluelightrightstep = ((int)r_lightptr[1].b - bluelightright) >> blockdivshift;

		for (i = 0; i < blocksize; i++)
		{
			redfracstep = (redlightright - redlightleft) >> blockdivshift;
			redfrac = redlightleft;

			greenfracstep = (greenlightright - greenlightleft) >> blockdivshift;
			greenfrac = greenlightleft;

			bluefracstep = (bluelightright - bluelightleft) >> blockdivshift;
			bluefrac = bluelightleft;

			for (b = 0; b < blocksize; b++)
			{
				if (is15bit)
					prowdest[b] = PACKEDRGB555(r_lut[(redfrac & 0xFF00) + 255], r_lut[(greenfrac & 0xFF00) + 255], r_lut[(bluefrac & 0xFF00) + 255]);
				else
					prowdest[b] = PACKEDRGB565(r_lut[(redfrac & 0xFF00) + 255], r_lut[(greenfrac & 0xFF00) + 255], r_lut[(bluefrac & 0xFF00) + 255]);

				if (i == 0 && b == 0)
				{
					word srcpix = prowdest[b];
					if (is15bit)
						prowdest[b] = ((srcpix & 0xFC00) + ((~0xFE * (srcpix & 0x7C00)) >> r_drawsurf.surfmip >> 8)) & 0x7C00
									| ((srcpix & 0xFFE0) + ((~0xFE * (srcpix & 0x3E0)) >> r_drawsurf.surfmip >> 8)) & 0x3E0
									| srcpix & 0x1F;
					else
						prowdest[b] = ((srcpix & 0xF800) + ((~0xFE * (srcpix & 0xF800)) >> r_drawsurf.surfmip >> 8)) & 0xF800
									| ((srcpix & 0xFFE0) + ((~0xFE * (srcpix & 0x7E0)) >> r_drawsurf.surfmip >> 8)) & 0x7E0
									| srcpix & 0x1F;
				}
				
				redfrac += redfracstep;
				greenfrac += greenfracstep;
				bluefrac += bluefracstep;
			}

			redlightright += redlightrightstep;
			redlightleft += redlightleftstep;
			greenlightright += greenlightrightstep;
			greenlightleft += greenlightleftstep;
			bluelightright += bluelightrightstep;
			bluelightleft += bluelightleftstep;

			prowdest += surfrowbytes >> 1;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Fullbright5(void)
{
	int				v, i, b;
	word			*prowdest, pix;
	int redlightleft, redlightright, redlightleftstep, redlightrightstep;
	int greenlightleft, greenlightright, greenlightleftstep, greenlightrightstep;
	int bluelightleft, bluelightright, bluelightleftstep, bluelightrightstep;
	int redfrac, greenfrac, bluefrac;
	int redfracstep, greenfracstep, bluefracstep;
	colorVec mipcolors[4] = {
		{ 255, 0, 0, 0 },
		{ 255, 255, 0, 0 },
		{ 0, 255, 0, 0 },
		{ 0, 255, 255, 0 }
	};

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		redlightleft = r_lightptr[0].r;
		greenlightleft = r_lightptr[0].g;
		bluelightleft = r_lightptr[0].b;

		redlightright = r_lightptr[1].r;
		greenlightright = r_lightptr[1].g;
		bluelightright = r_lightptr[1].b;

		r_lightptr += r_lightwidth;

		redlightleftstep = ((int)r_lightptr[0].r - redlightleft) >> blockdivshift;
		greenlightleftstep = ((int)r_lightptr[0].g - greenlightleft) >> blockdivshift;
		bluelightleftstep = ((int)r_lightptr[0].b - bluelightleft) >> blockdivshift;
		redlightrightstep = ((int)r_lightptr[1].r - redlightright) >> blockdivshift;
		greenlightrightstep = ((int)r_lightptr[1].g - greenlightright) >> blockdivshift;
		bluelightrightstep = ((int)r_lightptr[1].b - bluelightright) >> blockdivshift;

		for (i = 0; i < blocksize; i++)
		{
			redfracstep = (redlightright - redlightleft) >> blockdivshift;
			redfrac = redlightleft;

			greenfracstep = (greenlightright - greenlightleft) >> blockdivshift;
			greenfrac = greenlightleft;

			bluefracstep = (bluelightright - bluelightleft) >> blockdivshift;
			bluefrac = bluelightleft;

			for (b = 0; b < blocksize; b++)
			{
				if (is15bit)
				{
					pix = PACKEDRGB555(r_lut[(redfrac & 0xFF00) + mipcolors[r_drawsurf.surfmip].r], r_lut[(greenfrac & 0xFF00) + mipcolors[r_drawsurf.surfmip].g], r_lut[(bluefrac & 0xFF00) + mipcolors[r_drawsurf.surfmip].b]);
					prowdest[b] = pix;
					if (!i || !b)
						prowdest[b] = ((pix & WORD(~0x1F)) + ((~0xFE * (pix & 0x3E0)) >> r_drawsurf.surfmip >> 8)) & 0x3E0 // green part
						| (pix & 0xFF + ((((0xFF * (0x1F - (pix & 0x1F))) >> r_drawsurf.surfmip) & 0xFFFF) >> 8)) & 0x1F // blue part*
						| ((pix & WORD(~0x3FF)) + ((~0xFE * (pix & 0x7C00)) >> r_drawsurf.surfmip >> 8)) & 0x7C00;		// red part
				}
				else
				{
					pix = PACKEDRGB565(r_lut[(redfrac & 0xFF00) + mipcolors[r_drawsurf.surfmip].r], r_lut[(greenfrac & 0xFF00) + mipcolors[r_drawsurf.surfmip].g], r_lut[(bluefrac & 0xFF00) + mipcolors[r_drawsurf.surfmip].b]);
					prowdest[b] = pix;
					if (!i || !b)
						prowdest[b] = ((pix & WORD(~0x1F)) + ((~0xFE * (pix & 0x7E0)) >> r_drawsurf.surfmip >> 8)) & 0x7E0 // green part
						| (pix & 0xFF + ((((0xFF * (0x1F - (pix & 0x1F))) >> r_drawsurf.surfmip) & 0xFFFF) >> 8)) & 0x1F // blue part*
						| ((pix & WORD(~0x3FF)) + ((~0xFE * (pix & 0xF800)) >> r_drawsurf.surfmip >> 8)) & 0xF800;		// red part
				}

				redfrac += redfracstep;
				greenfrac += greenfracstep;
				bluefrac += bluefracstep;
			}

			redlightright += redlightrightstep;
			redlightleft += redlightleftstep;
			greenlightright += greenlightrightstep;
			greenlightleft += greenlightleftstep;
			bluelightright += bluelightrightstep;
			bluelightleft += bluelightleftstep;

			prowdest += surfrowbytes >> 1;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

void R_DrawSurfaceBlock16Holes(void)
{
	int				v, i, b;
	byte			*psource;
	word			*prowdest;
	PackedColorVec	*palette, pix;
	texture_t*		texture;
	int redlightleft, redlightright, redlightleftstep, redlightrightstep;
	int greenlightleft, greenlightright, greenlightleftstep, greenlightrightstep;
	int bluelightleft, bluelightright, bluelightleftstep, bluelightrightstep;
	int redfrac, greenfrac, bluefrac;
	int redfracstep, greenfracstep, bluefracstep;

	prowdest = (word*)prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		redlightleft = r_lightptr[0].r;
		greenlightleft = r_lightptr[0].g;
		bluelightleft = r_lightptr[0].b;

		redlightright = r_lightptr[1].r;
		greenlightright = r_lightptr[1].g;
		bluelightright = r_lightptr[1].b;

		r_lightptr += r_lightwidth;

		redlightleftstep = ((int)r_lightptr[0].r - redlightleft) >> blockdivshift;
		greenlightleftstep = ((int)r_lightptr[0].g - greenlightleft) >> blockdivshift;
		bluelightleftstep = ((int)r_lightptr[0].b - bluelightleft) >> blockdivshift;
		redlightrightstep = ((int)r_lightptr[1].r - redlightright) >> blockdivshift;
		greenlightrightstep = ((int)r_lightptr[1].g - greenlightright) >> blockdivshift;
		bluelightrightstep = ((int)r_lightptr[1].b - bluelightright) >> blockdivshift;

		texture = r_basetexture[r_deltav + v];
		psource = r_offset + (byte*)texture + texture->offsets[r_drawsurf.surfmip];
		palette = (PackedColorVec*)((byte*)texture + texture->paloffset);

		for (i = 0; i < blocksize; i++)
		{
			redfracstep = (redlightright - redlightleft) >> blockdivshift;
			redfrac = redlightleft;

			greenfracstep = (greenlightright - greenlightleft) >> blockdivshift;
			greenfrac = greenlightleft;

			bluefracstep = (bluelightright - bluelightleft) >> blockdivshift;
			bluefrac = bluelightleft;

			for (b = 0; b < blocksize; b++)
			{
				if (psource[b] == TRANSPARENT_COLOR)
				{
					prowdest[b] = TRANSPARENT_COLOR16;
				}
				else
				{
					pix = palette[psource[b]];

					if (is15bit)
						prowdest[b] = PACKEDRGB555(r_lut[(redfrac & 0xFF00) + pix.b], r_lut[(greenfrac & 0xFF00) + pix.g], r_lut[(bluefrac & 0xFF00) + pix.r]);
					else
						prowdest[b] = PACKEDRGB565(r_lut[(redfrac & 0xFF00) + pix.b], r_lut[(greenfrac & 0xFF00) + pix.g], r_lut[(bluefrac & 0xFF00) + pix.r]);
				}

				redfrac += redfracstep;
				greenfrac += greenfracstep;
				bluefrac += bluefracstep;
			}

			redlightright += redlightrightstep;
			redlightleft += redlightleftstep;
			greenlightright += greenlightrightstep;
			greenlightleft += greenlightleftstep;
			bluelightright += bluelightrightstep;
			bluelightleft += bluelightleftstep;

			prowdest += surfrowbytes >> 1;
			psource += sourcetstep;
		}

		r_offset += sourcevstep;
		if (r_offset >= r_stepback)
			r_offset -= r_stepback;
	}
}

word* R_DecalFilteredSurface(byte* psource, word* prowdest)
{
	int				i, b, idx;
	int				red, green, blue;

	r_lightptr += r_lightwidth;

	if (blocksize <= 0)
		return prowdest;

	for (i = 0; i < blocksize; i++)
	{
		for (b = 0; b < blocksize; b++)
		{
			idx = (i * blocksize + b) * 4;

			red = psource[idx] * filterBrightness * filterColorRed;
			green = psource[idx + 1] * filterBrightness * filterColorGreen;
			blue = psource[idx + 2] * filterBrightness * filterColorBlue;

			if (is15bit)
				prowdest[b] = PACKEDRGB555(red, green, blue);
			else
				prowdest[b] = PACKEDRGB565(red, green, blue);
		}

		prowdest += surfrowbytes >> 1;
	}

	return prowdest;
}

word* R_DecalFullbrightSurface(byte* base, word* dest)
{
	int				i, b;

	r_lightptr += r_lightwidth;

	if (blocksize <= 0)
		return dest;

	for (i = 0; i < blocksize; i++)
	{
		for (b = 0; b < blocksize; b++)
		{
			int idx = (i * blocksize + b) * 4;

			if (is15bit)
				dest[b] = PACKEDRGB555(base[idx], base[idx + 1], base[idx + 2]);
			else
				dest[b] = PACKEDRGB565(base[idx], base[idx + 1], base[idx + 2]);
		}

		dest += surfrowbytes >> 1;
	}

	return dest;
}

word* R_DecalLightSurface(byte* psource, word* prowdest)
{
	int redlightleft, redlightright, redlightleftstep, redlightrightstep;
	int greenlightleft, greenlightright, greenlightleftstep, greenlightrightstep;
	int bluelightleft, bluelightright, bluelightleftstep, bluelightrightstep;
	int redfrac, greenfrac, bluefrac;
	int redfracstep, greenfracstep, bluefracstep;
	
	redlightleft = r_lightptr[0].r;
	greenlightleft = r_lightptr[0].g;
	bluelightleft = r_lightptr[0].b;

	redlightright = r_lightptr[1].r;
	greenlightright = r_lightptr[1].g;
	bluelightright = r_lightptr[1].b;

	r_lightptr += r_lightwidth;

	redlightleftstep = ((int)r_lightptr[0].r - redlightleft) >> blockdivshift;
	greenlightleftstep = ((int)r_lightptr[0].g - greenlightleft) >> blockdivshift;
	bluelightleftstep = ((int)r_lightptr[0].b - bluelightleft) >> blockdivshift;
	redlightrightstep = ((int)r_lightptr[1].r - redlightright) >> blockdivshift;
	greenlightrightstep = ((int)r_lightptr[1].g - greenlightright) >> blockdivshift;
	bluelightrightstep = ((int)r_lightptr[1].b - bluelightright) >> blockdivshift;

	if (blocksize <= 0)
		return prowdest;

	for (int i = 0; i < blocksize; i++)
	{
		redfracstep = (redlightright - redlightleft) >> blockdivshift;
		redfrac = redlightleft;

		greenfracstep = (greenlightright - greenlightleft) >> blockdivshift;
		greenfrac = greenlightleft;

		bluefracstep = (bluelightright - bluelightleft) >> blockdivshift;
		bluefrac = bluelightleft;

		for (int j = 0; j < blocksize; j++)
		{
			int idx = (i * blocksize + j) * 4;
			// особенность, по которой каждая текстура, имеющая в начале '~', имеет fullbright палитру последние её 32 байта
			if (r_drawsurf.texture->name[2] == '~' && psource[idx + 3] >= 0b11100000)
			{
				if (is15bit)
					prowdest[i * (surfrowbytes >> 1) + j] = PACKEDRGB555(psource[idx], psource[idx + 1], psource[idx + 2]);
				else
					prowdest[i * (surfrowbytes >> 1) + j] = PACKEDRGB565(psource[idx], psource[idx + 1], psource[idx + 2]);
			}
			else
			{
				if (is15bit)
					prowdest[i * (surfrowbytes >> 1) + j] = PACKEDRGB555(r_lut[(redfrac & 0xFF00) + psource[idx]], r_lut[(greenfrac & 0xFF00) + psource[idx + 1]], r_lut[(bluefrac & 0xFF00) + psource[idx + 2]]);
				else
					prowdest[i * (surfrowbytes >> 1) + j] = PACKEDRGB565(r_lut[(redfrac & 0xFF00) + psource[idx]], r_lut[(greenfrac & 0xFF00) + psource[idx + 1]], r_lut[(bluefrac & 0xFF00) + psource[idx + 2]]);
			}

			redfrac += redfracstep;
			greenfrac += greenfracstep;
			bluefrac += bluefracstep;
		}

		redlightright += redlightrightstep;
		greenlightright += greenlightrightstep;
		bluelightright += bluelightrightstep;
		redlightleft += redlightleftstep;
		greenlightleft += greenlightleftstep;
		bluelightleft += bluelightleftstep;
	}

	return &prowdest[blocksize * (surfrowbytes >> 1)];
}

void R_DecalSurface(decal_t* pdecal, byte* row, int width, int height)
{
	texture_t* texture;
	int dmx, dmy;
	byte *texdata, *palette, *rowstart;
	PackedColorVec palunit;
	int i, j;
	int rowinc;
	int mx, my;
	int summarymy, summarymx;
	int myclip, mxclip;

	for (; pdecal; pdecal = pdecal->pnext)
	{
		if (pdecal->scale != 0)
		{
			R_DecalSurfaceScaled(pdecal, row, width, height);
		}
		else
		{
			texture = (texture_s*)Draw_DecalTexture(pdecal->texture);

			dmx = (pdecal->dx >> r_drawsurf.surfmip) - width;
			dmy = (pdecal->dy >> r_drawsurf.surfmip) - height;
			mx = texture->width >> r_drawsurf.surfmip;
			my = texture->height >> r_drawsurf.surfmip;

			if (dmx < blocksize && dmy < blocksize && dmx > -mx && dmy > -my)
			{
				texdata = (byte*)texture + texture->offsets[r_drawsurf.surfmip];
				palette = (byte*)texture + texture->paloffset;

				if (pdecal->flags & FDECAL_HFLIP)
					texdata += mx - 1;

				summarymx = dmx + mx;
				summarymy = dmy + my;

				if (dmy < 0)
				{
					texdata -= dmy * mx;
					dmy = 0;
				}

				if (dmx < 0)
				{
					if (pdecal->flags & FDECAL_HFLIP)
						texdata += dmx;
					else
						texdata -= dmx;
					dmx = 0;
				}

				if (summarymx > blocksize)
					summarymx = blocksize;
				if (summarymy > blocksize)
					summarymy = blocksize;

				mxclip = summarymx - dmx;
				myclip = summarymy - dmy;

				rowstart = &row[4 * dmx + 4 * dmy * blocksize];
				rowinc = 4 * (blocksize - mxclip);

				if (texture->name[0] == '{')
				{
					if (texture->name[1] == '^')
					{
						for (int i = 0; i < myclip; i++)
						{
							for (int j = 0; j < mxclip; j++)
							{
								if (pdecal->flags & FDECAL_HFLIP)
									palunit = ((PackedColorVec*)palette)[texdata[-j]];
								else
									palunit = ((PackedColorVec*)palette)[texdata[j]];

								rowstart[0] = palunit.b;
								rowstart[1] = palunit.g;
								rowstart[2] = palunit.r;
								rowstart[3] = 0;

								rowstart += 4;
							}
							rowstart += (blocksize - mxclip) * 4;
							texdata += mx;
						}
					}
					else
					{
						for (int i = 0; i < myclip; i++)
						{
							for (int j = 0; j < mxclip; j++)
							{
								if (texdata[j] != TRANSPARENT_COLOR)
								{
									*(color24*)&rowstart[0] = ((color24*)palette)[texdata[j]];
									rowstart[3] = 0;
								}
								rowstart += 4;
							}
							rowstart += (blocksize - mxclip) * 4;
							texdata += mx;
						}
					}
				}
				else
				{
					for (int i = 0; i < myclip; i++)
					{
						for (int j = 0; j < mxclip; j++)
						{
							if (texdata[j])
							{
								color24* rgbPalette = (color24*)palette;

								rowstart[0] += (texdata[j] * (rgbPalette[255].r - rowstart[0])) >> 8;
								rowstart[1] += (texdata[j] * (rgbPalette[255].g - rowstart[1])) >> 8;
								rowstart[2] += (texdata[j] * (rgbPalette[255].b - rowstart[2])) >> 8;
								rowstart[3] = 0;
							}
								rowstart += 4;
						}
						rowstart += rowinc;
						texdata += mx;
					}
				}
			}
		}
	}
}

void R_DecalSurfaceScaled(decal_t* pdecal, byte* row, int width, int height)
{
	byte scaledeg;
	texture_s* dectex;
	int dmx, dmy;
	int mx, my;
	byte *texpal, *rowstart, *texdata;
	int summarymx, summarymy;
	int mxclip, myclip;
	int scaledwidth, scaledheight;
	int scalemask;
	int mxscaled;
	PackedColorVec palunit;
	
	scaledeg = pdecal->scale;
	scalemask = (1 << scaledeg) - 1;

	dectex = (texture_s*)Draw_DecalTexture(pdecal->texture);

	dmx = (pdecal->dx >> r_drawsurf.surfmip) - width;
	dmy = (pdecal->dy >> r_drawsurf.surfmip) - height;

	mx = dectex->width >> r_drawsurf.surfmip;
	mxscaled = mx << scaledeg;

	my = dectex->height >> r_drawsurf.surfmip << scaledeg;

	if (dmx < blocksize && dmy < blocksize && dmx > -mxscaled && dmy > -my)
	{
		texdata = (byte*)dectex + dectex->offsets[r_drawsurf.surfmip];

		if (pdecal->flags & FDECAL_HFLIP)
			texdata = &texdata[mxscaled - 1];

		texpal = (byte*)dectex + dectex->paloffset;

		summarymy = dmy + my;
		summarymx = dmx + mxscaled;
		scaledwidth = 0;
		scaledheight = scalemask;

		if (dmy < 0)
		{
			texdata += mx * (-dmy >> scaledeg);
			dmy = 0;
			scaledheight = scalemask - (scalemask & -dmy);
		}

		if (dmx < 0)
		{
			if (pdecal->flags & FDECAL_HFLIP)
			{
				texdata -= -dmx >> scaledeg;
				scaledwidth = scalemask & dmx;
			}
			else
			{
				texdata += -dmx >> scaledeg;
				scaledwidth = scalemask & -dmx;
			}
			dmx = 0;
		}

		if (summarymx > blocksize)
			summarymx = blocksize;
		if (summarymy > blocksize)
			summarymy = blocksize;

		myclip = summarymy - dmy;
		mxclip = summarymx - dmx;

		rowstart = &row[4 * (dmx + dmy * blocksize)];

		if (pdecal->scale)
		{
			if (dectex->name[0] == '{')
			{
				// old strange format of decals with rgba-word palette instead of rgb-byte
				if (dectex->name[1] == '^')
				{
					for (int i = 0; i < (int)myclip; i++)
					{
						for (int j = 0; j < mxclip; j++)
						{
							int idx = (j + scaledwidth) >> scaledeg;

							if (pdecal->flags & FDECAL_HFLIP)
								palunit = ((PackedColorVec*)texpal)[texdata[-idx]];
							else
								palunit = ((PackedColorVec*)texpal)[texdata[idx]];

							rowstart[0] = palunit.b;
							rowstart[1] = palunit.g;
							rowstart[2] = palunit.r;
							rowstart[3] = 0;

							rowstart += 4;
						}
						rowstart += (blocksize - mxclip) * 4;

						if (scaledheight)
							scaledheight--;
						else
						{
							texdata += mx;
							scaledheight = scalemask;
						}
					}
				}
				else
				{
					for (int i = 0; i < (int)myclip; i++)
					{
						for (int j = 0; j < mxclip; j++)
						{
							int idx = (j + scaledwidth) >> scaledeg;
							if (texdata[idx] != TRANSPARENT_COLOR)
							{
								*(color24*)&rowstart[0] = ((color24*)texpal)[texdata[idx]];
								rowstart[3] = 0;
							}
							rowstart += 4;
						}
						rowstart += (blocksize - mxclip) * 4;

						if (scaledheight)
							scaledheight--;
						else
						{
							texdata += mx;
							scaledheight = scalemask;
						}
					}
				}
			}
			else
			{
				// another strange decal format that doesn't use palette properly
				for (int i = 0; i < (int)myclip; i++)
				{
					for (int j = 0; j < mxclip; j++)
					{
						byte pix = texdata[(j + scaledwidth) >> scaledeg];
						if (pix)
						{
							color24* rgbPalette = (color24*)texpal;

							rowstart[0] += (pix * (rgbPalette[255].r - rowstart[0])) >> 8;
							rowstart[1] += (pix * (rgbPalette[255].g - rowstart[1])) >> 8;
							rowstart[2] += (pix * (rgbPalette[255].b - rowstart[2])) >> 8;
							rowstart[3] = 0;
						}
						rowstart += 4;
					}
					rowstart += (blocksize - mxclip) * 4;

					if (scaledheight)
						scaledheight--;
					else
					{
						texdata += mx;
						scaledheight = scalemask;
					}
				}
			}
		}
	}
}

// целая процедура для одной итерации?
void R_DecalPickRow(byte* data, PackedColorVec* pal, byte* row)
{
	for (int i = 0; i < blocksize; i++)
	{
		for (int j = 0; j < blocksize; j++)
		{
			row[((i * blocksize + j) * 4) + 0] = pal[data[i * sourcetstep + j]].b;
			row[((i * blocksize + j) * 4) + 1] = pal[data[i * sourcetstep + j]].g;
			row[((i * blocksize + j) * 4) + 2] = pal[data[i * sourcetstep + j]].r;
			row[((i * blocksize + j) * 4) + 3] = data[i * sourcetstep + j];
		}
	}

	r_offset += sourcevstep;
	if (r_offset >= r_stepback)
		r_offset -= r_stepback;
}

void R_DecalResolution(decal_t* pdecals, int* scaleinfo, int miplevel)
{
	int maxwidth, maxheight;
	decal_t* curdecal;
	int mipdx, mipdy;
	int summary_width, summary_height;

	curdecal = pdecals;

	maxwidth = (r_drawsurf.surf->extents[0] >> miplevel) - 1;
	maxheight = (r_drawsurf.surf->extents[1] >> miplevel) - 1;

	scaleinfo[2] = 0;                             // dx+tx
	scaleinfo[3] = 0;                             // dy+ty
	scaleinfo[0] = maxwidth;                        // decal maxwidth
	scaleinfo[1] = maxheight;                     // decal maxheight

	if (pdecals)
	{
		do
		{
			texture_t* decaltexture = Draw_DecalTexture(curdecal->texture);

			mipdx = curdecal->dx >> miplevel;
			mipdy = curdecal->dy >> miplevel;

			if (mipdx < scaleinfo[0])
				scaleinfo[0] = mipdx;
			if (mipdy < scaleinfo[1])
				scaleinfo[1] = mipdy;

			summary_width = (decaltexture->width >> miplevel << curdecal->scale) + mipdx;
			summary_height = (decaltexture->height >> miplevel << curdecal->scale) + mipdy;

			if (summary_width > scaleinfo[2])
				scaleinfo[2] = summary_width;
			if (summary_height > scaleinfo[3])
				scaleinfo[3] = summary_height;

			curdecal = curdecal->pnext;
		} while (curdecal);
	}

	if (scaleinfo[0] < 0)
		scaleinfo[0] = 0;
	if (scaleinfo[1] < 0)
		scaleinfo[1] = 0;
	if (scaleinfo[2] > maxwidth)
		scaleinfo[2] = maxwidth;
	if (scaleinfo[3] > maxheight)
		scaleinfo[3] = maxheight;
}

decal_t* R_DecalIntersect(decal_t* head, int* pcount, int x, int y)
{
	decal_t *pdecal = head, *plast = NULL;
	texture_t *tex;
	unsigned maxWidth;
	int iArea, lastArea;

	lastArea = MAXWORD;
	*pcount = 0;
	maxWidth = (gDecalTexture->width * 3) / 2;

	while (pdecal)
	{
		tex = Draw_DecalTexture(pdecal->texture);

		if (!(pdecal->flags & (FDECAL_DONTSAVE | FDECAL_PERMANENT)))
		{
			if (maxWidth >= tex->width)
			{
				// Now figure out the part of the projection that intersects pDecal's
				// clip box [0,0,1,1].
				int dx, dy;

				dx = x - pdecal->dx - (tex->width / 2);
				if (dx < 0)
					dx = -dx;

				dy = y - pdecal->dy - (tex->height / 2);
				if (dy < 0)
					dy = -dy;

				if (dy <= dx)
					iArea = dx + (dy / 2);
				else
					iArea = dy + (dx / 2);

				if (iArea < 6)
				{
					*pcount += 1;

					if (!plast || iArea <= lastArea)
					{
						lastArea = iArea;
						plast = pdecal;
					}
				}
			}
		}

		pdecal = pdecal->pnext;
	}

	return plast;
}

void R_InvalidateSurface(msurface_t *decalSurf)
{
	for (int i = 0; i < MIPLEVELS; i++)
		if (decalSurf->cachespots[i] != NULL)
			decalSurf->cachespots[i]->texture = NULL;
}

// Unlink pdecal from any surface it's attached to
static void R_DecalUnlink(decal_t *pdecal)
{
	decal_t *cur;

	if (pdecal->psurface != NULL)
	{
		if (pdecal == pdecal->psurface->pdecals)
			pdecal->psurface->pdecals = pdecal->pnext;
		else
		{
			cur = pdecal->psurface->pdecals;

			if (pdecal->psurface->pdecals == NULL)
				return Sys_Error("Bad decal list");

			while (cur->pnext)
			{
				if (cur->pnext == pdecal)
				{
					cur->pnext = pdecal->pnext;
					break;
				}

				cur = cur->pnext;
			}
		}
	}

	if (pdecal->psurface)
		R_InvalidateSurface(pdecal->psurface);

	pdecal->psurface = NULL;
}

// Just reuse next decal in list
// A decal that spans multiple surfaces will use multiple decal_t pool entries, as each surface needs
// it's own.
static decal_t *R_DecalAlloc(decal_t *pdecal)
{
	int limit = MAX_DECALS;

	if (r_decals.value < limit && !(gDecalFlags & FDECAL_DONTSAVE))
		limit = r_decals.value;

	if (!limit)
		return NULL;

	if (!pdecal)
	{
		int count;

		count = 0;		// Check for the odd possiblity of infinte loop
		do
		{
			gDecalCount++;
			if (gDecalCount >= limit)
				gDecalCount = 0;
			pdecal = gDecalPool + gDecalCount;	// reuse next decal
			count++;
		} while ((pdecal->flags & (FDECAL_PERMANENT | FDECAL_DONTSAVE)) && count < limit);
	}

	// If decal is already linked to a surface, unlink it.
	R_DecalUnlink(pdecal);

	return pdecal;
}

void R_InsertCustomDecal(decal_t *pdecal, msurface_t *surface)
{
	decal_t *prevdecal, *decals;

	prevdecal = NULL;
	decals = surface->pdecals;
	if (decals)
	{
		while (!(decals->flags & FDECAL_DONTSAVE))
		{
			prevdecal = decals;
			decals = decals->pnext;
			if (!decals)
			{
				prevdecal->pnext = pdecal;
				return;
			}
		}
	}
	decals->pnext = decals;
	if (prevdecal) prevdecal->pnext = pdecal;
	else surface->pdecals = pdecal;
}

void R_DecalCreate(msurface_t *psurface, int textureIndex, float scale, float x, float y)
{
	texture_t *tex;
	int count;
	decal_t *pold, *pdecal;

	if (r_drawflat.value == 2.0)
	{
		Con_Printf(const_cast<char*>("Leaf: %d\n"), LeafId(psurface));
	}

	pold = R_DecalIntersect(psurface->pdecals, &count, x + (gDecalTexture->width / 2), y + (gDecalTexture->height / 2));

	if (count < MAX_OVERLAP_DECALS)
		pold = NULL;

	pdecal = R_DecalAlloc(pold);

	pdecal->texture = textureIndex;
	pdecal->flags = gDecalFlags;
	pdecal->dx = x;
	pdecal->dy = y;
	pdecal->pnext = NULL;

	if (psurface->pdecals == NULL)
		psurface->pdecals = pdecal;
	else
	{
		if (pdecal->flags & FDECAL_CUSTOM)
			R_InsertCustomDecal(pdecal, psurface);
		else
		{
			pold = psurface->pdecals;

			while (pold->pnext != NULL)
				pold = pold->pnext;

			pold->pnext = pdecal;
		}
	}

	pdecal->psurface = psurface;
	pdecal->scale = scale;
	pdecal->entityIndex = gDecalEntity;

	R_InvalidateSurface(psurface);
}

void R_DecalNode(mnode_t *node)
{
	float dist, flScaleCoeff, s, t, w, h, dir;
	msurface_t *psurf;
	vec3_t normal, cross;

	if (!node) 
		return;
	
	if (node->contents < 0)
		return;

	dist = DotProduct(gDecalPos, node->plane->normal) - node->plane->dist;

	if (dist > gDecalSize)
	{
		R_DecalNode(node->children[0]);
	}
	else if (dist < -gDecalSize)
	{
		R_DecalNode(node->children[1]);
	}
	else
	{
		if (dist < DECAL_DISTANCE && dist > -DECAL_DISTANCE)
		{
			psurf = gDecalModel->surfaces + node->firstsurface;

			for (int i = 0; i < node->numsurfaces; i++, psurf++)
			{
				// декали не проецируются на жидкости, скайбоксы и
				// прозрачные текстуры
				if (psurf->flags & (SURF_DRAWTURB | SURF_DRAWTILED))
					continue;

				flScaleCoeff = Length(psurf->texinfo->vecs[0]);

				if (flScaleCoeff < 0)
					flScaleCoeff = -flScaleCoeff;

				if (flScaleCoeff == 0)
					continue;

				s = DotProduct(psurf->texinfo->vecs[0], gDecalPos) + psurf->texinfo->vecs[0][3] - psurf->texturemins[0];
				t = DotProduct(psurf->texinfo->vecs[1], gDecalPos) + psurf->texinfo->vecs[1][3] - psurf->texturemins[1];

				if (flScaleCoeff < 1)
				{
					flScaleCoeff = 0;
					w = gDecalTexture->width;
					h = gDecalTexture->height;
				}
				else
				{
					if (flScaleCoeff > 7)
						flScaleCoeff = 3;
					else
						flScaleCoeff = gBits[(int)flScaleCoeff];

					if (gDecalFlags & FDECAL_DONTSAVE)
						flScaleCoeff = 0;

					w = float((int)gDecalTexture->width << (int)flScaleCoeff);
					h = float((int)gDecalTexture->height << (int)flScaleCoeff);
				}

				s -= (float)w / 2.0f;
				t -= (float)h / 2.0f;
				
				//Con_SafePrintf("s %.04f w %.04f t %.04f  h %.04f extents 0 %d 1 %d\n", s, w, t, h, psurf->extents[0], psurf->extents[1]);
				if (s <= -w || t <= -h || s > (w + psurf->extents[0]) || t > (h + psurf->extents[1]))
					continue;

				if (gDecalFlags & (FDECAL_DONTSAVE | FDECAL_CUSTOM))
				{
					if (psurf->flags & SURF_PLANEBACK)
						VectorScale(psurf->plane->normal, -1, normal);
					else
						VectorCopy(psurf->plane->normal, normal);

					CrossProduct(psurf->texinfo->vecs[0], normal, cross);
					if (DotProduct(cross, psurf->texinfo->vecs[1]) < 0)
						gDecalFlags |= FDECAL_HFLIP;
					else
						gDecalFlags &= ~FDECAL_HFLIP;

					CrossProduct(psurf->texinfo->vecs[1], normal, cross);
					dir = DotProduct(cross, psurf->texinfo->vecs[0]);

					if (gDecalFlags & FDECAL_HFLIP)
						dir = -dir;

					if (dir > 0)
						gDecalFlags |= FDECAL_VFLIP;
					else
						gDecalFlags &= ~FDECAL_VFLIP;
				}

				if ((gDecalFlags & FDECAL_DONTSAVE) && gDecalAppliedScale == -1.0)
					gDecalAppliedScale = Length(psurf->texinfo->vecs[0]);

				R_DecalCreate(psurf, gDecalIndex, flScaleCoeff, s, t);
			}
		}

		R_DecalNode(node->children[0]);
		R_DecalNode(node->children[1]);
	}
}

void R_DecalShoot_(texture_t *ptexture, int index, int entity, int modelIndex, vec_t *position, int flags)
{
	cl_entity_t *ent;
	mnode_t *pnodes;
	vec3_t forward, right, up;
	
	if (ptexture != NULL && g_bUsingInGameAdvertisements && !Q_strcasecmp(ptexture->name, "}lambda06"))
		flags |= FDECAL_CUSTOM;

	ent = cl_entities + entity;

	VectorCopy(position, gDecalPos);	// Pass position in global

	if (ent == NULL)
	{
		gDecalModel = NULL;
		Con_DPrintf(const_cast<char*>("Decals must hit mod_brush!\n"));
		return;
	}

	gDecalModel = ent->model;

	if (gDecalModel == NULL && (!modelIndex || (gDecalModel = CL_GetModelByIndex(modelIndex)) == NULL))
	{
		if (sv.active)
			gDecalModel = sv.models[sv.edicts[entity].v.modelindex];
	}

	if (!gDecalModel || gDecalModel->type != mod_brush || !ptexture)
	{
		Con_DPrintf(const_cast<char*>("Decals must hit mod_brush!\n"));
		return;
	}

	pnodes = gDecalModel->nodes;
	if (entity)
	{
		if (gDecalModel->firstmodelsurface)
		{
			VectorSubtract(position, gDecalModel->hulls[0].clip_mins, gDecalPos);
			VectorSubtract(gDecalPos, ent->origin, gDecalPos);
			pnodes += gDecalModel->hulls[0].firstclipnode;
		}
		
		if (ent->angles[0] || ent->angles[1] || ent->angles[2])
		{
			AngleVectors(ent->angles, forward, right, up);
			gDecalPos[0] = DotProduct(forward, gDecalPos);
			gDecalPos[1] = -DotProduct(right, gDecalPos);
			gDecalPos[2] = DotProduct(up, gDecalPos);
		}
	}

	gDecalEntity = entity;
	gDecalTexture = ptexture;
	gDecalAppliedScale = -1;
	gDecalIndex = index;

	//if (flags & FDECAL_CUSTOM)
	//	flags |= FDECAL_CLIPTEST;

	gDecalFlags = flags;
	gDecalSize = max(ptexture->width / 2, ptexture->height / 2);

	R_DecalNode(pnodes);
}

void R_DecalShoot(int textureIndex, int entity, int modelIndex, float * position, int flags)
{
	R_DecalShoot_(Draw_DecalTexture(textureIndex), textureIndex, entity, modelIndex, position, flags);
}

void R_CustomDecalShoot(texture_t* ptexture, int playernum, int entity, int modelIndex, vec_t* position, int flags)
{
	R_DecalShoot_(ptexture, ~playernum, entity, modelIndex, position, flags);
}

void R_FireCustomDecal(int textureIndex, int entity, int modelIndex, vec_t* position, int flags, float scale)
{
}

void R_DecalRemoveNonPermanent(int textureIndex)
{
	for (int i = 0; i < MAX_DECALS; i++)
	{
		if (textureIndex != gDecalPool[i].texture)
			continue;

		if (gDecalPool[i].flags & FDECAL_PERMANENT)
			continue;

		R_DecalUnlink(gDecalPool + i);
		Q_memset(gDecalPool + i, 0, sizeof(decal_t));
	}
}

void R_DecalRemoveAll(int textureIndex)
{
	for (int i = 0; i < MAX_DECALS; i++)
	{
		if (gDecalPool[i].texture == textureIndex)
		{
			R_DecalUnlink(&gDecalPool[i]);
			Q_memset(&gDecalPool[i], 0, sizeof(decal_t));
		}
	}
}

void R_DecalInit(void)
{
	Q_memset(gDecalPool, 0, sizeof(gDecalPool));
	gDecalCount = 0;
}

int R_DecalUnProject(decal_t* pdecal, vec_t* position)
{
	int index = 0;
	int scaleFactor;
	int width, height;

	float x, y;
	float planedist;
	float inverseScale;

	vec3_t forward, right, up;

	mtexinfo_t* tex = NULL;
	model_t* model = NULL;
	edict_t* ent = NULL;
	texture_t* ptex = NULL;

	if (!pdecal || !pdecal->psurface)
		return -1;

	tex = pdecal->psurface->texinfo;

	inverseScale = fabs(Length(tex->vecs[0]));

	if (inverseScale != 0.0f)
		inverseScale = (1.0 / inverseScale) * (1.0 / inverseScale);

	ptex = Draw_DecalTexture(pdecal->texture);

	if (inverseScale >= 1.0f)
	{
		if (inverseScale > 7)
			scaleFactor = 3;
		else
			scaleFactor = gBits[(int)inverseScale];

		width = ptex->width << scaleFactor;
		height = ptex->height << scaleFactor;
	}
	else
	{
		width = ptex->width;
		height = ptex->height;
	}

	x = (float)width / 2.0f + (float)pdecal->dx + (float)pdecal->psurface->texturemins[0] - tex->vecs[0][3];
	y = (float)height / 2.0f + (float)pdecal->dy + (float)pdecal->psurface->texturemins[1] - tex->vecs[1][3];

	VectorScale(tex->vecs[0], x * inverseScale, position);
	VectorMA(position, y * inverseScale, tex->vecs[1], position);

	planedist = pdecal->psurface->plane->dist + (pdecal->psurface->flags & SURF_PLANEBACK ? 2.0 : -2.0);

	VectorMA(position, planedist, pdecal->psurface->plane->normal, position);

	index = pdecal->entityIndex;

	if (pdecal->entityIndex)
	{
		ent = &sv.edicts[index];

		if (!ent->v.modelindex)
			return 0;

		model = sv.models[ent->v.modelindex];

		if (!model || model->type != mod_brush)
			return 0;

//		VectorCompare(ent->v.angles, vec3_origin)
		if (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2])
		{
			AngleVectorsTranspose(ent->v.angles, forward, right, up);

			position[0] = DotProduct(position, forward);
			position[1] = DotProduct(position, right);
			position[1] = DotProduct(position, up);
		}

		if (model->firstmodelsurface)
		{
			VectorAdd(position, model->hulls[0].clip_mins, position);
			VectorAdd(position, ent->v.origin, position);
		}
	}

	return index;
}

int DecalListAdd(DECALLIST* pList, int count)
{
	int			i;
	vec3_t		tmp;
	DECALLIST* pdecal;

	pdecal = pList + count;
	for (i = 0; i < count; i++)
	{
		if (pdecal->name == pList[i].name &&
			pdecal->entityIndex == pList[i].entityIndex)
		{
			VectorSubtract(pdecal->position, pList[i].position, tmp);	// Merge
			if (Length(tmp) < 2)	// UNDONE: Tune this '2' constant
				return count;
		}
	}

	// This is a new decal
	return count + 1;
}


typedef int (*qsortFunc_t)(const void*, const void*);

int DecalDepthCompare(const DECALLIST* elem1, const DECALLIST* elem2)
{
	if (elem1->depth > elem2->depth)
		return -1;
	if (elem1->depth < elem2->depth)
		return 1;

	return 0;
}

int DecalListCreate(DECALLIST* pList)
{
	int total = 0;
	int i, depth;

	for (i = 0; i < MAX_DECALS; i++)
	{
		decal_t* decal = &gDecalPool[i];

		// Decal is in use and is not a custom decal
		if (!decal->psurface || (decal->flags & (FDECAL_CUSTOM | FDECAL_DONTSAVE)))
			continue;

		// compute depth
		decal_t* pdecals = decal->psurface->pdecals;
		depth = 0;
		while (pdecals && pdecals != decal)
		{
			depth++;
			pdecals = pdecals->pnext;
		}
		pList[total].depth = depth;
		pList[total].flags = decal->flags;

		pList[total].entityIndex = R_DecalUnProject(decal, pList[total].position);
		pList[total].name = Draw_DecalTexture(decal->texture)->name;

		// Check to see if the decal should be added
		total = DecalListAdd(pList, total);
	}

	// Sort the decals lowest depth first, so they can be re-applied in order
	qsort(pList, total, sizeof(DECALLIST), (qsortFunc_t)DecalDepthCompare);

	return total;
}
