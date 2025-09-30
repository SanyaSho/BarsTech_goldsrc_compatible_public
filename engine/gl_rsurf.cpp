//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// gl_rsurf.cpp: surface-related refresh code

#include "quakedef.h"
#include "cmodel.h"
#include "r_studio.h"
#include "decals.h"
#include "draw.h"
#include "pr_cmds.h"
#include "cl_spectator.h"
#include "DetailTexture.h"
#include "view.h"

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64

#define MAX_DECALSURFS		500

int		lightmap_bytes;		// 1, 2, or 4
GLint	lightmap_used;

int		lightmap_textures[MAX_LIGHTMAPS];
#define MAX_BLOCK_LIGHTS	(18 * 18)
colorVec blocklights[MAX_BLOCK_LIGHTS];

int			active_lightmaps;

struct glRect_t
{
	int l, t, w, h;
};

glpoly_t* lightmap_polys[MAX_LIGHTMAPS];
qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];


int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

msurface_t* gDecalSurfs[MAX_DECALSURFS];
int gDecalSurfCount;

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];
// For gl_texsort 0
msurface_t* skychain = NULL;
msurface_t* waterchain = NULL;

extern colorVec gWaterColor;
extern qboolean g_bUsingInGameAdvertisements;
extern bool gl_texsort;

extern cvar_t gl_keeptjunctions;
extern cvar_t gl_watersides;
extern float turbsin[];

// Forward declarations
void R_RenderDynamicLightmaps( msurface_t* fa );
void DrawGLPolyScroll( msurface_t* psurface, cl_entity_t* pEntity );
void R_DrawDecals( const bool bMultitexture );
void DrawGLSolidPoly( glpoly_t* p );
float ScrollOffset( msurface_t* psurface, cl_entity_t* pEntity );

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights( msurface_t* surf )
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			smax, tmax;
	mtexinfo_t* tex;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue;		// not lit by this light

		VectorSubtract(cl_dlights[lnum].origin, currententity->origin, impact);

		rad = cl_dlights[lnum].radius;
		dist = DotProduct(impact, surf->plane->normal)
			- surf->plane->dist;
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
					unsigned temp;
					temp = (rad - dist) * 256;

					blocklights[t * smax + s].r += (int)(temp * cl_dlights[lnum].color.r) >> 8;
					blocklights[t * smax + s].g += (int)(temp * cl_dlights[lnum].color.g) >> 8;
					blocklights[t * smax + s].b += (int)(temp * cl_dlights[lnum].color.b) >> 8;
				}
			}
		}
	}
}

