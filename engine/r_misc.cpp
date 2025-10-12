#include "quakedef.h"
#include "r_local.h"
#include "render.h"
#include "screen.h"
#include "client.h"
#include "server.h"
#include "d_local.h"
#include "view.h"
#include "host.h"

int filterMode;

/*
===============
R_CheckVariables
===============
*/
void R_CheckVariables(void)
{
	static float oldFilterMode;
	static float oldFilterColorRed;
	static float oldFilterColorGreen;
	static float oldFilterColorBlue;
	static float oldFilterBrightness;
	static float oldbright, oldmap, oldstyle;

	if (r_fullbright.value != oldbright || r_lightmap.value != oldmap || r_lightstyle.value != oldstyle)
	{
		oldbright = r_fullbright.value;
		oldmap = r_lightmap.value;
		oldstyle = r_lightstyle.value;
		D_FlushCaches();
	}

	if (filterMode != oldFilterMode
		|| filterColorRed != oldFilterColorRed
		|| filterColorGreen != oldFilterColorGreen
		|| filterColorBlue != oldFilterColorBlue
		|| filterBrightness != oldFilterBrightness)
	{
		R_FilterSkyBox();
		oldFilterMode = filterMode;
		oldFilterColorRed = filterColorRed;
		oldFilterColorGreen = filterColorGreen;
		oldFilterColorBlue = filterColorBlue;
		oldFilterBrightness = filterBrightness;
		D_FlushCaches();	// so all lighting changes
	}
}


