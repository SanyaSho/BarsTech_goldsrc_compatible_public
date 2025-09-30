#include "quakedef.h"
#include "client.h"
#if defined(GLQUAKE)
#include "glquake.h"
#include "gl_rmain.h"
#include "gl_rmisc.h"
#else
#include "r_local.h"
#include "d_local.h"
#include "r_shared.h"
#endif
#include "r_triangle.h"
#include "r_trans.h"
#include "r_studio.h"
#include "cl_main.h"
#include "cl_tent.h"
#include "pmove.h"
#include "pmovetst.h"
#include "cl_parse.h"

transObjRef* transObjects;
int maxTransObjs;
int numTransObjs;
qboolean r_intentities;

void R_DestroyObjects()
{
	if (transObjects)
	{
		Mem_Free(transObjects);
		transObjects = 0;
	}
	maxTransObjs = 0;
}

void R_AllocObjects(int nMax)
{
	if (transObjects)
		Con_Printf(const_cast<char*>("Transparent objects reallocate\n"));
	transObjects = (transObjRef *)Mem_Malloc(sizeof(transObjRef) * nMax);
	Q_memset(transObjects, 0, sizeof(transObjRef) * nMax);
	maxTransObjs = nMax;
}

void AppendTEntity(cl_entity_t *pEnt)
{
	float   dist;
	vec3_t  v;

	if (numTransObjs >= maxTransObjs)
		Sys_Error("AddTentity: Too many objects");

	VectorAdd(pEnt->model->mins, pEnt->model->maxs, v);
	VectorScale(v, 0.5, v);
	VectorAdd(v, pEnt->origin, v);
	VectorSubtract(r_origin, v, v);

	dist = DotProduct(v, v);

	transObjects[numTransObjs].pEnt = pEnt;
	transObjects[numTransObjs].distance = dist;

	numTransObjs++;
}

void AddTEntity(cl_entity_t *pEnt)
{
	int     i;
	float   dist;
	vec3_t  v;

	if (numTransObjs >= maxTransObjs)
		Sys_Error("AddTentity: Too many objects");

	if (!pEnt->model || pEnt->model->type != mod_brush || pEnt->curstate.rendermode != kRenderTransAlpha)
	{
		VectorAdd(pEnt->model->maxs, pEnt->model->mins, v);
		VectorScale(v, 0.5, v);
		VectorAdd(v, pEnt->origin, v);
		VectorSubtract(r_origin, v, v);

		dist = DotProduct(v, v);
	}
	else
	{
		// max distance
		dist = 1E9F;
	}

	i = numTransObjs;
	while (i > 0)
	{
		if (transObjects[i - 1].distance >= dist)
			break;

		transObjects[i].pEnt = transObjects[i - 1].pEnt;
		transObjects[i].distance = transObjects[i - 1].distance;
		i--;
	}

	transObjects[i].pEnt = pEnt;
	transObjects[i].distance = dist;

	numTransObjs++;
}

float GlowBlend(cl_entity_t* pEntity)
{
	vec3_t tmp;
	int traceFlags;
	pmtrace_t trace;
	float brightness, dist;

	VectorSubtract(r_entorigin, r_origin, tmp);
	dist = Length(tmp);
	pmove->usehull = 2;

	traceFlags = PM_GLASS_IGNORE;
	
	if (r_traceglow.value == 0.0)
		traceFlags = PM_STUDIO_IGNORE;

	trace = PM_PlayerTrace(r_origin, r_entorigin, traceFlags, -1);

	if (dist * (1.0f - trace.fraction) > 8)
		return .0f;

	if (pEntity->curstate.renderfx == kRenderFxNoDissipation)
		return (float)pEntity->curstate.renderamt / 255.0f;

	brightness = clamp(19000.0f / (dist * dist), 0.5f, 1.0f);
	pEntity->curstate.scale = dist * 0.005f;

	return brightness;
}

