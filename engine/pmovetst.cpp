#include "quakedef.h"
#include "pmove.h"
#include "pmovetst.h"
#include "world.h"
#include "r_studio.h"

int g_contentsresult;
static hull_t box_hull;
static box_clipnodes_t box_clipnodes;
static box_planes_t box_planes;

qboolean PM_RecursiveHullCheck(hull_t* hull, int num, float p1f, float p2f, const vec_t* p1, const vec_t* p2, pmtrace_t* trace);

int PM_HullPointContents(hull_t *hull, int num, vec_t *p)
{
	float d;
	dclipnode_t *node;
	mplane_t *plane;

	if (hull->firstclipnode >= hull->lastclipnode)
		return -1;

	while (num >= 0)
	{
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error(__FUNCTION__ ": bad node number");
		node = &hull->clipnodes[num];
		plane = &hull->planes[node->planenum];

		if (plane->type >= 3)
			d = _DotProduct(p, plane->normal) - plane->dist;
		else
			d = p[plane->type] - plane->dist;

		if (d >= 0.0)
			num = node->children[0];
		else
			num = node->children[1];
	}

	return num;
}

int PM_LinkContents(vec_t *p, int *pIndex)
{
	physent_t *pe;
	vec3_t test;

	for (int i = 1; i < pmove->numphysent; i++) {
		pe = &pmove->physents[i];
		model_t* model = pmove->physents[i].model;
		if (pmove->physents[i].solid || model == NULL)
			continue;

		test[0] = p[0] - pe->origin[0];
		test[1] = p[1] - pe->origin[1];
		test[2] = p[2] - pe->origin[2];
		if (PM_HullPointContents(model->hulls, model->hulls[0].firstclipnode, test) != -1) {
			if (pIndex)
				*pIndex = pe->info;
			return pe->skin;
		}
	}

	return -1;
}

void* PM_HullForBsp(physent_t *pe, vec_t *offset)
{
	hull_t *hull;

	switch (pmove->usehull) {
	case 1:
		hull = &pe->model->hulls[3];
		break;

	case 2:
		hull = &pe->model->hulls[0];
		break;

	case 3:
		hull = &pe->model->hulls[2];
		break;

	default:
		hull = &pe->model->hulls[1];
		break;
	}

	VectorSubtract(hull->clip_mins, player_mins[pmove->usehull], offset);
	VectorAdd(offset, pe->origin, offset);
	return hull;
}

hull_t *PM_HullForBox(vec_t *mins, vec_t *maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];
	return &box_hull;
}

hull_t *PM_HullForStudioModel(model_t *pModel, vec_t *offset, float frame, int sequence, const vec_t *angles, const vec_t *origin, const unsigned char *pcontroller, const unsigned char *pblending, int *pNumHulls)
{
	vec3_t size;

	VectorSubtract(player_maxs[pmove->usehull], player_mins[pmove->usehull], size);
	VectorScale(size, 0.5, size);
	offset[0] = 0;
	offset[1] = 0;
	offset[2] = 0;
	return R_StudioHull(pModel, frame, sequence, angles, origin, size, pcontroller, pblending, pNumHulls, 0, 0);
}

int PM_PointContents( vec_t* p, int* truecontents )
{
	g_engdstAddrs.PM_PointContents(&p, &truecontents);

	if (pmove->physents[0].model->hulls != NULL)
	{
		int entityContents = PM_HullPointContents(
			pmove->physents[0].model->hulls,
			pmove->physents[0].model->hulls[0].firstclipnode,
			p);
		if (truecontents)
			*truecontents = entityContents;
		if (entityContents > -9 || entityContents < -14)
		{
			if (entityContents == -2)
				return entityContents;
		}
		else
		{
			entityContents = -3;
		}
		int cont = PM_LinkContents(p, 0);
		if (cont != -1)
			return cont;
		return entityContents;
	}
	if (truecontents)
		*truecontents = -1;
	return -2;

}

int PM_WaterEntity( vec_t* p )
{
	int cont;
	int entityIndex;

	g_engdstAddrs.PM_WaterEntity(&p);

	model_t* model = pmove->physents[0].model;
	cont = PM_HullPointContents(model->hulls, model->hulls[0].firstclipnode, p);
	if (cont == -2) {
		return -1;
	}

	entityIndex = 0;
	return PM_LinkContents(p, &entityIndex);
}

