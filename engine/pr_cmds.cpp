#include <cstdint>
#include <ctime>

#include "quakedef.h"
#include "cdll_int.h"
#include "pr_cmds.h"
#include "cl_pmove.h"
#include "world.h"
#include "r_local.h"
#include "sv_main.h"
#include "host.h"
#include "eiface.h"
#include "r_studioint.h"
#include "sound.h"
#include "client.h"
#include "cmodel.h"
#include "sv_move.h"
#include "sv_phys.h"
#include "cl_main.h"
#include "sv_user.h"
#include "pmove.h"
#include "server.h"
#include "protocol.h"

vec_t gHullMins[4][3] = {
	{ 0.0f, 0.0f, 0.0f },
	{ -16.0f, -16.0f, -36.0f },
	{ -32.0f, -32.0f, -32.0f },
	{ -16.0f, -16.0f, -18.0f },
};

vec_t gHullMaxs[4][3] = {
	{ 0.0f, 0.0f, 0.0f },
	{ 16.0f, 16.0f, 36.0f },
	{ 32.0f, 32.0f, 32.0f },
	{ 16.0f, 16.0f, 18.0f },
};

edict_t* gMsgEntity;
int gMsgDest;
int gMsgType;
qboolean gMsgStarted;
vec3_t gMsgOrigin;
unsigned char checkpvs[1024];
int c_invis;
int c_notvis;
unsigned char gMsgData[512];
sizebuf_t gMsgBuffer = { "MessageBegin/End", 0, gMsgData, sizeof(gMsgData), 0 };

void PF_makevectors_I(const float* rgflVector)
{
	AngleVectors(rgflVector, gGlobalVariables.v_forward, gGlobalVariables.v_right, gGlobalVariables.v_up);
}

float PF_Time(void)
{
	return Sys_FloatTime();
}

void PF_setorigin_I(edict_t* e, const float* org)
{
	if (!e)
		return;

	VectorCopy(org, e->v.origin);
	SV_LinkEdict(e, FALSE);
}

void SetMinMaxSize(edict_t* e, const float* min, const float* max, qboolean rotate)
{
	float* angles;
	vec3_t	rmin, rmax;
	float	bounds[2][3];
	float	xvector[2], yvector[2];
	float	a;
	vec3_t	base, transformed;
	int		i, j, k, l;

	for (i = 0; i < 3; i++)
		if (min[i] > max[i])
			Host_Error("backwards mins/maxs");

	rotate = FALSE;		// FIXME: implement rotation properly again

	if (!rotate)
	{
		VectorCopy(min, rmin);
		VectorCopy(max, rmax);
	}
	else
	{
		// find min / max for rotations
		angles = e->v.angles;

		a = angles[1] / 180 * M_PI;

		xvector[0] = cos(a);
		xvector[1] = sin(a);
		yvector[0] = -sin(a);
		yvector[1] = cos(a);

		VectorCopy(min, bounds[0]);
		VectorCopy(max, bounds[1]);

		rmin[0] = rmin[1] = rmin[2] = 9999;
		rmax[0] = rmax[1] = rmax[2] = -9999;

		for (i = 0; i <= 1; i++)
		{
			base[0] = bounds[i][0];
			for (j = 0; j <= 1; j++)
			{
				base[1] = bounds[j][1];
				for (k = 0; k <= 1; k++)
				{
					base[2] = bounds[k][2];

					// transform the point
					transformed[0] = xvector[0] * base[0] + yvector[0] * base[1];
					transformed[1] = xvector[1] * base[0] + yvector[1] * base[1];
					transformed[2] = base[2];

					for (l = 0; l < 3; l++)
					{
						if (transformed[l] < rmin[l])
							rmin[l] = transformed[l];
						if (transformed[l] > rmax[l])
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}

	// set derived values
	VectorCopy(min, e->v.mins);
	VectorCopy(max, e->v.maxs);
	VectorSubtract(max, min, e->v.size);

	SV_LinkEdict(e, FALSE);
}

void PF_setsize_I(edict_t* e, const float* rgflMin, const float* rgflMax)
{
	SetMinMaxSize(e, rgflMin, rgflMax, false);
}

void PF_setmodel_I(edict_t* e, const char* m)
{
	int		i;
	char** check;
	model_t* mod;

	// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
		if (!Q_stricmp(*check, m))
			break;

	if (!*check)
		Host_Error("no precache: %s\n", m);

	e->v.model = m - pr_strings;
	e->v.modelindex = i; // SV_ModelIndex(m);

	mod = sv.models[(int)e->v.modelindex];  // Mod_ForName(m, TRUE);

	if (mod)
		SetMinMaxSize(e, mod->mins, mod->maxs, TRUE);
	else
		SetMinMaxSize(e, vec3_origin, vec3_origin, TRUE);
}

int PF_modelindex(const char* pstr)
{
	return SV_ModelIndex(pstr);
}

int ModelFrames(int modelIndex)
{
	if (modelIndex <= 0 || modelIndex >= MAX_MODELS)
	{
		Con_DPrintf(const_cast<char*>("Bad sprite index!\n"));
		return 1;
	}

	return ModelFrameCount(sv.models[modelIndex]);
}

void PF_bprint(char* s)
{
	SV_BroadcastPrintf("%s", s);
}

void PF_sprint(char* s, int entnum)
{
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("tried to sprint to a non-client\n"));
		return;
	}

	client_t* client = &svs.clients[entnum - 1];
	if (!client->fakeclient)
	{
		MSG_WriteChar(&client->netchan.message, svc_print);
		MSG_WriteString(&client->netchan.message, s);
	}
}

void ServerPrint(const char* szMsg)
{
	Con_Printf(const_cast<char*>("%s"), szMsg);
}

void ClientPrintf(edict_t* pEdict, PRINT_TYPE ptype, const char* szMsg)
{
	client_t* client;
	int entnum;

	entnum = NUM_FOR_EDICT(pEdict);
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("tried to sprint to a non-client\n"));
		return;
	}

	client = &svs.clients[entnum - 1];
	if (client->fakeclient)
		return;

	switch (ptype)
	{
	case print_center:
		MSG_WriteChar(&client->netchan.message, svc_centerprint);
		MSG_WriteString(&client->netchan.message, szMsg);
		break;

	case print_chat:
	case print_console:
		MSG_WriteByte(&client->netchan.message, svc_print);
		MSG_WriteString(&client->netchan.message, szMsg);
		break;

	default:
		Con_Printf(const_cast<char*>("invalid PRINT_TYPE %i\n"), ptype);
		break;
	}
}

float PF_vectoyaw_I(const float* rgflVector)
{
	float yaw = 0.0f;
	if (rgflVector[1] == 0.0f && rgflVector[0] == 0.0f)
		return 0.0f;

	yaw = (float)(int)floor(atan2((double)rgflVector[1], (double)rgflVector[0]) * 180.0 / M_PI);
	if (yaw < 0.0)
		yaw = yaw + 360.0;

	return yaw;
}

void PF_vectoangles_I(const float* rgflVectorIn, float* rgflVectorOut)
{
	VectorAngles(rgflVectorIn, rgflVectorOut);
}

void PF_particle_I(const float *org, const float *dir, float color, float count)
{
	SV_StartParticle(org, dir, color, count);
}

void PF_ambientsound_I(edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch)
{
	int i;
	int soundnum;
	int ent;
	sizebuf_t *pout;

	if (samp[0] == '!')
	{
		fFlags |= SND_SENTENCE;
		soundnum = Q_atoi(samp + 1);
		if (soundnum >= CVOXFILESENTENCEMAX)
		{
			Con_Printf(const_cast<char*>("invalid sentence number: %s"), &samp[1]);
			return;
		}
	}
	else
	{
		for (i = 0; i < MAX_SOUNDS; i++)
		{
			if (sv.sound_precache[i] && !Q_strcasecmp(sv.sound_precache[i], samp))
			{
				soundnum = i;
				break;
			}
		}

		if (i == MAX_SOUNDS)
		{
			Con_Printf(const_cast<char*>("no precache: %s\n"), samp);
			return;
		}
	}

	ent = NUM_FOR_EDICT(entity);
	pout = &sv.signon;

	if (!(fFlags & SND_SPAWNING))
		pout = &sv.datagram;

	MSG_WriteByte(pout, svc_spawnstaticsound);
	MSG_WriteCoord(pout, pos[0]);
	MSG_WriteCoord(pout, pos[1]);
	MSG_WriteCoord(pout, pos[2]);

	MSG_WriteShort(pout, soundnum);
	MSG_WriteByte(pout, (vol * 255.0));
	MSG_WriteByte(pout, (attenuation * 64.0));
	MSG_WriteShort(pout, ent);
	MSG_WriteByte(pout, pitch);
	MSG_WriteByte(pout, fFlags);
}

void PF_sound_I(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch)
{
	if (volume < 0.0 || volume > 255.0)
		Sys_Error("EMIT_SOUND: volume = %i", volume);
	if (attenuation < 0.0 || attenuation > 4.0)
		Sys_Error("EMIT_SOUND: attenuation = %f", attenuation);
	if (channel < CHAN_AUTO || channel > CHAN_NETWORKVOICE_BASE)
		Sys_Error("EMIT_SOUND: channel = %i", channel);
	if (pitch < 0 || pitch > 255)
		Sys_Error("EMIT_SOUND: pitch = %i", pitch);

	SV_StartSound(0, entity, channel, sample, (int)(volume * 255.0f), attenuation, fFlags, pitch);
}