#if defined ( GLQUAKE )
void R_DrawTEntitiesOnList(qboolean clientOnly)
{
	int i, j;
	float* attachment = NULL;

	if (!r_drawentities.value)
		return;

	if (!clientOnly)
	{
		for (i = 0; i < numTransObjs; i++)
		{
			qglDisable(GL_FOG);
			currententity = transObjects[i].pEnt;

			r_blend = CL_FxBlend(currententity);

			if (r_blend <= 0.0)
				continue;

			r_blend /= 255.f;

			// Glow is only for sprite models
			if (currententity->curstate.rendermode == kRenderGlow && currententity->model->type != mod_sprite)
				Con_DPrintf(const_cast<char*>("Non-sprite set to glow!\n"));

			switch (currententity->model->type)
			{
			case mod_brush:
				if (g_bUserFogOn)
				{
					if (currententity->curstate.rendermode != kRenderGlow &&
						currententity->curstate.rendermode != kRenderTransAdd)
					{
						qglEnable(GL_FOG);
					}
				}

				R_DrawBrushModel(currententity);
				break;

			case mod_sprite:
				// look up for attachment
				if (currententity->curstate.body)
				{
					attachment = R_GetAttachmentPoint(currententity->curstate.skin, currententity->curstate.body);
					VectorCopy(attachment, r_entorigin);
				}
				else
				{
					VectorCopy(currententity->origin, r_entorigin);
				}

				// Glow sprite
				if (currententity->curstate.rendermode == kRenderGlow)
				{
					r_blend *= GlowBlend(currententity);
				}

				if (r_blend != 0.0)
					R_DrawSpriteModel(currententity);
				break;

			case mod_alias:
				R_DrawAliasModel(currententity);
				break;

			case mod_studio:
				if (currententity->curstate.renderamt)
				{
					// Drawing player model
					if (currententity->player)
					{
						pStudioAPI->StudioDrawPlayer(STUDIO_RENDER | STUDIO_EVENTS, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1]);
					}
					else if (currententity->curstate.movetype == MOVETYPE_FOLLOW)
					{
						for (j = 0; j < numTransObjs; j++)
						{
							if (transObjects[j].pEnt->index == currententity->curstate.aiment)
							{
								currententity = transObjects[j].pEnt;

								if (currententity->player)
									pStudioAPI->StudioDrawPlayer(0, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1]);
								else
									pStudioAPI->StudioDrawModel(0);

								currententity = transObjects[i].pEnt;
								pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
							}
						}
					}
					else
					{
						pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
					}
				}
				break;

			default:
				continue;
			}
		}
	}

	GL_DisableMultitexture();

	qglEnable(GL_ALPHA_TEST);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (g_bUserFogOn)
		qglDisable(GL_FOG);

	ClientDLL_DrawTransparentTriangles();

	if (g_bUserFogOn)
	{
		qglEnable(GL_FOG);
	}

	numTransObjs = 0;
	r_blend = 1.0;
}
#else
void R_DrawTEntitiesOnList(qboolean clientOnly)
{
	edge_t	ledges[NUMSTACKEDGES +
		((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
	surf_t	lsurfs[NUMSTACKSURFACES +
		((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];
	float minmaxs[6];
	int j;

	if (!r_drawentities.value)
		return;

	if (auxedges)
	{
		r_edges = auxedges;
	}
	else
	{
		r_edges = (edge_t *)
			(((uintptr_t)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack)
	{
		surfaces = (surf_t *)
			(((uintptr_t)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
	}

	vec3_t oldorigin;
	VectorCopy(modelorg, oldorigin);

	r_intentities = true;
	insubmodel = true;
	r_dlightframecount = r_framecount;

	if (!clientOnly)
	{
		for (int i = 0; i < numTransObjs; i++)
		{
			currententity = transObjects[i].pEnt;
			r_blend = CL_FxBlend(currententity);

			if (r_blend)
			{
				if (currententity->curstate.rendermode == kRenderGlow && currententity->model->type != mod_sprite)
				{
					Con_Printf(const_cast<char*>("Non-sprite set to glow!\n"));
				}

				modtype_t type = currententity->model->type;
				model_t* clmodel = currententity->model;

				if (type == mod_studio)
				{
					void (*poldpolysetdraw)(spanpackage_t*) = polysetdraw;

					if (currententity->curstate.rendermode == kRenderTransAdd)
						polysetdraw = D_PolysetDrawSpansTransAdd;
					else if (currententity->curstate.rendermode == kRenderTransAlpha)
						polysetdraw = D_PolysetDrawHole;
					else
						polysetdraw = D_PolysetDrawSpansTrans;

					if (currententity->player)
					{
						pStudioAPI->StudioDrawPlayer(STUDIO_RENDER | STUDIO_EVENTS, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1]);
					}
					else if (currententity->curstate.movetype == MOVETYPE_FOLLOW)
					{
						for (j = 0; j < numTransObjs; j++)
						{
							if (transObjects[j].pEnt->index != currententity->curstate.aiment)
								continue;

							currententity = transObjects[j].pEnt;

							if (currententity->player)
								pStudioAPI->StudioDrawPlayer(0, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1]);
							else
								pStudioAPI->StudioDrawModel(0);

							currententity = transObjects[i].pEnt;

							pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);
						}
					}
					else
						pStudioAPI->StudioDrawModel(STUDIO_RENDER | STUDIO_EVENTS);

					polysetdraw = poldpolysetdraw;
				}

				if (type == mod_sprite)
				{
					switch (currententity->curstate.rendermode)
					{
					case kRenderTransTexture:
						spritedraw = D_SpriteDrawSpansTrans;
						break;
					case kRenderGlow:
						spritedraw = D_SpriteDrawSpansGlow;
						break;
					case kRenderTransAlpha:
						spritedraw = D_SpriteDrawSpansAlpha;
						break;
					case kRenderTransAdd:
						spritedraw = D_SpriteDrawSpansAdd;
						break;
					default:
						spritedraw = D_SpriteDrawSpans;
						break;
					}

					if (currententity->curstate.body)
					{
						VectorCopy(R_GetAttachmentPoint(currententity->curstate.skin, currententity->curstate.body), r_entorigin);
					}
					else
					{
						VectorCopy(currententity->origin, r_entorigin);
					}

					VectorSubtract(r_origin, r_entorigin, modelorg);

					if (currententity->curstate.rendermode == kRenderGlow)
						r_blend *= GlowBlend(currententity);

					if (r_blend)
						R_DrawSprite();
				}

				if (type == mod_brush)
				{
					RotatedBBox(currententity->model->mins, currententity->model->maxs, currententity->angles, &minmaxs[0], &minmaxs[3]);
					// see if the bounding box lets us trivially reject, also sets
					// trivial accept status
					for (j = 0; j<3; j++)
					{
						minmaxs[j] = currententity->origin[j] +
							clmodel->mins[j];
						minmaxs[3 + j] = currententity->origin[j] +
							clmodel->maxs[j];
					}

					int clipflags = R_BmodelCheckBBox(currententity->model, minmaxs);

					if (clipflags != BMODEL_FULLY_CLIPPED)
					{
						R_BeginEdgeFrame();
						VectorCopy(currententity->origin, r_entorigin);
						VectorSubtract(r_origin, r_entorigin, modelorg);
						r_pcurrentvertbase = currententity->model->vertexes;
						R_RotateBmodel();

						if (currententity->curstate.rendermode == kRenderTransAlpha && currententity->model->firstmodelsurface != 0)
						{
							for (int k = 0; k < MAX_DLIGHTS; k++)
							{
								vec3_t temporg;

								if ((cl_dlights[k].die < cl.time) ||
									(!cl_dlights[k].radius))
								{
									continue;
								}

								VectorCopy(cl_dlights[k].origin, temporg);
								VectorSubtract(cl_dlights[k].origin, currententity->origin, cl_dlights[k].origin);
								R_MarkLights(&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
								VectorCopy(temporg, cl_dlights[k].origin);
							}
						}

						switch (currententity->curstate.rendermode)
						{
							//case kRenderNormal:
							//	break;
						case kRenderTransColor:
							d_drawspans = D_DrawTranslucentColor;
							break;
						case kRenderTransTexture:
							d_drawspans = D_DrawTranslucentTexture;
							break;
							//case kRenderGlow:
							// а в 0.52 здесь была процедура!
							//	break;
						case kRenderTransAlpha:
							d_drawspans = D_DrawTransHoles;
							break;
						case kRenderTransAdd:
							d_drawspans = D_DrawTranslucentAdd;
							break;
						}

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
								// not a leaf; has to be clipped to the world BSP
								r_clipflags = clipflags;

								currententity->topnode = r_pefragtopnode;

								// falls entirely in one leaf, so we just put all the
								// edges in the edge list and let 1/z sorting handle
								// drawing order
								R_DrawSubmodelPolygons(clmodel, clipflags);
								currententity->topnode = NULL;
							}
						}

						// put back world rotation and frustum clipping		
						// FIXME: R_RotateBmodel should just work off base_vxx
						VectorCopy(base_vpn, vpn);
						VectorCopy(base_vup, vup);
						VectorCopy(base_vright, vright);
						VectorCopy(base_modelorg, modelorg);
						VectorCopy(oldorigin, modelorg);
						R_TransformFrustum();
						R_ScanEdges();

					}
				}
			}
		}
	}

	insubmodel = false;
	ClientDLL_DrawTransparentTriangles();
	r_intentities = false;
	numTransObjs = 0;
	d_drawspans = D_DrawSpans8;
}
#endif