pmtrace_t _PM_PlayerTrace(vec_t *start, vec_t *end, int traceFlags, int numphysent, physent_t *physents, int ignore_pe, int(*pfnIgnore)(physent_t *))
{
	hull_t *hull;
	pmtrace_t trace;
	pmtrace_t testtrace;
	pmtrace_t total;
	vec3_t maxs;
	vec3_t mins;
	int closest;
	bool rotated;
	int pNumHulls;
	vec_t end_l[3];
	vec_t start_l[3];
	vec3_t offset;

	Q_memset(&trace, 0, sizeof(trace));
	trace.fraction = 1.0f;
	trace.ent = -1;
	trace.endpos[0] = end[0];
	trace.endpos[1] = end[1];
	trace.endpos[2] = end[2];

	for (int i = 0; i < numphysent; i++)
	{
		physent_t* pe = &physents[i];
		if (i > 0 && (traceFlags & PM_WORLD_ONLY))
			break;

		if (pfnIgnore)
		{
			if (pfnIgnore(pe))
				continue;
		}
		else
		{
			if (ignore_pe != -1 && i == ignore_pe)
				continue;
		}

		if ((pe->model && !pe->solid && pe->skin) || ((traceFlags & PM_GLASS_IGNORE) && pe->rendermode))
			continue;


		offset[0] = pe->origin[0];
		offset[1] = pe->origin[1];
		offset[2] = pe->origin[2];
		pNumHulls = 1;
		if (pe->model)
		{
			switch (pmove->usehull)
			{
			case 1:
				hull = &pe->model->hulls[3];
				break;
			case 2:
				hull = &pe->model->hulls[0];
				break;
			case 3:
				hull = &pe->model->hulls[2];
				break;
			default:
				hull = &pe->model->hulls[1];
				break;
			}
			VectorSubtract(hull->clip_mins, player_mins[pmove->usehull], offset);
			VectorAdd(offset, pe->origin, offset);
		}
		else
		{
			hull = NULL;
			if (pe->studiomodel)
			{
				if (traceFlags & PM_STUDIO_IGNORE)
					continue;


				if (pe->studiomodel->type == mod_studio && (pe->studiomodel->flags & STUDIO_TRACE_HITBOX || (pmove->usehull == 2 && !(traceFlags & PM_STUDIO_BOX))))
				{
					hull = PM_HullForStudioModel(pe->studiomodel, offset, pe->frame, pe->sequence, pe->angles, pe->origin, pe->controller, pe->blending, &pNumHulls);
				}
			}

			if (!hull)
			{
				VectorSubtract(pe->mins, player_maxs[pmove->usehull], mins);
				VectorSubtract(pe->maxs, player_mins[pmove->usehull], maxs);
				hull = PM_HullForBox(mins, maxs);
			}
		}

		VectorSubtract(start, offset, start_l);
		VectorSubtract(end, offset, end_l);

		if (pe->solid == SOLID_BSP && (pe->angles[0] != 0.0 || pe->angles[1] != 0.0 || pe->angles[2] != 0.0))
		{
			vec3_t forward, right, up;
			AngleVectors(pe->angles, forward, right, up);

			vec3_t temp_start = { start_l[0], start_l[1], start_l[2] };
			start_l[0] = _DotProduct(forward, temp_start);
			start_l[1] = -_DotProduct(right, temp_start);
			start_l[2] = _DotProduct(up, temp_start);

			vec3_t temp_end = { end_l[0], end_l[1], end_l[2] };
			end_l[0] = _DotProduct(forward, temp_end);
			end_l[1] = -_DotProduct(right, temp_end);
			end_l[2] = _DotProduct(up, temp_end);

			rotated = true;
		}
		else
		{
			rotated = false;
		}

		Q_memset(&total, 0, sizeof(total));
		total.endpos[0] = end[0];
		total.endpos[1] = end[1];
		total.endpos[2] = end[2];
		total.fraction = 1.0f;
		total.allsolid = 1;
		if (pNumHulls <= 0)
		{
			total.allsolid = 0;
		}
		else
		{
			if (pNumHulls == 1)
			{
				PM_RecursiveHullCheck(hull, hull->firstclipnode, 0.0, 1.0, start_l, end_l, &total);
			}
			else
			{
				closest = 0;
				for (int j = 0; j < pNumHulls; j++)
				{
					Q_memset(&testtrace, 0, sizeof(testtrace));
					testtrace.endpos[0] = end[0];
					testtrace.endpos[1] = end[1];
					testtrace.endpos[2] = end[2];
					testtrace.fraction = 1.0f;
					testtrace.allsolid = 1;
					PM_RecursiveHullCheck(&hull[j], hull[j].firstclipnode, 0.0, 1.0, start_l, end_l, &testtrace);
					if (j == 0 || testtrace.allsolid || testtrace.startsolid || testtrace.fraction < total.fraction)
					{
						bool remember = (total.startsolid == 0);
						Q_memcpy(&total, &testtrace, sizeof(total));
						if (!remember)
							total.startsolid = 1;
						closest = j;
					}
					total.hitgroup = SV_HitgroupForStudioHull(closest);
				}
			}

			if (total.allsolid)
				total.startsolid = 1;

		}

		if (total.startsolid)
			total.fraction = 0;

		if (total.fraction != 1.0)
		{
			if (rotated)
			{
				vec3_t forward, right, up;
				AngleVectorsTranspose(pe->angles, forward, right, up);

				vec3_t temp = { total.plane.normal[0], total.plane.normal[1], total.plane.normal[2] };
				total.plane.normal[0] = _DotProduct(forward, temp);
				total.plane.normal[1] = _DotProduct(right, temp);
				total.plane.normal[2] = _DotProduct(up, temp);
			}
			total.endpos[0] = (end[0] - start[0]) * total.fraction + start[0];
			total.endpos[1] = (end[1] - start[1]) * total.fraction + start[1];
			total.endpos[2] = (end[2] - start[2]) * total.fraction + start[2];
		}

		if (total.fraction < trace.fraction)
		{
			Q_memcpy(&trace, &total, sizeof(trace));
			trace.ent = i;
		}
	}

	return trace;
}