void PF_traceline_Shared(const float *v1, const float *v2, int nomonsters, edict_t *ent)
{
	trace_t trace = SV_Move(v1, vec3_origin, vec3_origin, v2, nomonsters, ent, FALSE);

	gGlobalVariables.trace_flags = 0;
	SV_SetGlobalTrace(&trace);
}

void PF_traceline_DLL(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
{
	PF_traceline_Shared(v1, v2, fNoMonsters, pentToSkip ? pentToSkip : sv.edicts);

	ptr->fAllSolid = (int)gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = (int)gGlobalVariables.trace_startsolid;
	ptr->fInOpen = (int)gGlobalVariables.trace_inopen;
	ptr->fInWater = (int)gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	ptr->pHit = gGlobalVariables.trace_ent;

	VectorCopy(gGlobalVariables.trace_endpos, ptr->vecEndPos);
	VectorCopy(gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal);

	ptr->iHitgroup = gGlobalVariables.trace_hitgroup;
}

void TraceHull(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr)
{
	if (hullNumber < 0 || hullNumber > 3)
		hullNumber = 0;

	trace_t trace = SV_Move(v1, gHullMins[hullNumber], gHullMaxs[hullNumber], v2, fNoMonsters, pentToSkip, FALSE);

	ptr->fAllSolid = trace.allsolid;
	ptr->fStartSolid = trace.startsolid;
	ptr->fInOpen = trace.inopen;
	ptr->fInWater = trace.inwater;
	ptr->flFraction = trace.fraction;
	ptr->flPlaneDist = trace.plane.dist;
	ptr->pHit = trace.ent;
	ptr->iHitgroup = trace.hitgroup;

	VectorCopy(trace.endpos, ptr->vecEndPos);
	VectorCopy(trace.plane.normal, ptr->vecPlaneNormal);
}

void TraceSphere(const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr)
{
	Sys_Error("TraceSphere not yet implemented!\n");
}

void TraceModel(const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr)
{
	int oldMovetype, oldSolid;

	if (hullNumber < 0 || hullNumber > 3)
		hullNumber = 0;

	model_t* pmodel = sv.models[pent->v.modelindex];

	if (pmodel && pmodel->type == mod_brush)
	{
		oldMovetype = pent->v.movetype;
		oldSolid = pent->v.solid;
		pent->v.solid = SOLID_BSP;
		pent->v.movetype = MOVETYPE_PUSH;
	}

	trace_t trace = SV_ClipMoveToEntity(pent, v1, gHullMins[hullNumber], gHullMaxs[hullNumber], v2);

	if (pmodel && pmodel->type == mod_brush)
	{
		pent->v.solid = oldSolid;
		pent->v.movetype = oldMovetype;
	}

	ptr->fAllSolid = trace.allsolid;
	ptr->fStartSolid = trace.startsolid;
	ptr->fInOpen = trace.inopen;
	ptr->fInWater = trace.inwater;
	ptr->flFraction = trace.fraction;
	ptr->flPlaneDist = trace.plane.dist;
	ptr->pHit = trace.ent;
	ptr->iHitgroup = trace.hitgroup;

	VectorCopy(trace.endpos, ptr->vecEndPos);
	VectorCopy(trace.plane.normal, ptr->vecPlaneNormal);
}

msurface_t* SurfaceAtPoint( model_t* pModel, mnode_t* node, vec_t* start, vec_t* end )
{
	float		front, back, frac;
	int			side;
	mplane_t* plane;
	vec3_t		mid;
	msurface_t* surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t* tex;

	if (node->contents < 0)
		return NULL;

	plane = node->plane;
	front = DotProduct(start, plane->normal) - plane->dist;
	back = DotProduct(end, plane->normal) - plane->dist;

	// test the front side first
	side = front < 0;

	// If they're both on the same side of the plane, don't bother to split
	// just check the appropriate child
	if ((back < 0.0) == side)
		return SurfaceAtPoint(pModel, node->children[side], start, end);

	// calculate mid point
	frac = front / (front - back);

	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side
	surf = SurfaceAtPoint(pModel, node->children[side], start, mid);
	if (surf)
		return surf;

	if ((back < 0.0) == side)
		return NULL;

	// check for impact on this node
	for (i = 0; i < node->numsurfaces; i++)
	{
		surf = &pModel->surfaces[node->firstsurface + i];
		tex = surf->texinfo;

		s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		// Check this surface to see if there's an intersection
		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		return surf;
	}

	// go down back side
	surf = SurfaceAtPoint(pModel, node->children[!side], mid, end);
	return surf;
}

const char* TraceTexture( edict_t* pTextureEntity, const float* v1, const float* v2 )
{
	model_t* pmodel;
	msurface_t* psurf;
	int		firstnode;
	vec3_t	start, end;

	firstnode = 0;

	if (pTextureEntity)
	{
		vec3_t offset;
		hull_t* phull; 
		int modelindex;

		modelindex = pTextureEntity->v.modelindex;
		if (modelindex < 0 || modelindex >= MAX_MODELS)
			return NULL;

		pmodel = sv.models[modelindex];
		if (pmodel && pmodel->type == mod_brush)
		{
			phull = SV_HullForBsp(pTextureEntity, vec3_origin, vec3_origin, offset);
			VectorSubtract(v1, offset, start);
			VectorSubtract(v2, offset, end);

			firstnode = phull->firstclipnode;

			if (pTextureEntity->v.angles[0] != 0.0 || pTextureEntity->v.angles[1] != 0.0 || pTextureEntity->v.angles[2] != 0.0)
			{
				vec3_t forward, right, up;
				vec3_t temp;

				AngleVectors(pTextureEntity->v.angles, forward, right, up);

				VectorCopy(start, temp);
				start[0] = DotProduct(forward, temp);
				start[1] = -DotProduct(right, temp);
				start[2] = DotProduct(up, temp);

				VectorCopy(end, temp);
				end[0] = DotProduct(forward, temp);
				end[1] = -DotProduct(right, temp);
				end[2] = DotProduct(up, temp);
			}
		}
	}
	else
	{
		pmodel = sv.worldmodel;
		VectorCopy(v1, start);
		VectorCopy(v2, end);
	}

	if (pmodel && pmodel->type == mod_brush)
	{
		if (pmodel->nodes)
		{
			psurf = SurfaceAtPoint(pmodel, &pmodel->nodes[firstnode], start, end);
			if (psurf)
				return psurf->texinfo->texture->name;
		}
	}
	return NULL;
}

void PF_TraceToss_Shared(edict_t* ent, edict_t* ignore)
{
	trace_t trace = SV_Trace_Toss(ent, ignore);
	SV_SetGlobalTrace(&trace);
}

void SV_SetGlobalTrace(trace_t* ptrace)
{
	gGlobalVariables.trace_fraction = ptrace->fraction;
	gGlobalVariables.trace_allsolid = (float)ptrace->allsolid;
	gGlobalVariables.trace_startsolid = (float)ptrace->startsolid;

	VectorCopy(ptrace->endpos, gGlobalVariables.trace_endpos);
	VectorCopy(ptrace->plane.normal, gGlobalVariables.trace_plane_normal);

	gGlobalVariables.trace_inwater = (float)ptrace->inwater;
	gGlobalVariables.trace_inopen = (float)ptrace->inopen;
	gGlobalVariables.trace_plane_dist = ptrace->plane.dist;

	if (ptrace->ent)
	{
		gGlobalVariables.trace_ent = ptrace->ent;
	}
	else
	{
		gGlobalVariables.trace_ent = sv.edicts;
	}

	gGlobalVariables.trace_hitgroup = ptrace->hitgroup;
}

void PF_TraceToss_DLL(edict_t* pent, edict_t* pentToIgnore, TraceResult* ptr)
{
	PF_TraceToss_Shared(pent, pentToIgnore ? pentToIgnore : &sv.edicts[0]);

	ptr->fAllSolid = (int)gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = (int)gGlobalVariables.trace_startsolid;
	ptr->fInOpen = (int)gGlobalVariables.trace_inopen;
	ptr->fInWater = (int)gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	ptr->pHit = gGlobalVariables.trace_ent;

	VectorCopy(gGlobalVariables.trace_endpos, ptr->vecEndPos);
	VectorCopy(gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal);

	ptr->iHitgroup = gGlobalVariables.trace_hitgroup;
}

int TraceMonsterHull(edict_t* pEdict, const float* v1, const float* v2, int fNoMonsters, edict_t* pentToSkip, TraceResult* ptr)
{
	qboolean monsterClip = (pEdict->v.flags & FL_MONSTERCLIP) ? TRUE : FALSE;
	trace_t trace = SV_Move(v1, pEdict->v.mins, pEdict->v.maxs, v2, fNoMonsters, pentToSkip, monsterClip);
	if (ptr)
	{
		ptr->fAllSolid = trace.allsolid;
		ptr->fStartSolid = trace.startsolid;
		ptr->fInOpen = trace.inopen;
		ptr->fInWater = trace.inwater;
		ptr->flPlaneDist = trace.plane.dist;
		ptr->pHit = trace.ent;
		ptr->iHitgroup = trace.hitgroup;
		ptr->flFraction = trace.fraction;

		VectorCopy(trace.endpos, ptr->vecEndPos);
		VectorCopy(trace.plane.normal, ptr->vecPlaneNormal);
	}

	return trace.allsolid == true || trace.fraction != 1.0f;
}

int PF_newcheckclient(int check)
{
	int i;
	unsigned char* pvs;
	edict_t* ent;
	mleaf_t* leaf;
	vec3_t org;

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;
	i = 1;
	if (check != svs.maxclients)
		i = check + 1;
	while (1)
	{
		if (i == svs.maxclients + 1)
			i = 1;

		ent = &sv.edicts[i];
		if (i == check)
			break;
		if (!ent->free && ent->pvPrivateData && !(ent->v.flags & FL_NOTARGET))
			break;
		++i;
	}

	VectorAdd(ent->v.view_ofs, ent->v.origin, org);
	leaf = Mod_PointInLeaf(org, sv.worldmodel);
	pvs = Mod_LeafPVS(leaf, sv.worldmodel);
	Q_memcpy(checkpvs, pvs, (sv.worldmodel->numleafs + 7) >> 3);
	return i;
}

edict_t* PF_checkclient_I(edict_t* pEdict)
{
	edict_t* ent;
	mleaf_t* leaf;
	int l;
	vec3_t view;

	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient(sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	ent = &sv.edicts[sv.lastcheck];
	if (!ent->free && ent->pvPrivateData)
	{
		VectorAdd(pEdict->v.view_ofs, pEdict->v.origin, view);

		leaf = Mod_PointInLeaf(view, sv.worldmodel);
		l = (leaf - sv.worldmodel->leafs) - 1;
		if (l >= 0 && ((1 << (l & 7)) & checkpvs[l >> 3]))
		{
			c_invis++;
			return ent;
		}
		else
		{
			c_notvis++;
		}
	}
	return &sv.edicts[0];
}

mnode_t* PVSNode(mnode_t* node, vec_t* emins, vec_t* emaxs)
{
	mplane_t* splitplane;
	int sides;
	mnode_t* splitNode;

	if (node->visframe != r_visframecount)
		return NULL;

	if (node->contents < 0)
		return node->contents != CONTENT_SOLID ? node : NULL;


	splitplane = node->plane;

	sides = BOX_ON_PLANE_SIDE(emins, emaxs, splitplane);

	if (sides & 1)
	{
		splitNode = PVSNode(node->children[0], emins, emaxs);
		if (splitNode)
			return splitNode;
	}

	if (sides & 2)
		return PVSNode(node->children[1], emins, emaxs);

	return NULL;
}

void PVSMark(model_t* pmodel, unsigned char* ppvs)
{
	r_visframecount++;
	for (int i = 0; i < pmodel->numleafs; i++)
	{
		if ((1 << (i & 7)) & ppvs[i >> 3])
		{
			mnode_t* node = (mnode_t*)&pmodel->leafs[i + 1];
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

edict_t* PVSFindEntities(edict_t* pplayer)
{
	edict_t* pent;
	edict_t* pchain;
	edict_t* pentPVS;
	vec3_t org;
	unsigned char* ppvs;
	mleaf_t* pleaf;

	VectorAdd(pplayer->v.view_ofs, pplayer->v.origin, org);

	pleaf = Mod_PointInLeaf(org, sv.worldmodel);
	ppvs = Mod_LeafPVS(pleaf, sv.worldmodel);
	PVSMark(sv.worldmodel, ppvs);
	pchain = sv.edicts;

	for (int i = 1; i < sv.num_edicts; i++)
	{
		pent = &sv.edicts[i];
		if (pent->free)
			continue;

		pentPVS = pent;
		if (pent->v.movetype == MOVETYPE_FOLLOW && pent->v.aiment)
			pentPVS = pent->v.aiment;

		if (PVSNode(sv.worldmodel->nodes, pentPVS->v.absmin, pentPVS->v.absmax))
		{
			pent->v.chain = pchain;
			pchain = pent;
		}

	}

	if (cl.worldmodel)
	{
		r_oldviewleaf = NULL; //clientside only
		R_MarkLeaves();
	}
	return pchain;
}

qboolean ValidCmd(char* pCmd)
{
	int len = Q_strlen(pCmd);
	return len && (pCmd[len - 1] == '\n' || pCmd[len - 1] == ';');
}

void PF_stuffcmd_I(edict_t* pEdict, char* szFmt, ...)
{
	int entnum;
	client_t* old;
	va_list argptr;
	static char szOut[1024];

	va_start(argptr, szFmt);
	entnum = NUM_FOR_EDICT(pEdict);
	_vsnprintf(szOut, sizeof(szOut), szFmt, argptr);
	va_end(argptr);

	szOut[sizeof(szOut) - 1] = 0;
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("\n!!!\n\nStuffCmd:  Some entity tried to stuff '%s' to console "
			"buffer of entity %i when maxclients was set to %i, ignoring\n\n"), szOut, entnum, svs.maxclients);
	}
	else
	{
		if (ValidCmd(szOut))
		{
			old = host_client;
			host_client = &svs.clients[entnum - 1];
			Host_ClientCommands("%s", szOut);
			host_client = old;
		}
		else
		{
			Con_Printf(const_cast<char*>("Tried to stuff bad command %s\n"), szOut);
		}
	}
}

void PF_localcmd_I(char* str)
{
	if (ValidCmd(str))
		Cbuf_AddText(str);
	else
		Con_Printf(const_cast<char*>("Error, bad server command %s\n"), str);
}

void PF_localexec_I(void)
{
	Cbuf_Execute();
}

edict_t* FindEntityInSphere(edict_t* pEdictStartSearchAfter, const float* org, float rad)
{
	int e = pEdictStartSearchAfter ? NUM_FOR_EDICT(pEdictStartSearchAfter) : 0;

	for (int i = e + 1; i < sv.num_edicts; i++)
	{
		edict_t* ent = &sv.edicts[i];
		if (ent->free || !ent->v.classname)
			continue;

		if (i <= svs.maxclients && !svs.clients[i - 1].active)
			continue;

		float distSquared = 0.0;
		for (int j = 0; j < 3 && distSquared <= (rad * rad); j++)
		{
			float eorg;
			if (org[j] < ent->v.absmin[j])
				eorg = org[j] - ent->v.absmin[j];
			else if (org[j] > ent->v.absmax[j])
				eorg = org[j] - ent->v.absmax[j];
			else
				eorg = 0.0;

			distSquared = eorg * eorg + distSquared;
		}

		if (distSquared <= (rad * rad))
		{
			return ent;
		}
	}

	return &sv.edicts[0];
}

edict_t* PF_Spawn_I(void)
{
	return ED_Alloc();
}

edict_t* CreateNamedEntity(int className)
{
	edict_t* pedict;
	ENTITYINIT pEntityInit;

	if (!className)
		Sys_Error("Spawned a NULL entity!");

	pedict = ED_Alloc();
	pedict->v.classname = className;

	pEntityInit = GetEntityInit(&pr_strings[className]);
	if (pEntityInit)
	{
		pEntityInit(&pedict->v);
		return pedict;
	}
	else
	{
		ED_Free(pedict);
		Con_DPrintf(const_cast<char*>("Can't create entity: %s\n"), &pr_strings[className]);
		return NULL;
	}
}

void PF_Remove_I(edict_t* ed)
{
	ED_Free(ed);
}

edict_t* PF_find_Shared(int eStartSearchAfter, int iFieldToMatch, const char* szValueToFind)
{
	for (int e = eStartSearchAfter + 1; e < sv.num_edicts; e++)
	{
		edict_t* ed = &sv.edicts[e];
		if (ed->free)
			continue;

		char* t = &pr_strings[*(string_t*)((size_t)&ed->v + iFieldToMatch)];
		if (t == 0 || t == &pr_strings[0])
			continue;

		if (!Q_strcmp(t, szValueToFind))
			return ed;

	}

	return sv.edicts;
}

int iGetIndex(const char* pszField)
{
	char sz[512];

	Q_strncpy(sz, pszField, sizeof(sz) - 1);
	sz[sizeof(sz) - 1] = 0;
	Q_strlwr(sz);

	if (!Q_strcmp(sz, "classname"))
		return offsetof(entvars_t, classname);
	if (!Q_strcmp(sz, "model"))
		return offsetof(entvars_t, model);
	if (!Q_strcmp(sz, "viewmodel"))
		return offsetof(entvars_t, viewmodel);
	if (!Q_strcmp(sz, "weaponmodel"))
		return offsetof(entvars_t, weaponmodel);
	if (!Q_strcmp(sz, "netname"))
		return offsetof(entvars_t, netname);
	if (!Q_strcmp(sz, "target"))
		return offsetof(entvars_t, target);
	if (!Q_strcmp(sz, "targetname"))
		return offsetof(entvars_t, targetname);
	if (!Q_strcmp(sz, "message"))
		return offsetof(entvars_t, message);
	if (!Q_strcmp(sz, "noise"))
		return offsetof(entvars_t, noise);
	if (!Q_strcmp(sz, "noise1"))
		return offsetof(entvars_t, noise1);
	if (!Q_strcmp(sz, "noise2"))
		return offsetof(entvars_t, noise2);
	if (!Q_strcmp(sz, "noise3"))
		return offsetof(entvars_t, noise3);
	if (!Q_strcmp(sz, "globalname"))
		return offsetof(entvars_t, globalname);

	return -1;
}

edict_t* FindEntityByString(edict_t* pEdictStartSearchAfter, const char* pszField, const char* pszValue)
{
	int e;
	int iField = iGetIndex(pszField);
	if (iField == -1 || !pszValue)
		return NULL;

	if (pEdictStartSearchAfter)
		e = NUM_FOR_EDICT(pEdictStartSearchAfter);
	else
		e = 0;

	return PF_find_Shared(e, iField, pszValue);
}

int GetEntityIllum(edict_t* pEnt)
{
	int iReturn;
	colorVec cvFloorColor = { 0, 0, 0, 0 };
	int iIndex;

	if (!pEnt)
		return -1;

	iIndex = NUM_FOR_EDICT(pEnt);
	if (iIndex <= svs.maxclients)
		return pEnt->v.light_level;

	if (cls.state != ca_connected && cls.state != ca_uninitialized && cls.state != ca_active)
		return 128;

#ifndef SWDS
	cvFloorColor = cl_entities[iIndex].cvFloorColor;
	iReturn = (cvFloorColor.r + cvFloorColor.g + cvFloorColor.b) / 3;
#else
	iReturn = 0;
#endif

	return iReturn;
}

qboolean PR_IsEmptyString(char* s)
{
	return s[0] < ' ';
}

int PF_precache_sound_I(char* s)
{
	if (!s)
		Host_Error(__FUNCTION__ ": NULL pointer");

	if (PR_IsEmptyString(s))
		Host_Error(__FUNCTION__ ": Bad string '%s'", s);

	if (s[0] == '!')
		Host_Error(__FUNCTION__ ": '%s' do not precache sentence names!", s);

	if (sv.state == ss_loading)
	{
		sv.sound_precache_hashedlookup_built = 0;

		for (int i = 0; i < MAX_SOUNDS; i++)
		{
			if (!sv.sound_precache[i])
			{
				sv.sound_precache[i] = s;

				return i;
			}

			if (!Q_strcasecmp(sv.sound_precache[i], s))
				return i;
		}

		Host_Error(__FUNCTION__ ": Sound '%s' failed to precache because the item count is over the %d limit.\n"
			"Reduce the number of brush models and/or regular models in the map to correct this.", s, MAX_SOUNDS);
	}

	// precaching not enabled. check if already exists.
	for (int i = 0; i < MAX_SOUNDS; i++)
	{
		if (sv.sound_precache[i] && !Q_strcasecmp(sv.sound_precache[i], s))
			return i;
	}

	Host_Error(__FUNCTION__ ": '%s' Precache can only be done in spawn functions", s);

	return 0;
}

unsigned short EV_Precache(int type, const char* psz)
{
	if (!psz)
		Host_Error(__FUNCTION__ ": NULL pointer");

	if (PR_IsEmptyString((char*)psz))
		Host_Error(__FUNCTION__ ": Bad string '%s'", psz);

	if (sv.state == ss_loading)
	{
		for (int i = 1; i < MAX_EVENTS; i++)
		{
			event_t* ev = &sv.event_precache[i];
			if (!ev->filename)
			{
				if (type != 1)
					Host_Error(__FUNCTION__ ":  only file type 1 supported currently\n");

				char szpath[MAX_PATH];
				_snprintf(szpath, sizeof(szpath), "%s", psz);
				COM_FixSlashes(szpath);

				int scriptSize = 0;
				char* evScript = (char*)COM_LoadFile(szpath, 5, &scriptSize);
				if (!evScript)
					Host_Error(__FUNCTION__ ":  file %s missing from server\n", psz);
				sv.event_precache[i].filename = psz;
				sv.event_precache[i].filesize = scriptSize;
				sv.event_precache[i].pszScript = evScript;
				sv.event_precache[i].index = i;

				return i;
			}

			if (!Q_strcasecmp(ev->filename, psz))
				return i;
		}
		Host_Error(__FUNCTION__ ": '%s' overflow", psz);
	}
	else
	{
		for (int i = 1; i < MAX_EVENTS; i++)
		{
			event_t* ev = &sv.event_precache[i];
			if (!Q_strcasecmp(ev->filename, psz))
				return i;
		}

		Host_Error(__FUNCTION__ ": '%s' Precache can only be done in spawn functions", psz);
	}

	return 1;
}

void EV_PlayReliableEvent(client_t* cl, int entindex, unsigned short eventindex, float delay, event_args_t* pargs)
{
	unsigned char data[1024];
	event_args_t from;
	event_args_t to;
	sizebuf_t msg;

	if (cl->fakeclient)
		return;

	Q_memset(&msg, 0, sizeof(msg));
	msg.buffername = "Reliable Event";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof(data);

	Q_memset(&from, 0, sizeof(from));
	to = *pargs;
	to.entindex = entindex;

	MSG_WriteByte(&msg, svc_event_reliable);
	MSG_StartBitWriting(&msg);
	MSG_WriteBits(eventindex, 10);
	DELTA_WriteDelta((unsigned char*)&from, (unsigned char*)&to, 1, g_peventdelta, 0);
	if (delay == 0.0)
	{
		MSG_WriteBits(0, 1);
	}
	else
	{
		MSG_WriteBits(1, 1);
		MSG_WriteBits((int)(delay * 100.0f), 16);
	}
	MSG_EndBitWriting(&msg);

	if (msg.cursize + cl->netchan.message.cursize <= cl->netchan.message.maxsize)
		SZ_Write(&cl->netchan.message, msg.data, msg.cursize);
	else
		Netchan_CreateFragments(true, &cl->netchan, &msg);
}

void EV_Playback(int flags, const edict_t* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
{
	client_t* cl;
	signed int j;
	event_args_t eargs;
	vec3_t event_origin;
	int invoker;
	int slot;
	int leafnum;

	if (flags & FEV_CLIENT)
		return;

	invoker = -1;
	Q_memset(&eargs, 0, sizeof(eargs));
	if (!VectorCompare(origin, vec3_origin))
	{
		VectorCopy(origin, eargs.origin);
		eargs.flags |= FEVENT_ORIGIN;
	}
	if (!VectorCompare(angles, vec3_origin))
	{
		VectorCopy(angles, eargs.angles);
		eargs.flags |= FEVENT_ANGLES;
	}
	eargs.fparam1 = fparam1;
	eargs.fparam2 = fparam2;
	eargs.iparam1 = iparam1;
	eargs.iparam2 = iparam2;
	eargs.bparam1 = bparam1;
	eargs.bparam2 = bparam2;

	if (eventindex < 1 || eventindex >= MAX_EVENTS)
	{
		Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  index out of range %i\n"), eventindex);
		return;
	}

	if (!sv.event_precache[eventindex].pszScript)
	{
		Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  no event for index %i\n"), eventindex);
		return;
	}

	if (pInvoker)
	{
		VectorCopy(pInvoker->v.origin, event_origin);

		invoker = NUM_FOR_EDICT(pInvoker);
		if (invoker >= 1)
		{
			if (invoker <= svs.maxclients)
			{
				if (pInvoker->v.flags & FL_DUCKING)
					eargs.ducking = 1;
			}
		}
		if (!(eargs.flags & FEVENT_ORIGIN))
		{
			VectorCopy(pInvoker->v.origin, eargs.origin);
		}
		if (!(eargs.flags & FEVENT_ANGLES))
		{
			VectorCopy(pInvoker->v.angles, eargs.angles);
		}
	}
	else
	{
		VectorCopy(eargs.origin, event_origin);
	}

	leafnum = SV_PointLeafnum(event_origin);

	for (slot = 0; slot < svs.maxclients; slot++)
	{
		cl = &svs.clients[slot];
		if (!cl->active || !cl->spawned || !cl->connected || !cl->fully_connected || cl->fakeclient)
			continue;

		if (pInvoker)
		{
			if (pInvoker->v.groupinfo)
			{
				if (cl->edict->v.groupinfo)
				{
					if (g_groupop)
					{
						if (g_groupop == GROUP_OP_NAND && (cl->edict->v.groupinfo & pInvoker->v.groupinfo))
							continue;
					}
					else
					{
						if (!(cl->edict->v.groupinfo & pInvoker->v.groupinfo))
							continue;
					}
				}
			}
		}

		if (pInvoker && !(flags & FEV_GLOBAL))
		{
			if (!SV_ValidClientMulticast(cl, leafnum, MSG_FL_PAS))
				continue;
		}

		if (cl == host_client && (flags & FEV_NOTHOST) && cl->lw)
			continue;

		if ((flags & FEV_HOSTONLY) && cl->edict != pInvoker)
			continue;

		if (flags & FEV_RELIABLE)
		{
			EV_PlayReliableEvent(cl, pInvoker ? NUM_FOR_EDICT(pInvoker) : 0, eventindex, delay, &eargs);
			continue;
		}

		if (flags & FEV_UPDATE)
		{
			for (j = 0; j < MAX_EVENT_QUEUE; j++)
			{
				event_info_s* ei = &cl->events.ei[j];
				if (ei->index == eventindex && invoker != -1 && ei->entity_index == invoker)
					break;
			}

			if (j < MAX_EVENT_QUEUE)
			{
				event_info_s* ei = &cl->events.ei[j];
				ei->entity_index = -1;
				ei->index = eventindex;
				ei->packet_index = -1;
				if (pInvoker)
					ei->entity_index = invoker;
				Q_memcpy(&ei->args, &eargs, sizeof(ei->args));
				ei->fire_time = delay;
				continue;
			}
		}

		for (j = 0; j < MAX_EVENT_QUEUE; j++)
		{
			event_info_s* ei = &cl->events.ei[j];
			if (ei->index == 0)
				break;
		}

		if (j < MAX_EVENT_QUEUE)
		{
			event_info_s* ei = &cl->events.ei[j];
			ei->entity_index = -1;
			ei->index = eventindex;
			ei->packet_index = -1;
			if (pInvoker)
				ei->entity_index = invoker;
			Q_memcpy(&ei->args, &eargs, sizeof(ei->args));
			ei->fire_time = delay;
		}
	}
}

void EV_SV_Playback(int flags, int clientindex, unsigned short eventindex, float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
{
	if (flags & FEV_CLIENT)
		return;

	if (clientindex < 0 || clientindex >= svs.maxclients)
		Host_Error(__FUNCTION__ ":  Client index %i out of range\n", clientindex);

	edict_t* pEdict = svs.clients[clientindex].edict;
	EV_Playback(flags, pEdict, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
}

int PF_precache_model_I(char* s)
{
	int iOptional = 0;
	if (!s)
		Host_Error(__FUNCTION__ ": NULL pointer");

	if (PR_IsEmptyString(s))
		Host_Error(__FUNCTION__ ": Bad string '%s'", s);

	if (*s == '!')
	{
		s++;
		iOptional = 1;
	}

	if (sv.state == ss_loading)
	{
		for (int i = 0; i < MAX_MODELS; i++)
		{
			if (!sv.model_precache[i])
			{
				sv.model_precache[i] = s;

				sv.models[i] = Mod_ForName(s, TRUE, TRUE);

				if (!iOptional)
					sv.model_precache_flags[i] |= RES_FATALIFMISSING;

				return i;
			}

			// use case-sensitive names to increase performance
			if (!Q_strcasecmp(sv.model_precache[i], s))
				return i;
		}
		Host_Error(__FUNCTION__ ": Model '%s' failed to precache because the item count is over the %d limit.\n"
			"Reduce the number of brush models and/or regular models in the map to correct this.", s, MAX_MODELS);
	}
	else
	{
		for (int i = 0; i < MAX_MODELS; i++)
		{
			// use case-sensitive names to increase performance
			if (!Q_strcasecmp(sv.model_precache[i], s))
				return i;
		}
		Host_Error(__FUNCTION__ ": '%s' Precache can only be done in spawn functions", s);
	}
}

int PF_precache_generic_I(char* s)
{
	if (!s)
		Host_Error(__FUNCTION__ ": NULL pointer");

	if (PR_IsEmptyString(s))
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ": Bad string '%s'"), s);
	}

	if (sv.state == ss_loading)
	{
		for (int i = 0; i < MAX_GENERIC; i++)
		{
			if (!sv.generic_precache[i])
			{
				sv.generic_precache[i] = (char*)s;
				return i;
			}

			if (!Q_strcasecmp(sv.generic_precache[i], s))
				return i;
		}
		Host_Error(__FUNCTION__ ": Generic item '%s' failed to precache because the item count is over the %d limit.\n"
			"Reduce the number of brush models and/or regular models in the map to correct this.", s, MAX_GENERIC);
	}
	else
	{
		for (int i = 0; i < MAX_GENERIC; i++)
		{
			if (!Q_strcasecmp(sv.generic_precache[i], s))
				return i;
		}
		Host_Error(__FUNCTION__ ": '%s' Precache can only be done in spawn functions", s);
	}
}

int PF_IsMapValid_I(char* mapname)
{
	char cBuf[MAX_PATH];
	if (!mapname || Q_strlen(mapname) == 0)
		return 0;

	_snprintf(cBuf, sizeof(cBuf), "maps/%.32s.bsp", mapname);
	return FS_FileExists(cBuf);
}

int PF_NumberOfEntities_I(void)
{
	int ent_count = 0;
	for (int i = 1; i < sv.num_edicts; i++)
	{
		if (!sv.edicts[i].free)
			ent_count++;
	}

	return ent_count;
}

char* PF_GetInfoKeyBuffer_I(edict_t* e)
{
	int e1;
	char* value;

	if (e)
	{
		e1 = NUM_FOR_EDICT(e);
		if (e1)
		{
			if (e1 > 32)
				value = (char*)"";
			else
				value = (char*)&svs.clients[e1 - 1].userinfo;
		}
		else
		{
			value = Info_Serverinfo();
		}
	}
	else
	{
		value = localinfo;
	}

	return value;
}

char* PF_InfoKeyValue_I(char* infobuffer, char* key)
{
	return (char*)Info_ValueForKey(infobuffer, key);
}

void PF_SetKeyValue_I(char* infobuffer, char* key, char* value)
{
	if (infobuffer == localinfo)
	{
		Info_SetValueForKey(infobuffer, key, value, MAX_LOCALINFO);
	}
	else
	{
		if (infobuffer != Info_Serverinfo())
		{
			Sys_Error("Can't set client keys with SetKeyValue");
		}
		
		Info_SetValueForKey(infobuffer, key, value, 512);
	}
}

void PF_RemoveKey_I(char* s, const char* key)
{
	Info_RemoveKey(s, key);
}

void PF_SetClientKeyValue_I(int clientIndex, char* infobuffer, char* key, char* value)
{
	client_t* pClient;

	if (infobuffer == localinfo ||
		infobuffer == Info_Serverinfo() ||
		clientIndex < 1 ||
		clientIndex > svs.maxclients)
	{
		return;
	}

	if (Q_strcmp(Info_ValueForKey(infobuffer, key), value))
	{
		Info_SetValueForStarKey(infobuffer, key, value, MAX_INFO_STRING);
		pClient = &svs.clients[clientIndex - 1];
		pClient->sendinfo = TRUE;
		pClient->sendinfo_time = 0.0f;
	}
}

int PF_walkmove_I(edict_t* ent, float yaw, float dist, int iMode)
{
	vec3_t	move;
	int 	returnValue;

	if (!(ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
	{
		return FALSE;
	}

	yaw = yaw * M_PI * 2.0 / 360.0;

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0;

	switch (iMode)
	{
	default:
	case WALKMOVE_NORMAL:
		returnValue = SV_movestep(ent, move, TRUE);
		break;
	case WALKMOVE_WORLDONLY:
		returnValue = SV_movetest(ent, move, TRUE);
		break;
	case WALKMOVE_CHECKONLY:
		returnValue = SV_movestep(ent, move, FALSE);
		break;
	}

	return returnValue;
}

int PF_droptofloor_I(edict_t* ent)
{
	vec3_t end;
	trace_t trace;
	qboolean monsterClip = (ent->v.flags & FL_MONSTERCLIP) ? TRUE : FALSE;

	VectorCopy(ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent, monsterClip);
	if (trace.allsolid)
		return -1;

	if (trace.fraction == 1.0f)
		return 0;

	VectorCopy(trace.endpos, ent->v.origin);
	SV_LinkEdict(ent, FALSE);
	ent->v.flags |= FL_ONGROUND;
	ent->v.groundentity = trace.ent;

	return 1;
}

int PF_DecalIndex(const char* name)
{
	for (int i = 0; i < sv_decalnamecount; i++)
	{
		if (!Q_strcasecmp(sv_decalnames[i].name, name))
			return i;
	}

	return -1;
}

void PF_lightstyle_I(int style, char* val)
{
	sv.lightstyles[style] = val;

	if (sv.state != ss_active)
		return;

	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t* cl = &svs.clients[i];
		if ((cl->active || cl->spawned) && !cl->fakeclient)
		{
			MSG_WriteChar(&cl->netchan.message, svc_lightstyle);
			MSG_WriteChar(&cl->netchan.message, style);
			MSG_WriteString(&cl->netchan.message, val);
		}
	}
}

int PF_checkbottom_I(edict_t* pEdict)
{
	return SV_CheckBottom(pEdict);
}

int PF_pointcontents_I(const float* rgflVector)
{
	return SV_PointContents(rgflVector);
}

void PF_aim_I(edict_t* ent, float speed, float* rgflReturn)
{
	vec3_t start;
	vec3_t dir;
	vec3_t end;
	vec3_t bestdir;
	trace_t tr;
	float dist;
	float bestdist;

	if (!ent || (ent->v.flags & FL_FAKECLIENT))
	{
		VectorCopy(gGlobalVariables.v_forward, rgflReturn);
		return;
	}

	VectorCopy(gGlobalVariables.v_forward, dir);
	VectorAdd(ent->v.origin, ent->v.view_ofs, start);
	VectorMA(start, 2048.0, dir, end);
	tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, FALSE);

	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM && (ent->v.team <= 0 || ent->v.team != tr.ent->v.team))
	{
		VectorCopy(gGlobalVariables.v_forward, rgflReturn);
		return;
	}

	VectorCopy(dir, bestdir);
	bestdist = sv_aim.value;

	for (int i = 1; i < sv.num_edicts; i++)
	{
		edict_t* check = &sv.edicts[i];
		if (check->v.takedamage != DAMAGE_AIM || (check->v.flags & FL_FAKECLIENT) || check == ent)
			continue;

		if (ent->v.team > 0 && ent->v.team == check->v.team)
			continue;

		for (int j = 0; j < 3; j++)
		{
			end[j] = (check->v.maxs[j] + check->v.mins[j]) * 0.75 + check->v.origin[j] + ent->v.view_ofs[j] * 0.0;
		}

		VectorSubtract(end, start, dir);
		VectorNormalize(dir);
		dist = DotProduct(dir, gGlobalVariables.v_forward);

		if (dist >= bestdist)
		{
			tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, FALSE);
			if (tr.ent == check)
			{
				bestdist = dist;
				VectorCopy(dir, bestdir);
			}
		}
	}

	VectorCopy(bestdir, rgflReturn);
}

void PF_changeyaw_I(edict_t* ent)
{
	float ideal;
	float current;
	float move;
	float speed;

	current = anglemod(ent->v.angles[YAW]);
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;
	if (current == ideal)
		return;

	move = ideal - current;
	if (ideal <= current)
	{
		if (move <= -180.0)
			move += 360.0;
	}
	else
	{
		if (move >= 180.0)
			move -= 360.0;
	}

	if (move <= 0.0f)
	{
		if (move < -speed)
			move = -speed;
	}
	else
	{
		if (move > speed)
			move = speed;
	}

	ent->v.angles[YAW] = anglemod(move + current);
}

void PF_changepitch_I(edict_t* ent)
{
	float ideal;
	float current;
	float move;
	float speed;

	current = anglemod(ent->v.angles[PITCH]);
	ideal = ent->v.idealpitch;
	speed = ent->v.pitch_speed;
	if (current == ideal)
		return;

	move = ideal - current;
	if (ideal <= current)
	{
		if (move <= -180.0)
			move += 360.0;
	}
	else
	{
		if (move >= 180.0)
			move -= 360;
	}

	if (move <= 0.0)
	{
		if (move < -speed)
			move = -speed;
	}
	else
	{
		if (move > speed)
			move = speed;
	}

	ent->v.angles[PITCH] = anglemod(move + current);
}

void PF_setview_I(const edict_t* clientent, const edict_t* viewent)
{
	int clientnum = NUM_FOR_EDICT(clientent);
	if (clientnum < 1 || clientnum > svs.maxclients)
		Host_Error("%s: not a client", __func__);

	client_t* client = &svs.clients[clientnum - 1];
	if (!client->fakeclient)
	{
		client->pViewEntity = viewent;
		MSG_WriteByte(&client->netchan.message, svc_setview);
		MSG_WriteShort(&client->netchan.message, NUM_FOR_EDICT(viewent));
	}
}

void PF_crosshairangle_I(const edict_t* clientent, float pitch, float yaw)
{
	int clientnum = NUM_FOR_EDICT(clientent);
	if (clientnum < 1 || clientnum > svs.maxclients)
		return;


	client_t* client = &svs.clients[clientnum - 1];
	if (!client->fakeclient)
	{
		if (pitch > 180.0)
			pitch -= 360.0;

		if (pitch < -180.0)
			pitch += 360.0;

		if (yaw > 180.0)
			yaw -= 360.0;

		if (yaw < -180.0)
			yaw += 360.0;

		MSG_WriteByte(&client->netchan.message, svc_crosshairangle);
		MSG_WriteChar(&client->netchan.message, (int)(pitch * 5.0));
		MSG_WriteChar(&client->netchan.message, (int)(yaw * 5.0));
	}
}

edict_t* PF_CreateFakeClient_I(const char* netname)
{
	client_t* fakeclient;
	edict_t* ent;

	int i = 0;
	fakeclient = svs.clients;
	for (i = 0; i < svs.maxclients; i++, fakeclient++)
	{
		if (!fakeclient->active && !fakeclient->spawned && !fakeclient->connected)
			break;
	}

	if (i >= svs.maxclients)
		return NULL;

	ent = EDICT_NUM(i + 1);
	if (fakeclient->frames)
		SV_ClearFrames(&fakeclient->frames);

	Q_memset(fakeclient, 0, sizeof(client_t));
	fakeclient->resourcesneeded.pPrev = &fakeclient->resourcesneeded;
	fakeclient->resourcesneeded.pNext = &fakeclient->resourcesneeded;
	fakeclient->resourcesonhand.pPrev = &fakeclient->resourcesonhand;
	fakeclient->resourcesonhand.pNext = &fakeclient->resourcesonhand;

	Q_strncpy(fakeclient->name, netname, sizeof(fakeclient->name) - 1);
	fakeclient->name[sizeof(fakeclient->name) - 1] = 0;

	fakeclient->active = TRUE;
	fakeclient->spawned = TRUE;
	fakeclient->fully_connected = TRUE;
	fakeclient->connected = TRUE;
	fakeclient->fakeclient = TRUE;
	fakeclient->userid = g_userid++;
	fakeclient->uploading = FALSE;
	fakeclient->edict = ent;
	ent->v.netname = (size_t)fakeclient->name - (size_t)pr_strings;
	ent->v.pContainingEntity = ent;
	ent->v.flags = FL_FAKECLIENT | FL_CLIENT;

	Info_SetValueForKey(fakeclient->userinfo, "name", netname, MAX_INFO_STRING);
	Info_SetValueForKey(fakeclient->userinfo, "model", "gordon", MAX_INFO_STRING);
	Info_SetValueForKey(fakeclient->userinfo, "topcolor", "1", MAX_INFO_STRING);
	Info_SetValueForKey(fakeclient->userinfo, "bottomcolor", "1", MAX_INFO_STRING);
	fakeclient->sendinfo = TRUE;
	SV_ExtractFromUserinfo(fakeclient);

	fakeclient->network_userid.m_SteamID = ISteamGameServer_CreateUnauthenticatedUserConnection();
	fakeclient->network_userid.idtype = AUTH_IDTYPE_STEAM;
	ISteamGameServer_BUpdateUserData(fakeclient->network_userid.m_SteamID, netname, 0);

	return ent;
}

void PF_RunPlayerMove_I(edict_t* fakeclient, const float* viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, unsigned char impulse, unsigned char msec)
{
	usercmd_t cmd;
	edict_t* oldclient;
	client_t* old;
	int entnum;

	oldclient = sv_player;
	old = host_client;
	entnum = NUM_FOR_EDICT(fakeclient);
	sv_player = fakeclient;
	host_client = &svs.clients[entnum - 1];

	host_client->svtimebase = host_frametime + sv.time - msec / 1000.0;
	Q_memset(&cmd, 0, sizeof(cmd));
	cmd.lightlevel = 0;
	pmove = &g_svmove;

	VectorCopy(viewangles, cmd.viewangles);
	cmd.forwardmove = forwardmove;
	cmd.sidemove = sidemove;
	cmd.upmove = upmove;
	cmd.buttons = buttons;
	cmd.impulse = impulse;
	cmd.msec = msec;

	SV_PreRunCmd();
	SV_RunCmd(&cmd, 0);

	host_client->lastcmd = cmd;

	sv_player = oldclient;
	host_client = old;
}

sizebuf_t* WriteDest_Parm(int dest)
{
	int entnum;

	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;
	case MSG_ONE:
	case MSG_ONE_UNRELIABLE:
		entnum = NUM_FOR_EDICT(gMsgEntity);
		if (entnum < 1 || entnum > svs.maxclients)
		{
			Host_Error(__FUNCTION__ ": not a client");
		}
		if (dest == MSG_ONE)
		{
			return &svs.clients[entnum - 1].netchan.message;
		}
		else
		{
			return &svs.clients[entnum - 1].datagram;
		}
	case MSG_INIT:
		return &sv.signon;
	case MSG_ALL:
		return &sv.reliable_datagram;
	case MSG_PVS:
	case MSG_PAS:
		return &sv.multicast;
	case MSG_SPEC:
		return &sv.spectator;
	default:
		Host_Error(__FUNCTION__ ": bad destination = %d", dest);
	}
}

