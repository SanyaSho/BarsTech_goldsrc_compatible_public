#include "quakedef.h"
#include "d_local.h"
#include "cl_main.h"
#include "pr_cmds.h"
#include "r_trans.h"
#include "r_local.h"
#include "d_local.h"
#include "render.h"
#include "color.h"
#include "draw.h"

static int	miplevel;

float		scale_for_mip;
int			screenwidth;
int			ubasestep, errorterm, erroradjustup, erroradjustdown;
int			vstartscan;

// FIXME: should go away
extern void			R_RotateBmodel(void);
extern void			R_TransformFrustum(void);

vec3_t		transformed_modelorg;

int LeafId(msurface_t* psurface)
{
	int leafs = cl.worldmodel->numleafs;


	for (int i = 0; i < leafs; i++)
	{
		if (cl.worldmodel->leafs[i].nummarksurfaces > 0)
		{
			int j = 0;
			msurface_t** surflist = cl.worldmodel->leafs[i].firstmarksurface;
			while (psurface != *surflist++)
			{
				if (++j > cl.worldmodel->leafs[i].nummarksurfaces)
					break;
			}

			if (psurface == *surflist)
				return i;
		}
	}


	int nodes = cl.worldmodel->numnodes;
	int surfidx = psurface - cl.worldmodel->surfaces;
	int firstsurf;


	for (int i = 0; i < nodes; i++)
	{
		if (surfidx >= cl.worldmodel->nodes[i].firstsurface)
		{
			firstsurf = cl.worldmodel->nodes[i].firstsurface;
			if (surfidx < firstsurf + cl.worldmodel->nodes[i].numsurfaces)
				return leafs + i;
		}
	}


	return 1;
}

/*
=============
D_MipLevelForScale
=============
*/
int D_MipLevelForScale(float scale)
{
	int		lmiplevel;

	if (scale >= d_scalemip[0])
		lmiplevel = 0;
	else if (scale >= d_scalemip[1])
		lmiplevel = 1;
	else if (scale >= d_scalemip[2])
		lmiplevel = 2;
	else
		lmiplevel = 3;

	if (lmiplevel < d_minmip)
		lmiplevel = d_minmip;

	return lmiplevel;
}


/*
==============
D_DrawSolidSurface
==============
*/

// FIXME: clean this up

void D_DrawSolidSurface(surf_t *surf, unsigned short color)
{
	espan_t	*span;
	word	*pdest;
	int		u, u2, pix;

	pix = color | color << 16;
	for (span = surf->spans; span; span = span->pnext)
	{
		pdest = (word *)(d_viewbuffer + screenwidth*span->v);
		u = span->u;
		u2 = span->u + span->count - 1;
		((word *)pdest)[u] = pix;

		if (u2 - u < 8)
		{
			for (u++; u <= u2; u++)
				((word *)pdest)[u] = pix;
		}
		else
		{
			for (u++; u & 1; u++)
				((word *)pdest)[u] = pix;

			u2 -= 2;
			for (; u <= u2; u += 2)
				*(int *)((word *)pdest + u) = pix;
			u2 += 2;
			for (; u <= u2; u++)
				((word *)pdest)[u] = pix;
		}
	}
}

/*
==============
D_CalcGradients
==============
*/
void D_CalcGradients(msurface_t* pface)
{
	mplane_t* pplane;
	float		mipscale;
	vec3_t		p_temp1;
	vec3_t		p_saxis, p_taxis;
	float		t;

	pplane = pface->plane;

	mipscale = 1.0 / (float)(1 << miplevel);

	TransformVector(pface->texinfo->vecs[0], p_saxis);
	TransformVector(pface->texinfo->vecs[1], p_taxis);

	t = xscaleinv * mipscale;
	d_sdivzstepu = p_saxis[0] * t;
	d_tdivzstepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	d_sdivzstepv = -p_saxis[1] * t;
	d_tdivzstepv = -p_taxis[1] * t;

	d_sdivzorigin = p_saxis[2] * mipscale - xcenter * d_sdivzstepu -
		ycenter * d_sdivzstepv;
	d_tdivzorigin = p_taxis[2] * mipscale - xcenter * d_tdivzstepu -
		ycenter * d_tdivzstepv;

	VectorScale(transformed_modelorg, mipscale, p_temp1);

	t = MAX_TEXTURE_PIXELS * mipscale;
	sadjust = ((fixed16_t)(DotProduct(p_temp1, p_saxis) * MAX_TEXTURE_PIXELS + 0.5)) -
		((pface->texturemins[0] << 16) >> miplevel)
		+ pface->texinfo->vecs[0][3] * t;
	tadjust = ((fixed16_t)(DotProduct(p_temp1, p_taxis) * MAX_TEXTURE_PIXELS + 0.5)) -
		((pface->texturemins[1] << 16) >> miplevel)
		+ pface->texinfo->vecs[1][3] * t;

	//
	// -1 (-epsilon) so we never wander off the edge of the texture
	//
	bbextents = ((pface->extents[0] << 16) >> miplevel) - 1;
	bbextentt = ((pface->extents[1] << 16) >> miplevel) - 1;
}