pmtrace_t* PM_TraceLine(float* start, float* end, int flags, int usehull, int ignore_pe)
{
	int oldhull;
	static pmtrace_t tr;

	g_engdstAddrs.PM_TraceLine(&start, &end, &flags, &usehull, &ignore_pe);

	oldhull = pmove->usehull;
	pmove->usehull = usehull;
	if (flags)
	{
		if (flags == 1)
			tr = _PM_PlayerTrace(start, end, PM_NORMAL, pmove->numvisent, pmove->visents, ignore_pe, NULL);
	}
	else
	{
		tr = _PM_PlayerTrace(start, end, PM_NORMAL, pmove->numphysent, pmove->physents, ignore_pe, NULL);
	}
	pmove->usehull = oldhull;
	return &tr;
}

void PM_InitBoxHull(void)
{
	box_hull.clipnodes = &box_clipnodes[0];
	box_hull.planes = &box_planes[0];
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (int i = 0; i < 6; i++)
	{
		int side = i & 1;
		box_clipnodes[i].planenum = i;
		box_clipnodes[i].children[side] = -1;
		box_clipnodes[i].children[side ^ 1] = (i != 5) ? i + 1 : CONTENTS_SOLID;
		box_planes[i].type = i >> 1;
		box_planes[i].normal[i >> 1] = 1.0f;
	}
}