void PF_MessageBegin_I(int msg_dest, int msg_type, const float* pOrigin, edict_t* ed)
{
	if (msg_dest == MSG_ONE || msg_dest == MSG_ONE_UNRELIABLE)
	{
		if (!ed)
			Sys_Error("MSG_ONE or MSG_ONE_UNRELIABLE with no target entity\n");
	}
	else
	{
		if (ed)
			Sys_Error("Invalid message: Cannot use broadcast message with a target entity");
	}

	if (gMsgStarted)
		Sys_Error("New message started when msg '%d' has not been sent yet", gMsgType);

	if (msg_type == 0)
		Sys_Error("Tried to create a message with a bogus message type ( 0 )");

	gMsgStarted = true;
	gMsgType = msg_type;
	gMsgEntity = ed;
	gMsgDest = msg_dest;
	if (msg_dest == MSG_PVS || msg_dest == MSG_PAS)
	{
		if (pOrigin)
		{
			VectorCopy(pOrigin, gMsgOrigin);
		}
	}

	gMsgBuffer.flags = FSB_ALLOWOVERFLOW;
	gMsgBuffer.cursize = 0;
}

void PF_MessageEnd_I(void)
{
	qboolean MsgIsVarLength = 0;
	if (!gMsgStarted)
		Sys_Error("MESSAGE_END called with no active message\n");
	gMsgStarted = 0;

	if (gMsgEntity && (gMsgEntity->v.flags & FL_FAKECLIENT))
		return;

	if (gMsgBuffer.flags & FSB_OVERFLOWED)
		Sys_Error("MESSAGE_END called, but message buffer from .dll had overflowed\n");

	if (gMsgType > SVC_LASTMSG)
	{
		UserMsg* pUserMsg = sv_gpUserMsgs;
		while (pUserMsg && pUserMsg->iMsg != gMsgType)
			pUserMsg = pUserMsg->next;

		if (!pUserMsg && gMsgDest == MSG_INIT)
		{
			pUserMsg = sv_gpNewUserMsgs;
			while (pUserMsg && pUserMsg->iMsg != gMsgType)
				pUserMsg = pUserMsg->next;
		}

		if (!pUserMsg)
		{
			Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  Unknown User Msg %d\n"), gMsgType);
			return;
		}

		if (pUserMsg->iSize == -1)
		{
			MsgIsVarLength = 1;

			// Limit packet sizes
			if (gMsgBuffer.cursize > MAX_USERPACKET_SIZE)
				Host_Error(__FUNCTION__ ": Refusing to send user message %s of %i bytes to client, user message size limit is %i bytes\n", pUserMsg->szName, gMsgBuffer.cursize, MAX_USERPACKET_SIZE);
		}
		else
		{
			if (pUserMsg->iSize != gMsgBuffer.cursize)
				Sys_Error("User Msg '%s': %d bytes written, expected %d\n", pUserMsg->szName, gMsgBuffer.cursize, pUserMsg->iSize);
		}
	}

	sizebuf_t* pBuffer = WriteDest_Parm(gMsgDest);
	if ((gMsgDest == MSG_BROADCAST && gMsgBuffer.cursize + pBuffer->cursize > pBuffer->maxsize) || !pBuffer->data)
		return;

	if (gMsgType > SVC_LASTMSG && (gMsgDest == MSG_ONE || gMsgDest == MSG_ONE_UNRELIABLE))
	{
		int entnum = NUM_FOR_EDICT(gMsgEntity);
		if (entnum < 1 || entnum > svs.maxclients)
			Host_Error("WriteDest_Parm: not a client");

		client_t* client = &svs.clients[entnum - 1];
		if (client->fakeclient || !client->hasusrmsgs || (!client->active && !client->spawned))
			return;
	}

	MSG_WriteByte(pBuffer, gMsgType);
	if (MsgIsVarLength)
		MSG_WriteByte(pBuffer, gMsgBuffer.cursize);
	MSG_WriteBuf(pBuffer, gMsgBuffer.cursize, gMsgBuffer.data);


	switch (gMsgDest)
	{
	case MSG_PVS:
		SV_Multicast(gMsgEntity, gMsgOrigin, MSG_FL_PVS, false);
		break;
	
	case MSG_PAS:
		SV_Multicast(gMsgEntity, gMsgOrigin, MSG_FL_PAS, false);
		break;

	case MSG_PVS_R:
		SV_Multicast(gMsgEntity, gMsgOrigin, MSG_FL_PAS, true); // TODO: Should be MSG_FL_PVS, investigation needed
		break;

	case MSG_PAS_R:
		SV_Multicast(gMsgEntity, gMsgOrigin, MSG_FL_PAS, true);
		break;

	default:
		return;
	}
}