/*
===============
R_BuildLightMap

Build the blocklights array for a given surface and copy to dest
Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap( msurface_t* psurf, byte* dest, int stride )
{
	int			smax, tmax;
	int			i, j, size;
	color24* lightmap = NULL;
	colorVec* bl = NULL;
	float		scale;
	int			maps;

	int fixed_lightscale = 256;

	if (!gl_overbright.value || !gl_texsort)
		fixed_lightscale = pow(2.0, 1.0 / v_lightgamma.value) * 256 + 0.5;

	smax = (psurf->extents[0] >> 4) + 1;
	tmax = (psurf->extents[1] >> 4) + 1;

	size = smax * tmax;
	lightmap = psurf->samples;

	psurf->cached_dlight = (psurf->dlightbits & r_dlightactive);
	psurf->dlightbits &= r_dlightactive;

	if (size > MAX_BLOCK_LIGHTS)
		Sys_Error("Error: lightmap for texture %s too large (%d x %d = %d luxels); cannot exceed %d\n",
			psurf->texinfo->texture->name, smax, tmax, size, MAX_BLOCK_LIGHTS);

	if (filterMode)
	{
		for (i = 0; i < size; i++)
		{
			blocklights[i].r = filterColorRed * 255 * 256 * filterBrightness;
			blocklights[i].g = filterColorGreen * 255 * 256 * filterBrightness;
			blocklights[i].b = filterColorBlue * 255 * 256 * filterBrightness;
		}
		goto store;
	}

	// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		for (i = 0; i < size; i++)
		{
			blocklights[i].r = 255 * 256;
			blocklights[i].g = 255 * 256;
			blocklights[i].b = 255 * 256;
		}
		goto store;
	}

	// clear to no light
	for (i = 0; i < size; i++)
	{
		blocklights[i].r = 0;
		blocklights[i].g = 0;
		blocklights[i].b = 0;
	}
	
	// add all the lightmaps
	if (lightmap)
	{
		for (maps = 0; maps < MAXLIGHTMAPS && psurf->styles[maps] != 255; maps++)
		{
			scale = d_lightstylevalue[psurf->styles[maps]];

			psurf->cached_light[maps] = scale;	// 8.8 fraction			
			for (i = 0; i < size; i++)
			{
				blocklights[i].r += lightmap[i].r * scale;
				blocklights[i].g += lightmap[i].g * scale;
				blocklights[i].b += lightmap[i].b * scale;
			}
			lightmap += size;	// skip to next lightmap
		}
	}
	
	// add all the dynamic lights
	if (psurf->dlightframe == r_framecount)
		R_AddDynamicLights(psurf);

	// bound, invert, and shift
store:
	switch (gl_lightmap_format)
	{
		case GL_RGBA:
			stride -= (smax << 2);
			bl = blocklights;

			for (i = 0; i < tmax; i++, dest += stride)
			{
				for (j = 0; j < smax; j++)
				{
					dest[0] = lightgammatable[min((bl->r * fixed_lightscale) >> 14, (unsigned int)1023)] >> 2;
					dest[1] = lightgammatable[min((bl->g * fixed_lightscale) >> 14, (unsigned int)1023)] >> 2;
					dest[2] = lightgammatable[min((bl->b * fixed_lightscale) >> 14, (unsigned int)1023)] >> 2;

					bl++;

					if (gl_monolights.value)
					{
						// monochromatic lighting
						dest[3] = 255 - ((unsigned short)(dest[0] * 76 + dest[1] * 149 + dest[2] * 29) >> 8);
						dest[0] = 255;
						dest[1] = 255;
						dest[2] = 255;
					}
					else
					{
						dest[3] = 255;
					}

					dest += 4;
				}
			}
			break;

		case GL_LUMINANCE:
		case GL_INTENSITY:
			bl = blocklights;

			for (i = 0; i < tmax; i++, dest += stride)
			{
				for (j = 0; j < smax; j++)
				{
					dest[j] = 255 - min(bl->r >> 8, (unsigned int)255);
					bl++;
				}
			}
			break;

		default:
			Sys_Error("Bad lightmap format");
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t* R_TextureAnimation( msurface_t* s )
{
	static int rtable[20][20];

	int		i, j;
	int		reletive;
	int		count;

	texture_t* tex = s->texinfo->texture;

	if (!rtable[0][0])
	{
		for (i = 0; i < 20; i++)
		{
			for (j = 0; j < 20; j++)
				rtable[i][j] = RandomLong(0, 0x7FFF);
		}
	}

	if (currententity->curstate.frame)
	{
		if (tex->alternate_anims)
			tex = tex->alternate_anims;
	}

	if (!tex->anim_total)
		return tex;

	if (tex->name[0] == '-')
	{
		int tx = (int)((s->texturemins[0] + (tex->width << 16)) / tex->width) % 20;
		int ty = (int)((s->texturemins[1] + (tex->height << 16)) / tex->height) % 20;

		reletive = rtable[tx][ty] % tex->anim_total;
	}
	else
	{
		reletive = (int)(cl.time * 10.0) % tex->anim_total;
	}

	count = 0;
	while (tex->anim_min > reletive || tex->anim_max <= reletive)
	{
		tex = tex->anim_next;
		if (!tex)
			Sys_Error("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error("R_TextureAnimation: infinite cycle");
	}

	return tex;
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

bool mtexenabled = false;

void GL_SelectTexture( GLenum target );

void GL_DisableMultitexture( void )
{
	if (mtexenabled)
	{
		qglDisable(GL_TEXTURE_2D);
		GL_SelectTexture(TEXTURE0_SGIS);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture( void )
{
	if (gl_mtexable)
	{
		GL_SelectTexture(TEXTURE1_SGIS);
		qglEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

extern int gl_FilterTexture;

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly( msurface_t* s, int face )
{
	int i;
	int detail;

	float f;
	float (*v)[VERTEXSIZE];

	glpoly_t* p = NULL;
	glRect_t* theRect = NULL;

	// animated texture
	texture_t* t = NULL;

	if (currententity->curstate.rendermode == kRenderTransColor)
	{
		GL_DisableMultitexture();

		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable(GL_BLEND);

		t = R_TextureAnimation(s);
		GL_Bind(t->gl_texturenum);

		qglDisable(GL_TEXTURE_2D);
		DrawGLSolidPoly(s->polys);
		qglEnable(GL_TEXTURE_2D);

		GL_EnableMultitexture();
		return;
	}
	
	//
	// normal lightmaped poly
	//
	if (!(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER)))
	{
		if (!filterMode)
			R_RenderDynamicLightmaps(s);

		if (gl_mtexable && (currententity->curstate.rendermode == kRenderNormal || currententity->curstate.rendermode == kRenderTransAlpha))
		{
			t = R_TextureAnimation(s);
			// Binds world to texture env 0
			GL_SelectTexture(TEXTURE0_SGIS);
			GL_Bind(t->gl_texturenum);

			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			if (currententity->curstate.rendermode == kRenderTransColor)
				qglDisable(GL_TEXTURE_2D);

			// Binds lightmap to texenv 1
			GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)

			if (filterMode)
				GL_Bind(gl_FilterTexture);
			else
				GL_Bind(lightmap_textures[s->lightmaptexturenum]);

			i = s->lightmaptexturenum;

			if (lightmap_modified[i])
			{
				lightmap_modified[i] = false;

				theRect = &lightmap_rectchange[i];

				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t,
					BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmaps + (i * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * lightmap_bytes);

				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}

			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

#ifndef DISABLE_DT
			detail = DT_SetRenderState(t->gl_texturenum);
#endif

			qglBegin(GL_POLYGON);

			if (s->flags & SURF_DRAWTILED)
			{
				f = ScrollOffset(s, currententity);

				for (i = 0, v = s->polys->verts; i < s->polys->numverts; v++, i++)
				{
					qglMTexCoord2fSGIS(TEXTURE0_SGIS, (*v)[3] + f, (*v)[4]);
					qglMTexCoord2fSGIS(TEXTURE1_SGIS, (*v)[5], (*v)[6]);

#ifndef DISABLE_DT
					if (detail)
					{
						DT_SetTextureCoordinates((*v)[3] + f, (*v)[4]);
					}
#endif

					qglVertex3fv((const GLfloat*)v);
				}
			}
			else
			{
				for (i = 0, v = s->polys->verts; i < s->polys->numverts; v++, i++)
				{
					qglMTexCoord2fSGIS(TEXTURE0_SGIS, (*v)[3], (*v)[4]);
					qglMTexCoord2fSGIS(TEXTURE1_SGIS, (*v)[5], (*v)[6]);

#ifndef DISABLE_DT
					if (detail)
					{
						DT_SetTextureCoordinates((*v)[3], (*v)[4]);
					}
#endif

					qglVertex3fv((const GLfloat*)v);
				}
			}

			qglEnd();

#ifndef DISABLE_DT
			if (detail)
			{
				DT_ClearRenderState();
			}
#endif

			if (!gl_texsort && s->pdecals)
			{
				gDecalSurfs[gDecalSurfCount] = s;
				gDecalSurfCount++;

				if (gDecalSurfCount > MAX_DECALSURFS)
					Sys_Error("Too many decal surfaces!\n");

				if (currententity->curstate.rendermode == kRenderNormal)
				{
					R_DrawDecals(true);
				}
			}

			if (gl_wireframe.value)
			{
				qglDisable(GL_TEXTURE_2D);
				qglColor3f(1, 1, 1);

				if (gl_wireframe.value == 2.0)
					qglDisable(GL_DEPTH_TEST);

				qglBegin(GL_LINE_LOOP);

				for (i = 0, v = s->polys->verts; i < s->polys->numverts; v++, i++)
				{
					qglVertex3fv((const GLfloat*)v);
				}

				qglEnd();
				qglEnable(GL_TEXTURE_2D);

				if (gl_wireframe.value == 2.0)
					qglEnable(GL_DEPTH_TEST);
			}
		}
		else
		{
			t = R_TextureAnimation(s);
			// Binds world to texture env 0
			GL_DisableMultitexture();
			GL_Bind(t->gl_texturenum);

			if (s->flags & SURF_DRAWTILED)
			{
				DrawGLPolyScroll(s, currententity);
			}
			else
			{
				qglBegin(GL_POLYGON);

				for (i = 0, v = s->polys->verts; i < s->polys->numverts; v++, i++)
				{
					qglTexCoord2f((*v)[3], (*v)[4]);
					qglVertex3fv((const GLfloat*)v);
				}

				qglEnd();
			}

			if (!gl_texsort && s->pdecals)
			{
				gDecalSurfs[gDecalSurfCount] = s;
				gDecalSurfCount++;

				if (gDecalSurfCount > MAX_DECALSURFS)
					Sys_Error("Too many decal surfaces!\n");

				R_DrawDecals(false);
			}

			if (gl_wireframe.value)
			{
				qglDisable(GL_TEXTURE_2D);
				qglColor3f(1, 1, 1);

				if (gl_wireframe.value == 2.0)
					qglDisable(GL_DEPTH_TEST);

				qglBegin(GL_LINE_LOOP);

				for (i = 0, v = s->polys->verts; i < s->polys->numverts; v++, i++)
				{
					qglVertex3fv((const GLfloat*)v);
				}

				qglEnd();

				qglEnable(GL_TEXTURE_2D);

				if (gl_wireframe.value == 2.0)
					qglEnable(GL_DEPTH_TEST);
			}

			if (currententity->curstate.rendermode == kRenderNormal)
			{
				if (filterMode)
					GL_Bind(gl_FilterTexture);
				else
					GL_Bind(lightmap_textures[s->lightmaptexturenum]);

				qglEnable(GL_BLEND);
				qglBegin(GL_POLYGON);

				for (i = 0, v = s->polys->verts; i < s->polys->numverts; v++, i++)
				{
					qglTexCoord2f((*v)[5], (*v)[6]);
					qglVertex3fv((const GLfloat*)v);
				}

				qglEnd();
				qglDisable(GL_BLEND);
			}
		}

		return;
	}

	//
	// subdivided water surface warp
	//
	if (s->flags & SURF_DRAWTURB)
	{
		GL_DisableMultitexture();
		GL_Bind(s->texinfo->texture->gl_texturenum);
		EmitWaterPolys(s, face);
	}
}

/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void R_RenderDynamicLightmaps( msurface_t* fa )
{
	byte* base;
	int			maps;
	glRect_t* theRect;
	int smax, tmax;

	c_brush_polys++;

	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
	{
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;
	}

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
	dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];

			if (fa->light_t < theRect->t)
			{
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				
				theRect->t = fa->light_t;
			}

			if (fa->light_s < theRect->l)
			{
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;

				theRect->l = fa->light_s;
			}

			smax = (fa->extents[0] >> 4) + 1;
			tmax = (fa->extents[1] >> 4) + 1;

			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s - theRect->l) + smax;

			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t - theRect->t) + tmax;

			base = lightmaps + fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;

			R_BuildLightMap(fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
}

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly( glpoly_t* p )
{
	int i;
	float* v;
	vec3_t	nv;

	qglBegin(GL_TRIANGLE_FAN);

	v = p->verts[0];

	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		qglTexCoord2f(v[3], v[4]);

		nv[0] = v[0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		nv[1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		nv[2] = v[2];

		qglVertex3fv(nv);
	}

	qglEnd();
}

void DrawGLWaterPolyLightmap( glpoly_t* p )
{
	int		i;
	float* v;
	vec3_t	nv;

	qglBegin(GL_TRIANGLE_FAN);

	v = p->verts[0];

	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		qglTexCoord2f(v[5], v[6]);

		nv[0] = v[0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		nv[1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		nv[2] = v[2];

		qglVertex3fv(nv);
	}

	qglEnd();
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly( glpoly_t* p )
{
	int		i;
	float* v;

	qglBegin(GL_POLYGON);

	v = p->verts[0];

	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		qglTexCoord2f(v[3], v[4]);
		qglVertex3fv(v);
	}

	qglEnd();
}

/*
================
ScrollOffset
================
*/
float ScrollOffset( msurface_t* psurface, cl_entity_t* pEntity )
{
	float x, y;
	float sOffset;

	sOffset = (float)(pEntity->curstate.rendercolor.b + (pEntity->curstate.rendercolor.g << 8)) / 16.0f;

	if (!pEntity->curstate.rendercolor.r)
		sOffset = -sOffset;

	x = 1.0f / (float)psurface->texinfo->texture->width * sOffset * cl.time;

	if (x < 0.0)
		y = -1.0f;
	else
		y = 1.0f;

	return fmod(x, y);
}