int _PM_TestPlayerPosition(vec_t *pos, pmtrace_t *ptrace, int(*pfnIgnore)(physent_t *))
{
	hull_t *hull;
	pmtrace_t tr;
	vec3_t mins;
	vec3_t maxs;
	vec3_t offset;
	int numhulls;
	vec3_t test;

	tr = PM_PlayerTrace(pmove->origin, pmove->origin, PM_NORMAL, -1);
	if (ptrace)
		Q_memcpy(ptrace, &tr, sizeof(tr));

	for (int i = 0; i < pmove->numphysent; i++)
	{
		physent_t *pe = &pmove->physents[i];
		if (pfnIgnore && pfnIgnore(pe))
			continue;

		if (pe->model && pe->solid == SOLID_NOT && pe->skin != 0)
			continue;

		offset[0] = pe->origin[0];
		offset[1] = pe->origin[1];
		offset[2] = pe->origin[2];
		numhulls = 1;

		if (pe->model)
		{
			hull = (hull_t*)PM_HullForBsp(pe, offset);
		}
		else
		{
			if (pe->studiomodel && pe->studiomodel->type == mod_studio && ((pe->studiomodel->flags & STUDIO_TRACE_HITBOX) || pmove->usehull == 2))
			{
				hull = PM_HullForStudioModel(pe->studiomodel, offset, pe->frame, pe->sequence, pe->angles, pe->origin, pe->controller, pe->blending, &numhulls);
			}
			else
			{
				VectorSubtract(pe->mins, player_maxs[pmove->usehull], mins);
				VectorSubtract(pe->maxs, player_mins[pmove->usehull], mins);

				hull = PM_HullForBox(mins, maxs);
			}
		}
		VectorSubtract(pos, offset, test);
		if (pe->solid == SOLID_BSP && (pe->angles[0] != 0.0 || pe->angles[1] != 0.0 || pe->angles[2] != 0.0))
		{
			vec3_t forward, right, up;
			AngleVectors(pe->angles, forward, right, up);

			vec3_t temp = { test[0], test[1], test[2] };
			test[0] = _DotProduct(forward, temp);
			test[1] = -_DotProduct(right, temp);
			test[2] = _DotProduct(up, temp);
		}
		if (numhulls != 1)
		{
			for (int j = 0; j < numhulls; j++)
			{
				g_contentsresult = PM_HullPointContents(&hull[j], hull[j].firstclipnode, test);
				if (g_contentsresult == CONTENTS_SOLID)
					return i;
			}
		}
		else
		{
			g_contentsresult = PM_HullPointContents(hull, hull->firstclipnode, test);
			if (g_contentsresult == CONTENTS_SOLID)
				return i;
		}
	}

	return -1;

}

pmtrace_t PM_PlayerTrace(vec_t *start, vec_t *end, int traceFlags, int ignore_pe)
{
	pmtrace_t tr = _PM_PlayerTrace(start, end, traceFlags, pmove->numphysent, pmove->physents, ignore_pe, NULL);
	return tr;
}

pmtrace_t PM_PlayerTraceEx(vec_t *start, vec_t *end, int traceFlags, int(*pfnIgnore)(physent_t *))
{
	pmtrace_t tr = _PM_PlayerTrace(start, end, traceFlags, pmove->numphysent, pmove->physents, -1, pfnIgnore);
	return tr;
}

int PM_TestPlayerPosition(float *pos, pmtrace_t *ptrace)
{
	return _PM_TestPlayerPosition(pos, ptrace, 0);
}

int PM_TestPlayerPositionEx(vec_t *pos, pmtrace_t *ptrace, int(*pfnIgnore)(physent_t *))
{
	return _PM_TestPlayerPosition(pos, ptrace, pfnIgnore);
}