void PF_WriteByte_I(int iValue)
{
	if (!gMsgStarted)
		Sys_Error("WRITE_BYTE called with no active message\n");
	MSG_WriteByte(&gMsgBuffer, iValue);
}

void PF_WriteChar_I(int iValue)
{
	if (!gMsgStarted)
		Sys_Error("WRITE_CHAR called with no active message\n");
	MSG_WriteChar(&gMsgBuffer, iValue);
}

void PF_WriteShort_I(int iValue)
{
	if (!gMsgStarted)
		Sys_Error("WRITE_SHORT called with no active message\n");
	MSG_WriteShort(&gMsgBuffer, iValue);
}

void PF_WriteLong_I(int iValue)
{
	if (!gMsgStarted)
		Sys_Error(__FUNCTION__ " called with no active message\n");
	MSG_WriteLong(&gMsgBuffer, iValue);
}

void PF_WriteAngle_I(float flValue)
{
	if (!gMsgStarted)
		Sys_Error(__FUNCTION__ " called with no active message\n");
	MSG_WriteAngle(&gMsgBuffer, flValue);
}

void PF_WriteCoord_I(float flValue)
{
	if (!gMsgStarted)
		Sys_Error(__FUNCTION__ " called with no active message\n");
	// с коэффициентом 8 достигается точность в 0.125 юнита, при передаче параметра двумя байтами устанавливается сетевое ограничение в 8192 юнита
	MSG_WriteShort(&gMsgBuffer, (int)(flValue * 8.0));
}