/*
================
DrawGLPolyScroll
================
*/
void DrawGLPolyScroll( msurface_t* psurface, cl_entity_t* pEntity )
{
	int		i;
	float (*v)[VERTEXSIZE];

	float offset = ScrollOffset(psurface, pEntity);

	qglBegin(GL_POLYGON);

	for (i = 0, v = psurface->polys->verts; i < psurface->polys->numverts; v++, i++)
	{
		qglTexCoord2f((*v)[3] + offset, (*v)[4]);
		qglVertex3fv((const GLfloat*)v);
	}

	qglEnd();
}

/*
================
DrawGLSolidPoly
================
*/
void DrawGLSolidPoly( glpoly_t* p )
{
	int		i;
	float (*v)[VERTEXSIZE];

	qglColor4f(currententity->curstate.rendercolor.r / 256.f,
		currententity->curstate.rendercolor.g / 256.f,
		currententity->curstate.rendercolor.b / 256.f, r_blend);

	qglBegin(GL_POLYGON);

	for (i = 0, v = p->verts; i < p->numverts; i++, v++)
	{
		qglVertex3fv((const GLfloat*)v);
	}

	qglEnd();

	if (gl_wireframe.value)
	{
		if (gl_wireframe.value == 2.0)
			qglDisable(GL_DEPTH_TEST);

		qglColor3f(1, 1, 1);

		qglBegin(GL_LINE_LOOP);

		for (i = 0, v = p->verts; i < p->numverts; i++, v++)
		{
			qglVertex3fv((const GLfloat*)v);
		}

		qglEnd();

		if (gl_wireframe.value == 2.0)
			qglEnable(GL_DEPTH_TEST);
	}
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps( void )
{
	int			i, j;
	glpoly_t* p = NULL;
	glpoly_t* p2 = NULL;

	float (*v)[VERTEXSIZE];
	float tempVert[3];
	float* vertex = NULL;

	if (filterMode)
		return;

	if (r_fullbright.value)
		return;

	if (!gl_texsort)
		return;

	qglDepthMask(GL_FALSE);		// don't bother writing Z

	if (gl_lightmap_format == GL_LUMINANCE)
		qglBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	else if (gl_lightmap_format != GL_INTENSITY && gl_lightmap_format == GL_RGBA && gl_monolights.value == 0.0)
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		if (gl_overbright.value == 0.0)
			qglBlendFunc(GL_ZERO, GL_SRC_COLOR);
		else
		{
			qglBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			qglColor4f(0.7, 0.7, 0.7, 1);
		}
	}
	else
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglColor4f(0, 0, 0, 1);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (!r_lightmap.value)
		qglEnable(GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		p = lightmap_polys[i];

		if (!p)
			continue;

		GL_Bind(lightmap_textures[i]);

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, BLOCK_HEIGHT, gl_lightmap_format, GL_UNSIGNED_BYTE, &lightmaps[BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes * i]);
		}

		for (; p; p = p->chain)
		{
			if (p->flags & SURF_UNDERWATER)
			{
				DrawGLWaterPolyLightmap(p);
				continue;
			}

			if (p->flags & SURF_DRAWTURB)
			{
				for (p2 = p; p2; p2 = p2->next)
				{
					vertex = (float*)p2->verts;

					qglBegin(GL_POLYGON);
					qglColor3f(1, 1, 1);

					for (j = 0; j < p2->numverts; j++, vertex += VERTEXSIZE)
					{
						qglTexCoord2f(vertex[5], vertex[6]);
						
						tempVert[0] = vertex[0];
						tempVert[1] = vertex[1];
						tempVert[2] = turbsin[(byte)(cl.time * 160.0 + vertex[0] + vertex[1])] * gl_wateramp.value + vertex[2];
						tempVert[2] += turbsin[(byte)(cl.time * 171.0 + vertex[0] * 5.0 - vertex[1])] * gl_wateramp.value * 0.8;

						qglVertex3fv(tempVert);
					}

					qglEnd();
				}
				continue;
			}

			qglBegin(GL_POLYGON);
			v = p->verts;

			for (j = 0; j < p->numverts; j++, v++)
			{
				qglTexCoord2f((*v)[5], (*v)[6]);
				qglVertex3fv((const GLfloat*)v);
			}

			qglEnd();
		}
	}

	qglDisable(GL_BLEND);

	if (gl_lightmap_format == GL_LUMINANCE)
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else if (gl_lightmap_format == GL_INTENSITY || gl_monolights.value != 0.0)
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglColor4f(1, 1, 1, 1);
	}

	qglDepthMask(GL_TRUE);		// back to normal Z buffering
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly( msurface_t* fa )
{
	texture_t* t;
	byte* base;
	int			maps;
	msurface_t* surf = NULL;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY)
		return;

	t = R_TextureAnimation(fa);
	GL_Bind(t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{
		EmitWaterPolys(fa, 0);
		return;
	}

	if (fa->flags & SURF_UNDERWATER)
	{
		DrawGLWaterPoly(fa->polys);
	}
	else if (currententity->curstate.rendermode == kRenderTransColor)
	{
		qglDisable(GL_TEXTURE_2D);
		DrawGLSolidPoly(fa->polys);
		qglEnable(GL_TEXTURE_2D);
	}
	else if (fa->flags & SURF_DRAWTILED)
	{
		DrawGLPolyScroll(fa, currententity);
	}
	else
	{
		DrawGLPoly(fa->polys);
	}

	// add the poly to the proper lightmap chain

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	if (fa->pdecals)
	{
		gDecalSurfs[gDecalSurfCount] = fa;
		gDecalSurfCount++;

		if (gDecalSurfCount > MAX_DECALSURFS)
			Sys_Error("Too many decal surfaces!\n");
	}

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
	{
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;
	}

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
	dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;

			base = lightmaps + fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap(fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain( msurface_t* s )
{
	if (mirror)
		return;

	mirror = true;
	mirror_plane = s->plane;
}

/*
================
R_DrawWaterChain
================
*/
void R_DrawWaterChain( msurface_t* pChain )
{
	msurface_t* wchain;

	if (!pChain)
		return;
	
	qglColor4ub(255, 255, 255, 255);

	for (wchain = pChain; wchain; wchain = wchain->texturechain)
	{
		GL_Bind(wchain->texinfo->texture->gl_texturenum);
		EmitWaterPolys(wchain, 0);
	}
}

/*
================
DrawTextureChains
================
*/
void DrawTextureChains( void )
{
	msurface_t* s;
	texture_t* t;

	currententity = cl_entities;

	if (!gl_texsort)
	{
		GL_DisableMultitexture();

		if (skychain)
		{
			R_DrawSkyChain(skychain);
			skychain = NULL;
		}

		if (waterchain)
		{
			R_DrawWaterChain(waterchain);
			waterchain = NULL;
		}
	}

	for (int i = 0, j = 100; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t)
			continue;

		s = t->texturechain;

		if (!s)
			continue;

		if (i == skytexturenum)
		{
			R_DrawSkyChain(t->texturechain);
		}
		else if (i == mirrortexturenum && r_mirroralpha.value != 1.0)
		{
			R_MirrorChain(s);
			continue;
		}
		else
		{
			if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0)
				continue;	// draw translucent water later

			for (; s; s = s->texturechain)
				R_RenderBrushPoly(s);
		}

		t->texturechain = NULL;

		if (!j--)
		{
			S_ExtraUpdate();
			j = 100;
			continue;
		}
	}
}

