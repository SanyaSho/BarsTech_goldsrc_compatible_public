// r_alias.c: routines for setting up to draw alias models

#include "quakedef.h"
#include "model.h"
#include "d_iface.h"
#include "r_local.h"
#include "d_local.h"	// FIXME: shouldn't be needed (is needed for patch
// right now, but that should move)
#include "client.h"
#include "studio.h"
#include "r_studio.h"

#define LIGHT_MIN	5		// lowest light value we'll allow, to avoid the
//  need for inner-loop light clamping

mtriangle_t		*ptriangles;
EXTERN_C
{
affinetridesc_t	r_affinetridesc;
};

void *			acolormap;	// FIXME: should go away

trivertx_t		*r_apverts;
//trivertx_t r_averts[MAXALIASVERTS + 2];

// TODO: these probably will go away with optimized rasterization
mdl_t				*pmdl;
vec3_t				r_plightvec;
aliashdr_t			*paliashdr;
finalvert_t			*pfinalverts;
//finalvert_t finalverts[MAXSTUDIOVERTS];
float		ziscale;
//static model_t		*pmodel;

static vec3_t		alias_forward, alias_right, alias_up;

static maliasskindesc_t	*pskindesc;

int				a_skinwidth;
int				r_anumverts;

float	aliastransform[3][4];

aedge_t	aedges[12] = {
	{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
	{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
	{ 0, 5 }, { 1, 4 }, { 2, 7 }, { 3, 6 }
};

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

/*
================
R_AliasProjectFinalVert
================
*/
void R_AliasProjectFinalVert(finalvert_t* fv, auxvert_t* av)
{
	float	zi;

	// project points
	zi = 1.0 / av->fv[2];

	fv->v[5] = zi * ziscale;

	fv->v[0] = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
	fv->v[1] = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}

/*
=================
R_AliasSetupFrame

set r_apverts
=================
*/
void xR_AliasSetupFrame(void)
{
	int				frame, prevframe;
	int				i, numframes;
	maliasgroup_t* paliasgroup;
	trivertx_t* ptrivert, *pprevtrivert;
	float* pintervals, fullinterval, targettime, time, scale;
	static trivertx_t iterpverts[MAXALIASVERTS];

	frame = currententity->curstate.frame;
	if ((frame >= pmdl->numframes) || (frame < 0))
	{
		Con_DPrintf(const_cast<char*>("R_AliasSetupFrame: no such frame %d\n"), frame);
		frame = 0;
	}

	if (paliashdr->frames[frame].type == ALIAS_SINGLE)
	{
		if (frame != currententity->prevstate.frame)
		{
			currententity->latched.prevanimtime = cl.time;
			currententity->prevstate.frame = frame;
		}

		if (currententity == &cl.viewent || !r_dointerp || (currententity->prevstate.frame >= (float)pmdl->numframes))
		{
			r_apverts = (trivertx_t*)
				((byte*)paliashdr + paliashdr->frames[frame].frame);
			return;
		}

		fullinterval = cl.time * 10.0 - currententity->latched.prevanimtime * 10.0;
		scale = 1.0 - fullinterval;

		if (fullinterval > 1.0 || fullinterval < 0.0)
		{
			fullinterval = 1.0;
			scale = 0.0;
		}

		prevframe = paliashdr->frames[(int)currententity->prevstate.frame].frame;

		ptrivert = (trivertx_t*)((byte*)paliashdr + paliashdr->frames[frame].frame);
		pprevtrivert = (trivertx_t*)((byte*)ptrivert + prevframe - paliashdr->frames[frame].frame);

		for (i = 0; i < pmdl->numverts; i++, ptrivert++, pprevtrivert++)
		{
			iterpverts[i].v[0] = (byte)((float)pprevtrivert->v[0] * scale + (float)ptrivert->v[0] * fullinterval);
			iterpverts[i].v[1] = (byte)((float)pprevtrivert->v[1] * scale + (float)ptrivert->v[1] * fullinterval);
			iterpverts[i].v[2] = (byte)((float)pprevtrivert->v[2] * scale + (float)ptrivert->v[2] * fullinterval);
			iterpverts[i].lightnormalindex = ptrivert->lightnormalindex;
		}

		r_apverts = iterpverts;
		return;
	}

	paliasgroup = (maliasgroup_t*)
		((byte*)paliashdr + paliashdr->frames[frame].frame);
	pintervals = (float*)((byte*)paliashdr + paliasgroup->intervals);
	numframes = paliasgroup->numframes;
	fullinterval = pintervals[numframes - 1];

	time = cl.time + currententity->syncbase;

	//
	// when loading in Mod_LoadAliasGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
	//
	targettime = time - ((int)(time / fullinterval)) * fullinterval;

	for (i = 0; i < (numframes - 1); i++)
	{
		if (pintervals[i] > targettime)
			break;
	}

	r_apverts = (trivertx_t*)
		((byte*)paliashdr + paliasgroup->frames[i].frame);
}