void PF_WriteString_I(const char* sz)
{
	if (!gMsgStarted)
		Sys_Error(__FUNCTION__ " called with no active message\n");
	MSG_WriteString(&gMsgBuffer, sz);
}

void PF_WriteEntity_I(int iValue)
{
	if (!gMsgStarted)
		Sys_Error(__FUNCTION__ " called with no active message\n");
	MSG_WriteShort(&gMsgBuffer, iValue);
}

void PF_makestatic_I(edict_t* ent)
{
	MSG_WriteByte(&sv.signon, svc_spawnstatic);
	MSG_WriteShort(&sv.signon, SV_ModelIndex(&pr_strings[ent->v.model]));
	MSG_WriteByte(&sv.signon, ent->v.sequence);
	MSG_WriteByte(&sv.signon, (int)ent->v.frame);
	MSG_WriteWord(&sv.signon, ent->v.colormap);
	MSG_WriteByte(&sv.signon, ent->v.skin);

	for (int i = 0; i < 3; i++)
	{
		MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
	}

	MSG_WriteByte(&sv.signon, ent->v.rendermode);
	if (ent->v.rendermode != kRenderNormal)
	{
		MSG_WriteByte(&sv.signon, (int)ent->v.renderamt);
		MSG_WriteByte(&sv.signon, (int)ent->v.rendercolor[0]);
		MSG_WriteByte(&sv.signon, (int)ent->v.rendercolor[1]);
		MSG_WriteByte(&sv.signon, (int)ent->v.rendercolor[2]);
		MSG_WriteByte(&sv.signon, ent->v.renderfx);
	}

	ED_Free(ent);
}