/*
============
Show

Debugging use
============
*/
void Show(void)
{
	VID_FlipScreen();
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f(void)
{
	int			i;
	float		start, stop, time;
	int			startangle;
	vrect_t		vr;

	if (cl.worldmodel == NULL)
	{
		Con_Printf(const_cast<char*>("No map loaded\n"));
		return;
	}

	if (sv_cheats.value == 0)
	{
		Con_Printf(const_cast<char*>("sv_cheats not enabled\n"));
		return;
	}

	startangle = r_refdef.viewangles[1];

	start = Sys_FloatTime();
	for (i = 0; i < 128; i++)
	{
		r_refdef.viewangles[1] = i / 128.0 * 360.0;

		R_RenderView();
		VID_FlipScreen();
	}
	stop = Sys_FloatTime();
	time = stop - start;
	Con_Printf(const_cast<char*>("%f seconds (%f fps)\n"), time, 128.0 / time);

	r_refdef.viewangles[1] = startangle;
}

/*
===================
R_TransformFrustum
===================
*/
void R_TransformFrustum(void)
{
	int		i;
	vec3_t	v, v2;

	for (i = 0; i < 4; i++)
	{
		v[0] = screenedge[i].normal[2];
		v[1] = -screenedge[i].normal[0];
		v[2] = screenedge[i].normal[1];

		v2[0] = v[1] * vright[0] + v[2] * vup[0] + v[0] * vpn[0];
		v2[1] = v[1] * vright[1] + v[2] * vup[1] + v[0] * vpn[1];
		v2[2] = v[1] * vright[2] + v[2] * vup[2] + v[0] * vpn[2];

		VectorCopy(v2, view_clipplanes[i].normal);

		view_clipplanes[i].dist = DotProduct(modelorg, v2);
	}
}

//#if !id386

/*
================
TransformVector
================
*/
void TransformVector(vec3_t in, vec3_t out)
{
	out[0] = DotProduct(in, vright);
	out[1] = DotProduct(in, vup);
	out[2] = DotProduct(in, vpn);
}

//#endif

/*
================
R_LineGraph

Only called by R_DisplayTime
================
*/
void R_LineGraph(int x, int y, int h)
{
	int		i;
	byte	*dest;
	int		s;

	// FIXME: should be disabled on no-rgba adapters, or should be in the driver

	x += r_refdef.vrect.x;
	y += r_refdef.vrect.y;

	dest = (byte*)(((word*)vid.buffer) + (vid.rowbytes / 2)*y + x);

	s = r_graphheight.value;

	if (h>s)
		h = s;

	for (i = 0; i<h; i++, dest -= vid.rowbytes * 2)
	{
		((word*)dest)[0] = 0xffff;
		*((word*)(dest - vid.rowbytes)) = 0x30;
	}
	for (; i<s; i++, dest -= vid.rowbytes * 2)
	{
		((word*)dest)[0] = 0xffff;
		*((word*)(dest - vid.rowbytes)) = 0x30;
	}
}

/*
==============
R_TimeGraph

Performance monitoring tool
==============
*/
#define	MAX_TIMINGS		100
void R_TimeGraph(void)
{
	static	int		timex;
	int		a;
	static float	r_time2;
	static byte	r_timings[MAX_TIMINGS];
	int		x;

	a = (r_time1 - r_time2) / 0.01;

	r_timings[timex] = a;
	a = timex;

	if (r_refdef.vrect.width <= MAX_TIMINGS)
		x = r_refdef.vrect.width - 1;
	else
		x = r_refdef.vrect.width -
		(r_refdef.vrect.width - MAX_TIMINGS) / 2;
	do
	{
		R_LineGraph(x, r_refdef.vrect.height - 2, r_timings[a]);
		if (x == 0)
			break;		// screen too small to hold entire thing
		x--;
		a--;
		if (a == -1)
			a = MAX_TIMINGS - 1;
	} while (a != timex);

	timex = (timex + 1) % MAX_TIMINGS;
}

/*
=============
R_PrintTimes
=============
*/
void R_PrintTimes(void)
{
	static float	ms_rspeeds;
	float		r_time2;
	float		ms;

	r_time2 = Sys_FloatTime();

	ms = 1000.0 * (r_time2 - r_time1);

	if (r_speeds.value > 1.0)
	{
		ms_rspeeds = (1.0 - 1.0 / r_speeds.value) * ms_rspeeds + ms * (1.0 / r_speeds.value);
		ms = ms_rspeeds;
	}

	Con_DPrintf(const_cast<char*>("%5.1f ms %3i/%3i/%3i poly %3i surf\n"),
		ms, c_faceclip, r_polycount, r_drawnpolycount, c_surf);

	c_surf = 0;
}

/*
=============
R_PrintDSpeeds
=============
*/
void R_PrintDSpeeds(void)
{
	float	ms, dp_time, r_time2, rw_time, db_time, se_time, de_time, dv_time;

	r_time2 = Sys_FloatTime();

	dp_time = (dp_time2 - dp_time1) * 1000;
	rw_time = (rw_time2 - rw_time1) * 1000;
	db_time = (db_time2 - db_time1) * 1000;
	se_time = (se_time2 - se_time1) * 1000;
	de_time = (de_time2 - de_time1) * 1000;
	dv_time = (dv_time2 - dv_time1) * 1000;
	ms = (r_time2 - r_time1) * 1000;

	Con_DPrintf(const_cast<char*>("%3i %4.1fp %3iw %4.1fb %3is %4.1fe %4.1fv\n"),
		(int)ms, dp_time, (int)rw_time, db_time, (int)se_time, de_time,
		dv_time);
}

void R_ScreenLuminance(void)
{
	word z;
	int			i, j;
	int			zmin, zmax;
	float		lumcolors[32];

	for (i = 0; i < 32; i++)
		lumcolors[i] = pow((float)i / 31.0f, v_gamma.value / 1.8f) * 256.0f;

	if (vid.buffer == NULL)
		return;

	zmin = 999999;
	zmax = 0;

	for (i = 0; i < (int)vid.height; i++)
	{
		for (j = 0; (j & MAXWORD) < (int)vid.width; j++)
		{
			if (r_pixbytes == 1)
			{
				z = d_pzbuffer[i * vid.width + j];

				if (z < ZISCALE)
				{
					if (zmin > z)
						zmin = z;
					if (zmax < z)
						zmax = z;
				}

				((word*)vid.buffer)[i * vid.width + j] = z;
			}
			else
			{
				int z32 = d_pzbuffer32[i * vid.width + j];

				if (z32 < ZISCALE)
				{
					if (zmin > z32)
						zmin = z32;
					if (zmax < z32)
						zmax = z32;
				}

				((unsigned int*)vid.buffer)[i * vid.width + j] = z32;
			}
		}
	}

	Con_DPrintf(const_cast<char*>("min %d max %d\n"), zmin, zmax);
}


/*
=============
R_PrintAliasStats
=============
*/
void R_PrintAliasStats(void)
{
	Con_Printf(const_cast<char*>("%3i polygon model drawn\n"), r_amodels_drawn);
}


/*
===============
R_SetUpFrustumIndexes
===============
*/
void R_SetUpFrustumIndexes(void)
{
	int		i, j, * pindex;

	pindex = r_frustum_indexes;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 3; j++)
		{
			if (view_clipplanes[i].normal[j] < 0)
			{
				pindex[j] = j;
				pindex[j + 3] = j + 3;
			}
			else
			{
				pindex[j] = j + 3;
				pindex[j + 3] = j;
			}
		}

		// FIXME: do just once at start
		pfrustum_indexes[i] = pindex;
		pindex += 6;
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame(void)
{
	int				edgecount;
	vrect_t			vrect;
	float			w, h;
	int				alpha;

	R_ForceCVars(cl.maxclients > 1);

	if (r_numsurfs.value)
	{
		if ((surface_p - surfaces) > r_maxsurfsseen)
			r_maxsurfsseen = surface_p - surfaces;

		Con_Printf(const_cast<char*>("Used %d of %d surfs; %d max\n"), surface_p - surfaces,
			surf_max - surfaces, r_maxsurfsseen);
	}

	if (r_numedges.value)
	{
		edgecount = edge_p - r_edges;

		if (edgecount > r_maxedgesseen)
			r_maxedgesseen = edgecount;

		Con_Printf(const_cast<char*>("Used %d of %d edges; %d max\n"), edgecount,
			r_numallocatededges, r_maxedgesseen);
	}

	r_refdef.ambientlight.r = max(r_ambient_r.value, 0.f);
	r_refdef.ambientlight.g = max(r_ambient_g.value, 0.f);
	r_refdef.ambientlight.b = max(r_ambient_b.value, 0.f);

	if (!Host_IsServerActive())
		r_draworder.value = 0;	// don't let cheaters look behind walls

	R_CheckVariables();

	R_AnimateLight();

	r_framecount++;

	numbtofpolys = 0;

	// build the transformation matrix for the given view angles
	VectorCopy(r_refdef.vieworg, modelorg);
	VectorCopy(r_refdef.vieworg, r_origin);

	AngleVectors(r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf(r_origin, cl.worldmodel);

	r_dowarpold = r_dowarp;
	r_dowarp = FALSE;

	if (r_waterwarp.value)
	{
		if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents != CONTENTS_TRANSLUCENT)
			r_dowarp = TRUE;

		if (cl.waterlevel > 2)
			r_dowarp = TRUE;
	}

	alpha = V_FadeAlpha();
	if (alpha)
	{
		r_dowarp = TRUE;
		D_SetScreenFade(cl.sf.fader, cl.sf.fadeg, cl.sf.fadeb, alpha, (~cl.sf.fadeFlags & FFADE_MODULATE) ? TRUE : FALSE);
	}

	if ((r_dowarp != r_dowarpold) || r_viewchanged || lcd_x.value)
	{
		if (r_dowarp)
		{
			if ((vid.width <= vid.maxwarpwidth) &&
				(vid.height <= vid.maxwarpheight))
			{
				vrect.x = 0;
				vrect.y = 0;
				vrect.width = vid.width;
				vrect.height = vid.height;

				R_ViewChanged(&vrect, sb_lines, vid.aspect);
			}
			else
			{
				w = vid.width;
				h = vid.height;

				if (w > vid.maxwarpwidth)
				{
					h *= (float)vid.maxwarpwidth / w;
					w = vid.maxwarpwidth;
				}

				if (h > vid.maxwarpheight)
				{
					h = vid.maxwarpheight;
					w *= (float)vid.maxwarpheight / h;
				}

				vrect.x = 0;
				vrect.y = 0;
				vrect.width = (int)w;
				vrect.height = (int)h;

				R_ViewChanged(&vrect,
					(int)((float)sb_lines * (h / (float)vid.height)),
					vid.aspect * (h / w) *
					((float)vid.width / (float)vid.height));
			}
		}
		else
		{
			vrect.x = 0;
			vrect.y = 0;
			vrect.width = vid.width;
			vrect.height = vid.height;

			R_ViewChanged(&vrect, sb_lines, vid.aspect);
		}

		r_viewchanged = FALSE;
	}

	// start off with just the four screen edge clip planes
	R_TransformFrustum();

	// save base values
	VectorCopy(vpn, base_vpn);
	VectorCopy(vright, base_vright);
	VectorCopy(vup, base_vup);
	VectorCopy(modelorg, base_modelorg);

	R_SetUpFrustumIndexes();

	r_cache_thrash = FALSE;

	// clear frame counts
	c_faceclip = 0;
	d_spanpixcount = 0;
	r_polycount = 0;
	r_drawnpolycount = 0;
	r_wholepolycount = 0;
	r_amodels_drawn = 0;
	r_outofsurfaces = 0;
	r_outofedges = 0;

	D_SetupFrame();
}
