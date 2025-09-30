//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// gl_rmain.cpp

#include "quakedef.h"
#include "cl_spectator.h"
#include "gl_model.h"
#include "draw.h"
#include "sv_main.h"
#include "r_studio.h"
#include "r_part.h"
#include "r_triangle.h"
#include "r_trans.h"
#include "cl_tent.h"
#include "sound.h"
#include "shake.h"
#include "screen.h"
#include "r_studio.h"
#include "bitmap_win.h"
#include "gl_screen.h"
#include "chase.h"
#include "view.h"
#include "pm_shared/pm_movevars.h"
#include "gl_rmisc.h"

cl_entity_t r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
cl_entity_t* currententity;

int			r_visframecount;
int			r_framecount;

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture
int			currenttexture = -1;	// to avoid unnecessary texture sets
int			cnttextures[2] = { -1, -1 };     // cached

int			particletexture;	// little dot for particles
int			playertextures[16];	// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t*	mirror_plane;

alight_t	r_viewlighting;
vec3_t		r_plightvec; // light vector in model reference frame


//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];
float	gProjectionMatrix[16];
float	gWorldToScreen[16];
float	gScreenToWorld[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t*	r_viewleaf, * r_oldviewleaf;

texture_t*	r_notexture_mip;

int			d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves( void );
void R_CheckVariables( void );

extern cshift_t	cshift_water;
extern cvar_t snd_show;
extern cvar_t v_lambert;

/*-----------------------------------------------------------------*/

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F

void ProjectPointOnPlane( vec_t* dst, const vec_t* p, const vec_t* normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct(normal, normal);

	d = DotProduct(normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec_t* dst, const vec_t* src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for (pos = 0, i = 0; i < 3; i++)
	{
		if (fabs(src[i]) < minelem)
		{
			pos = i;
			minelem = fabs(src[i]);
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane(dst, tempvec, src);

	/*
	** normalize the result
	*/
	VectorNormalize(dst);
}


void RotatePointAroundVector( vec_t* dst, const vec_t* dir, const vec_t* point, float degrees )
{
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	int	i;
	vec3_t vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector(vr, dir);
	CrossProduct(vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	Q_memcpy(im, m, sizeof(im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	Q_memset(zrot, 0, sizeof(zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = cos(DEG2RAD(degrees));
	zrot[0][1] = sin(DEG2RAD(degrees));
	zrot[1][0] = -sin(DEG2RAD(degrees));
	zrot[1][1] = cos(DEG2RAD(degrees));

	R_ConcatRotations(m, zrot, tmpmat);
	R_ConcatRotations(tmpmat, im, rot);

	for (i = 0; i < 3; i++)
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
}

// JAY: Setup frustum
/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox( vec_t* mins, vec_t* maxs )
{
	int		i;

	for (i = 0; i < 4; i++)
	{
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return TRUE;
	}

	return FALSE;
}

void R_RotateForEntity( vec_t* origin, cl_entity_t* e )
{
	int i;
	vec3_t angles;
	vec3_t modelpos;

	VectorCopy(origin, modelpos);
	VectorCopy(e->angles, angles);

	if (e->curstate.movetype != MOVETYPE_NONE)
	{
		float f = 0.0;
		float d;
		if (e->curstate.animtime + 0.2 > cl.time && e->curstate.animtime != e->latched.prevanimtime)
			f = (cl.time - e->curstate.animtime) / (e->curstate.animtime - e->latched.prevanimtime);

		for (i = 0; i < 3; i++)
		{
			d = e->latched.prevorigin[i] - e->origin[i];
			modelpos[i] -= d * f;
		}

		if (f > 0.0f && f < 1.5)
		{
			f = 1.0 - f;

			for (i = 0; i < 3; i++)
			{
				d = e->latched.prevangles[i] - e->angles[i];
				if (d > 180.0)
					d -= 360.0;
				else if (d < -180.0)
					d += 360.0;

				angles[i] += d * f;
			}
		}
	}

	qglTranslatef(modelpos[0], modelpos[1], modelpos[2]);

	qglRotatef(angles[1], 0, 0, 1);
	qglRotatef(angles[0], 0, 1, 0);
	qglRotatef(angles[2], 1, 0, 0);
}

/*
=================
R_DrawSpriteModel

Generic sprite model renderer
=================
*/
void R_DrawSpriteModel( cl_entity_t* e )
{
	vec3_t			point;
	vec3_t			forward, right, up;
	mspriteframe_t* frame;
	float			scale;
	msprite_t* psprite;
	colorVec		color;

	psprite = (msprite_t*)e->model->cache.data;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame(psprite, e->curstate.frame);

	if (!frame)
	{
		Sys_Warning("R_DrawSpriteModel:  couldn't get sprite frame for %s\n", e->model->name);
		return;
	}

	if (e->curstate.scale > 0.0)
		scale = e->curstate.scale;
	else
		scale = 1.0;

	if (e->curstate.rendermode == kRenderNormal)
		r_blend = 1.0;

	R_SpriteColor(&color, e, r_blend * 255.0);

	if (gl_spriteblend.value || e->curstate.rendermode != kRenderNormal)
	{
		switch (e->curstate.rendermode)
		{
		case kRenderTransColor:
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
			qglColor4ub(color.r, color.g, color.b, r_blend * 255.0);
			qglEnable(GL_BLEND);
			break;

		case kRenderTransAdd:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglBlendFunc(1, 1);
			qglColor4ub(color.r, color.g, color.b, 255);
			qglDepthMask(GL_FALSE);
			qglEnable(GL_BLEND);
			break;

		case kRenderGlow:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglBlendFunc(1, 1);
			qglColor4ub(color.r, color.g, color.b, 255);
			qglDisable(GL_DEPTH_TEST);
			qglDepthMask(GL_FALSE);
			qglEnable(GL_BLEND);
			break;

		case kRenderTransAlpha:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglColor4ub(color.r, color.g, color.b, r_blend * 255.0);
			qglDepthMask(GL_FALSE);
			qglEnable(GL_BLEND);
			break;

		default:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglColor4ub(color.r, color.g, color.b, r_blend * 255.0);
			qglEnable(GL_BLEND);
			break;
		}
	}
	else
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglColor4ub(color.r, color.g, color.b, 255);
		qglDisable(GL_BLEND);
	}

	// Get orthonormal bases
	R_GetSpriteAxes(e, psprite->type, forward, right, up);

	GL_DisableMultitexture();

	GL_Bind(frame->gl_texturenum);

	qglEnable(GL_ALPHA_TEST);
	qglBegin(GL_QUADS);

	qglTexCoord2f(0, 1);

	VectorMA(r_entorigin, scale * frame->down, up, point);
	VectorMA(point, scale * frame->left, right, point);

	qglVertex3fv(point);
	qglTexCoord2f(0, 0);

	VectorMA(r_entorigin, scale * frame->up, up, point);
	VectorMA(point, scale * frame->left, right, point);

	qglVertex3fv(point);
	qglTexCoord2f(1, 0);

	VectorMA(r_entorigin, scale * frame->up, up, point);
	VectorMA(point, scale * frame->right, right, point);

	qglVertex3fv(point);
	qglTexCoord2f(1, 1);

	VectorMA(r_entorigin, scale * frame->down, up, point);
	VectorMA(point, scale * frame->right, right, point);

	qglVertex3fv(point);

	qglEnd();
	qglDisable(GL_ALPHA_TEST);

	qglDepthMask(GL_TRUE);

	if (e->curstate.rendermode != kRenderNormal || gl_spriteblend.value)
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglDisable(GL_BLEND);
		qglEnable(GL_DEPTH_TEST);
	}
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


float r_avertexnormals[NUMVERTEXNORMALS][3] = {
	#include "anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
{
	#include "anorm_dots.h"
};
float* shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame( aliashdr_t* paliashdr, int posenum )
{
	float 	l;
	trivertx_t* verts;
	int* order;
	int		count;

	lastposenum = posenum;

	verts = (trivertx_t*)((byte*)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int*)((byte*)paliashdr + paliashdr->commands);

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			qglBegin(GL_TRIANGLE_FAN);
		}
		else
			qglBegin(GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			qglTexCoord2f(((float*)order)[0], ((float*)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list
			l = shadedots[verts->lightnormalindex] * shadelight;
			qglColor3f(l, l, l);
			qglVertex3f(verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		qglEnd();
	}
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow( aliashdr_t* paliashdr, int posenum )
{
	trivertx_t* verts;
	int* order;
	vec3_t	point;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t*)((byte*)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int*)((byte*)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			qglBegin(GL_TRIANGLE_FAN);
		}
		else
			qglBegin(GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) qglTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
			//			height -= 0.001;
			qglVertex3fv(point);

			verts++;
		} while (--count);

		qglEnd();
	}
}



/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame( int frame, aliashdr_t* paliashdr )
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf(const_cast<char*>("R_AliasSetupFrame: no such frame %d\n"), frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame(paliashdr, pose);
}



/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel( cl_entity_t* e )
{
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t* clmodel;
	vec3_t		mins, maxs;
	aliashdr_t* paliashdr;
	float		an;

	clmodel = currententity->model;

	VectorAdd(currententity->origin, clmodel->mins, mins);
	VectorAdd(currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox(mins, maxs))
		return;

	VectorCopy(currententity->origin, r_entorigin);
	VectorSubtract(r_origin, r_entorigin, modelorg);

	// allways give the gun some light
	if (e == &cl.viewent && ambientlight < 24.0)
		ambientlight = shadelight = 24;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract(currententity->origin, cl_dlights[lnum].origin, dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0)
			{
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (!Q_strcmp(clmodel->name, "progs/flame2.mdl") || !Q_strcmp(clmodel->name, "progs/flame.mdl"))
		ambientlight = shadelight = 256;

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;

	an = e->angles[1] / 180 * M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t*)Mod_Extradata(currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	qglPushMatrix();
	R_RotateForEntity(e->origin, e);

	qglTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	qglScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	GL_Bind(paliashdr->gl_texturenum[currententity->curstate.skin]);

	qglShadeModel(GL_SMOOTH);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	R_SetupAliasFrame(currententity->curstate.frame, paliashdr);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qglShadeModel(GL_FLAT);

	if (gl_affinemodels.value)
		qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	qglPopMatrix();

	if (r_shadows.value)
	{
		qglPushMatrix();
		R_RotateForEntity(e->origin, e);
		qglDisable(GL_TEXTURE_2D);
		qglEnable(GL_BLEND);
		qglColor4f(0, 0, 0, 0.5);
		GL_DrawAliasShadow(paliashdr, lastposenum);
		qglEnable(GL_TEXTURE_2D);
		qglDisable(GL_BLEND);
		qglColor4f(1, 1, 1, 1);
		qglPopMatrix();
	}
}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList( void )
{
	int		i, j;

	if (!r_drawentities.value)
		return;

	if (cl_numvisedicts <= 0)
	{
		r_blend = 1.0;
		return;
	}

	// draw sprites seperately, because of alpha blending
	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity->curstate.rendermode)
		{
			AddTEntity(currententity);
			continue;
		}

		switch (currententity->model->type)
		{
			case mod_studio:
				// Drawing player model
				if (currententity->player)
				{
					pStudioAPI->StudioDrawPlayer(STUDIO_RENDER | STUDIO_EVENTS, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1]);
				}
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
				{
					pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
				}
				break;

			case mod_brush:
				R_DrawBrushModel(currententity);
				break;

			default:
				break;
		}
	}

	r_blend = 1.0;

	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity->curstate.rendermode != kRenderNormal)
			continue;

		switch (currententity->model->type)
		{
			case mod_sprite:
				if (currententity->curstate.body)
				{
					float* pAttachment;

					pAttachment = R_GetAttachmentPoint(currententity->curstate.skin, currententity->curstate.body);
					VectorCopy(pAttachment, r_entorigin);
				}
				else
				{
					VectorCopy(currententity->origin, r_entorigin);
				}

				R_DrawSpriteModel(currententity);
				break;

			default:
				break;
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel( void )
{
	float		lightvec[3];
	colorVec	c;
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	float		oldShadows;
	dlight_t* dl;

	lightvec[0] = -1.0;
	lightvec[1] = 0.0;
	lightvec[2] = 0.0;

	currententity = &cl.viewent;

	if (!r_drawviewmodel.value)
	{
		c = R_LightPoint(currententity->origin);
		cl.light_level = (c.r + c.g + c.b) / 3;
		return;
	}

	// Don't draw viewmodel if we are in third person mode
	if (ClientDLL_IsThirdPerson() || chase_active.value || envmap || !r_drawentities.value)
	{
		c = R_LightPoint(currententity->origin);
		cl.light_level = (c.r + c.g + c.b) / 3;
		return;
	}

	if (cl.stats[STAT_HEALTH] <= 0 || !currententity->model || cl.viewentity > cl.maxclients)
	{
		c = R_LightPoint(currententity->origin);
		cl.light_level = (c.r + c.g + c.b) / 3;
		return;
	}

	qglDepthRange(gldepthmin, (gldepthmax - gldepthmin) * 0.3 + gldepthmin);

	switch (currententity->model->type)
	{
		case mod_alias:
			c = R_LightPoint(currententity->origin);

			j = (c.r + c.g + c.b) / 3;

			if (j < 24)
				j = 24;		// allways give some light on gun

			r_viewlighting.ambientlight = j;
			r_viewlighting.shadelight = j;

			// add dynamic lights
			for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
			{
				dl = &cl_dlights[lnum];

				if (!dl->radius)
					continue;

				if (dl->die < cl.time)
					continue;

				VectorSubtract(currententity->origin, dl->origin, dist);

				add = dl->radius - Length(dist);

				if (add > 0)
					r_viewlighting.ambientlight += add;
			}

			if (r_viewlighting.ambientlight > 128)
				r_viewlighting.ambientlight = 128;

			if (r_viewlighting.shadelight > 192 - r_viewlighting.ambientlight)
				r_viewlighting.shadelight = 192 - r_viewlighting.ambientlight;

			r_viewlighting.plightvec = lightvec;
			R_DrawAliasModel(currententity);
			break;

		case mod_studio:
			if (cl.weaponstarttime == 0.0)
				cl.weaponstarttime = cl.time;

			currententity->curstate.frame = 0.0;
			currententity->curstate.framerate = 1.0;
			currententity->curstate.sequence = cl.weaponsequence;
			currententity->curstate.animtime = cl.weaponstarttime;

			currententity->curstate.colormap = MAKEWORD(cl.players[cl.playernum].topcolor, cl.players[cl.playernum].bottomcolor);

			c = R_LightPoint(currententity->origin);

			oldShadows = r_shadows.value;
			r_shadows.value = 0.0;

			cl.light_level = (c.r + c.g + c.b) / 3;

			pStudioAPI->StudioDrawModel(STUDIO_RENDER);

			r_shadows.value = oldShadows;
			break;

		case mod_brush:
			R_DrawBrushModel(currententity);
			break;
	}

	qglDepthRange(gldepthmin, gldepthmax);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
=============
R_PreDrawViewModel

=============
*/
void R_PreDrawViewModel( void )
{
	int			i;

	currententity = &cl.viewent;

	if (!r_drawviewmodel.value || ClientDLL_IsThirdPerson())
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	if (!currententity->model)
		return;

	if (cl.viewentity <= cl.maxclients && currententity->model->type == mod_studio)
	{
		if (cl.weaponstarttime == 0.0)
			cl.weaponstarttime = cl.time;

		currententity->curstate.frame = 0.0;

		currententity->curstate.framerate = 1.0;
		currententity->curstate.sequence = cl.weaponsequence;
		currententity->curstate.animtime = cl.weaponstarttime;

		// Initialize attachment positions
		for (i = 0; i < 4; i++)
		{
			VectorCopy(cl_entities[currententity->index].origin, currententity->attachment[i]);
		}

		// Begin model drawing via r_studio routines
		pStudioAPI->StudioDrawModel(STUDIO_EVENTS);
	}
}

void AllowFog( int allowed )
{
	static int isFogEnabled;

    if (allowed)
    {
        if (isFogEnabled == TRUE)
            qglEnable(GL_FOG);
    }
    else
    {
        isFogEnabled = qglIsEnabled(GL_FOG);

        if (isFogEnabled == TRUE)
            qglDisable(GL_FOG);
    }
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend( void )
{
	unsigned char	color[4];
	int		alpha;

	alpha = V_FadeAlpha();

	if (!alpha)
		return;

	GL_DisableMultitexture();

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_TEXTURE_2D);

	if (cl.sf.fadeFlags & FFADE_MODULATE)
	{
		qglBlendFunc(GL_FALSE, GL_SRC_COLOR);

		color[0] = (alpha * (cl.sf.fader - 255) - 511) >> 8;
		color[1] = (alpha * (cl.sf.fadeg - 255) - 511) >> 8;
		color[2] = (alpha * (cl.sf.fadeb - 255) - 511) >> 8;
		color[3] = 255;
	}
	else
	{
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		color[0] = cl.sf.fader;
		color[1] = cl.sf.fadeg;
		color[2] = cl.sf.fadeb;
		color[3] = alpha;
	}

	AllowFog(0);

	qglViewport(glx, gly, glwidth, glheight);

	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();

	qglLoadIdentity();

	qglOrtho(0.0, glwidth, glheight, 0.0, -99999.0, 99999.0);

	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();

	qglLoadIdentity();

	qglDisable(GL_CULL_FACE);

	qglColor4ubv(color);

	qglBegin(GL_QUADS);

	qglVertex2f(0, 0);
	qglVertex2f(0, glheight);
	qglVertex2f(glwidth, glheight);
	qglVertex2f(glwidth, 0);

	qglEnd();

	qglPopMatrix();
	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();

	qglEnable(GL_DEPTH_TEST);
	qglEnable(GL_CULL_FACE);
	qglEnable(GL_TEXTURE_2D);
	qglEnable(GL_ALPHA_TEST);

	AllowFog(1);
}

int SignbitsForPlane( mplane_t* out )
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


void R_SetFrustum( void )
{
	int		i;
	float	fovx;
	float	fovy;

	fovx = scr_fov_value;
	fovy = CalcFov(fovx, glwidth, glheight);

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - scr_fov_value / 2));
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - scr_fov_value / 2);
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - fovy / 2);
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - fovy / 2));

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

/*
=============
R_ForceCVars

Make sure the values of the graphics cvars are within acceptable
limits in multiplayer
=============
*/
void R_ForceCVars( qboolean mp )
{
	if (!mp)
		return;
    
	if (r_lightmap.value)
		Cvar_DirectSet(&r_lightmap, const_cast<char*>("0"));

	if (gl_clear.value)
		Cvar_DirectSet(&gl_clear, const_cast<char*>("0"));

	if (r_novis.value)
		Cvar_DirectSet(&r_novis, const_cast<char*>("0"));

	if (r_fullbright.value)
		Cvar_DirectSet(&r_fullbright, const_cast<char*>("0"));

	if (snd_show.value)
		Cvar_DirectSet(&snd_show, const_cast<char*>("0"));

	if (chase_active.value)
		Cvar_DirectSet(&chase_active, const_cast<char*>("0"));

	if (v_lambert.value != 1.5)
		Cvar_DirectSet(&v_lambert, const_cast<char*>("1.5"));

	if (gl_monolights.value)
	{
		Cvar_DirectSet(&gl_monolights, const_cast<char*>("0"));
		GL_BuildLightmaps();
	}

	if (gl_wireframe.value)
		Cvar_DirectSet(&gl_wireframe, const_cast<char*>("0"));

	if (r_dynamic.value != 1.0)
		Cvar_DirectSet(&r_dynamic, const_cast<char*>("1"));

	if (gl_alphamin.value != 0.25)
		Cvar_DirectSet(&gl_alphamin, const_cast<char*>("0.25"));

	if (gl_max_size.value < 255.0)
		Cvar_DirectSet(&gl_max_size, const_cast<char*>("256"));

	if (gl_polyoffset.value <= 0.0)
	{
		if (gl_polyoffset.value < -0.001)
			Cvar_DirectSet(&gl_polyoffset, const_cast<char*>("-0.001"));
	}
	else if (gl_polyoffset.value < 0.001)
	{
		Cvar_DirectSet(&gl_polyoffset, const_cast<char*>("0.001"));
	}
	else if (gl_polyoffset.value > 25.0)
	{
		Cvar_DirectSet(&gl_polyoffset, const_cast<char*>("25"));
	}

	if (sv_cheats.value == 0.0 && r_drawentities.value != 1.0)
		Cvar_DirectSet(&r_drawentities, const_cast<char*>("1.0"));

	if (v_lightgamma.value < 1.8)
	{
		Cvar_DirectSet(&v_lightgamma, const_cast<char*>("1.8"));
		GL_BuildLightmaps();
		D_FlushCaches();
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame( void )
{
	R_ForceCVars(cl.maxclients > 1);

	R_CheckVariables();

	R_AnimateLight();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy(r_refdef.vieworg, r_origin);

	AngleVectors(r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf(r_origin, cl.worldmodel);

	if (cl.waterlevel > 2 && !r_refdef.onlyClientDraws)
	{
		float	fogColor[4];
		fogColor[0] = (float)cshift_water.destcolor[0] / 255.0;
		fogColor[1] = (float)cshift_water.destcolor[1] / 255.0;
		fogColor[2] = (float)cshift_water.destcolor[2] / 255.0;
		fogColor[3] = 1.0;

		qglFogi(GL_FOG_MODE, GL_LINEAR);
		qglFogfv(GL_FOG_COLOR, fogColor);
		qglFogf(GL_FOG_START, GL_ZERO);

		qglFogf(GL_FOG_END, 1536 - 4 * cshift_water.percent);
		qglEnable(GL_FOG);
	}

	V_CalcBlend();

	r_cache_thrash = FALSE;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

void MYgluPerspective( GLdouble fovy, GLdouble aspect,
	GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	qglFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

/*
====================
CalcFov
====================
*/
float CalcFov( float fov_x, float width, float height )
{
	float	a;
	float	x;

	if (fov_x < 1 || fov_x > 179)
		fov_x = 90;	// error, set to 90

	x = width / tan(fov_x / 360 * M_PI);

	a = atan(height / x);

	a = a * 360 / M_PI;

	return a;
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL( void )
{
	float	screenaspect;
	float	yfov;
	int		i, j, k;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();

	x = r_refdef.vrect.x;
	x2 = r_refdef.vrect.width + r_refdef.vrect.x;
	y = glheight - r_refdef.vrect.y;
	y2 = glheight - (r_refdef.vrect.height + r_refdef.vrect.y);

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = gl_envmapsize.value;
		glwidth = glheight = h;
	}

	qglViewport(glx + x, gly + y2, w, h);

	screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;

	// Calculate the FOV value
	yfov = CalcFov(scr_fov_value, r_refdef.vrect.width, r_refdef.vrect.height);

	if (r_refdef.onlyClientDraws)
	{
		MYgluPerspective(yfov, screenaspect, 4.0, 16000.0);
	}
	else
	{
		if (CL_IsDevOverviewMode())
		{
			qglOrtho(-4096.0 / gDevOverview.zoom, 4096.0 / gDevOverview.zoom,
				-4096.0 / (gDevOverview.zoom * screenaspect), 4096.0 / (gDevOverview.zoom * screenaspect),
				16000.0 - gDevOverview.z_min, 16000.0 - gDevOverview.z_max);
		}
		else
			MYgluPerspective(yfov, screenaspect, 4.0, movevars.zmax);
	}

	if (mirror)
	{
		if (mirror_plane->normal[2])
			qglScalef(1, -1, 1);
		else
			qglScalef(-1, 1, 1);

		qglCullFace(GL_BACK);
	}
	else
		qglCullFace(GL_FRONT);

	qglGetFloatv(GL_PROJECTION_MATRIX, gProjectionMatrix);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	qglRotatef(-90, 1, 0, 0);	    // put Z going up
	qglRotatef(90, 0, 0, 1);	    // put Z going up
	qglRotatef(-r_refdef.viewangles[2], 1, 0, 0);
	qglRotatef(-r_refdef.viewangles[0], 0, 1, 0);
	qglRotatef(-r_refdef.viewangles[1], 0, 0, 1);
	qglTranslatef(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	qglGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		qglEnable(GL_CULL_FACE);
	else
		qglDisable(GL_CULL_FACE);

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);

	if (gl_flipmatrix.value)
	{
		// transpose
		for (i = 0; i < 16; i += 4)
		{
			for (j = 0; j < 4; j++)
			{
				gWorldToScreen[i + j] = 0;

				for (k = 0; k < 4; k++)
					gWorldToScreen[i + j] += gProjectionMatrix[i + k] * r_world_matrix[k * 4 + j];
			}
		}
	}
	else
	{
		for (i = 0; i < 16; i += 4)
		{
			for (j = 0; j < 4; j++)
			{
				gWorldToScreen[i + j] = 0;

				for (k = 0; k < 4; k++)
					gWorldToScreen[i + j] += r_world_matrix[i + k] * gProjectionMatrix[k * 4 + j];
			}
		}
	}

	InvertMatrix(gWorldToScreen, gScreenToWorld);
}

/*
=============
R_Clear
=============
*/
void R_Clear( void )
{
	R_ForceCVars(cl.maxclients > 1);

	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear(GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		qglDepthFunc(GL_LEQUAL);
	}
	else if (gl_ztrick.value)
	{
		static int trickframe;

		if (gl_clear.value)
			qglClear(GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.5;
			qglDepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc(GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear.value)
			qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear(GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc(GL_LEQUAL);
	}

	qglDepthRange(gldepthmin, gldepthmax);
}

#define MAPIMAGE_SIZE		1024 * 768 * 4
#define MAPSPRITE_SIZE		128

void Mod_SpriteTextureName( char* pszName, int nNameSize, const char* pcszModelName, int framenum );

/*
=============
R_LoadOverviewSpriteModel

=============
*/
void R_LoadOverviewSpriteModel( model_t* mod, unsigned char* pdata, int XTiles, int yTiles )
{
	mspriteframe_t* pspriteframe;
	int				width = MAPSPRITE_SIZE;
	int				height = MAPSPRITE_SIZE;
	//int				origin;
	int				textureType;
	char			name[256];
	int				numframes;
	int				size;
	int				i;
	msprite_t* psprite;

	numframes = XTiles * yTiles;
	
	size = 8 * numframes + 28;
	psprite = (msprite_t*)Hunk_AllocName(size, mod->name);
	mod->cache.data = psprite;

	psprite->maxwidth = width;
	psprite->maxheight = height;

	psprite->type = SPR_VP_PARALLEL_ORIENTED;
	textureType = 3;
	psprite->texFormat = textureType;
	psprite->beamlength = 0.0;

	mod->synctype = ST_SYNC;

	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = (float)(psprite->maxwidth / -2);
	mod->maxs[0] = mod->maxs[1] = (float)(psprite->maxwidth / 2);
	mod->mins[2] = (float)(psprite->maxheight / -2);
	mod->maxs[2] = (float)(psprite->maxheight / 2);

	mod->numframes = numframes;
	mod->flags = 0;

	for (i = 0; i < numframes; i++)
	{
		psprite->frames[i].type = SPR_SINGLE;

		pspriteframe = (mspriteframe_t*)Hunk_AllocName(sizeof(*pspriteframe), mod->name);
		Q_memset(pspriteframe, 0, sizeof(*pspriteframe));

		psprite->frames[i].frameptr = pspriteframe;

		pspriteframe->width = width;
		pspriteframe->height = height;
		pspriteframe->up = 0.0;
		pspriteframe->down = -width;
		pspriteframe->left = 0.0;
		pspriteframe->right = height;

		Mod_SpriteTextureName(name, sizeof(name), mod->name, i);
		pspriteframe->gl_texturenum = GL_LoadTexture(name, GLT_SPRITE, pspriteframe->width, pspriteframe->height, pdata, 0, TEX_TYPE_RGBA, NULL);
		
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		pdata += 0x10000;
	}

	mod->type = mod_sprite;
}

/*
=============
R_LoadMapSprite

=============
*/
model_t* R_LoadMapSprite( const char* szFilename )
{
	char			filename[MAX_PATH];
	int				i;
	int				x, y;
	int				width, height;
	int				palCount;
	//int				xStep, yStep;






	int				colorBytes = 4;

	unsigned char* buffer, * buffer2, * buffer3;

	model_t* mod;
	int		size;


	buffer3 = NULL;

	RecEngR_LoadMapSprite(szFilename);

	Q_strncpy(filename, szFilename, sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = 0;

	Q_strlwr(filename);

	mod = Mod_FindName(false, filename);
	mod->needload = NL_PRESENT;

	if (!Q_strcmp("tga", &filename[Q_strlen(filename) - 3]))
	{
		colorBytes = 4;
	}
	else if (!Q_strcmp("bmp", &filename[Q_strlen(filename) - 3]))
	{
		colorBytes = 1;
	}
	else
	{
		Sys_Error("Unsupported map image file format.\n");
	}

	buffer = (unsigned char*)Mem_Malloc(0x300000);

	if (!buffer)
		Sys_Error("Not enough memory to load map images (1).\n");

	if (colorBytes == 4) // TGA Image
	{
		if (!LoadTGA(filename, buffer, 0x300000, &width, &height))
		{
			Con_Printf(const_cast<char*>("Couldn't open %s\n"), szFilename);
			Mem_Free(buffer);
			Mem_Free(NULL);
			return NULL;
		}

		size = width * height;

		for (i = 0; i < size; i++)
		{
			if (buffer[4 * i + 0] == 0 &&
				buffer[4 * i + 1] == 0xFF &&
				buffer[4 * i + 2] == 0)
			{
				*(unsigned long*)&buffer[4 * i] = 0;
			}
		}
	}
	else // BMP
	{
		FileHandle_t hFile;
		unsigned char* packPalette;

		hFile = FS_Open(filename, "rb");

		if (!hFile)
		{
			Con_Printf(const_cast<char*>("Couldn't open %s\n"), filename);
			Mem_Free(buffer);
			Mem_Free(NULL);
			return NULL;
		}

		packPalette = NULL;
		LoadBMP8(hFile, &packPalette, &palCount, &buffer3, &width, &height);

		if (buffer3 == NULL || packPalette == NULL)
		{
			Con_Printf(const_cast<char*>("Error loading %s\n"), filename);
			Mem_Free(buffer);
			Mem_Free(NULL);

			if (buffer3)
				Mem_Free(buffer3);
			if (packPalette)
				Mem_Free(packPalette);

			return NULL;
		}

		size = width * height;

		for (i = 0; i < size; i++)
		{
			buffer[4 * i + 0] = packPalette[3 * buffer3[i] + 0];
			buffer[4 * i + 1] = packPalette[3 * buffer3[i] + 1];
			buffer[4 * i + 2] = packPalette[3 * buffer3[i] + 2];

			if (buffer[4 * i + 0] == 0 &&
				buffer[4 * i + 1] == 0xFF &&
				buffer[4 * i + 2] == 0)
			{
				*(unsigned long*)&buffer[4 * i] = 0;
			}
			else
			{
				buffer[4 * i + 3] = -1;
			}
		}

		Mem_Free(buffer3);
		Mem_Free(packPalette);
	}

	if (size == 0 || (((unsigned char)height | (unsigned char)width) & 0x7F))
	{
		Con_Printf(const_cast<char*>("Wrong map image dimensions or file could not be loaded in %s\n"), filename);
		Mem_Free(buffer);
		Mem_Free(NULL);
		return NULL;
	}

	buffer2 = (unsigned char*)Mem_Malloc(0x300000);
	
	if (!buffer2)
		Sys_Error("Not enough memory to load map images (2).\n");

	unsigned char* buf;
	unsigned char* buf2;

	buf = buffer2;
	for (i = 0; i < height; i += 128)
	{
		for (x = 0; x < width; x += 128)
		{
			buf2 = &buffer[4 * x + 4 * width * i];
			for (y = 128; y != 0; y--)
			{
				memcpy(buf, buf2, 512);
				buf += 512;
				buf2 += 4 * width;
			}
		}
	}

	R_LoadOverviewSpriteModel(mod, buffer2, width / 128, height / 128);

	Mem_Free(buffer);
	Mem_Free(buffer2);

	return mod;
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene( void )
{
	if (CL_IsDevOverviewMode())
		CL_SetDevOverView(&r_refdef);

	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	R_MarkLeaves();		// done here so we know if we're in water

	if (!r_refdef.onlyClientDraws)
	{
		if (CL_IsDevOverviewMode())
		{
			qglClearColor(0, 1, 0, 0);
			qglClear(GL_COLOR_BUFFER_BIT);
		}

		R_DrawWorld();		// adds static entities to the list
		S_ExtraUpdate();	// don't let sound get messed up if going slow
		R_DrawEntitiesOnList();
	}

	if (g_bUserFogOn)
		R_RenderFinalFog();

	AllowFog(false);
	ClientDLL_DrawNormalTriangles();
	AllowFog(true);

	if (cl.waterlevel > 2 && !r_refdef.onlyClientDraws || !g_bUserFogOn)
		qglDisable(GL_FOG);

	R_DrawTEntitiesOnList(r_refdef.onlyClientDraws);
	S_ExtraUpdate();

	if (!r_refdef.onlyClientDraws)
	{
		R_RenderDlights();
		GL_DisableMultitexture();
		R_DrawParticles();
	}
}

/*
================
R_SetStackBase
================
*/
void R_SetStackBase( void )
{
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView( void )
{
	float	framerate;
	double	time1 = 0, time2;

	if (r_norefresh.value)
		return;

	// r_worldentity.model is initialized in R_NewMap()
	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		qglFinish();
		time1 = Sys_FloatTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	R_Clear();

	if (!r_refdef.onlyClientDraws)
	{
		R_PreDrawViewModel();
	}

	// render normal view
	R_RenderScene();

	if (!r_refdef.onlyClientDraws)
	{
		R_DrawViewModel();
		R_PolyBlend();
	}

	S_ExtraUpdate();

	if (r_speeds.value)
	{
		framerate = cl.time - cl.oldtime;

		if (framerate > 0.0)
			framerate = 1.0 / framerate;

		time2 = Sys_FloatTime();
		Con_Printf(const_cast<char*>("%3ifps %3i ms  %4i wpoly %4i epoly\n"), (int)(framerate + 0.5), (int)((time2 - time1) * 1000.0), c_brush_polys, c_alias_polys);
	}
}