qboolean PM_RecursiveHullCheck(hull_t *hull, int num, float p1f, float p2f, const vec_t *p1, const vec_t *p2, pmtrace_t *trace)
{
	dclipnode_t *node;
	mplane_t *plane;
	vec3_t mid;
	float pdif;
	float frac;
	float t1;
	float t2;
	float midf;

	float DIST_EPSILON = 0.03125f;

	if (num < 0)
	{
		if (num == CONTENTS_SOLID)
		{
			trace->startsolid = TRUE;
		}
		else
		{
			trace->allsolid = FALSE;
			if (num == CONTENTS_EMPTY)
			{
				trace->inopen = TRUE;
			}
			else
			{
				trace->inwater = TRUE;
			}
		}
		return TRUE;
	}

	if (hull->firstclipnode >= hull->lastclipnode)
	{
		trace->allsolid = FALSE;
		trace->inopen = TRUE;
		return TRUE;
	}

	node = &hull->clipnodes[num];
	plane = &hull->planes[node->planenum];
	if (plane->type >= 3u)
	{
		t1 = _DotProduct((vec_t*)p1, plane->normal) - plane->dist;
		t2 = _DotProduct((vec_t*)p2, plane->normal) - plane->dist;
	}
	else
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	if (t1 >= 0.0 && t2 >= 0.0)
		return PM_RecursiveHullCheck(hull, node->children[0], p1f, p2f, p1, p2, trace);

	if (t1 >= 0.0)
	{
		midf = t1 - DIST_EPSILON;
	}
	else
	{
		if (t2 < 0.0)
			return PM_RecursiveHullCheck(hull, node->children[1], p1f, p2f, p1, p2, trace);

		midf = t1 + DIST_EPSILON;
	}
	midf = midf / (t1 - t2);
	if (midf >= 0.0)
	{
		if (midf > 1.0)
			midf = 1.0;
	}
	else
	{
		midf = 0.0;
	}

	pdif = p2f - p1f;
	frac = pdif * midf + p1f;
	mid[0] = (p2[0] - p1[0]) * midf + p1[0];
	mid[1] = (p2[1] - p1[1]) * midf + p1[1];
	mid[2] = (p2[2] - p1[2]) * midf + p1[2];

	int side = (t1 >= 0.0) ? 0 : 1;

	if (!PM_RecursiveHullCheck(hull, node->children[side], p1f, frac, p1, mid, trace))
		return 0;

	if (PM_HullPointContents(hull, node->children[side ^ 1], mid) != -2)
		return PM_RecursiveHullCheck(hull, node->children[side ^ 1], frac, p2f, mid, p2, trace);

	if (trace->allsolid)
		return 0;

	if (side)
	{
		VectorSubtract(vec3_origin, plane->normal, trace->plane.normal);
		trace->plane.dist = -plane->dist;
	}
	else
	{
		trace->plane.normal[0] = plane->normal[0];
		trace->plane.normal[1] = plane->normal[1];
		trace->plane.normal[2] = plane->normal[2];
		trace->plane.dist = plane->dist;
	}

	if (PM_HullPointContents(hull, hull->firstclipnode, mid) != -2)
	{
		trace->fraction = frac;
		trace->endpos[0] = mid[0];
		trace->endpos[1] = mid[1];
		trace->endpos[2] = mid[2];
		return 0;
	}

	while (true)
	{
		midf = (float)(midf - 0.05);
		if (midf < 0.0)
			break;

		frac = pdif * midf + p1f;
		mid[0] = (p2[0] - p1[0]) * midf + p1[0];
		mid[1] = (p2[1] - p1[1]) * midf + p1[1];
		mid[2] = (p2[2] - p1[2]) * midf + p1[2];
		if (PM_HullPointContents(hull, hull->firstclipnode, mid) != -2)
		{
			trace->fraction = frac;
			trace->endpos[0] = mid[0];
			trace->endpos[1] = mid[1];
			trace->endpos[2] = mid[2];
			return 0;
		}
	}

	trace->fraction = frac;
	trace->endpos[0] = mid[0];
	trace->endpos[1] = mid[1];
	trace->endpos[2] = mid[2];
	Con_DPrintf(const_cast<char*>("Trace backed up past 0.0.\n"));
	return 0;
}

int PM_TruePointContents(float *p)
{
	if (pmove->physents[0].model->hulls == NULL)
		return -1;
	else
		return PM_HullPointContents(pmove->physents[0].model->hulls, pmove->physents[0].model->hulls[0].firstclipnode, p);
}

float PM_TraceModel( physent_t *pEnt, float *start, float *end, trace_t *trace )
{
	hull_t *pHull;
	int saveHull;
	vec3_t start_l;
	vec3_t end_l;
	vec3_t offset;

	saveHull = pmove->usehull;
	pmove->usehull = 2;
	pHull = (hull_t*)PM_HullForBsp(pEnt, offset);
	pmove->usehull = saveHull;
	VectorSubtract(start, offset, start_l);
	VectorSubtract(end, offset, end_l);
	SV_RecursiveHullCheck(pHull, pHull->firstclipnode, 0.0, 1.0, start_l, end_l, trace);
	trace->ent = 0;
	return trace->fraction;

}

int PM_GetModelType(struct model_s *mod)
{
	return mod->type;
}

void PM_GetModelBounds(struct model_s *mod, float *mins, float *maxs)
{
	VectorCopy(mod->mins, mins);
	VectorCopy(mod->maxs, maxs);
}

struct pmtrace_s* PM_TraceLineEx(float *start, float *end, int flags, int usehull, int(*pfnIgnore)(physent_t *pe))
{
	int oldhull;
	static pmtrace_t tr;

	oldhull = pmove->usehull;
	pmove->usehull = usehull;
	if (flags)
	{
		tr = _PM_PlayerTrace(start, end, PM_NORMAL, pmove->numvisent, pmove->visents, -1, pfnIgnore);
	}
	else
	{
		tr = PM_PlayerTraceEx(start, end, PM_NORMAL, pfnIgnore);
	}
	pmove->usehull = oldhull;
	return &tr;

}