void PF_StaticDecal(const float* origin, int decalIndex, int entityIndex, int modelIndex)
{
	MSG_WriteByte(&sv.signon, svc_tempentity);
	MSG_WriteByte(&sv.signon, TE_BSPDECAL);
	MSG_WriteCoord(&sv.signon, origin[0]);
	MSG_WriteCoord(&sv.signon, origin[1]);
	MSG_WriteCoord(&sv.signon, origin[2]);
	MSG_WriteShort(&sv.signon, decalIndex);
	MSG_WriteShort(&sv.signon, entityIndex);

	if (entityIndex)
		MSG_WriteShort(&sv.signon, modelIndex);
}

void PF_setspawnparms_I(edict_t* ent)
{
	int i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients)
		Host_Error("Entity is not a client");
}

void PF_changelevel_I(char* s1, char* s2)
{
	static int last_spawncount;

	if (svs.spawncount != last_spawncount)
	{
		last_spawncount = svs.spawncount;
		SV_SkipUpdates();
		if (s2)
			Cbuf_AddText(va(const_cast<char*>("changelevel2 %s %s\n"), s1, s2));
		else
			Cbuf_AddText(va(const_cast<char*>("changelevel %s\n"), s1));
	}
}

#define IA	16807
#define IM	2147483647
#define IQ	127773
#define IR	2836