/*
=================
R_SetRenderMode
=================
*/
void R_SetRenderMode( cl_entity_t* pEntity )
{
	switch (pEntity->curstate.rendermode)
	{
		case kRenderNormal:
			qglColor4f(1, 1, 1, 1);
			return;

		case kRenderTransColor:
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
			qglEnable(GL_BLEND);
			return;

		case kRenderTransAlpha:
			qglEnable(GL_ALPHA_TEST);
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglColor4f(1, 1, 1, 1);
			qglDisable(GL_BLEND);
			qglAlphaFunc(GL_GREATER, gl_alphamin.value);
			return;

		case kRenderTransAdd:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglBlendFunc(1, 1);
			qglColor4f(r_blend, r_blend, r_blend, 1);
			qglDepthMask(GL_FALSE);
			qglEnable(GL_BLEND);
			return;

		default:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglColor4f(1, 1, 1, r_blend);
			qglDepthMask(GL_FALSE);
			qglEnable(GL_BLEND);
			break;
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel( cl_entity_t* e )
{
	int			i;
	vec3_t		mins, maxs;
	msurface_t* psurf;
	float		dot;
	mplane_t*	pplane;
	bool		rotated;

	currententity = e;
	currenttexture = -1;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;

		for (i = 0; i < 3; i++)
		{
			mins[i] = e->origin[i] - e->model->radius;
			maxs[i] = e->origin[i] + e->model->radius;
		}
	}
	else
	{
		rotated = false;

		VectorAdd(e->origin, e->model->mins, mins);
		VectorAdd(e->origin, e->model->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	qglColor3f(1, 1, 1);
	Q_memset(lightmap_polys, 0, sizeof(lightmap_polys));

	VectorSubtract(r_refdef.vieworg, e->origin, modelorg);

	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	psurf = &e->model->surfaces[e->model->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if (e->model->firstmodelsurface && !gl_flashblend.value)
	{
		for (i = 0; i < MAX_DLIGHTS; i++)
		{
			if (cl_dlights[i].die < cl.time || (!cl_dlights[i].radius))
				continue;

			VectorSubtract(cl_dlights[i].origin, e->origin, cl_dlights[i].origin);

			R_MarkLights(&cl_dlights[i], 1 << i, &e->model->nodes[e->model->hulls[0].firstclipnode]);

			VectorAdd(cl_dlights[i].origin, e->origin, cl_dlights[i].origin);
		}
	}

	qglPushMatrix();

	R_RotateForEntity(e->origin, e);
	R_SetRenderMode(e);
	
	//
	// draw texture
	//
	for (i = 0; i < e->model->nummodelsurfaces; i++, psurf++)
	{	
		// find which side of the node we are on
		pplane = psurf->plane;

		if (!(psurf->flags & SURF_DRAWTURB) || (pplane->type == PLANE_Z || gl_watersides.value != 0.0) && (mins[2] + 1.0 < psurf->plane->dist))
		{
			dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

			// draw the polygon
			if ((!(psurf->flags & SURF_PLANEBACK) || (dot >= -BACKFACE_EPSILON)) && ((psurf->flags & SURF_PLANEBACK) || (dot <= BACKFACE_EPSILON)))
			{
				if (psurf->flags & SURF_DRAWTURB)
				{
					R_SetRenderMode(e);
					R_DrawSequentialPoly(psurf, 1);
				}
			}
			else if (gl_texsort)
			{
				R_RenderBrushPoly(psurf);
			}
			else
			{
				R_SetRenderMode(e);
				R_DrawSequentialPoly(psurf, 0);
			}
		}
	}

	if (currententity->curstate.rendermode != kRenderNormal)
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable(GL_BLEND);
	}

	if (gl_texsort || currententity->curstate.rendermode == kRenderTransColor)
	{
		if (currententity->curstate.rendermode == kRenderTransAlpha)
		{
			if (gl_lightholes.value != 0.0)
			{
				qglDepthFunc(GL_EQUAL);

				R_BlendLightmaps();

				if (gl_ztrick.value == 0.0 || gldepthmin < 0.5)
					qglDepthFunc(GL_LEQUAL);
				else
					qglDepthFunc(GL_GEQUAL);
			}
		}
		else
		{
			R_DrawDecals(false);

			if (currententity->curstate.rendermode == kRenderNormal)
				R_BlendLightmaps();
		}
	}

	qglPopMatrix();

	qglDepthMask(GL_TRUE);
	qglDisable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_NOTEQUAL, 0.0);
	qglDisable(GL_BLEND);
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode( mnode_t* node )
{
	int			c, side;
	mplane_t* plane;
	msurface_t* surf, ** mark;
	mleaf_t* pleaf;
	double		dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t*)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags(&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct(modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side]);

	// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < 0 - BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;
		{
			for (; c; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

				// don't backface underwater surfaces, because they warp
				if (!(surf->flags & SURF_UNDERWATER) && ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
					continue;		// wrong side

				// if sorting by texture, just store it out
				if (gl_texsort)
				{
					if (!mirror
						|| surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				}
				else if (surf->flags & SURF_DRAWSKY)
				{
					surf->texturechain = skychain;
					skychain = surf;
				}
				else if (surf->flags & SURF_DRAWTURB)
				{
					surf->texturechain = waterchain;
					waterchain = surf;
				}
				else
				{
					R_DrawSequentialPoly(surf, 0);
				}
			}
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode(node->children[!side]);
}



/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	cl_entity_t	ent;

	Q_memset(&ent, 0, sizeof(ent));

	currententity = &ent;
	ent.model = cl.worldmodel;

	VectorCopy(r_refdef.vieworg, modelorg);

	currenttexture = -1;

	ent.curstate.rendercolor.r = gWaterColor.r;
	ent.curstate.rendercolor.g = gWaterColor.g;
	ent.curstate.rendercolor.b = gWaterColor.b;

	qglColor3f(1, 1, 1);

	Q_memset(lightmap_polys, 0, sizeof(lightmap_polys));

	R_ClearSkyBox();

	if (gl_texsort == false)
	{
		switch (gl_lightmap_format)
		{
			case GL_LUMINANCE:
				qglBlendFunc(0, GL_ONE_MINUS_SRC_COLOR);
				break;

			case GL_INTENSITY:
				qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				qglColor4f(0, 0, 0, 1);
				qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				break;

			case GL_RGBA:
				qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				break;
		}
	}

	gDecalSurfCount = 0;
	R_RecursiveWorldNode(cl.worldmodel->nodes);
	gDecalSurfCount = 0;

	DrawTextureChains();
	S_ExtraUpdate();

	if (!CL_IsDevOverviewMode())
	{
		R_DrawDecals(false);
		R_BlendLightmaps();
	}
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves( void )
{
	byte* vis;
	mnode_t* node;
	int		i;
	byte	solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis.value)
		return;

	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		Q_memset(solid, 255, (cl.worldmodel->numleafs + 7) >> 3);
	}
	else
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
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int AllocBlock( int w, int h, int* x, int* y )
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error("AllocBlock: full");
	return 0;
}


mvertex_t* r_pcurrentvertbase;
model_t* currentmodel;

int	nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList( msurface_t* fa )
{
	int			i, lindex, lnumverts;
	medge_t* pedges, * r_pedge;
	int			vertpage;
	float* vec;
	float		s, t;
	glpoly_t* poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//
	// draw texture
	//
	poly = (glpoly_t*)Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	//
	// remove co-linear points - Ed
	//
	if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER))
	{
		for (i = 0; i < lnumverts; ++i)
		{
			vec3_t v1, v2;
			float* prev, * this_, * next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this_ = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract(this_, prev, v1);
			VectorNormalize(v1);
			VectorSubtract(next, prev, v2);
			VectorNormalize(v2);

			// skip co-linear points
#define COLINEAR_EPSILON 0.001
			if ((fabs(v1[0] - v2[0]) <= COLINEAR_EPSILON) &&
				(fabs(v1[1] - v2[1]) <= COLINEAR_EPSILON) &&
				(fabs(v1[2] - v2[2]) <= COLINEAR_EPSILON))
			{
				int j;
				for (j = i + 1; j < lnumverts; ++j)
				{
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap( msurface_t* surf )
{
	int		smax, tmax;
	byte* base;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	if ((surf->flags & SURF_DRAWTILED) && (surf->texinfo->flags & TEX_SPECIAL))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap(surf, base, BLOCK_WIDTH * lightmap_bytes);
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps( void )
{
	int		i, j;
	model_t* m;

	Q_memset(allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	if (!lightmap_textures[0])
	{
		for (i = 0; i < MAX_LIGHTMAPS; i++)
			lightmap_textures[i] = GL_GenTexture();
	}

	gl_lightmap_format = GL_RGBA;

	lightmap_bytes = 4;
	lightmap_used = GL_RGBA;

	for (i = 1; i < MAX_MODELS; i++)
	{
		m = cl.model_precache[i];

		if (!m)
			break;

		if (m->name[0] == '*')
			continue;

		m = CL_GetModelByIndex(i);

		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;

		for (j = 0; j < m->numsurfaces; j++)
		{
			GL_CreateSurfaceLightmap(&m->surfaces[j]);

			if (m->surfaces[j].flags & SURF_DRAWTURB)
				continue;

			BuildSurfaceDisplayList(&m->surfaces[j]);
		}
	}

	if (!gl_texsort)
		GL_SelectTexture(TEXTURE1_SGIS);

	//
	// upload all lightmaps that were filled
	//
	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break;		// no more used

		lightmap_modified[i] = false;
		GL_Bind(lightmap_textures[i]);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		qglTexImage2D(GL_TEXTURE_2D, GL_ZERO, lightmap_used, BLOCK_WIDTH, BLOCK_HEIGHT, GL_ZERO, gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes);
	}

	if (!gl_texsort)
		GL_SelectTexture(TEXTURE0_SGIS);
}

//-----------------------------------------------------------------------------
//
// Decal system
//
//-----------------------------------------------------------------------------

// UNDONE: Compress this???  256K here?
static decal_t			gDecalPool[MAX_DECALS];
static int				gDecalCount;
static vec3_t			gDecalPos;

static model_t*			gDecalModel = NULL;
static texture_t*		gDecalTexture = NULL;
static int				gDecalSize, gDecalIndex;
static int				gDecalFlags, gDecalEntity;
static float			gDecalAppliedScale;

int R_DecalUnProject( decal_t* pdecal, vec_t* position );
void R_DecalCreate( msurface_t* psurface, int textureIndex, float scale, float x, float y );
void R_DecalShoot( int textureIndex, int entity, int modelIndex, vec_t* position, int flags );

#define DECAL_DISTANCE			4

// Empirically determined constants for minimizing overalpping decals
#define MAX_OVERLAP_DECALS		4
#define DECAL_OVERLAP_DIST		8

struct decalcache_t
{
	int			decalIndex;
	float		decalVert[4][VERTEXSIZE];
};

#define DECAL_CACHEENTRY	256		// MUST BE POWER OF 2 or code below needs to change!
decalcache_t gDecalCache[DECAL_CACHEENTRY];


// Init the decal pool
void R_DecalInit( void )
{
	int i;

	Q_memset(gDecalPool, 0, sizeof(gDecalPool));
	gDecalCount = 0;

	for (i = 0; i < DECAL_CACHEENTRY; i++)
		gDecalCache[i].decalIndex = -1;
}


int R_DecalIndex( decal_t* pdecal )
{
	return (pdecal - gDecalPool);
}


int R_DecalCacheIndex( int index )
{
	return index & (DECAL_CACHEENTRY - 1);
}


decalcache_t* R_DecalCacheSlot( int decalIndex )
{
	int				cacheIndex;

	cacheIndex = R_DecalCacheIndex(decalIndex);	// Find the cache slot we want

	return gDecalCache + cacheIndex;
}


// Release the cache entry for this decal
void R_DecalCacheClear( decal_t* pdecal )
{
	int				index;
	decalcache_t* pCache;

	index = R_DecalIndex(pdecal);
	pCache = R_DecalCacheSlot(index);		// Find the cache slot

	if (pCache->decalIndex == index)		// If this is the decal that's cached here, clear it.
		pCache->decalIndex = -1;
}


// Unlink pdecal from any surface it's attached to
void R_DecalUnlink( decal_t* pdecal )
{
	decal_t* tmp;

	R_DecalCacheClear(pdecal);

	if (pdecal->psurface)
	{
		if (pdecal->psurface->pdecals == pdecal)
		{
			pdecal->psurface->pdecals = pdecal->pnext;
		}
		else
		{
			tmp = pdecal->psurface->pdecals;

			if (!tmp)
			{
				Sys_Error("Bad decal list");
				return; // To shut up IntelliSense -Enko
			}

			while (tmp->pnext)
			{
				if (tmp->pnext == pdecal)
				{
					tmp->pnext = pdecal->pnext;
					break;
				}

				tmp = tmp->pnext;
			}
		}
	}

	pdecal->psurface = NULL;
}


// Just reuse next decal in list
// A decal that spans multiple surfaces will use multiple decal_t pool entries, as each surface needs
// it's own.
decal_t* R_DecalAlloc( decal_t* pdecal )
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


// remove all decals
void R_DecalRemoveAll( int textureIndex )
{
	int	i;
	decal_t* pDecal;

	for (i = 0; i < MAX_DECALS; i++)
	{
		pDecal = &gDecalPool[i];

		if (pDecal->texture == textureIndex)
		{
			R_DecalUnlink(pDecal);
			Q_memset(pDecal, 0, sizeof(decal_t));
		}
	}
}


void R_DecalRemoveNonPermanent( int textureIndex )
{
	int	i;
	decal_t* pDecal;

	for (i = 0; i < MAX_DECALS; i++)
	{
		pDecal = &gDecalPool[i];

		if (pDecal->texture == textureIndex && (!pDecal || !(pDecal->flags & FDECAL_PERMANENT)))
		{
			R_DecalUnlink(pDecal);
			Q_memset(pDecal, 0, sizeof(decal_t));
		}
	}
}

void R_DecalRemoveWithFlag( int flags )
{
	int	i;
	decal_t* pDecal;

	for (i = 0; i < MAX_DECALS; i++)
	{
		pDecal = &gDecalPool[i];

		if (pDecal->flags & flags)
		{
			R_DecalUnlink(pDecal);
			Q_memset(pDecal, 0, sizeof(decal_t));
		}
	}
}

// iterate over all surfaces on a node, looking for surfaces to decal
void R_DecalNode( mnode_t* node, float flScale )
{
	mplane_t* splitplane;
	float		dist;

	if (!node)
		return;

	if (node->contents < 0)
		return;

	splitplane = node->plane;
	dist = DotProduct(gDecalPos, splitplane->normal) - splitplane->dist;

	// This is arbitrarily set to 10 right now.  In an ideal world we'd have the 
	// exact surface but we don't so, this tells me which planes are "sort of 
	// close" to the gunshot -- the gunshot is actually 4 units in front of the 
	// wall (see dlls\weapons.cpp). We also need to check to see if the decal 
	// actually intersects the texture space of the surface, as this method tags
	// parallel surfaces in the same node always.
	// JAY: This still tags faces that aren't correct at edges because we don't 
	// have a surface normal

	if (dist > gDecalSize)
	{
		R_DecalNode(node->children[0], flScale);
	}
	else if (dist < -gDecalSize)
	{
		R_DecalNode(node->children[1], flScale);
	}
	else
	{
		if (dist < DECAL_DISTANCE && dist > -DECAL_DISTANCE)
		{
			int			w, h;
			float		s, t, scale;
			msurface_t* surf;
			int			i;
			mtexinfo_t* tex;
			vec3_t		cross, normal;
			float		face;

			surf = gDecalModel->surfaces + node->firstsurface;

			// iterate over all surfaces in the node
			for (i = 0; i < node->numsurfaces; i++, surf++)
			{
				if ((surf->flags & (SURF_DRAWTILED | SURF_DRAWTURB)) || ((surf->flags & SURF_DONTWARP) && (gDecalFlags & FDECAL_CUSTOM)))
					continue;

				tex = surf->texinfo;

				if (flScale == -1)
					scale = Length(tex->vecs[0]);
				else
					scale = flScale;

				if (scale == 0)
					continue;

				s = DotProduct(gDecalPos, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
				t = DotProduct(gDecalPos, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

				w = gDecalTexture->width * scale;
				h = gDecalTexture->height * scale;

				// move s,t to upper left corner
				s -= (w * 0.5);
				t -= (h * 0.5);

				if (s <= -w || t <= -h || s > (surf->extents[0] + w) || t > (surf->extents[1] + h))
				{
					continue; // nope
				}

				scale = 1.0 / scale;
				s = (s + surf->texturemins[0]) / (float)tex->texture->width;
				t = (t + surf->texturemins[1]) / (float)tex->texture->height;

				if (gDecalFlags & (FDECAL_CUSTOM | FDECAL_DONTSAVE))
				{
					if (surf->flags & SURF_PLANEBACK)
						VectorScale(surf->plane->normal, -1, normal);
					else
						VectorCopy(surf->plane->normal, normal);

					CrossProduct(surf->texinfo->vecs[0], normal, cross);
					face = DotProduct(cross, surf->texinfo->vecs[1]);

					if (face < 0)
						gDecalFlags |= FDECAL_HFLIP;
					else
						gDecalFlags &= ~FDECAL_HFLIP;

					CrossProduct(surf->texinfo->vecs[1], normal, cross);
					face = DotProduct(cross, surf->texinfo->vecs[0]);

					if (gDecalFlags & FDECAL_HFLIP)
						face = -face;

					if (face > 0)
						gDecalFlags |= FDECAL_VFLIP;
					else
						gDecalFlags &= ~FDECAL_VFLIP;
				}

				if ((gDecalFlags & FDECAL_DONTSAVE) && gDecalAppliedScale == -1.0)
					gDecalAppliedScale = Length(tex->vecs[0]);

				// stamp it
				R_DecalCreate(surf, gDecalIndex, scale, s, t);
			}
		}

		R_DecalNode(node->children[0], flScale);
		R_DecalNode(node->children[1], flScale);
	}
}

int DecalListAdd( DECALLIST* pList, int count )
{
	int			i;
	vec3_t		tmp;
	DECALLIST* pdecal;

	pdecal = pList + count;
	for (i = 0; i < count; i++)
	{
		if (!Q_strcmp(pdecal->name, pList[i].name) &&
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


typedef int (*qsortFunc_t)( const void *, const void * );

int DecalDepthCompare( const DECALLIST* elem1, const DECALLIST* elem2 )
{
	if (elem1->depth > elem2->depth)
		return -1;
	if (elem1->depth < elem2->depth)
		return 1;

	return 0;
}

int DecalListCreate( DECALLIST* pList )
{
	int total = 0;
	int i, depth;

	if (cl.worldmodel)
	{
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
			pList[total].name = (char*)Draw_DecalTexture(decal->texture);

			// Check to see if the decal should be addedo
			total = DecalListAdd(pList, total);
		}
	}

	// Sort the decals lowest depth first, so they can be re-applied in order
	qsort(pList, total, sizeof(DECALLIST), (qsortFunc_t)DecalDepthCompare);

	return total;
}
// ---------------------------------------------------------

int R_DecalUnProject( decal_t* pdecal, vec_t* position )
{
	int index = 0;

	float x, y, len;
	float inverseScale;

	vec3_t forward, right, up;

	mtexinfo_t* tex = NULL;
	model_t* model = NULL;
	edict_t* ent = NULL;
	texture_t* ptex = NULL;

	if (!pdecal || !pdecal->psurface)
		return -1;

	tex = pdecal->psurface->texinfo;

	x = (float)tex->texture->width * pdecal->dx - (float)pdecal->psurface->texturemins[0];
	y = (float)tex->texture->height * pdecal->dy - (float)pdecal->psurface->texturemins[1];

	len = Length((const vec_t*)tex) * 0.5;
	ptex = Draw_DecalTexture(pdecal->texture);

	x = (float)ptex->width * len + x + (float)pdecal->psurface->texturemins[0] - tex->vecs[0][3];
	y = (float)ptex->height * len + y + (float)pdecal->psurface->texturemins[1] - tex->vecs[1][3];

	inverseScale = fabs(Length((const vec_t*)tex));

	if (inverseScale != 0.0)
		inverseScale = (1.0 / inverseScale) * (1.0 / inverseScale);

	VectorScale((const vec_t*)tex, x * inverseScale, position);

	VectorMA(position, y * inverseScale, tex->vecs[1], position);
	VectorMA(position, pdecal->psurface->plane->dist, pdecal->psurface->plane->normal, position);

	index = pdecal->entityIndex;

	if (pdecal->entityIndex)
	{
		ent = &sv.edicts[index];
	
		if (!ent->v.modelindex)
			return 0;

		model = sv.models[ent->v.modelindex];

		// Make sure it's a brush model
		if (!model || model->type != mod_brush)
			return 0;

		if (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2])
		{
			AngleVectorsTranspose(ent->v.angles, forward, right, up);

			position[0] = DotProduct(position, forward);
			position[1] = DotProduct(position, right);
			position[1] = DotProduct(position, up);
		}

		if (model->firstmodelsurface)
		{
			position[0] += model->hulls[0].clip_mins[0];
			position[1] += model->hulls[0].clip_mins[1];
			position[2] += model->hulls[0].clip_mins[2];

			position[0] += ent->v.origin[0];
			position[1] += ent->v.origin[1];
			position[2] += ent->v.origin[2];
		}
	}

	return index;
}


// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
void R_DecalShoot_( texture_t* ptexture, int index, int entity, int modelIndex, vec_t* position, int flags, float flScale )
{
	hull_t* phull = NULL;
	mnode_t* pnodes = NULL;
	cl_entity_t* pent = NULL;

	// Check for custom decal
	if (g_bUsingInGameAdvertisements)
	{
		if (ptexture && !Q_stricmp(ptexture->name, "}lambda06"))
			flags |= FDECAL_CUSTOM;
	}

	VectorCopy(position, gDecalPos);	// Pass position in global

	pent = &cl_entities[entity];

	// Check for model
	if (pent)
		gDecalModel = pent->model;
	else
		gDecalModel = NULL;

	if (!pent)
	{
		Con_DPrintf(const_cast<char*>("Decals must hit mod_brush!\n"));
		return;
	}

	// Try all ways to get the model
	if (gDecalModel == NULL)
	{
		if (modelIndex)
		{
			gDecalModel = CL_GetModelByIndex(modelIndex);

			if (gDecalModel == NULL)
			{
				if (sv.active)
					gDecalModel = sv.models[sv.edicts[entity].v.modelindex];
			}
		}
		else
		{
			if (sv.active)
				gDecalModel = sv.models[sv.edicts[entity].v.modelindex];
		}
	}

	if (gDecalModel == NULL || gDecalModel->type != mod_brush || ptexture == NULL)
	{
		Con_DPrintf(const_cast<char*>("Decals must hit mod_brush!\n"));
		return;
	}

	pnodes = gDecalModel->nodes;

	if (entity)
	{
		if (gDecalModel->firstmodelsurface)
		{
			phull = &gDecalModel->hulls[0];

			VectorSubtract(position, phull->clip_mins, gDecalPos);
			VectorSubtract(gDecalPos, pent->origin, gDecalPos);
			pnodes = pnodes + phull->firstclipnode;
		}

		if (pent->angles[0] || pent->angles[1] || pent->angles[2])
		{
			vec3_t forward, right, up;
			AngleVectors(pent->angles, forward, right, up);
			
			gDecalPos[0] = DotProduct(gDecalPos, forward);
			gDecalPos[1] = -DotProduct(gDecalPos, right);
			gDecalPos[2] = DotProduct(gDecalPos, up);
		}
	}

	// More state used by R_DecalNode()
	gDecalEntity = entity;
	gDecalTexture = ptexture;
	gDecalAppliedScale = -1.0;
	gDecalIndex = index;

	// Don't optimize custom decals
	if (!(flags & FDECAL_CUSTOM))
		flags |= FDECAL_CLIPTEST;

	gDecalFlags = flags;
	gDecalSize = ptexture->width >> 1;

	if (gDecalSize < (int)(ptexture->height >> 1))
		gDecalSize = ptexture->height >> 1;

	if (Q_strstr(ptexture->name, "}z_"))
	{
		R_DecalNode(pnodes, RandomFloat(0.1, 0.15));
	}
	else if (flScale == 1.0)
	{
		R_DecalNode(pnodes, -1.0);
	}
	else
	{
		R_DecalNode(pnodes, flScale);
	}
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
// This is called from cl_parse.cpp, cl_tent.cpp
void R_DecalShoot( int textureIndex, int entity, int modelIndex, vec_t* position, int flags )
{
	texture_t* ptexture;

	ptexture = Draw_DecalTexture(textureIndex);
	R_DecalShoot_(ptexture, textureIndex, entity, modelIndex, position, flags, 1.0);
}

void R_DecalShootScaled( int textureIndex, int entity, int modelIndex, vec_t* position, int flags, float flScale )
{
	texture_t* ptexture;

	ptexture = Draw_DecalTexture(textureIndex);
	R_DecalShoot_(ptexture, textureIndex, entity, modelIndex, position, flags, flScale);
}

void R_FireCustomDecal( int textureIndex, int entity, int modelIndex, vec_t* position, int flags, float scale )
{
	vec3_t temp;
	vec3_t forward, right, up;

	hull_t* phull = NULL;
	mnode_t* pnodes = NULL;

	texture_t* ptex = Draw_DecalTexture(textureIndex);

	cl_entity_t* pent = &cl_entities[entity];

	VectorCopy(position, gDecalPos);	// Pass position in global

	if (pent)
		gDecalModel = pent->model;
	else
		gDecalModel = NULL;

	if (!pent)
	{		
		Con_DPrintf(const_cast<char*>("Decals must hit mod_brush!\n"));
		return;
	}

	// Try all ways to get the model
	if (gDecalModel == NULL)
	{
		if (modelIndex)
		{
			gDecalModel = CL_GetModelByIndex(modelIndex);

			if (gDecalModel == NULL)
			{
				if (sv.active)
					gDecalModel = sv.models[sv.edicts[entity].v.modelindex];
			}
		}
		else
		{
			if (sv.active)
				gDecalModel = sv.models[sv.edicts[entity].v.modelindex];
		}
	}

	if (gDecalModel == NULL || gDecalModel->type != mod_brush || ptex == NULL)
	{
		Con_DPrintf(const_cast<char*>("Decals must hit mod_brush!\n"));
		return;
	}

	pnodes = gDecalModel->nodes;

	if (entity)
	{
		if (gDecalModel->firstmodelsurface)
		{
			phull = &gDecalModel->hulls[0];

			VectorSubtract(position, phull->clip_mins, temp);
			VectorSubtract(temp, pent->origin, gDecalPos);

			pnodes = pnodes + phull->firstclipnode;
		}

		if (pent->angles[0] || pent->angles[1] || pent->angles[2])
		{
			AngleVectors(pent->angles, forward, right, up);
			VectorCopy(gDecalPos, temp);

			gDecalPos[0] = DotProduct(temp, forward);
			gDecalPos[1] = -DotProduct(temp, right);
			gDecalPos[2] = DotProduct(temp, up);
		}
	}

	// More state used by R_DecalNode()
	gDecalTexture = ptex;
	gDecalIndex = textureIndex;

	// Don't optimize custom decals
	if (!(flags & FDECAL_CUSTOM))
		flags |= FDECAL_CLIPTEST;

	gDecalFlags = flags;
	gDecalEntity = entity;
	gDecalSize = ptex->width >> 1;

	if (gDecalSize < (int)(ptex->height >> 1))
		gDecalSize = ptex->height >> 1;

	R_DecalNode(pnodes, scale);
}

void R_CustomDecalShoot( texture_t* ptexture, int playernum, int entity, int modelIndex, vec_t* position, int flags )
{
	R_DecalShoot_(ptexture, ~playernum, entity, modelIndex, position, flags, 1.0);
}

// Check for intersecting decals on this surface
decal_t* R_DecalIntersect( msurface_t* psurf, int* pcount, float x, float y )
{
	decal_t* pDecal;
	decal_t* plast;

	qboolean bPermanent;

	texture_t* ptexture;

	plast = NULL;
	*pcount = 0;

	float lastArea = 2;
	float maxWidth = (float)(gDecalTexture->width) * 1.5;

	pDecal = psurf->pdecals;
	while (pDecal)
	{
		// Don't steal bigger decals and replace them with smaller decals
		// Don't steal permanent decals
		bPermanent = (pDecal->flags & (FDECAL_DONTSAVE | FDECAL_PERMANENT));
		if (!bPermanent)
		{
			ptexture = Draw_DecalTexture(pDecal->texture);

			if (maxWidth >= (float)ptexture->width)
			{
				float w = abs((int)((gDecalTexture->width >> 1) + psurf->texinfo->texture->width * x - (psurf->texinfo->texture->width * pDecal->dx + (ptexture->width >> 1))));
				float h = abs((int)((gDecalTexture->height >> 1) + psurf->texinfo->texture->height * y - (psurf->texinfo->texture->height * pDecal->dy + (ptexture->height >> 1))));

				// Now figure out the part of the projection that intersects pDecal's
				// clip box [0,0,1,1].
				int dx, dy;
				if (h <= w)
				{
					dx = w;
					dy = h;
				}
				else
				{
					dx = h;
					dy = w;
				}

				// Figure out how much of this intersects the (0,0) - (1,1) bbox
				float flArea = (float)dx + (float)dy * 0.5;
				if ((flArea * pDecal->scale) < 8)
				{
					*pcount += 1;

					if (!plast || flArea <= lastArea)
					{
						lastArea = flArea;
						plast = pDecal;
					}
				}
			}
		}

		if (pDecal == pDecal->pnext)
		{
			decal_t* p = psurf->pdecals;
			while (p)
			{
				ptexture = Draw_DecalTexture(p->texture);
				
				if (p == p->pnext)
					break;

				p = p->pnext;
			}
			break;
		}

		pDecal = pDecal->pnext;
	}

	return plast;
}

void R_InsertCustomDecal( decal_t* pdecal, msurface_t* surface )
{
	decal_t* pDecal, * pPrev = NULL;

	pDecal = surface->pdecals;

	while (pDecal)
	{
		if (pDecal->flags & FDECAL_DONTSAVE)
		{
			pdecal->pnext = pDecal;

			if (pPrev)
				pPrev->pnext = pdecal;
			else
				surface->pdecals = pdecal;

			return;
		}

		pPrev = pDecal;
		pDecal = pDecal->pnext;
	}

	pPrev->pnext = pdecal;
}


// Allocate and initialize a decal from the pool, on surface with offsets x, y
void R_DecalCreate( msurface_t* psurface, int textureIndex, float scale, float x, float y )
{
	decal_t* pdecal;
	int				count;

	decal_t* pold = R_DecalIntersect(psurface, &count, x, y);

	if (count < MAX_OVERLAP_DECALS)
		pold = NULL;

	pdecal = R_DecalAlloc(pold);

	pdecal->texture = textureIndex;
	pdecal->flags = gDecalFlags;
	pdecal->dx = x;
	pdecal->dy = y;
	pdecal->pnext = NULL;

	if (psurface->pdecals)
	{
		if (pdecal->flags & FDECAL_CUSTOM)
		{
			R_InsertCustomDecal(pdecal, psurface);
		}
		else
		{
			pold = psurface->pdecals;

			while (pold->pnext)
				pold = pold->pnext;

			pold->pnext = pdecal;
		}
	}
	else
	{
		psurface->pdecals = pdecal;
	}

	// Tag surface
	pdecal->psurface = psurface;

	// Set scaling
	pdecal->scale = scale;
	pdecal->entityIndex = gDecalEntity;
}

// clip edges
#define LEFT_EDGE			0
#define RIGHT_EDGE			1
#define TOP_EDGE			2
#define BOTTOM_EDGE			3

// Quick and dirty sutherland Hodgman clipper
// Clip polygon to decal in texture space
// JAY: This code is lame, change it later.  It does way too much work per frame
// It can be made to recursively call the clipping code and only copy the vertex list once
bool Inside( float* vert, int edge )
{
	switch (edge)
	{
		case LEFT_EDGE:
			if (vert[3] > 0.0f)
				return true;
			return false;

		case RIGHT_EDGE:
			if (vert[3] < 1.0f)
				return true;
			return false;

		case TOP_EDGE:
			if (vert[4] > 0.0f)
				return true;
			return false;

		case BOTTOM_EDGE:
			if (vert[4] < 1.0f)
				return true;
			return false;
	}

	return false;
}

void Intersect( float* one, float* two, int edge, float* out )
{
	float t;

	// t is the parameter of the line between one and two clipped to the edge
	// or the fraction of the clipped point between one & two
	// vert[3] is u
	// vert[4] is v
	// vert[0], vert[1], vert[2] is X, Y, Z

	if (edge < TOP_EDGE)
	{
		if (edge == LEFT_EDGE)
		{
			// left
			t = ((one[3] - 0) / (one[3] - two[3]));
			out[3] = 0;
		}
		else
		{
			// right
			t = ((one[3] - 1) / (one[3] - two[3]));
			out[3] = 1;
		}

		out[4] = one[4] + (two[4] - one[4]) * t;
	}
	else
	{
		if (edge == TOP_EDGE)
		{
			// top
			t = ((one[4] - 0) / (one[4] - two[4]));
			out[4] = 0;
		}
		else
		{
			// bottom
			t = ((one[4] - 1) / (one[4] - two[4]));
			out[4] = 1;
		}

		out[3] = one[3] + (two[3] - one[3]) * t;
	}

	out[0] = one[0] + (two[0] - one[0]) * t;
	out[1] = one[1] + (two[1] - one[1]) * t;
	out[2] = one[2] + (two[2] - one[2]) * t;
}

int SHClip( float* vert, int vertCount, float* out, int edge )
{
	int	j, outCount;
	float* s, * p;

	outCount = 0;

	s = &vert[(vertCount - 1) * VERTEXSIZE];

	for (j = 0; j < vertCount; j++)
	{
		p = &vert[j * VERTEXSIZE];

		if (Inside(p, edge))
		{
			if (Inside(s, edge))
			{
				// Add a vertex and advance out to next vertex
				Q_memcpy(out, p, sizeof(float) * VERTEXSIZE);
				outCount++;
				out += VERTEXSIZE;
			}
			else
			{
				Intersect(s, p, edge, out);
				out += VERTEXSIZE;
				outCount++;
				Q_memcpy(out, p, sizeof(float) * VERTEXSIZE);
				outCount++;
				out += VERTEXSIZE;
			}
		}
		else
		{
			if (Inside(s, edge))
			{
				Intersect(p, s, edge, out);
				out += VERTEXSIZE;
				outCount++;
			}
		}

		s = p;
	}

	return outCount;
}

#define MAX_DECALCLIPVERT		32

static float vert[MAX_DECALCLIPVERT][VERTEXSIZE];
static float outvert[MAX_DECALCLIPVERT][VERTEXSIZE];

// Generate lighting coordinates at each vertex for decal vertices v[] on surface psurf
void R_DecalVertsLight( float* v, msurface_t* psurf, int vertCount )
{
	int	j;
	float s, t;

	mtexinfo_t* tex = psurf->texinfo;

	for (j = 0; j < vertCount; j++, v += VERTEXSIZE)
	{
		// lightmap texture coordinates
		s = DotProduct(v, tex->vecs[0]) + tex->vecs[0][3] - psurf->texturemins[0];
		s += (psurf->light_s * 16) + 8.0f;

		t = DotProduct(v, tex->vecs[1]) + tex->vecs[1][3] - psurf->texturemins[1];
		t += (psurf->light_t * 16) + 8.0f;

		v[5] = s / 2048.f;
		v[6] = t / 2048.f;
	}
}

// Generate clipped vertex list for decal pdecal projected onto polygon psurf
float* R_DecalVertsClip( float* poutVerts, decal_t* pdecal, msurface_t* psurf, texture_t* ptexture, int* pvertCount )
{
	float* v;
	float scalex, scaley;
	int j, outCount;

	scalex = (psurf->texinfo->texture->width * pdecal->scale) / (float)ptexture->width;
	scaley = (psurf->texinfo->texture->height * pdecal->scale) / (float)ptexture->height;

	if (poutVerts == NULL)
		poutVerts = (float*)&vert[0];

	v = psurf->polys->verts[0];

	for (j = 0; j < psurf->polys->numverts; j++, v += VERTEXSIZE)
	{
		VectorCopy(v, vert[j]);
		vert[j][3] = (v[3] - pdecal->dx) * scalex;
		vert[j][4] = (v[4] - pdecal->dy) * scaley;

		if (pdecal->flags & FDECAL_HFLIP)
			vert[j][3] = 1 - vert[j][3];

		if (pdecal->flags & FDECAL_VFLIP)
			vert[j][4] = 1 - vert[j][4];
	}

	// Clip the polygon to the decal texture space
	outCount = SHClip((float*)&vert[0], psurf->polys->numverts, (float*)&outvert[0], 0);
	outCount = SHClip((float*)&outvert[0], outCount, (float*)&vert[0], 1);
	outCount = SHClip((float*)&vert[0], outCount, (float*)&outvert[0], 2);
	outCount = SHClip((float*)&outvert[0], outCount, poutVerts, 3);

	if (outCount)
	{
		if (pdecal->flags & FDECAL_CLIPTEST)
		{
			pdecal->flags &= ~FDECAL_CLIPTEST;	// We're doing the test

			// If there are exactly 4 verts and they are all 0,1 tex coords, then we've got an unclipped decal
			// A more precise test would be to calculate the texture area and make sure it's one, but this
			// should work as well.
			if (outCount == 4)
			{
				bool clipped = false;
				float s, t;

				v = poutVerts;
				for (j = 0; j < outCount && !clipped; j++, v += VERTEXSIZE)
				{
					s = v[3];
					t = v[4];

					if ((s != 0.0 && s != 1.0) || (t != 0.0 && t != 1.0))
						clipped = true;
				}

				// We didn't need to clip this decal, it's a quad covering the full texture space, optimize
				// subsequent frames.
				if (!clipped)
					pdecal->flags |= FDECAL_NOCLIP;
			}
		}
	}

	*pvertCount = outCount;
	return poutVerts;
}

float* R_DecalVertsNoclip( decal_t* pdecal, msurface_t* psurf, texture_t* ptexture, const bool bMultitexture )
{
	int	decalIndex;
	int	outCount;

	float* vlist = NULL;
	decalcache_t* pCache = NULL;

	decalIndex = R_DecalIndex(pdecal);
	pCache = R_DecalCacheSlot(decalIndex);

	// Is the decal cached?
	if (pCache->decalIndex == decalIndex)
		return (float*)&pCache->decalVert[0];

	pCache->decalIndex = decalIndex;

	vlist = pCache->decalVert[0];

	// Use the old code for now, and just cache them
	vlist = R_DecalVertsClip(vlist, pdecal, psurf, ptexture, &outCount);

	if (bMultitexture)
		R_DecalVertsLight(vlist, psurf, 4);

	return vlist;
}

void R_DecalPoly( float* v, texture_t* ptexture, msurface_t* psurf, int vertCount )
{
	int i;

	GL_Bind(ptexture->gl_texturenum);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglBegin(GL_POLYGON);

	for (i = 0; i < vertCount; i++)
	{
		qglTexCoord2f(v[3], v[4]);
		qglVertex3fv(v);
		v += VERTEXSIZE;
	}

	qglEnd();
}

void R_DecalMPoly( float* v, texture_t* ptexture, msurface_t* psurf, int vertCount )
{
	int i;

	GL_SelectTexture(TEXTURE0_SGIS);
	GL_Bind(ptexture->gl_texturenum);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GL_EnableMultitexture();
	GL_Bind(lightmap_textures[psurf->lightmaptexturenum]);

	qglBegin(GL_POLYGON);

	for (i = 0; i < vertCount; i++)
	{
		qglMTexCoord2fSGIS(TEXTURE0_SGIS, v[3], v[4]);
		qglMTexCoord2fSGIS(TEXTURE1_SGIS, v[5], v[6]);
		qglVertex3fv(v);
		v += VERTEXSIZE;
	}

	qglEnd();
}

// Renders all decals
void R_DrawDecals( const bool bMultitexture )
{
	int		i, outCount;
	float* v = NULL;

	decal_t* pDecal = NULL; // decal list

	texture_t* ptex = NULL;
	msurface_t* psurf = NULL;

	if (gDecalSurfCount == 0)
		return;

	qglEnable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask(GL_FALSE);

	if (gl_polyoffset.value != 0.0)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);

		if (gl_ztrick.value == 0.0 || gldepthmin < 0.5)
			qglPolygonOffset(-1.0, -gl_polyoffset.value);
		else
			qglPolygonOffset(1.0, gl_polyoffset.value);
	}

	for (i = 0; i < gDecalSurfCount; i++)
	{
		psurf = gDecalSurfs[i];

		// Draw all decals
		for (pDecal = psurf->pdecals; pDecal; pDecal = pDecal->pnext)
		{
			ptex = Draw_DecalTexture(pDecal->texture);

			if (!(pDecal->flags & FDECAL_NOCLIP))
			{
				v = R_DecalVertsClip(NULL, pDecal, psurf, ptex, &outCount);

				if (outCount)
				{
					if (bMultitexture)
						R_DecalVertsLight(v, psurf, outCount);
				}
			}

			if (pDecal->flags & FDECAL_NOCLIP)
			{
				v = R_DecalVertsNoclip(pDecal, psurf, ptex, bMultitexture);
				outCount = 4;
			}
			else if (outCount == 0)
			{
				continue;
			}

			if (bMultitexture)
				R_DecalMPoly(v, ptex, psurf, outCount);
			else
				R_DecalPoly(v, ptex, psurf, outCount);
		}
	}

	if (gl_polyoffset.value != 0.0)
		qglDisable(GL_POLYGON_OFFSET_FILL);

	qglDisable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
	qglDepthMask(GL_TRUE);

	gDecalSurfCount = 0;
}