/*
==============
D_DrawSurfaces
==============
*/
void D_DrawSurfaces(void)
{
	surf_t			*s;
	msurface_t		*pface;
	surfcache_t		*pcurrentcache;
	vec3_t			world_transformed_modelorg;
	vec3_t			local_modelorg;

	static word flatcolors[1024];

	currententity = cl_entities;
	TransformVector(modelorg, transformed_modelorg);
	VectorCopy(transformed_modelorg, world_transformed_modelorg);

	// TODO: could preset a lot of this at mode set time
	if (r_drawflat.value)
	{
		// почему зелёный?
		if (!flatcolors[1])
		{
			PackedColorVec* pflatcolors = (PackedColorVec*)flatcolors;
			for (int i = 0; i < 256; i++)
			{
				do
				{
					pflatcolors[i].r = RandomLong(0, 255);
					pflatcolors[i].g = RandomLong(0, 255);
					pflatcolors[i].b = RandomLong(0, 255);
				} while (pflatcolors[i].r + pflatcolors[i].g + pflatcolors[i].b < 256);

				pflatcolors[0] = { 0, 255, 255, 0 };
				pflatcolors[1] = { 255, 255, 255, 0 };
			}
		}

		for (s = &surfaces[1]; s<surface_p; s++)
		{
			if (!s->spans || (r_intentities && (s->flags & SURF_DRAWBACKGROUND) != 0))
				continue;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;

			if (s->flags & SURF_DRAWBACKGROUND)
			{
				D_DrawSolidSurface(s, (uintptr_t)s->data & 0xFF);
			}
			else
			{
				if (r_drawflat.value != 2)
				{
					D_DrawSolidSurface(s, hlRGB(flatcolors, (((uintptr_t)s->data) >> 4) % 0xff));
				}
				else if (!s->insubmodel)
				{
					int Leaf = LeafId((msurface_t*)s->data);

					if (Leaf > 2)
					{
						// ограничение до максимально возможного индекса 8-битной палитры
						Leaf %= 0xff;
						// предотвращение использования зарезервированных 0 и 1 цветов
						if (Leaf < 2)
							Leaf += 2;
					}

					D_DrawSolidSurface(s, hlRGB(flatcolors, Leaf));
				}
				else
				{
					D_DrawSolidSurface(s, hlRGB(flatcolors, 0));
				}
			}

			D_DrawZSpans(s->spans);
		}
	}
	else
	{
		for (s = &surfaces[1]; s<surface_p; s++)
		{
			if (!s->spans)
				continue;

			r_drawnpolycount++;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;

			if (s->flags & SURF_DRAWSKY)
			{
				currententity = s->entity;

				if (currententity->index > 0)
					continue;
				

				VectorSubtract(r_origin, currententity->origin, local_modelorg);
				TransformVector(local_modelorg, transformed_modelorg);
				miplevel = 0;
				pface = (msurface_t*)s->data;
				cachewidth = pface->extents[0];
				cacheblock = (pixel_t*)pface->samples;
				D_CalcGradients(pface);
				d_drawspans(s->spans);
				d_zistepu = 0;
				d_zistepv = 0;
				d_ziorigin = -0.9;
				D_DrawZSpans(s->spans);
			}
			else if (s->flags & SURF_DRAWBACKGROUND)
			{
				if (r_intentities)
					continue;

				// set up a gradient for the background surface that places it
				// effectively at infinity distance from the viewpoint
				d_zistepu = 0;
				d_zistepv = 0;
				d_ziorigin = -0.9;

				D_DrawSolidSurface(s, 0);
				D_DrawZSpans(s->spans);
			}
			else if (s->flags & SURF_DRAWTILED)
			{
				pface = (msurface_t*)s->data;

				if (s->insubmodel)
				{
					// FIXME: we don't want to do all this for every polygon!
					// TODO: store once at start of frame
					currententity = s->entity;	//FIXME: make this passed in to
					// R_RotateBmodel ()
					VectorSubtract(r_origin, currententity->origin,
						local_modelorg);
					TransformVector(local_modelorg, transformed_modelorg);

					R_RotateBmodel();	// FIXME: don't mess with the frustum,
					// make entity passed in
				}

				if (s->flags & SURF_DRAWTURB)
				{
					texture_t* turbtex = pface->texinfo->texture;
					miplevel = 0;
					cacheblock = (byte*)turbtex + turbtex->offsets[0];
					r_palette = (word*)((byte*)turbtex + turbtex->paloffset);
					D_CalcGradients(pface);

					WaterTextureUpdate(r_palette, 0.1f, turbtex);
				}
				else
				{
					texture_t* tiletex;
					int mipwidth, mipheight, mipscale, sadjinc, rfxfrac;
					miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip
						* pface->texinfo->mipadjust);

					// FIXME: make this passed in to D_CacheSurface
					pcurrentcache = (surfcache_t*)D_CacheSurface(pface, miplevel);

					cacheblock = (pixel_t*)pcurrentcache->data;
					cachewidth = pcurrentcache->width;

					D_CalcGradients(pface);

					tiletex = pface->texinfo->texture;

					mipwidth = tiletex->width >> miplevel;
					mipheight = tiletex->height >> miplevel;
					
					TilingSetup(mipwidth - 1, mipheight - 1, Q_log2(mipwidth));
					rfxfrac = (float)(currententity->curstate.rendercolor.b + currententity->curstate.rendercolor.g << 8) * 0.625;
					if (!currententity->curstate.rendercolor.r)
						rfxfrac = -rfxfrac;

					mipscale = 1 << miplevel;
					sadjinc = (float)1.0f / (float)mipscale * rfxfrac * cl.time * MAX_TEXTURE_PIXELS;
					sadjust += sadjinc;
				}

				if (r_intentities)
				{
					D_DrawTiled8Trans(s->spans);
				}
				else
				{
					D_DrawTiled8(s->spans);
					D_DrawZSpans(s->spans);
				}

				if (s->insubmodel)
				{
					//
					// restore the old drawing state
					// FIXME: we don't want to do this every time!
					// TODO: speed up
					//
					currententity = cl_entities;
					VectorCopy(world_transformed_modelorg,
						transformed_modelorg);
					VectorCopy(base_vpn, vpn);
					VectorCopy(base_vup, vup);
					VectorCopy(base_vright, vright);
					VectorCopy(base_modelorg, modelorg);
					R_TransformFrustum();
				}
			}
			else
			{
				if (s->insubmodel)
				{
					// FIXME: we don't want to do all this for every polygon!
					// TODO: store once at start of frame
					currententity = s->entity;	//FIXME: make this passed in to
					// R_RotateBmodel ()
					VectorSubtract(r_origin, currententity->origin, local_modelorg);
					TransformVector(local_modelorg, transformed_modelorg);

					R_RotateBmodel();	// FIXME: don't mess with the frustum,
					// make entity passed in
				}

				pface = (msurface_t*)s->data;
				miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip
					* pface->texinfo->mipadjust);

				// FIXME: make this passed in to D_CacheSurface
				pcurrentcache = D_CacheSurface(pface, miplevel);

				cacheblock = (pixel_t *)pcurrentcache->data;
				cachewidth = pcurrentcache->width;

				D_CalcGradients(pface);

				(*d_drawspans) (s->spans);

				if (!r_intentities)
					D_DrawZSpans(s->spans);

				if (s->insubmodel)
				{
					//
					// restore the old drawing state
					// FIXME: we don't want to do this every time!
					// TODO: speed up
					//
					VectorCopy(world_transformed_modelorg,
						transformed_modelorg);
					VectorCopy(base_vpn, vpn);
					VectorCopy(base_vup, vup);
					VectorCopy(base_vright, vright);
					VectorCopy(base_modelorg, modelorg);
					R_TransformFrustum();
					currententity = cl_entities;
				}
			}
		}
	}
}


/*
==============
D_DrawPoly

==============
*/
void D_DrawPoly(void)
{
	// this driver takes spans, not polygons
}