#define NTAB	32
#define NDIV (1+(IM-1)/NTAB)

static int32 idum = 0;

void SeedRandomNumberGenerator( void )
{
	idum = -(int)time(NULL);
	if (idum > 1000)
	{
		idum = -idum;
	}
	else if (idum > -1000)
	{
		idum -= 22261048;
	}
}

int32 ran1( void )
{
	int j;
	int32 k;
	static int32 iy = 0;
	static int32 iv[NTAB];

	if (idum <= 0 || !iy)
	{
		if (-(idum) < 1)
			idum = 1;
		else
			idum = -(idum);

		for (j = NTAB + 7; j >= 0; j--)
		{
			k = (idum) / IQ;
			idum = IA * (idum - k * IQ) - IR * k;

			if (idum < 0)
				idum += IM;
			if (j < NTAB)
				iv[j] = idum;
		}

		iy = iv[0];
	}

	k = (idum) / IQ;
	idum = IA * (idum - k * IQ) - IR * k;

	if (idum < 0)
		idum += IM;

	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

#define AM (1.0/IM)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

float fran1( void )
{
	float temp = (float)AM * ran1();

	if (temp > RNMX)
		return (float)RNMX;
	else
		return temp;
}

// Generate a random float number in the range [ flLow, flHigh ]
float RandomFloat( float flLow, float flHigh )
{
	float fl;

	RecEngRandomFloat(flLow, flHigh);

	fl = fran1(); // float in [0..1)
	return (fl * (flHigh - flLow)) + flLow; // float in [low..high)
}

#define MAX_RANDOM_RANGE 0x7FFFFFFFUL

// Generate a random long number in the range [ lLow, lHigh ]
int32 RandomLong( int32 lLow, int32 lHigh )
{
	uint32 maxAcceptable;
	uint32 x;
	uint32 n;

	RecEngRandomLong(lLow, lHigh);

	x = lHigh - lLow + 1;
	if (x <= 0 || MAX_RANDOM_RANGE < x - 1)
	{
		return lLow;
	}

	// The following maps a uniform distribution on the interval [0..MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0..x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	maxAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE + 1) % x);

	do
	{
		n = ran1();
	} while (n > maxAcceptable);

	return lLow + (n % x);
}

void PF_FadeVolume(const edict_t* clientent, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds)
{
	int entnum = NUM_FOR_EDICT(clientent);
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("tried to PF_FadeVolume a non-client\n"));
		return;
	}

	client_t* client = &svs.clients[entnum - 1];
	if (client->fakeclient)
		return;

	MSG_WriteChar(&client->netchan.message, svc_soundfade);
	MSG_WriteByte(&client->netchan.message, (byte)fadePercent);
	MSG_WriteByte(&client->netchan.message, (byte)holdTime);
	MSG_WriteByte(&client->netchan.message, (byte)fadeOutSeconds);
	MSG_WriteByte(&client->netchan.message, (byte)fadeInSeconds);
}

void PF_SetClientMaxspeed(const edict_t* clientent, float fNewMaxspeed)
{
	int entnum = NUM_FOR_EDICT(clientent);
	if (entnum < 1 || entnum > svs.maxclients)
		Con_Printf(const_cast<char*>("tried to PF_SetClientMaxspeed a non-client\n"));

	// cast to non-const for write ability
	((edict_t*)clientent)->v.maxspeed = fNewMaxspeed;
}

int PF_GetPlayerUserId(edict_t* e)
{
	if (!sv.active || !e)
		return -1;

	for (int i = 0; i < svs.maxclients; i++)
	{
		if (svs.clients[i].edict == e)
			return svs.clients[i].userid;
	}

	return -1;
}

unsigned int PF_GetPlayerWONId(edict_t* e)
{
	return -1;
}

const char* PF_GetPlayerAuthId(edict_t* e)
{
	static char szAuthID[5][64];
	static int count = 0;
	client_t* cl;
	int i;

	count = (count + 1) % 5;
	szAuthID[count][0] = 0;

	if (!sv.active || e == NULL)
	{
		return szAuthID[count];
	}

	for (i = 0; i < svs.maxclients; i++)
	{
		cl = &svs.clients[i];
		if (cl->edict != e)
		{
			continue;
		}

		if (cl->fakeclient)
		{
			Q_strcpy(szAuthID[count], "BOT");
		}
		else if (cl->network_userid.idtype == AUTH_IDTYPE_LOCAL)
		{
			Q_strcpy(szAuthID[count], "HLTV");
		}
		else
		{
			_snprintf(szAuthID[count], sizeof(szAuthID[count]) - 1, "%s", SV_GetClientIDString(cl));
		}

		break;
	}

	return szAuthID[count];
}

void PF_BuildSoundMsg_I(edict_t* entity, int channel, const char* sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float* pOrigin, edict_t* ed)
{
	PF_MessageBegin_I(msg_dest, msg_type, pOrigin, ed);
	SV_BuildSoundMsg(entity, channel, sample, (int)volume, attenuation, fFlags, pitch, pOrigin, &gMsgBuffer);
	PF_MessageEnd_I();
}

int PF_IsDedicatedServer(void)
{
	return g_bIsDedicatedServer;
}

const char* PF_GetPhysicsInfoString(const edict_t* pClient)
{
	int entnum = NUM_FOR_EDICT(pClient);
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("tried to " __FUNCTION__ " a non-client\n"));
		return "";
	}

	client_t* client = &svs.clients[entnum - 1];
	return client->physinfo;
}

const char* PF_GetPhysicsKeyValue(const edict_t* pClient, const char* key)
{
	int entnum = NUM_FOR_EDICT(pClient);
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("tried to " __FUNCTION__ " a non-client\n"));
		return "";
	}

	client_t* client = &svs.clients[entnum - 1];
	return Info_ValueForKey(client->physinfo, key);
}

void PF_SetPhysicsKeyValue(const edict_t* pClient, const char* key, const char* value)
{
	int entnum = NUM_FOR_EDICT(pClient);
	if (entnum < 1 || entnum > svs.maxclients)
		Con_Printf(const_cast<char*>("tried to " __FUNCTION__ " a non-client\n"));

	client_t* client = &svs.clients[entnum - 1];
	Info_SetValueForKey(client->physinfo, key, value, MAX_INFO_STRING);
}

int PF_GetCurrentPlayer(void)
{
	int idx = host_client - svs.clients;
	if (idx < 0 || idx >= svs.maxclients)
		return -1;

	return idx;
}

int PF_CanSkipPlayer(const edict_t* pClient)
{
	int entnum = NUM_FOR_EDICT(pClient);
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("tried to " __FUNCTION__ " a non-client\n"));
		return 0;
	}

	client_t* client = &svs.clients[entnum - 1];
	return client->lw != 0;
}

void PF_SetGroupMask(int mask, int op)
{
	g_groupmask = mask;
	g_groupop = op;
}

int PF_CreateInstancedBaseline(int classname, entity_state_t* baseline)
{
	extra_baselines_t* bls = sv.instance_baselines;
	if (bls->number >= NUM_BASELINES - 1)
		return 0;

	bls->classname[bls->number] = classname;
	bls->baseline[bls->number] = *baseline;
	bls->number++;

	return bls->number;
}

void PF_Cvar_DirectSet(cvar_t* var, char* value)
{
	Cvar_DirectSet(var, value);
}

void PF_ForceUnmodified( FORCE_TYPE type, float* mins, float* maxs, const char* filename )
{
	int		i = 0;

	if (!filename)
	{
		Host_Error("PF_ForceUnmodified: NULL pointer");
	}

	if (PR_IsEmptyString((char*)filename))
	{
		Host_Error("PF_ForceUnmodified: Bad string '%s'", filename);
	}

	if (sv.state == ss_loading)
	{
		for (i = 0; i < MAX_CONSISTENCY_LIST; i++)
		{
			if (!sv.consistency_list[i].filename)
			{
				sv.consistency_list[i].check_type = type;
				sv.consistency_list[i].filename = (char*)filename;

				if (mins)
				{
					VectorCopy(mins, sv.consistency_list[i].mins);
				}
				if (maxs)
				{
					VectorCopy(maxs, sv.consistency_list[i].maxs);
				}
				return;
			}
			if (!Q_stricmp(sv.consistency_list[i].filename, filename))
				return;
		}
		Host_Error("ForceUnmodified: '%s' overflow", filename);
	}
	else
	{
		// No precaching, check if it's already there
		for (i = 0; i < MAX_CONSISTENCY_LIST; i++)
		{
			if (sv.consistency_list[i].filename)
			{
				if (!Q_stricmp(sv.consistency_list[i].filename, filename))
					return;
			}
		}
		Host_Error("ForceUnmodified: '%s' Precache can only be done in spawn functions", filename);
	}
}

void PF_GetPlayerStats(const edict_t* pClient, int* ping, int* packet_loss)
{
	*packet_loss = 0;
	*ping = 0;
	int c = NUM_FOR_EDICT(pClient);
	if (c < 1 || c > svs.maxclients)
	{
		Con_Printf(const_cast<char*>("tried to " __FUNCTION__ " a non-client\n"));
		return;
	}

	client_t* client = &svs.clients[c - 1];
	*packet_loss = client->packet_loss;
	*ping = client->latency * 1000.0f;
}

void QueryClientCvarValueCmd(void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf(const_cast<char*>("%s <player name> <cvar>\n"), Cmd_Argv(0));
		return;
	}

	for (int i = 0; i < svs.maxclients; i++)
	{
		client_t* cl = &svs.clients[i];

		if (cl->active || cl->connected)
		{
			if (!Q_strcasecmp(cl->name, Cmd_Argv(1)))
			{
				QueryClientCvarValue(cl->edict, Cmd_Argv(2));
				break;
			}
		}
	}
}

void QueryClientCvarValueCmd2(void)
{
	int i;
	client_t* cl;
	int requestID;

	if (Cmd_Argc() < 3)
	{
		Con_Printf(const_cast<char*>("%s <player name> <cvar> <requestID>"), Cmd_Argv(0));
		return;
	}

	requestID = Q_atoi(Cmd_Argv(3));
	for (i = 0; i < svs.maxclients; i++)
	{
		cl = &svs.clients[i];

		if (cl->active || cl->connected)
		{
			if (!Q_strcasecmp(cl->name, Cmd_Argv(1)))
			{
				QueryClientCvarValue2(cl->edict, Cmd_Argv(2), requestID);
				break;
			}
		}
	}
}

void QueryClientCvarValue(const edict_t* player, const char* cvarName)
{
	int entnum = NUM_FOR_EDICT(player);
	if (entnum < 1 || entnum > svs.maxclients)
	{
		if (gNewDLLFunctions.pfnCvarValue)
			gNewDLLFunctions.pfnCvarValue(player, "Bad Player");

		Con_Printf(const_cast<char*>("tried to " __FUNCTION__ " a non-client\n"));
		return;
	}
	client_t* client = &svs.clients[entnum - 1];
	MSG_WriteChar(&client->netchan.message, svc_sendcvarvalue);
	MSG_WriteString(&client->netchan.message, cvarName);
}

void QueryClientCvarValue2(const edict_t* player, const char* cvarName, int requestID)
{
	int entnum = NUM_FOR_EDICT(player);
	if (entnum < 1 || entnum > svs.maxclients)
	{
		if (gNewDLLFunctions.pfnCvarValue2)
			gNewDLLFunctions.pfnCvarValue2(player, requestID, cvarName, "Bad Player");

		Con_Printf(const_cast<char*>("tried to QueryClientCvarValue a non-client\n"));
		return;
	}
	client_t* client = &svs.clients[entnum - 1];
	MSG_WriteChar(&client->netchan.message, svc_sendcvarvalue2);
	MSG_WriteLong(&client->netchan.message, requestID);
	MSG_WriteString(&client->netchan.message, cvarName);
}

int hudCheckParm(char* parm, char** ppnext)
{
	RecEnghudCheckParm(parm, ppnext);

	int result = COM_CheckParm(parm);

	if (ppnext)
	{
		if (result && result < com_argc - 1)
			*ppnext = com_argv[result + 1];
		else
			*ppnext = NULL;
	}

	return result;
}

int EngCheckParm(const char* pchCmdLineToken, char** pchNextVal)
{
	return hudCheckParm((char*)pchCmdLineToken, pchNextVal);
}
