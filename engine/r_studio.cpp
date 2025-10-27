#include "quakedef.h"
#include "client.h"
#include "r_studio.h"
#if defined(GLQUAKE)
#include "gl_model.h"
#include "gl_rmain.h"
#include "gl_rmisc.h"
#include "gl_vidnt.h"
#else
#include "model.h"
#endif
#include "studio.h"
#include "r_local.h"
#include "cl_parse.h"
#include "render.h"
#include "pr_cmds.h"
#include "cl_main.h"
#include "host.h"
#include "pm_movevars.h"
#include "cl_pmove.h"
#include "view.h"
#include "r_triangle.h"
#include "cl_tent.h"
#include "color.h"
#include "sv_main.h"
#include "world.h"
#include "cl_ents.h"
#include "customentity.h"

mstudiomodel_t* psubmodel;
model_t* r_model;
player_info_t* r_playerinfo;
studiohdr_t* pstudiohdr;
mstudiobodyparts_t* pbodypart;

int cache_hull_hitgroup[STUDIO_NUM_HULLS];
hull_t cache_hull[STUDIO_NUM_HULLS];
mplane_t cache_planes[STUDIO_NUM_PLANES];
r_studiocache_t rgStudioCache[STUDIO_CACHE_SIZE];
cl_entity_t* cl_viewent;
int g_ForcedFaceFlags;

int nCurrentHull;
int nCurrentPlane;
int r_cachecurrent;

float omega, cosom, sinom, sclp, sclq;
int cached_numbones;
cache_user_t model_texture_cache[1024][32];

hull_t studio_hull[STUDIO_NUM_HULLS];
int studio_hull_hitgroup[STUDIO_NUM_HULLS];
dclipnode_t studio_clipnodes[6];
mplane_t studio_planes[STUDIO_NUM_PLANES];

char cached_bonename[MAXSTUDIOBONES * 32];
int r_amodels_drawn;
int r_smodels_total;

float bonetransform[STUDIO_NUM_HULLS][3][4];
float lighttransform[STUDIO_NUM_HULLS][3][4];
float cached_lighttransform[STUDIO_NUM_HULLS][3][4];
float cached_bonetransform[STUDIO_NUM_HULLS][3][4];
int chromeage[MAXSTUDIOBONES];
float rotationmatrix[3][4];

vec3_t r_studionormal[MAXSTUDIOVERTS];
vec3_t r_studiotangent[MAXSTUDIOVERTS];
int chrome[MAXSTUDIOVERTS][2];
float lightpos[MAXSTUDIOVERTS][3][4];
auxvert_t auxverts[MAXSTUDIOVERTS];
vec3_t lightvalues[MAXSTUDIOVERTS];
auxvert_t* pauxverts;
#if defined(GLQUAKE)
vec3_t* pvlightvalues;
#else
float* plightvalues;
#endif
vec3_t r_blightvec[MAXSTUDIOBONES];

int					r_ambientlight;
float				r_shadelight;

#if !defined(GLQUAKE)
int drawstyle;
float zishift;
#endif

int g_NormalIndex[MAXSTUDIOVERTS];
vec3_t g_ChromeOrigin;
int lightage[MAXSTUDIOBONES];
dlight_t* locallight[3];
vec_t lightbonepos[MAXSTUDIOBONES][3][3];
int numlights;
float locallightR2[3];
int locallinearlight[3][3];
vec_t r_chromeup[MAXSTUDIOBONES][3];
vec_t r_chromeright[MAXSTUDIOBONES][3];
vec3_t mergebones_pos[MAXSTUDIOBONES];

int r_topcolor, r_bottomcolor, r_remapindex;
// 2528 = 2048 + 512 - MAX_CLIENTS
skin_t* pDM_RemapSkin[(80 - 1) * MAX_CLIENTS][11];
skin_t DM_RemapSkin[64][11];
player_model_t DM_PlayerState[MAX_CLIENTS];

int r_playerindex;
float r_gaitmovement;
int pvlightvalues_size;
vec3_t g_txformednormal[MAXSTUDIOVERTS];
int g_rendermode;

int r_dointerp = 1;

int g_iBackFaceCull;

int boxpnt[6][4] =
{
	{ 0, 4, 6, 2 }, // +X
	{ 0, 1, 5, 4 }, // +Y
	{ 0, 2, 3, 1 }, // +Z
	{ 7, 5, 1, 3 }, // -X
	{ 7, 3, 2, 6 }, // -Y
	{ 7, 6, 4, 5 }, // -Z
};

vec3_t hullcolor[8] =
{
	{ 1.0f, 1.0f, 1.0f },
	{ 1.0f, 0.5f, 0.5f },
	{ 0.5f, 1.0f, 0.5f },
	{ 1.0f, 1.0f, 0.5f },
	{ 0.5f, 0.5f, 1.0f },
	{ 1.0f, 0.5f, 1.0f },
	{ 0.5f, 1.0f, 1.0f },
	{ 1.0f, 1.0f, 1.0f },
};

void R_FlushStudioCache();
#if defined(GLQUAKE)
void studioapi_GL_SetRenderMode(int rendermode);
void studioapi_GL_StudioDrawShadow();
#endif
void SV_StudioSetupBones(model_t* pModel, float frame, int sequence, const vec_t* angles, const vec_t* origin, const unsigned char* pcontroller, const unsigned char* pblending, int iBone, const edict_t* edict);

#ifdef _DEBUG
void* Dummy_Calloc(int nmemb, size_t size)
{
	return Mem_Calloc(nmemb, size);
}
#endif

cvar_t r_cachestudio = { const_cast<char*>("r_cachestudio"), const_cast<char*>("1") };
sv_blending_interface_t svBlending = { 1, SV_StudioSetupBones };
sv_blending_interface_t* g_pSvBlendingAPI = &svBlending;
#ifdef _DEBUG
server_studio_api_t server_studio_api = { Dummy_Calloc, Cache_Check, COM_LoadCacheFile, Mod_Extradata };
#else
server_studio_api_t server_studio_api = { Mem_Calloc, Cache_Check, COM_LoadCacheFile, Mod_Extradata };
#endif

int R_StudioDrawModel(int flags);
int R_StudioDrawPlayer(int flags, entity_state_t* pplayer);

r_studio_interface_t studio = {
	STUDIO_INTERFACE_VERSION,
	&R_StudioDrawModel,
	&R_StudioDrawPlayer
};

r_studio_interface_t* pStudioAPI = &studio;

cvar_t* cl_righthand = NULL;

vec3_t r_colormix;
colorVec r_icolormix;
colorVec r_icolormix32;

#if !defined(GLQUAKE)
float filterColorRed = 1.0f, filterColorGreen = 1.0f, filterColorBlue = 1.0f, filterBrightness = 1.0f;
#endif

#if defined(GLQUAKE)
float r_blend;
#else
int r_blend;
#endif

extern cvar_t r_cullsequencebox;

void R_ResetSvBlending()
{
	g_pSvBlendingAPI = &svBlending;
}

void SV_InitStudioHull()
{
	if (studio_hull[0].planes)
		return;	// already initailized

	for (int i = 0; i < 6; i++)
	{
		int side = i & 1;
		studio_clipnodes[i].planenum = i;
		studio_clipnodes[i].children[side] = -1;
		studio_clipnodes[i].children[side ^ 1] = (i < 5) ? i + 1 : -2;
	}

	for (int i = 0; i < STUDIO_NUM_HULLS; i++)
	{
		studio_hull[i].planes = &studio_planes[i * 6];
		studio_hull[i].clipnodes = &studio_clipnodes[0];
		studio_hull[i].firstclipnode = 0;
		studio_hull[i].lastclipnode = 5;
	}
}

r_studiocache_t* R_CheckStudioCache(model_t* pModel, float frame, int sequence, const vec_t* angles, const vec_t* origin, const vec_t* size, const unsigned char* controller, const unsigned char* blending)
{
	for (int i = 0; i < STUDIO_CACHE_SIZE; i++)
	{
		r_studiocache_t* pCached = &rgStudioCache[(r_cachecurrent - i) & STUDIO_CACHEMASK];

		if (pCached->pModel == pModel && pCached->frame == frame && pCached->sequence == sequence
			&& VectorCompare(pCached->angles, angles) && VectorCompare(pCached->origin, origin) && VectorCompare(pCached->size, size)
			&& !Q_memcmp(pCached->controller, (void*)controller, 4) && !Q_memcmp(pCached->blending, (void*)blending, 2))
		{
			return pCached;
		}
	}

	return NULL;
}

void R_AddToStudioCache(float frame, int sequence, const vec_t* angles, const vec_t* origin, const vec_t* size, const unsigned char* controller, const unsigned char* pblending, model_t* pModel, hull_t* pHulls, int numhulls)
{
	if (numhulls + nCurrentHull >= MAXSTUDIOBONES)
		R_FlushStudioCache();

	r_cachecurrent++;

	r_studiocache_t* pCache = &rgStudioCache[r_cachecurrent & STUDIO_CACHEMASK];

	pCache->frame = frame;
	pCache->sequence = sequence;

	VectorCopy(angles, pCache->angles);
	VectorCopy(origin, pCache->origin);
	VectorCopy(size, pCache->size);

	Q_memcpy(pCache->controller, (void*)controller, sizeof(pCache->controller));
	Q_memcpy(pCache->blending, (void*)pblending, sizeof(pCache->blending));

	pCache->pModel = pModel;
	pCache->nStartHull = nCurrentHull;
	pCache->nStartPlane = nCurrentPlane;

	Q_memcpy(&cache_hull[nCurrentHull], pHulls, numhulls * sizeof(hull_t));
	Q_memcpy(&cache_planes[nCurrentPlane], studio_planes, numhulls * sizeof(mplane_t) * 6);
	Q_memcpy(&cache_hull_hitgroup[nCurrentHull], studio_hull_hitgroup, numhulls * sizeof(int));

	pCache->numhulls = numhulls;

	nCurrentHull += numhulls;
	nCurrentPlane += numhulls * 6;
}

void AngleQuaternion(vec_t* angles, vec_t* quaternion)
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = angles[2] * 0.5f;
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[1] * 0.5f;
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[0] * 0.5f;
	sr = sin(angle);
	cr = cos(angle);

	quaternion[0] = sr * cp * cy - cr * sp * sy; // X
	quaternion[1] = cr * sp * cy + sr * cp * sy; // Y
	quaternion[2] = cr * cp * sy - sr * sp * cy; // Z
	quaternion[3] = cr * cp * cy + sr * sp * sy; // W
}

void QuaternionSlerp(vec_t* p, vec_t* q, float t, vec_t* qt)
{
	int i;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;

	for (i = 0; i < 4; i++)
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}
	if (a > b)
	{
		for (i = 0; i < 4; i++)
		{
			q[i] = -q[i];
		}
	}

	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if ((1.0 + cosom) > 0.000001)
	{
		if ((1.0 - cosom) > 0.000001)
		{
			omega = acos(cosom);
			sinom = sin(omega);
			sclp = sin((1.0 - t) * omega) / sinom;
			sclq = sin(t * omega) / sinom;
		}
		else
		{
			sclp = 1.0 - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin((1.0 - t) * (0.5 * M_PI));
		sclq = sin(t * (0.5 * M_PI));
		for (i = 0; i < 3; i++)
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

void QuaternionMatrix(vec_t* quaternion, float matrix[3][4])
{
	matrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
	matrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
	matrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];

	matrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
	matrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
	matrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];

	matrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
	matrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
	matrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}

void R_StudioCalcBoneAdj(float dadt, float* adj, const unsigned char* pcontroller1, const unsigned char* pcontroller2, unsigned char mouthopen)
{
	int					i, j;
	float				value;
	mstudiobonecontroller_t* pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t*)((byte*)pstudiohdr + pstudiohdr->bonecontrollerindex);

	for (j = 0; j < pstudiohdr->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if (pbonecontroller[j].type & STUDIO_RLOOP)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
				else
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0 / 256.0) + pbonecontroller[j].start;
				}
			}
			else
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				if (value < 0) value = 0;
				if (value > 1.0) value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", currententity->curstate.controller[j], currententity->latched.prevcontroller[j], value, dadt );
		}
		else
		{
			value = mouthopen / 64.0;
			if (value > 1.0) value = 1.0;
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

void R_StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q)
{
	int					j, k;
	vec4_t				q1, q2;
	vec3_t				angle1, angle2;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j + 3]);
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1, angle2))
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
	{
		AngleQuaternion(angle1, q);
	}
}

void R_StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos)
{
	int					j, k;
	mstudioanimvalue_t* panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t*)((byte*)panim + panim->offset[j]);
			/*
			if (i == 0 && j == 0)
				Con_DPrintf("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			*/

			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if (pbone->bonecontroller[j] != -1 && adj)
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

void R_StudioSlerpBones(vec4_t* q1, vec3_t* pos1, vec4_t* q2, vec3_t* pos2, float s)
{
	int			i;
	vec4_t		q3;
	float		s1;

	if (s < 0) s = 0;
	else if (s > 1.0) s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < pstudiohdr->numbones; i++)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

mstudioanim_t* R_StudioGetAnim(model_t* psubmodel, mstudioseqdesc_t* pseqdesc)
{
	mstudioseqgroup_t* pseqgroup;
	cache_user_t* paSequences;

	pseqgroup = (mstudioseqgroup_t*)((byte*)pstudiohdr + pstudiohdr->seqgroupindex);
	pseqgroup += pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t*)((byte*)pstudiohdr + pseqdesc->animindex);
	}

	paSequences = (cache_user_t*)psubmodel->submodels;

	if (paSequences == NULL)
	{
		paSequences = (cache_user_t*)Mem_Calloc(MAXSTUDIOGROUPS, sizeof(cache_user_t)); // UNDONE: leak!
		psubmodel->submodels = (dmodel_t*)paSequences;
	}

	if (!Cache_Check(&paSequences[pseqdesc->seqgroup]))
	{
		Con_DPrintf(const_cast<char*>("loading %s\n"), pseqgroup->name);
		COM_LoadCacheFile(pseqgroup->name, &paSequences[pseqdesc->seqgroup]);
	}

	return (mstudioanim_t*)((byte*)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

void SV_StudioSetupBones(model_t* pModel, float frame, int sequence, const vec_t* angles, const vec_t* origin, const unsigned char* pcontroller, const unsigned char* pblending, int iBone, const edict_t* edict)
{
	static vec4_t q[MAXSTUDIOBONES];
	static vec3_t pos[MAXSTUDIOBONES];
	static vec4_t q2[MAXSTUDIOBONES];
	static vec3_t pos2[MAXSTUDIOBONES];

	int chainlength;
	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	int chain[128];
	float bonematrix[3][4];
	mstudioanim_t* panim;
	float f;
	float adj[8];
	float s;

	chainlength = 0;
	if (sequence < 0 || sequence >= pstudiohdr->numseq)
	{
		Con_DPrintf(const_cast<char*>("SV_StudioSetupBones:  sequence %i/%i out of range for model %s\n"), sequence, pstudiohdr->numseq, pstudiohdr->name);
		sequence = 0;
	}

	pbones = (mstudiobone_t*)((char*)pstudiohdr + pstudiohdr->boneindex);
	pseqdesc = (mstudioseqdesc_t*)((char*)pstudiohdr + pstudiohdr->seqindex);
	pseqdesc += sequence;
	panim = R_StudioGetAnim(pModel, pseqdesc);

	if (iBone < -1 || iBone >= pstudiohdr->numbones)
		iBone = 0;

	if (iBone != -1)
	{
		do
		{
			chain[chainlength++] = iBone;
			iBone = pbones[iBone].parent;
		} while (iBone != -1);
	}
	else
	{
		chainlength = pstudiohdr->numbones;
		for (int i = 0; i < pstudiohdr->numbones; i++)
		{
			chain[pstudiohdr->numbones - 1 - i] = i;
		}
	}

	f = (pseqdesc->numframes > 1) ? (pseqdesc->numframes - 1) * frame / 256.0f : 0.0f;

	R_StudioCalcBoneAdj(0.0, adj, pcontroller, pcontroller, 0);
	s = f - (float)(int)f;

	for (int i = chainlength - 1; i >= 0; i--)
	{
		int bone = chain[i];
		R_StudioCalcBoneQuaterion((int)f, s, &pbones[bone], &panim[bone], adj, q[bone]);
		R_StudioCalcBonePosition((int)f, s, &pbones[bone], &panim[bone], adj, pos[bone]);

	}

	if (pseqdesc->numblends > 1)
	{
		panim = R_StudioGetAnim(pModel, pseqdesc);
		panim += pstudiohdr->numbones;

		for (int i = chainlength - 1; i >= 0; i--)
		{
			int bone = chain[i];
			R_StudioCalcBoneQuaterion((int)f, s, &pbones[bone], &panim[bone], adj, q2[bone]);
			R_StudioCalcBonePosition((int)f, s, &pbones[bone], &panim[bone], adj, pos2[bone]);
		}

		R_StudioSlerpBones(q, pos, q2, pos2, *pblending / 255.0f);
	}

	AngleMatrix(angles, rotationmatrix);
	rotationmatrix[0][3] = origin[0];
	rotationmatrix[1][3] = origin[1];
	rotationmatrix[2][3] = origin[2];
	for (int i = chainlength - 1; i >= 0; i--)
	{
		int bone = chain[i];
		int parent = pbones[bone].parent;
		QuaternionMatrix(q[bone], bonematrix);

		bonematrix[0][3] = pos[bone][0];
		bonematrix[1][3] = pos[bone][1];
		bonematrix[2][3] = pos[bone][2];
		R_ConcatTransforms((parent == -1) ? rotationmatrix : bonetransform[parent], bonematrix, bonetransform[bone]);
	}
}

void SV_SetStudioHullPlane(mplane_t* pplane, int iBone, int k, float dist)
{
	pplane->type = PLANE_ANYZ;

	pplane->normal[0] = bonetransform[iBone][0][k];
	pplane->normal[1] = bonetransform[iBone][1][k];
	pplane->normal[2] = bonetransform[iBone][2][k];

	pplane->dist = pplane->normal[2] * bonetransform[iBone][2][3]
		+ pplane->normal[1] * bonetransform[iBone][1][3]
		+ pplane->normal[0] * bonetransform[iBone][0][3]
		+ dist;
}

hull_t* R_StudioHull(model_t* pModel, float frame, int sequence, const vec_t* angles, const vec_t* origin, const vec_t* size, const unsigned char* pcontroller, const unsigned char* pblending, int* pNumHulls, const edict_t* pEdict, int bSkipShield)
{
	SV_InitStudioHull();

	if (r_cachestudio.value != 0)
	{
		r_studiocache_t* pCached = R_CheckStudioCache(pModel, frame, sequence, angles, origin, size, pcontroller, pblending);

		if (pCached)
		{
			Q_memcpy(studio_planes, &cache_planes[pCached->nStartPlane], pCached->numhulls * sizeof(mplane_t) * 6);
			Q_memcpy(studio_hull_hitgroup, &cache_hull_hitgroup[pCached->nStartHull], pCached->numhulls * sizeof(int));
			Q_memcpy(studio_hull, &cache_hull[pCached->nStartHull], pCached->numhulls * sizeof(hull_t));

			*pNumHulls = pCached->numhulls;
			return studio_hull;
		}
	}

	pstudiohdr = (studiohdr_t*)Mod_Extradata(pModel);

	vec_t angles2[3] = { -angles[0], angles[1], angles[2] };
	g_pSvBlendingAPI->SV_StudioSetupBones(pModel, frame, sequence, angles2, origin, pcontroller, pblending, -1, pEdict);

	// NOTE: numhitboxes range [0,21], so index 21 it's unreachable code for loop
	const int hitboxShieldIndex = 21;

	mstudiobbox_t* pbbox = (mstudiobbox_t*)((char*)pstudiohdr + pstudiohdr->hitboxindex);
	for (int i = 0; i < pstudiohdr->numhitboxes; i++)
	{
		if (bSkipShield && i == hitboxShieldIndex) continue;

		studio_hull_hitgroup[i] = pbbox[i].group;

		for (int j = 0; j < 3; j++)
		{
			mplane_t* plane0 = &studio_planes[i * 6 + j * 2 + 0];
			mplane_t* plane1 = &studio_planes[i * 6 + j * 2 + 1];
			SV_SetStudioHullPlane(plane0, pbbox[i].bone, j, pbbox[i].bbmax[j]);
			SV_SetStudioHullPlane(plane1, pbbox[i].bone, j, pbbox[i].bbmin[j]);

			plane0->dist += fabs(plane0->normal[0] * size[0]) + fabs(plane0->normal[1] * size[1]) + fabs(plane0->normal[2] * size[2]);
			plane1->dist -= fabs(plane1->normal[0] * size[0]) + fabs(plane1->normal[1] * size[1]) + fabs(plane1->normal[2] * size[2]);
		}
	}

	*pNumHulls = (bSkipShield == 1) ? pstudiohdr->numhitboxes - 1 : pstudiohdr->numhitboxes;
	if (r_cachestudio.value != 0)
	{
		R_AddToStudioCache(frame, sequence, angles, origin, size, pcontroller, pblending, pModel, studio_hull, *pNumHulls);
	}

	return &studio_hull[0];
}

int SV_HitgroupForStudioHull(int index)
{
	return studio_hull_hitgroup[index];
}

void R_InitStudioCache()
{
	Q_memset(rgStudioCache, 0, sizeof(rgStudioCache));

	r_cachecurrent = 0;
	nCurrentHull = 0;
	nCurrentPlane = 0;
}

void R_FlushStudioCache()
{
	R_InitStudioCache();
}

int R_StudioBodyVariations(model_t* model)
{
	if (model->type != mod_studio)
		return 0;

	studiohdr_t* shdr = (studiohdr_t*)Mod_Extradata(model);
	if (!shdr)
		return 0;

	int count = 1;
	mstudiobodyparts_t* pbodypart = (mstudiobodyparts_t*)((char*)shdr + shdr->bodypartindex);
	for (int i = 0; i < shdr->numbodyparts; i++, pbodypart++)
	{
		count *= pbodypart->nummodels;
	}

	return count;
}

int ModelFrameCount(model_t* model)
{
	int count;

	if (!model)
		return 1;

	switch (model->type)
	{
	case mod_sprite:
		count = ((msprite_t*)model->cache.data)->numframes;
		break;

	case mod_studio:
		count = R_StudioBodyVariations(model);
		break;

	default:
		return 1;
	}

	if (count < 1)
		return 1;

	return count;
}

void R_ResetStudio()
{
	pStudioAPI = &studio;
}

sfx_t* CL_LookupSound(const char* pName)
{
	for (int i = 0; i < MAX_SOUNDS; i++)
	{
		if (cl.sound_precache[i] != NULL && !Q_strcmp((char*)pName, cl.sound_precache[i]->name))
			return cl.sound_precache[i];
	}

	return S_PrecacheSound((char*)pName);
}

BOOL IsFlippedViewModel()
{
	return cl_righthand != NULL && cl_righthand->value > 0.0f && currententity->index == cl.viewent.index;
}

void R_StudioSetUpTransform(int trivial_accept)
{
	int				i;
	vec3_t			angles;
	vec3_t			modelpos;

	// tweek model origin	
		//for (i = 0; i < 3; i++)
		//	modelpos[i] = currententity->origin[i];

	VectorCopy(currententity->origin, modelpos);

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	angles[ROLL] = currententity->curstate.angles[ROLL];
	angles[PITCH] = currententity->curstate.angles[PITCH];
	angles[YAW] = currententity->curstate.angles[YAW];

	//Con_DPrintf("Angles %4.2f prev %4.2f for %i\n", angles[PITCH], currententity->index);
	//Con_DPrintf("movetype %d %d\n", currententity->movetype, currententity->aiment );
	if (currententity->curstate.movetype == MOVETYPE_STEP)
	{
		float			f = 0;
		float			d;

		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if ((cl.time < currententity->curstate.animtime + 1.0f) &&
			(currententity->curstate.animtime != currententity->latched.prevanimtime))
		{
			f = (cl.time - currententity->curstate.animtime) / (currententity->curstate.animtime - currententity->latched.prevanimtime);
			//Con_DPrintf("%4.2f %.2f %.2f\n", f, currententity->curstate.animtime, m_clTime);
		}

		if (r_dointerp)
		{
			// ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		for (i = 0; i < 3; i++)
		{
			modelpos[i] += (currententity->origin[i] - currententity->latched.prevorigin[i]) * f;
		}

		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		//if (f > -1.0 && f < 1.5) {}

//			Con_DPrintf("%.0f %.0f\n",currententity->msg_angles[0][YAW], currententity->msg_angles[1][YAW] );
		for (i = 0; i < 3; i++)
		{
			float ang1, ang2;

			ang1 = currententity->angles[i];
			ang2 = currententity->latched.prevangles[i];

			d = ang1 - ang2;
			if (d > 180)
			{
				d -= 360;
			}
			else if (d < -180)
			{
				d += 360;
			}

			angles[i] += d * f;
		}
		//Con_DPrintf("%.3f \n", f );
	}
	else if (currententity->curstate.movetype != MOVETYPE_NONE)
	{
		VectorCopy(currententity->angles, angles);
	}

	//Con_DPrintf("%.0f %0.f %0.f\n", modelpos[0], modelpos[1], modelpos[2] );
	//Con_DPrintf("%.0f %0.f %0.f\n", angles[0], angles[1], angles[2] );

	angles[PITCH] = -angles[PITCH];
	AngleMatrix(angles, rotationmatrix);

#ifndef GLQUAKE	
	static float viewmatrix[3][4];

	VectorCopy(vright, viewmatrix[0]);
	VectorCopy(vup, viewmatrix[1]);
	VectorInverse(viewmatrix[1]);
	VectorCopy(vpn, viewmatrix[2]);

	rotationmatrix[0][3] = modelpos[0] - r_origin[0];
	rotationmatrix[1][3] = modelpos[1] - r_origin[1];
	rotationmatrix[2][3] = modelpos[2] - r_origin[2];

	R_ConcatTransforms(viewmatrix, rotationmatrix, aliastransform);

	// do the scaling up of x and y to screen coordinates as part of the transform
	// for the unclipped case (it would mess up clipping in the clipped case).
	// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
	// correspondingly so the projected x and y come out right
	// FIXME: make this work for clipped case too?
	if (trivial_accept)
	{
		for (i = 0; i < 4; i++)
		{
			aliastransform[0][i] *= aliasxscale *
				(1.0 / (ZISCALE * INFINITE_DISTANCE));
			aliastransform[1][i] *= aliasyscale *
				(1.0 / (ZISCALE * INFINITE_DISTANCE));
			aliastransform[2][i] *= 1.0 / (ZISCALE * INFINITE_DISTANCE);

		}
	}
#endif

	rotationmatrix[0][3] = modelpos[0];
	rotationmatrix[1][3] = modelpos[1];
	rotationmatrix[2][3] = modelpos[2];
}

/*
================
R_StudioTransformVector
================
*/
void R_StudioTransformVector(vec_t* in, vec_t* out)
{
#if defined(GLQUAKE)
	out[0] = DotProduct(in, rotationmatrix[0]) + rotationmatrix[0][3];
	out[1] = DotProduct(in, rotationmatrix[1]) + rotationmatrix[1][3];
	out[2] = DotProduct(in, rotationmatrix[2]) + rotationmatrix[2][3];
#else
	out[0] = DotProduct(in, aliastransform[0]) + aliastransform[0][3];
	out[1] = DotProduct(in, aliastransform[1]) + aliastransform[1][3];
	out[2] = DotProduct(in, aliastransform[2]) + aliastransform[2][3];
#endif
}

BOOL R_StudioCheckBBox(void)
{
#if defined(GLQUAKE)
	mstudioseqdesc_t* pseqdesc;
	vec3_t mins, maxs;
	int i;

	if (VectorCompare(vec3_origin, pstudiohdr->bbmin))
	{
		if (VectorCompare(vec3_origin, pstudiohdr->min) == 0)
		{
			VectorAdd(pstudiohdr->min, currententity->origin, mins);
			VectorAdd(pstudiohdr->max, currententity->origin, maxs);
		}
		else
		{
			mins[0] = currententity->origin[0] - 16.0;
			mins[1] = currententity->origin[1] - 16.0;
			mins[2] = currententity->origin[2] - 16.0;

			maxs[0] = currententity->origin[0] + 16.0;
			maxs[1] = currententity->origin[1] + 16.0;
			maxs[2] = currententity->origin[2] + 16.0;
		}
	}
	else
	{
		VectorAdd(pstudiohdr->bbmin, currententity->origin, mins);
		VectorAdd(pstudiohdr->bbmax, currententity->origin, maxs);
	}

	if (currententity->curstate.sequence >= pstudiohdr->numseq)
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;


	for (i = 0; i < 8; i++)
	{
		vec3_t clip, out;

		if (i & 1)
			clip[0] = pseqdesc->bbmin[0];
		else
			clip[0] = pseqdesc->bbmax[0];

		if (i & 2)
			clip[1] = pseqdesc->bbmin[1];
		else
			clip[1] = pseqdesc->bbmax[1];

		if (i & 4)
			clip[2] = pseqdesc->bbmin[2];
		else
			clip[2] = pseqdesc->bbmax[2];

		R_StudioTransformVector(clip, out);

		if (mins[0] > out[0])
			mins[0] = out[0];
		if (out[0] > maxs[0])
			maxs[0] = out[0];
		if (mins[1] > out[1])
			mins[1] = out[1];
		if (out[1] > maxs[1])
			maxs[1] = out[1];
		if (mins[2] > out[2])
			mins[2] = out[2];
		if (out[2] > maxs[2])
			maxs[2] = out[2];
	}

	if (Host_IsSinglePlayerGame() || r_cullsequencebox.value == 0.0)
	{
		mplane_t plane;

		VectorCopy(vpn, plane.normal);

		plane.type = PLANE_ANYZ;
		plane.dist = DotProduct(vpn, r_origin);

		plane.signbits = SignbitsForPlane(&plane);

		return BoxOnPlaneSide(mins, maxs, &plane) != 2;
	}

	return R_CullBox(mins, maxs) == false;
#else
	mstudioseqdesc_t* pseqdesc;
	float basepts[8][3], v0, v1, zi, frac;
	finalvert_t* pv0, * pv1, viewpts[16];
	auxvert_t* pa0, * pa1, viewaux[16];
	int i, numv, flags;
	qboolean			zclipped, zfullyclipped;
	int					minz;
	unsigned			anyclip, allclip;


	currententity->trivial_accept = 0;
	if (currententity->curstate.sequence >= pstudiohdr->numseq)
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;

	basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] = min(pseqdesc->bbmin[0], pstudiohdr->bbmin[0]);
	basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] = max(pseqdesc->bbmax[0], pstudiohdr->bbmax[0]);

	basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] = min(pseqdesc->bbmin[1], pstudiohdr->bbmin[1]);
	basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] = max(pseqdesc->bbmax[1], pstudiohdr->bbmax[1]);

	basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] = min(pseqdesc->bbmin[2], pstudiohdr->bbmin[2]);
	basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] = max(pseqdesc->bbmax[2], pstudiohdr->bbmax[2]);

	zclipped = false;
	zfullyclipped = true;

	minz = 9999;
	for (i = 0; i < 8; i++)
	{
		R_StudioTransformVector(&basepts[i][0], &viewaux[i].fv[0]);

		if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE)
		{
			// we must clip points that are closer than the near clip plane
			viewpts[i].flags = ALIAS_Z_CLIP;
			zclipped = true;
		}
		else
		{
			if (viewaux[i].fv[2] < minz)
				minz = viewaux[i].fv[2];
			viewpts[i].flags = 0;
			zfullyclipped = false;
		}
	}

	if (zfullyclipped)
	{
		return false;	// everything was near-z-clipped
	}

	numv = 8;

	if (zclipped)
	{
		// organize points by edges, use edges to get new points (possible trivial
		// reject)
		for (i = 0; i < 12; i++)
		{
			// edge endpoints
			pv0 = &viewpts[aedges[i].index0];
			pv1 = &viewpts[aedges[i].index1];
			pa0 = &viewaux[aedges[i].index0];
			pa1 = &viewaux[aedges[i].index1];

			// if one end is clipped and the other isn't, make a new point
			if (pv0->flags ^ pv1->flags)
			{
				frac = (ALIAS_Z_CLIP_PLANE - pa0->fv[2]) /
					(pa1->fv[2] - pa0->fv[2]);
				viewaux[numv].fv[0] = pa0->fv[0] +
					(pa1->fv[0] - pa0->fv[0]) * frac;
				viewaux[numv].fv[1] = pa0->fv[1] +
					(pa1->fv[1] - pa0->fv[1]) * frac;
				viewaux[numv].fv[2] = ALIAS_Z_CLIP_PLANE;
				viewpts[numv].flags = 0;
				numv++;
			}
		}
	}

	// project the vertices that remain after clipping
	anyclip = 0;
	allclip = ALIAS_XY_CLIP_MASK;

	// TODO: probably should do this loop in ASM, especially if we use floats
	for (i = 0; i < numv; i++)
	{
		// we don't need to bother with vertices that were z-clipped
		if (viewpts[i].flags & ALIAS_Z_CLIP)
			continue;

		zi = 1.0 / viewaux[i].fv[2];

		// FIXME: do with chop mode in ASM, or convert to float
		v0 = (viewaux[i].fv[0] * xscale * zi) + xcenter;
		v1 = (viewaux[i].fv[1] * yscale * zi) + ycenter;

		flags = 0;

		if (v0 < r_refdef.fvrectx)
			flags |= ALIAS_LEFT_CLIP;
		if (v1 < r_refdef.fvrecty)
			flags |= ALIAS_TOP_CLIP;
		if (v0 > r_refdef.fvrectright)
			flags |= ALIAS_RIGHT_CLIP;
		if (v1 > r_refdef.fvrectbottom)
			flags |= ALIAS_BOTTOM_CLIP;

		anyclip |= flags;
		allclip &= flags;
	}

	if (allclip)
		return false;	// trivial reject off one side

	currententity->trivial_accept = !anyclip & !zclipped;

	return true;
#endif
}

float CL_StudioEstimateFrame(mstudioseqdesc_t* pseqdesc)
{
	double				dfdt, f;

	if (r_dointerp)
	{
		if (cl.time < currententity->curstate.animtime)
		{
			dfdt = 0;
		}
		else
		{
			dfdt = (cl.time - currententity->curstate.animtime) * currententity->curstate.framerate * pseqdesc->fps;

		}
	}
	else
	{
		dfdt = 0;
	}

	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (currententity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
	}

	f += dfdt;

	if (pseqdesc->flags & STUDIO_LOOPING)
	{
		if (pseqdesc->numframes > 1)
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) * (pseqdesc->numframes - 1);
		}
		if (f < 0)
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else
	{
		if (f >= pseqdesc->numframes - 1.001)
		{
			f = pseqdesc->numframes - 1.001;
		}
		if (f < 0.0)
		{
			f = 0.0;
		}
	}
	return f;
}

float CL_StudioEstimateInterpolant(void)
{
	float dadt = 1.0;

	if (r_dointerp && (currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01))
	{
		dadt = (cl.time - currententity->curstate.animtime) / 0.1;
		if (dadt > 2.0)
		{
			dadt = 2.0;
		}
	}
	return dadt;
}

void R_StudioCalcRotations(float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f)
{
	int					i;
	int					frame;
	mstudiobone_t* pbone;

	float				s;
	float				adj[MAXSTUDIOCONTROLLERS];
	float				dadt;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0;	// bah, fix this bug with changing sequences too fast
	}
	// BUG ( somewhere else ) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	//  and clamp it here
	else if (f < -0.01)
	{
		f = -0.01;
	}

	frame = (int)f;

	// Con_DPrintf("%d %.4f %.4f %.4f %.4f %d\n", currententity->curstate.sequence, m_clTime, currententity->animtime, currententity->frame, f, frame );

	// Con_DPrintf( "%f %f %f\n", currententity->angles[ROLL], currententity->angles[PITCH], currententity->angles[YAW] );

	// Con_DPrintf("frame %d %d\n", frame1, frame2 );


	dadt = CL_StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t*)((byte*)pstudiohdr + pstudiohdr->boneindex);

	R_StudioCalcBoneAdj(dadt, adj, currententity->curstate.controller, currententity->latched.prevcontroller, currententity->mouth.mouthopen);

	for (i = 0; i < pstudiohdr->numbones; i++, pbone++, panim++)
	{
		R_StudioCalcBoneQuaterion(frame, s, pbone, panim, adj, q[i]);

		R_StudioCalcBonePosition(frame, s, pbone, panim, adj, pos[i]);
		// if (0 && i == 0)
		//	Con_DPrintf("%d %d %d %d\n", currententity->curstate.sequence, frame, j, k );
	}

	if (pseqdesc->motiontype & STUDIO_X)
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Y)
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Z)
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * currententity->curstate.framerate;

	if (pseqdesc->motiontype & STUDIO_LX)
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if (pseqdesc->motiontype & STUDIO_LY)
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if (pseqdesc->motiontype & STUDIO_LZ)
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}

void MatrixCopy(float in[3][4], float out[3][4])
{
	Q_memcpy(out, in, sizeof(float) * 3 * 4);
}

void R_StudioSaveBones(void)
{
	int		i;

	mstudiobone_t* pbones;
	pbones = (mstudiobone_t*)((byte*)pstudiohdr + pstudiohdr->boneindex);

	cached_numbones = pstudiohdr->numbones;

	for (i = 0; i < pstudiohdr->numbones; i++)
	{
		Q_strcpy(&cached_bonename[i * sizeof(pbones[i].name)], pbones[i].name);
		MatrixCopy(bonetransform[i], cached_bonetransform[i]);
		MatrixCopy(lighttransform[i], cached_lighttransform[i]);
	}
}

void R_StudioMergeBones(model_t* psubmodel)
{
	int					i, j;
	double				f;
	int					do_hunt = true;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float		pos[MAXSTUDIOBONES][3];
	float				bonematrix[3][4];
	static vec4_t		q[MAXSTUDIOBONES];

	if (currententity->curstate.sequence >= pstudiohdr->numseq)
	{
		currententity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;

	f = CL_StudioEstimateFrame(pseqdesc);

	if (currententity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", currententity->prevframe, f );
	}

	panim = R_StudioGetAnim(psubmodel, pseqdesc);
	R_StudioCalcRotations(pos, q, pseqdesc, panim, f);

	pbones = (mstudiobone_t*)((byte*)pstudiohdr + pstudiohdr->boneindex);


	for (i = 0; i < pstudiohdr->numbones; i++)
	{
		for (j = 0; j < cached_numbones; j++)
		{
			if (Q_strcasecmp(pbones[i].name, &cached_bonename[j]) == 0)
			{
				MatrixCopy(cached_bonetransform[j], bonetransform[i]);
				MatrixCopy(cached_lighttransform[j], lighttransform[i]);
				break;
			}
		}
		if (j >= cached_numbones)
		{
			QuaternionMatrix(q[i], bonematrix);

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if (pbones[i].parent == -1)
			{
#ifdef GLQUAKE
				R_ConcatTransforms(rotationmatrix, bonematrix, bonetransform[i]);

				// MatrixCopy should be faster...
				R_ConcatTransforms(rotationmatrix, bonematrix, lighttransform[i]);
				//MatrixCopy(bonetransform[i], lighttransform[i]);
#else
				R_ConcatTransforms(aliastransform, bonematrix, bonetransform[i]);
				R_ConcatTransforms(rotationmatrix, bonematrix, lighttransform[i]);
#endif

				// Apply client-side effects to the transformation matrix
				CL_FxTransform(currententity, bonetransform[i]);
			}
			else
			{
				R_ConcatTransforms(bonetransform[pbones[i].parent], bonematrix, bonetransform[i]);
				R_ConcatTransforms(lighttransform[pbones[i].parent], bonematrix, lighttransform[i]);
			}
		}
	}
}

void R_StudioSetupBones(void)
{
	int					i;
	double				f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float		pos[MAXSTUDIOBONES][3];
	static vec4_t		q[MAXSTUDIOBONES];
	float				bonematrix[3][4];

	static float		pos2[MAXSTUDIOBONES][3];
	static vec4_t		q2[MAXSTUDIOBONES];
	static float		pos3[MAXSTUDIOBONES][3];
	static vec4_t		q3[MAXSTUDIOBONES];
	static float		pos4[MAXSTUDIOBONES][3];
	static vec4_t		q4[MAXSTUDIOBONES];

	if (currententity->curstate.sequence >= pstudiohdr->numseq)
	{
		currententity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;

	// always want new gait sequences to start on frame zero
/*	if ( r_playerinfo )
	{
		int playerNum = currententity->index - 1;

		// new jump gaitsequence?  start from frame zero
		if ( m_nPlayerGaitSequences[ playerNum ] != r_playerinfo->gaitsequence )
		{
	//		r_playerinfo->gaitframe = 0.0;
			Con_Printf( "Setting gaitframe to 0\n" );
		}

		m_nPlayerGaitSequences[ playerNum ] = r_playerinfo->gaitsequence;
//		Con_Printf( "index: %d     gaitsequence: %d\n",playerNum, r_playerinfo->gaitsequence);
	}
*/
	f = CL_StudioEstimateFrame(pseqdesc);

	if (currententity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", currententity->prevframe, f );
	}

	panim = R_StudioGetAnim(r_model, pseqdesc);
	R_StudioCalcRotations(pos, q, pseqdesc, panim, f);

	if (pseqdesc->numblends > 1)
	{
		float				s;
		float				dadt;

		panim += pstudiohdr->numbones;
		R_StudioCalcRotations(pos2, q2, pseqdesc, panim, f);

		dadt = CL_StudioEstimateInterpolant();
		s = (currententity->curstate.blending[0] * dadt + currententity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		R_StudioSlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations(pos3, q3, pseqdesc, panim, f);

			panim += pstudiohdr->numbones;
			R_StudioCalcRotations(pos4, q4, pseqdesc, panim, f);

			s = (currententity->curstate.blending[0] * dadt + currententity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones(q3, pos3, q4, pos4, s);

			s = (currententity->curstate.blending[1] * dadt + currententity->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones(q, pos, q3, pos3, s);
		}
	}

	if (r_dointerp &&
		currententity->latched.sequencetime &&
		(currententity->latched.sequencetime + 0.2 > cl.time) &&
		(currententity->latched.prevsequence < pstudiohdr->numseq))
	{
		// blend from last sequence
		static float		pos1b[MAXSTUDIOBONES][3];
		static vec4_t		q1b[MAXSTUDIOBONES];
		float				s;

		if (currententity->latched.prevsequence >= pstudiohdr->numseq)
		{
			currententity->latched.prevsequence = 0;
		}

		pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->latched.prevsequence;
		panim = R_StudioGetAnim(r_model, pseqdesc);
		// clip prevframe
		R_StudioCalcRotations(pos1b, q1b, pseqdesc, panim, currententity->latched.prevframe);

		if (pseqdesc->numblends > 1)
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations(pos2, q2, pseqdesc, panim, currententity->latched.prevframe);

			s = (currententity->latched.prevseqblending[0]) / 255.0;
			R_StudioSlerpBones(q1b, pos1b, q2, pos2, s);

			if (pseqdesc->numblends == 4)
			{
				panim += pstudiohdr->numbones;
				R_StudioCalcRotations(pos3, q3, pseqdesc, panim, currententity->latched.prevframe);

				panim += pstudiohdr->numbones;
				R_StudioCalcRotations(pos4, q4, pseqdesc, panim, currententity->latched.prevframe);

				s = (currententity->latched.prevseqblending[0]) / 255.0;
				R_StudioSlerpBones(q3, pos3, q4, pos4, s);

				s = (currententity->latched.prevseqblending[1]) / 255.0;
				R_StudioSlerpBones(q1b, pos1b, q3, pos3, s);
			}
		}

		s = 1.0 - (cl.time - currententity->latched.sequencetime) / 0.2;
		R_StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		//Con_DPrintf("prevframe = %4.2f\n", f);
		currententity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t*)((byte*)pstudiohdr + pstudiohdr->boneindex);

	// bounds checking
	if (r_playerinfo)
	{
		if (r_playerinfo->gaitsequence >= pstudiohdr->numseq)
		{
			r_playerinfo->gaitsequence = 0;
		}
	}

	// calc gait animation
	if (r_playerinfo && r_playerinfo->gaitsequence != 0)
	{
		if (r_playerinfo->gaitsequence >= pstudiohdr->numseq)
		{
			r_playerinfo->gaitsequence = 0;
		}

		int copy = 1;

		pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + r_playerinfo->gaitsequence;

		panim = R_StudioGetAnim(r_model, pseqdesc);
		R_StudioCalcRotations(pos2, q2, pseqdesc, panim, r_playerinfo->gaitframe);

		for (i = 0; i < pstudiohdr->numbones; i++)
		{
			if (!Q_strcmp(pbones[i].name, "Bip01 Spine"))
			{
				copy = 0;
			}

			if (copy)
			{
				Q_memcpy(pos[i], pos2[i], sizeof(pos[i]));
				Q_memcpy(q[i], q2[i], sizeof(q[i]));
			}
		}
	}

	for (i = 0; i < pstudiohdr->numbones; i++)
	{
		QuaternionMatrix(q[i], bonematrix);

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1)
		{
#if defined(GLQUAKE)
			R_ConcatTransforms(rotationmatrix, bonematrix, bonetransform[i]);

			// MatrixCopy should be faster...
			R_ConcatTransforms(rotationmatrix, bonematrix, lighttransform[i]);
			//MatrixCopy(bonetransform[i], lighttransform[i]);

#else

			R_ConcatTransforms(aliastransform, bonematrix, bonetransform[i]);
			R_ConcatTransforms(rotationmatrix, bonematrix, lighttransform[i]);

#endif

			// Apply client-side effects to the transformation matrix
			CL_FxTransform(currententity, bonetransform[i]);
		}
		else
		{
			R_ConcatTransforms(bonetransform[pbones[i].parent], bonematrix, bonetransform[i]);
			R_ConcatTransforms(lighttransform[pbones[i].parent], bonematrix, lighttransform[i]);
		}
	}
}

void R_StudioCalcAttachments(void)
{
	int i;
	mstudioattachment_t* pattachment;

	if (pstudiohdr->numattachments > 4)
	{
		Sys_Error("Too many attachments on %s\n", currententity->model->name);
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t*)((byte*)pstudiohdr + pstudiohdr->attachmentindex);
	for (i = 0; i < pstudiohdr->numattachments; i++)
	{
		VectorTransform(pattachment[i].org, lighttransform[pattachment[i].bone], currententity->attachment[i]);
	}
}

void R_StudioClientEvents(void)
{
	mstudioseqdesc_t* pseqdesc;
	mstudioevent_s* ev;
	float fEstFrame, flStart;

	if (cl.time == cl.oldtime)
		return;

	if (currententity->curstate.effects & EF_MUZZLEFLASH)
	{
		if (g_bIsTerrorStrike)
		{
			dlight_t* dl = CL_AllocDlight(0);
			VectorCopy(currententity->attachment[0], dl->origin);
			dl->radius = 300.0;
			dl->die = cl.time + 0.05;
			dl->color.r = 255;
			dl->color.g = 192;
			dl->color.b = 128;
		}
		else
		{
			dlight_t* dl = CL_AllocElight(0);
			VectorCopy(currententity->attachment[0], dl->origin);
			dl->radius = 16.0;
			dl->decay = 320.0;
			dl->die = cl.time + 0.05;
			dl->color.r = 255;
			dl->color.g = 192;
			dl->color.b = 64;
		}

		// эффект спаршен, снимаем флаг
		currententity->curstate.effects &= ~EF_MUZZLEFLASH;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;

	if (!pseqdesc->numevents)
		return;

	ev = (mstudioevent_s*)((byte*)pstudiohdr + pseqdesc->eventindex);

	fEstFrame = CL_StudioEstimateFrame(pseqdesc);
	flStart = fEstFrame - currententity->curstate.framerate * host_frametime * pseqdesc->fps;

	if (currententity->latched.sequencetime == currententity->curstate.animtime)
	{
		if (pseqdesc->flags & STUDIO_LOOPING)
			flStart = -0.01f;
	}

	for (int i = 0; i < pseqdesc->numevents; i++)
	{
		if (ev[i].event >= 5000 && ev[i].frame > flStart && fEstFrame >= ev[i].frame)
		{
			ClientDLL_StudioEvent(&ev[i], currententity);
		}
	}
}

void R_StudioDynamicLight(cl_entity_t* ent, alight_t* plight)
{
	if (filterMode)
	{
		plight->ambientlight = filterBrightness * 192.0f;
		plight->shadelight = 0;
		plight->color[0] = filterColorRed * filterBrightness;
		plight->color[1] = filterColorGreen * filterBrightness;
		plight->color[2] = filterColorBlue * filterBrightness;
		plight->plightvec[0] = plight->plightvec[1] = 0;
		plight->plightvec[1] = -1;
	}
	else if (r_fullbright.value == 1)
	{
		plight->ambientlight = 192.0f;
		plight->shadelight = 0;
		plight->color[0] = plight->color[1] = plight->color[2] = 1;
		plight->plightvec[0] = plight->plightvec[1] = 0;
		plight->plightvec[1] = -1;
	}
	else
	{
		float dr, len, scale, intensity;
		vec3_t light, uporigin, end, upend, dist;
		int r = 0, g = 0, b = 0, a = 0;

		light[0] = light[1] = 0;
		light[2] = -1.0f;

		if (currententity->curstate.effects & EF_INVLIGHT)
			light[2] = 1.0f;

		VectorCopy(ent->origin, uporigin);
		uporigin[2] -= light[2] * 8.0f;

		if ((movevars.skycolor_r + movevars.skycolor_g + movevars.skycolor_b) != 0.0f)
		{
			msurface_t* psurf;

			end[0] = ent->origin[0] - movevars.skyvec_x * 8192.0f;
			end[1] = ent->origin[1] - movevars.skyvec_y * 8192.0f;
			end[2] = ent->origin[2] - movevars.skyvec_z * 8192.0f;

			psurf = SurfaceAtPoint(cl.worldmodel, cl.worldmodel->nodes, uporigin, end);

			if ((ent->model->flags & FL_PARTIALGROUND) || (psurf != NULL && (psurf->flags & SURF_DRAWSKY)))
			{
				r = movevars.skycolor_r;
				g = movevars.skycolor_g;
				b = movevars.skycolor_b;

				light[0] = movevars.skyvec_x;
				light[1] = movevars.skyvec_y;
				light[2] = movevars.skyvec_z;
			}
		}

		if ((r + g + b) == 0)
		{
			colorVec c;
			vec_t fapprox[4];

			VectorScale(light, 2048, upend);
			VectorAdd(upend, uporigin, upend);

			c = R_LightVec(uporigin, upend);

			r = c.r;
			g = c.g;
			b = c.b;
			a = c.a;

			uporigin[0] -= 16.0f;
			uporigin[1] -= 16.0f;
			upend[0] -= 16.0f;
			upend[1] -= 16.0f;

			c = R_LightVec(uporigin, upend);

			fapprox[0] = (float)(c.r + c.g + c.b) / 768.0f;
			uporigin[0] += 32.0f;
			upend[0] += 32.0f;

			c = R_LightVec(uporigin, upend);

			fapprox[1] = (float)(c.r + c.g + c.b) / 768.0f;
			uporigin[1] += 32.0f;
			upend[1] += 32.0f;

			c = R_LightVec(uporigin, upend);

			fapprox[2] = (float)(c.r + c.g + c.b) / 768.0f;
			uporigin[0] -= 32.0f;
			upend[0] -= 32.0f;

			c = R_LightVec(uporigin, upend);

			fapprox[3] = (float)(c.r + c.g + c.b) / 768.0f;

			light[0] = fapprox[0] - fapprox[1] - fapprox[2] + fapprox[3];
			light[1] = fapprox[1] + fapprox[0] - fapprox[2] - fapprox[3];
			VectorNormalize(light);
		}

		if (currententity->curstate.renderfx == kRenderFxLightMultiplier)
		{
			// some magic for CZero
			if (currententity->curstate.iuser4 != 10)
			{
				float mul = (float)currententity->curstate.iuser4 / 10.0f;
				r = int((float)r * mul);
				g = int((float)g * mul);
				b = int((float)b * mul);
			}
		}

		currententity->cvFloorColor.r = r;
		currententity->cvFloorColor.g = g;
		currententity->cvFloorColor.b = b;
		currententity->cvFloorColor.a = a;

		end[0] = (float)r;
		end[1] = (float)g;
		end[2] = (float)b;

		intensity = max(max(end[0], end[1]), end[2]);

		if (intensity == 0.0f)
			intensity = 1.0f;

		VectorScale(light, intensity, light);

		for (int i = 0; i < MAX_DLIGHTS; i++)
		{
			if (cl_dlights[i].die < cl.time)
				continue;

			VectorSubtract(ent->origin, cl_dlights[i].origin, dist);
			len = Length(dist);
			dr = cl_dlights[i].radius - len;

			if (dr > 0.0f)
			{
				if (len > 1.0f)
					scale = dr / len;
				else
					scale = dr;

				intensity += dr;

				VectorScale(dist, scale, dist);
				VectorAdd(light, dist, light);

				end[0] += (float)cl_dlights[i].color.r * dr / 256.0f;
				end[1] += (float)cl_dlights[i].color.g * dr / 256.0f;
				end[2] += (float)cl_dlights[i].color.b * dr / 256.0f;
			}
		}

		if (currententity->model->flags & STUDIO_DYNAMIC_LIGHT)
			scale = 0.6f;
		else
			scale = clamp(v_direct.value, 0.75f, 1.0f);

		VectorScale(light, scale, light);
		len = Length(light);

		plight->shadelight = len;
		plight->ambientlight = intensity - len;

		intensity = max(max(end[0], end[1]), end[2]);

		if (intensity == 0.0f)
		{
			plight->color[0] = plight->color[1] = plight->color[2] = 1;
		}
		else
		{
			plight->color[0] = (1.0f / intensity) * end[0];
			plight->color[1] = (1.0f / intensity) * end[1];
			plight->color[2] = (1.0f / intensity) * end[2];
		}

		if (plight->ambientlight > 128)
			plight->ambientlight = 128;

		if (plight->ambientlight + len > 255)
			plight->shadelight = 255;

		VectorNormalize(light);
		VectorCopy(light, plight->plightvec);
	}
}

void R_StudioEntityLight(alight_t* plight)
{
	float lstrength[3]{};
	vec3_t org, end;
	float maxdist, mindist, dist, rad, strength;
	int idx, localnumlights;

	numlights = 0;
	localnumlights = 0;

	VectorCopy(currententity->origin, org);

	maxdist = 1000000.0f;
	mindist = 0.0f;

	for (int i = 0; i < MAX_ELIGHTS; i++)
	{
		if (cl_elights[i].die <= cl.time)
			continue;

		if (cl_elights[i].radius <= mindist)
			continue;

		if (BEAMENT_ENTITY(cl_elights[i].key) == currententity->index)
		{
			int attachment = BEAMENT_ATTACHMENT(cl_elights[i].key);
			if (attachment)
			{
				VectorCopy(currententity->attachment[attachment], cl_elights[i].origin);
			}
			else
			{
				VectorCopy(currententity->origin, cl_elights[i].origin);
			}
		}

		VectorCopy(cl_elights[i].origin, end);
		VectorSubtract(org, end, end);

		dist = DotProduct(end, end);
		rad = cl_elights[i].radius * cl_elights[i].radius;

		if (dist <= rad)
			strength = 1;
		else
			strength = rad / dist;

		if (strength > 0.004f)
		{
			if (localnumlights < 3)
				idx = localnumlights;
			else
			{
				idx = -1;
				for (int j = 0; j < localnumlights; j++)
				{
					if (lstrength[j] < maxdist && lstrength[j] < strength)
					{
						idx = j;
						maxdist = lstrength[i];
					}
				}
			}

			if (idx != -1)
			{
				lstrength[idx] = strength;
				locallight[idx] = &cl_elights[i];
				// квадрат радиуса
				locallightR2[idx] = rad;
				locallinearlight[idx][0] = lineargammatable[4 * cl_elights[i].color.r];
				locallinearlight[idx][1] = lineargammatable[4 * cl_elights[i].color.g];
				locallinearlight[idx][2] = lineargammatable[4 * cl_elights[i].color.b];

				if (idx >= localnumlights)
					localnumlights = idx + 1;
			}

		}

	}

	numlights = localnumlights;
}

void R_StudioSetupLighting(alight_t* plight)
{
	int numbones = pstudiohdr->numbones;

	r_ambientlight = plight->ambientlight;
	VectorCopy(plight->plightvec, r_plightvec);
	r_shadelight = plight->shadelight;

	for (int i = 0; i < numbones; i++)
		VectorIRotate(r_plightvec, lighttransform[i], r_blightvec[i]);

	r_icolormix.r = int(plight->color[0] * studiolightmax16f) & 0xFF00;
	r_icolormix.g = int(plight->color[1] * studiolightmax16f) & 0xFF00;
	r_icolormix.b = int(plight->color[2] * studiolightmax16f) & 0xFF00;

	r_icolormix32.r = int(plight->color[0] * (255.0f * 256.0f)) & 0xFF00;
	r_icolormix32.g = int(plight->color[1] * (255.0f * 256.0f)) & 0xFF00;
	r_icolormix32.b = int(plight->color[2] * (255.0f * 256.0f)) & 0xFF00;

	VectorCopy(plight->color, r_colormix);
}

/*
===========
R_StudioLighting
===========
*/
void R_StudioLighting(float* lv, int bone, int flags, vec_t* normal)
{
	float shadescale;
	float illum, lightcos;

	illum = r_ambientlight;

	if (flags & STUDIO_NF_FLATSHADE
#if !defined ( GLQUAKE )
		&& drawstyle != 1
#endif
		)
	{
		illum += r_shadelight * 0.8;
	}
	else
	{
		float r;
		if (bone == -1)
			lightcos = DotProduct(normal, r_plightvec);
		else
			lightcos = DotProduct(normal, r_blightvec[bone]); // -1 colinear, 1 opposite

		if (lightcos > 1.0)
			lightcos = 1;

		r = v_lambert1;

		if (r < 1.0)
		{
			shadescale = (r - lightcos) / (r + 1.0);

			if (shadescale > 0.0)
				illum += r_shadelight * shadescale;
		}
		else
		{
			illum += r_shadelight;
			lightcos = (lightcos + (r - 1.0)) / r;         // do modified hemispherical lighting

			if (lightcos > 0.0)
				illum -= r_shadelight * lightcos;
		}

		if (illum <= 0)
			illum = 0;
	}

	if (illum > 255)
		illum = 255;

#if defined ( GLQUAKE )
	*lv = (float)lightgammatable[(int)illum * 4] / 1023;    // Light from 0 to 1.0
#else
	*lv = (float)lightgammatable[(int)illum * 4] * 64.0;    // Software lighting
#endif
}

void R_StudioDrawBones(void)
{
	mstudiobone_t* pbone;
	vec_t p[8][3];
	vec3_t up, a1, tmp, right, forward;
	float lv;
	int parent;

	pbone = (mstudiobone_t*)((byte*)pstudiohdr + pstudiohdr->boneindex);

	tri.SpriteTexture(cl_sprite_white, 0);

	for (int i = 0; i < pstudiohdr->numbones; i++, pbone++)
	{
		parent = pbone->parent;

		if (parent == -1)
			continue;

		a1[0] = a1[1] = a1[2] = 1;

		up[0] = lighttransform[i][0][3] - lighttransform[parent][0][3];
		up[1] = lighttransform[i][1][3] - lighttransform[parent][1][3];
		up[2] = lighttransform[i][2][3] - lighttransform[parent][2][3];

		if (up[0] <= up[1])
		{
			if (up[1] > up[2])
				a1[1] = 0;
			else
				a1[2] = 0;
		}
		else
		{
			if (up[0] > up[2])
				a1[0] = 0;
			else
				a1[2] = 0;
		}

		CrossProduct(up, a1, right);
		VectorNormalize(right);
		CrossProduct(up, right, forward);
		VectorNormalize(forward);

		VectorScale(right, 2.0f, right);
		VectorScale(forward, 2.0f, forward);

		for (int j = 0; j < 8; j++)
		{
			p[j][0] = lighttransform[parent][0][3];
			p[j][1] = lighttransform[parent][1][3];
			p[j][2] = lighttransform[parent][2][3];


			if (j & 1)
			{
				VectorSubtract(p[j], right, p[j]);
			}
			else
			{
				VectorAdd(p[j], right, p[j]);
			}

			if (j & 2)
			{
				VectorSubtract(p[j], forward, p[j]);
			}
			else
			{
				VectorAdd(p[j], forward, p[j]);
			}

			if (j & 4)
			{
				VectorAdd(p[j], up, p[j]);
			}
		}

		VectorNormalize(up);
		VectorNormalize(right);
		VectorNormalize(forward);

		tri.Begin(TRI_QUADS);
		tri.Color4f(1.0f, 1.0f, 1.0f, 1.0f);
		tri.TexCoord2f(0, 0);

		for (int j = 0; j < 6; j++)
		{
			switch (j)
			{
			case 0:
				VectorCopy(right, tmp);
				break;
			case 1:
				VectorCopy(forward, tmp);
				break;
			case 2:
				VectorCopy(up, tmp);
				break;
			case 3:
				VectorScale(right, -1.0, tmp);
				break;
			case 4:
				VectorScale(forward, -1.0, tmp);
				break;
			case 5:
				VectorScale(up, -1.0, tmp);
				break;
			default:
				break;
			}

			R_StudioLighting(&lv, -1, 0, tmp);
#if !defined(GLQUAKE)
			// така€ операци€ выполн€етс€ только в софтваре
			// деление на 65536 происходит от того, что дл€ ogl значение €ркости делитс€ на 1023, а дл€ software умножаетс€ на 64
			lv /= (float)0x10000;
#endif

			tri.Brightness(lv);
			tri.Vertex3fv(p[boxpnt[j][0]]);
			tri.Vertex3fv(p[boxpnt[j][1]]);
			tri.Vertex3fv(p[boxpnt[j][2]]);
			tri.Vertex3fv(p[boxpnt[j][3]]);
		}

		tri.End();

	}
}

void R_StudioDrawHulls(void)
{
	vec_t p[8][3];
	vec3_t tmp;
	float lv;
	mstudiobbox_t* pbbox = (mstudiobbox_t*)((byte*)pstudiohdr + pstudiohdr->hitboxindex);

	tri.SpriteTexture(cl_sprite_white, 0);

	for (int i = 0; i < pstudiohdr->numhitboxes; i++, pbbox++)
	{
		for (int j = 0; j < 8; j++)
		{
			tmp[0] = (j & 1) ? pbbox->bbmin[0] : pbbox->bbmax[0];
			tmp[1] = (j & 2) ? pbbox->bbmin[1] : pbbox->bbmax[1];
			tmp[2] = (j & 4) ? pbbox->bbmin[2] : pbbox->bbmax[2];
			VectorTransform(tmp, lighttransform[pbbox->bone], p[j]);
		}
		tri.Begin(TRI_QUADS);
		tri.Color4f(hullcolor[pbbox->group % 8][0], hullcolor[pbbox->group % 8][1], hullcolor[pbbox->group % 8][1], 1.0f);
		tri.TexCoord2f(0, 0);

		for (int j = 0; j < 6; j++)
		{
			tmp[0] = tmp[1] = tmp[2] = 0;
			tmp[j % 3] = (j >= 3) ? -1.0f : 1.0f;
			R_StudioLighting(&lv, pbbox->bone, 0, tmp);
#if !defined(GLQUAKE)
			// 1 / (1023 * 64)
			lv /= 0x10000;
#endif
			tri.Brightness(lv);
			tri.Vertex3fv(p[boxpnt[j][0]]);
			tri.Vertex3fv(p[boxpnt[j][1]]);
			tri.Vertex3fv(p[boxpnt[j][2]]);
			tri.Vertex3fv(p[boxpnt[j][3]]);
		}

		tri.End();
	}
}

void R_StudioSetupModel(int bodypart)
{
	if (pstudiohdr->numbodyparts < bodypart)
	{
		Con_DPrintf(const_cast<char*>("R_StudioSetupModel: no such bodypart %d\n"), bodypart);
		bodypart = 0;
	}

	pbodypart = (mstudiobodyparts_t*)((byte*)pstudiohdr + pstudiohdr->bodypartindex) + bodypart;
	psubmodel = (mstudiomodel_t*)((byte*)pstudiohdr + pbodypart->modelindex) + (currententity->curstate.body / pbodypart->base % pbodypart->nummodels);
}

studiohdr_t* R_LoadTextures(model_t* psubmodel)
{
	model_t* ptexinfo;
	char filename[MAX_PATH];

	if (pstudiohdr->textureindex)
		return pstudiohdr;

	ptexinfo = (model_t*)psubmodel->texinfo;

	if (ptexinfo != NULL && Cache_Check(&ptexinfo->cache))
		return (studiohdr_t*)ptexinfo->cache.data;

	Q_strncpy(filename, psubmodel->name, sizeof(filename) - 2);
	filename[sizeof(filename) - 2] = '\0';
	Q_strcpy(&filename[Q_strlen(filename) - 4], "T.mdl");

	ptexinfo = Mod_ForName(filename, true, false);
	psubmodel->texinfo = (mtexinfo_t*)ptexinfo;

	studiohdr_t* phdr = ((studiohdr_t*)ptexinfo->cache.data);

	Q_strncpy(phdr->name, filename, sizeof(phdr->name) - 1);
	phdr->name[sizeof(phdr->name) - 1] = '\0';

	return phdr;
}

void BuildNormalIndexTable(void)
{
	int i, j;
	mstudiomesh_t* pmesh;
	short* ptri, tri;
	mstudiotrivert_t* ptrivert;

	for (i = 0; i < psubmodel->numverts; i++)
		g_NormalIndex[i] = 255;

	pmesh = (mstudiomesh_t*)((byte*)psubmodel + psubmodel->meshindex);
	// mstudiotrivert_t в количестве n сразу же после задани€ n (2 байта)
	ptri = (short*)((byte*)pstudiohdr + pmesh->triindex);

	for (i = 0; i < psubmodel->nummesh; i++, pmesh++)
	{
		while (((tri = *ptri++) != 0))
		{
			if (tri < 0)
				tri = -tri;

			ptrivert = (mstudiotrivert_t*)(ptri);
			for (j = 0; j < tri; j++, ptri = (short*)++ptrivert)
			{
				if (g_NormalIndex[ptrivert->normindex] < 0)
					g_NormalIndex[ptrivert->normindex] = ptrivert->s;
			}

		}
	}
}

void R_StudioTransformAuxVert(auxvert_t* av, int bone, vec_t* vert)
{
	av->fv[0] = DotProduct(vert, bonetransform[bone][0]) + bonetransform[bone][0][3];
	av->fv[1] = DotProduct(vert, bonetransform[bone][1]) + bonetransform[bone][1][3];
	av->fv[2] = DotProduct(vert, bonetransform[bone][2]) + bonetransform[bone][2][3];
}

#if !defined(GLQUAKE)
void R_StudioProjectFinalVert(finalvert_t* fv, auxvert_t* av)
{
	float	zi;

	// project points
	zi = 1.0 / av->fv[2];

	fv->v[5] = zi * ziscale;

	fv->v[0] = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
	fv->v[1] = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}
#endif

void BuildGlowShellVerts(vec3_t* pstudioverts, auxvert_t* pauxverts)
{
	int i, numbones;
	vec3_t* pstudionorm, vert;
#if !defined(GLQUAKE)
	finalvert_t* fv;
#endif
	byte* pvbone;
	float scale;

#if !defined(GLQUAKE)
	fv = pfinalverts;
#endif
	pstudionorm = (vec3_t*)((byte*)pstudiohdr + psubmodel->normindex);
	scale = currententity->curstate.renderamt * 0.05;
	pvbone = ((byte*)pstudiohdr + psubmodel->vertinfoindex);

	for (i = 0; i < pstudiohdr->numbones; i++)
		chromeage[i] = 0;

	for (i = 0; i < psubmodel->numverts; i++)
	{
		VectorMA(pstudioverts[i], scale, pstudionorm[g_NormalIndex[i]], vert);

		R_StudioTransformAuxVert(&pauxverts[i], pvbone[i], vert);

#if !defined(GLQUAKE)
		fv->flags = 0;

		if ((&pauxverts[i])->fv[2] < ALIAS_Z_CLIP_PLANE)
			fv->flags = ALIAS_Z_CLIP;
		else
		{
			R_StudioProjectFinalVert(fv, &pauxverts[i]);

			if (fv->v[0] < r_refdef.aliasvrect.x)
				fv->flags |= ALIAS_LEFT_CLIP;
			if (fv->v[1] < r_refdef.aliasvrect.y)
				fv->flags |= ALIAS_TOP_CLIP;
			if (fv->v[0] > r_refdef.aliasvrectright)
				fv->flags |= ALIAS_RIGHT_CLIP;
			if (fv->v[1] > r_refdef.aliasvrectbottom)
				fv->flags |= ALIAS_BOTTOM_CLIP;
		}
#endif
	}

	g_ChromeOrigin[0] = cos(r_glowshellfreq.value * cl.time) * 4000.0f;
	g_ChromeOrigin[1] = sin(r_glowshellfreq.value * cl.time) * 4000.0f;
	g_ChromeOrigin[2] = cos(r_glowshellfreq.value * cl.time * 0.33f) * 4000.0f;

	tri.Color4ub(currententity->curstate.rendercolor.r, currententity->curstate.rendercolor.g, currententity->curstate.rendercolor.b, 255);
}

void R_StudioChrome(int* pchrome, int bone, vec_t* normal)
{
	vec3_t chromeupvec, chromerightvec, tmp;

	if (chromeage[bone] != r_smodels_total)
	{
		VectorScale(g_ChromeOrigin, -1.0f, tmp);
		tmp[0] += lighttransform[bone][0][3];
		tmp[1] += lighttransform[bone][1][3];
		tmp[2] += lighttransform[bone][2][3];
		VectorNormalize(tmp);
		CrossProduct(tmp, vright, chromeupvec);
		VectorNormalize(chromeupvec);
		CrossProduct(chromeupvec, tmp, chromerightvec);
		VectorNormalize(chromerightvec);
		VectorIRotate(chromeupvec, lighttransform[bone], r_chromeup[bone]);
		VectorIRotate(chromerightvec, lighttransform[bone], r_chromeright[bone]);
		chromeage[bone] = r_smodels_total;
	}

#ifdef GLQUAKE
	pchrome[0] = 1024.0f * ((DotProduct(normal, r_chromeright[bone]) + 1.0f) * 32.0f);
	pchrome[1] = 1024.0f * ((DotProduct(normal, r_chromeup[bone]) + 1.0f) * 32.0f);
#else
	pchrome[0] = 1024.0f * ((DotProduct(normal, r_chromeright[bone]) + 1.0f) * 2.0f * 1024.0f);
	pchrome[1] = 1024.0f * ((DotProduct(normal, r_chromeup[bone]) + 1.0f) * 2.0f * 1024.0f);
#endif
}

void R_LightStrength(int bone, float* vert, float(*light)[4])
{
	vec3_t tmp;

	if (lightage[bone] != r_smodels_total)
	{
		for (int i = 0; i < numlights; i++)
		{
			tmp[0] = locallight[i]->origin[0] - lighttransform[bone][0][3];
			tmp[1] = locallight[i]->origin[1] - lighttransform[bone][1][3];
			tmp[2] = locallight[i]->origin[2] - lighttransform[bone][2][3];

			VectorIRotate(tmp, lighttransform[bone], lightbonepos[bone][0]);
		}

		lightage[bone] = r_smodels_total;
	}

	for (int i = 0; i < numlights; i++)
	{
		VectorSubtract(vert, lightbonepos[bone][i], light[i]);
		light[i][3] = 0;
	}
}

int R_IsRemapSkin(const char* texture, int* low, int* mid, int* high)
{
	int len;
	char sz[32];

	if (Q_strncasecmp((char*)texture, "Remap", 5))
		return 0;

	len = Q_strlen((char*)texture);

	if (len != 18 && len != 22)
		return FALSE;

	if (len == 18)
	{
		if (texture[5] != 'c' && texture[5] != 'C')
			return FALSE;

		Q_memset(sz, 0, sizeof(sz));
		Q_strncpy(sz, (char*)&texture[7], 3);
		*low = Q_atoi(sz);
		Q_strncpy(sz, (char*)&texture[11], 3);
		*mid = Q_atoi(sz);
	}
	else
	{
		Q_memset(sz, 0, sizeof(sz));
		Q_strncpy(sz, (char*)&texture[7], 3);
		*low = Q_atoi(sz);
		Q_strncpy(sz, (char*)&texture[11], 3);
		*mid = Q_atoi(sz);
		if (len == 22)
		{
			Q_strncpy(sz, (char*)&texture[15], 3);
			*high = Q_atoi(sz);
			return TRUE;
		}
	}
	*high = 0;
	return TRUE;
}

skin_t* R_StudioGetSkin(int keynum, int index)
{
	if (index >= 11)
		index = 0;

	if (pDM_RemapSkin[keynum][index] != NULL && pDM_RemapSkin[keynum][index]->keynum == keynum)
		return pDM_RemapSkin[keynum][index];

	r_remapindex = (r_remapindex + 1) % 64;
	pDM_RemapSkin[keynum][index] = &DM_RemapSkin[r_remapindex][index];
	pDM_RemapSkin[keynum][index]->keynum = keynum;
	pDM_RemapSkin[keynum][index]->topcolor = pDM_RemapSkin[keynum][index]->bottomcolor = -1;

	return pDM_RemapSkin[keynum][index];
}

#define SUIT_HUE_START 192
#define SUIT_HUE_END 223
#define PLATE_HUE_START 160
#define PLATE_HUE_END 191

// дл€ софта тип палитры PackedColorVec*, дл€ огл color24*
static void PaletteHueReplace(
#if !defined(GLQUAKE)
	PackedColorVec* palSrc,
#else
	color24* palSrc,
#endif
	int newHue, int Start, int end)
{
	int i;
	float r, b, g;
	float maxcol, mincol;
	float hue, val, sat;

	hue = (float)(newHue * (360.0 / 255));

	for (i = Start; i <= end; i++)
	{
		b = palSrc[i].b;
		g = palSrc[i].g;
		r = palSrc[i].r;

		maxcol = max(max(r, g), b) / 255.0f;
		mincol = min(min(r, g), b) / 255.0f;

		val = maxcol;
		sat = (maxcol - mincol) / maxcol;

		mincol = val * (1.0f - sat);

		if (hue <= 120)
		{
			b = mincol;
			if (hue < 60)
			{
				r = val;
				g = mincol + hue * (val - mincol) / (120 - hue);
			}
			else
			{
				g = val;
				r = mincol + (120 - hue) * (val - mincol) / hue;
			}
		}
		else if (hue <= 240)
		{
			r = mincol;
			if (hue < 180)
			{
				g = val;
				b = mincol + (hue - 120) * (val - mincol) / (240 - hue);
			}
			else
			{
				b = val;
				g = mincol + (240 - hue) * (val - mincol) / (hue - 120);
			}
		}
		else
		{
			g = mincol;
			if (hue < 300)
			{
				b = val;
				r = mincol + (hue - 240) * (val - mincol) / (360 - hue);
			}
			else
			{
				r = val;
				b = mincol + (360 - hue) * (val - mincol) / (hue - 240);
			}
		}

		palSrc[i].b = (unsigned char)(b * 255);
		palSrc[i].g = (unsigned char)(g * 255);
		palSrc[i].r = (unsigned char)(r * 255);
	}
}

#if defined(GLQUAKE)
extern model_t mod_known[MAX_KNOWN_MODELS];

byte* R_StudioReloadSkin(model_t* pModel, int index, skin_t* pskin)
{
	void* pData;

	int idx = pModel - mod_known;

	if (idx < 0 || idx >= MAX_KNOWN_MODELS)
		return NULL;

	struct skin_extents_t
	{
		int width;
		int height;
	};

	if (Cache_Check(&model_texture_cache[idx][index]))
	{
		pData = model_texture_cache[idx][index].data;
	}
	else
	{
		byte* pbase = COM_LoadFileForMe(pModel->name, NULL);

		mstudiotexture_t* ptexture = (mstudiotexture_t*)(pbase + ((studiohdr_t*)pbase)->textureindex);
		ptexture += index;

		int size = ptexture->width * ptexture->height + 768;

		// 768 - palette size, 8 - 4x2 (width + height)
		Cache_Alloc(&model_texture_cache[idx][index], size + sizeof(skin_extents_t), pskin->name);

		pData = model_texture_cache[idx][index].data;

		((skin_extents_t*)pData)->width = ptexture->width;
		((skin_extents_t*)pData)->height = ptexture->height;

		// copy texture data with palette
		Q_memcpy(&((skin_extents_t*)pData)[1], pbase + ptexture->index, size);

		Mem_Free(pbase);
	}

	pskin->index = index;
	pskin->width = ((skin_extents_t*)pData)->width;
	pskin->height = ((skin_extents_t*)pData)->height;

	return (byte*)&((skin_extents_t*)pData)[1];
}
#endif

void R_StudioSetupSkin(studiohdr_t* ptexturehdr, int index)
{
	skin_t* skin;
	mstudiotexture_t* ptexture;

#if !defined(GLQUAKE)
	static word white_pal[4] = { 192, 192, 192, 0 };
	static int white_texture = 0;
#endif

	if (g_ForcedFaceFlags & STUDIO_NF_CHROME)
		return;

	ptexture = (mstudiotexture_t*)((byte*)ptexturehdr + ptexturehdr->textureindex);

#if !defined(GLQUAKE)
	r_affinetridesc.pskindesc = 0;

	a_skinwidth = ptexture[index].width;

	r_affinetridesc.skinwidth = a_skinwidth;
	r_affinetridesc.pskin = (byte*)ptexturehdr + ptexture[index].index;
	r_affinetridesc.seamfixupX16 = (a_skinwidth >> 1) << 16;
	r_affinetridesc.skinheight = ptexture[index].height;

	r_palette = (word*)((byte*)r_affinetridesc.pskin + r_affinetridesc.skinwidth * r_affinetridesc.skinheight);

	if (drawstyle == 1)
	{
		r_affinetridesc.pskin = &white_texture;
		r_palette = white_pal;
		return;
	}
#endif

	if (currententity->index <= 0)
	{
#if defined(GLQUAKE)
		if (ptexture[index].index != 0)
			GL_Bind(ptexture[index].index);
#endif
		return;
	}

	int l = PLATE_HUE_START, m = PLATE_HUE_END, h = SUIT_HUE_END;

	if (Q_stricmp(ptexture[index].name, "DM_Base.bmp") != 0 && !R_IsRemapSkin(ptexture[index].name, &l, &m, &h))
	{
#if defined(GLQUAKE)
		if (ptexture[index].index != 0)
			GL_Bind(ptexture[index].index);
#endif
		return;
	}

	skin = R_StudioGetSkin(currententity->index, index);

	if (skin->model != r_model || skin->topcolor != r_topcolor || skin->bottomcolor != r_bottomcolor)
	{
#if !defined(GLQUAKE)
		skin->topcolor = r_topcolor;
		skin->bottomcolor = r_bottomcolor;
		skin->model = r_model;

		Q_memcpy(skin->palette, r_palette, sizeof(skin->palette));

		// Remap palette
		PaletteHueReplace((PackedColorVec*)skin->palette, skin->topcolor, l, m);

		if (h != 0)
			PaletteHueReplace((PackedColorVec*)skin->palette, skin->bottomcolor, m + 1, h);
#else
		byte* pData = R_StudioReloadSkin(r_model, index, skin);

		if (pData != NULL)
		{
			byte tmp_palette[768];
			char name[256];

			_snprintf(name, sizeof(name), "%s%d", ptexture[index].name, currententity->index);
			Q_memcpy(tmp_palette, &pData[ptexture[index].height * ptexture[index].width], sizeof(tmp_palette));

			skin->topcolor = r_topcolor;
			skin->bottomcolor = r_bottomcolor;
			skin->model = r_model;

			// Remap palette
			PaletteHueReplace((color24*)tmp_palette, skin->topcolor, l, m);

			if (h != 0)
				PaletteHueReplace((color24*)tmp_palette, skin->bottomcolor, m + 1, h);

			GL_UnloadTexture(name);

			float fOldDither = gl_dither.value;
			gl_dither.value = 0.0;
			skin->gl_index = GL_LoadTexture(name, GLT_STUDIO, skin->width, skin->height, pData, false, TEX_TYPE_NONE, tmp_palette);
			gl_dither.value = fOldDither;

			if (skin->gl_index != 0)
				GL_Bind(skin->gl_index);
		}
	}
	else
	{
		if (skin->gl_index != 0)
			GL_Bind(skin->gl_index);
#endif
	}

#if !defined(GLQUAKE)
	r_palette = (unsigned short*)skin->palette;
#endif
}

// alter variation for ogl
#if defined (GLQUAKE)
void R_LightLambert(float(*light)[4], float* normal, float* src, float* lambert)
#else
void R_LightLambert(float(*light)[4], float* normal, int* lambert)
#endif
{
	float dist, r2, l;

#if defined(GLQUAKE)
	float adjr, adjg, adjb;
#else
	float adj;
#endif

	if (!numlights)
	{
#if defined (GLQUAKE)
		// 3 аргумент - float*src, тип 4-го - float*
		VectorCopy(src, lambert);
#endif
		return;
	}

#if defined(GLQUAKE)
	adjr = adjg = adjb = 0;
#else
	adj = 0;
#endif

	for (int i = 0; i < numlights; i++)
	{
		dist = -DotProduct(light[i], normal);

		if (dist > 0)
		{
			if (light[i][3] == 0)
			{
				r2 = DotProduct(light[i], light[i]);

				if (r2 <= 0)
					light[i][3] = 1;
				else
				{
					l = locallightR2[i] / (sqrt(r2) * r2);
#if !defined(GLQUAKE)
					if (l > 2.0f)
						light[i][3] = 2.0f;
					else
						light[i][3] = l;
#endif
				}
			}

#if defined(GLQUAKE)
			adjr += dist * light[i][3] * (float)locallinearlight[i][0];
			adjg += dist * light[i][3] * (float)locallinearlight[i][1];
			adjb += dist * light[i][3] * (float)locallinearlight[i][2];
#else
			adj += dist * light[i][3] * 1024.0f;
#endif
		}
	}

#if defined(GLQUAKE)
	if (adjr != 0.0f || adjg != 0.0f || adjb != 0.0f)
	{
		lambert[0] = adjr + (float)lineargammatable[(int)(src[0] * 1023.0f)];

		if (lambert[0] > 1023)
			lambert[0] = 1.0f;
		else
			lambert[0] = (float)screengammatable[(int)lambert[0]] / 1023.0f;

		lambert[1] = adjg + (float)lineargammatable[(int)(src[1] * 1023.0f)];

		if (lambert[1] > 1023)
			lambert[1] = 1.0f;
		else
			lambert[1] = (float)screengammatable[(int)lambert[1]] / 1023.0f;

		lambert[2] = adjb + lineargammatable[(int)(src[2] * 1023.0f)];

		if (lambert[2] > 1023)
			lambert[2] = 1.0f;
		else
			lambert[2] = (float)screengammatable[(int)lambert[2]] / 1023.0f;
	}
	else
	{
		VectorCopy(src, lambert);
	}
#else
	if (adj)
	{
		*lambert = (int)adj + lineargammatable[*lambert / 64];

		if (*lambert > 1023)
			*lambert = 255 * 256 + 128;
		else
			*lambert = screengammatable[*lambert] * 64;
	}
#endif
}

#if defined(GLQUAKE)
void R_GLStudioDrawPoints(void)
{
	byte *pvertbone, *pnormbone;
	studiohdr_t* ptexturehdr;
	mstudiotexture_t* ptexture;
	vec3_t* pstudioverts;
	mstudiomesh_t* pmesh;
	auxvert_t* av;
	qboolean bIsGlowShell;
	vec3_t* lv;
	int flags;
	short* pskinref;
	vec3_t* pstudionorms;
	float lv_tmp;
	int iFlippedVModel;
	short* ptricmds;
	int i;
	float s, t;
	short tri;
	vec3_t vNormal;
	float fl[3];
	int j;

	pvertbone = ((byte*)pstudiohdr + psubmodel->vertinfoindex);
	pnormbone = ((byte*)pstudiohdr + psubmodel->norminfoindex);

	ptexturehdr = R_LoadTextures(r_model);
	ptexture = (mstudiotexture_t*)((byte*)ptexturehdr + ptexturehdr->textureindex);

	pstudioverts = (vec3_t*)((byte*)pstudiohdr + psubmodel->vertindex);
	pmesh = (mstudiomesh_t*)((byte*)pstudiohdr + psubmodel->meshindex);

	pskinref = (short*)((byte*)ptexturehdr + ptexturehdr->skinindex);

	if (currententity->curstate.skin != 0 && currententity->curstate.skin < ptexturehdr->numskinfamilies)
		pskinref += ptexturehdr->numskinref * currententity->curstate.skin;

	av = pauxverts;

	bIsGlowShell = currententity->curstate.renderfx == kRenderFxGlowShell;

	if (bIsGlowShell)
	{
		BuildNormalIndexTable();
		BuildGlowShellVerts(pstudioverts, av);
	}
	else
	{
		for (i = 0; i < psubmodel->numverts; i++)
		{
			R_StudioTransformAuxVert(av + i, pvertbone[i], pstudioverts[i]);
		}
	}

	for (i = 0; i < psubmodel->numverts; i++)
		R_LightStrength(pvertbone[i], pstudioverts[i], lightpos[i]);

	lv = pvlightvalues;
	pstudionorms = (vec3_t*)((byte*)pstudiohdr + psubmodel->normindex);

	for (i = 0; i < psubmodel->nummesh; i++)
	{
		flags = g_ForcedFaceFlags | ptexture[pskinref[pmesh[i].skinref]].flags;

		if (r_fullbright.value >= 2)
			flags &= ~(STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME);

		if (currententity->curstate.rendermode == kRenderTransAdd)
		{
			for (j = 0; j < pmesh[i].numnorms && (int)(lv - pvlightvalues) < pvlightvalues_size; j++, lv++/*, pnormbone++, pstudionorms++*/)
			{
				(*lv)[0] = (*lv)[1] = (*lv)[2] = r_blend;
				if (flags & STUDIO_NF_CHROME)
					R_StudioChrome(chrome[lv - pvlightvalues], *pnormbone, *pstudionorms);
			}
		}
		else
		{
			for (j = 0; j < pmesh[i].numnorms && (int)(lv - pvlightvalues) < pvlightvalues_size; j++, lv++, pnormbone++, pstudionorms++)
			{
				R_StudioLighting(&lv_tmp, *pnormbone, flags, *pstudionorms);
				if (flags & STUDIO_NF_CHROME)
					R_StudioChrome(chrome[lv - pvlightvalues], *pnormbone, *pstudionorms);

				(*lv)[0] = r_colormix[0] * lv_tmp;
				(*lv)[1] = r_colormix[1] * lv_tmp;
				(*lv)[2] = r_colormix[2] * lv_tmp;
			}
		}
	}

	qglCullFace(GL_FRONT);

	iFlippedVModel = 0;

	if (IsFlippedViewModel())
	{
		qglDisable(GL_CULL_FACE);
		iFlippedVModel = 1;
	}

	pstudionorms = (vec3_t*)((byte*)pstudiohdr + psubmodel->normindex);

	for (i = 0; i < psubmodel->nummesh; i++)
	{
		c_alias_polys += pmesh[i].numtris;

		ptricmds = (short*)((byte*)pstudiohdr + pmesh[i].triindex);

		flags = g_ForcedFaceFlags | ptexture[pskinref[pmesh[i].skinref]].flags;

		if (r_fullbright.value >= 2)
			flags &= ~(STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME);

		if (flags & STUDIO_NF_MASKED)
		{
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GREATER, 0.5f);
			qglDepthMask(1);
		}

		if ((flags & STUDIO_NF_ADDITIVE) && currententity->curstate.rendermode != kRenderNormal)
		{
			qglBlendFunc(1, 1);
			qglEnable(GL_BLEND);
			qglDepthMask(0);
			qglShadeModel(GL_SMOOTH);
		}

		if (r_fullbright.value >= 2.0)
		{
			::tri.SpriteTexture(cl_sprite_white, 0);
			// white sprite has 256x256 size
			s = t = 1.0f / (float)256;
		}
		else
		{
			s = 1.0f / (float)ptexture[pskinref[pmesh[i].skinref]].width;
			t = 1.0f / (float)ptexture[pskinref[pmesh[i].skinref]].height;

			R_StudioSetupSkin(ptexturehdr, pskinref[pmesh[i].skinref]);
		}

		if (flags & STUDIO_NF_CHROME)
		{
			s *= 1.0f / (float)1024;
			t *= 1.0f / (float)1024;

			if (g_ForcedFaceFlags & STUDIO_NF_CHROME)
			{
				while ((tri = *(ptricmds++)) != 0)
				{
					if (tri < 0)
					{
						tri = -tri;
						qglBegin(GL_TRIANGLE_FAN);
					}
					else
					{
						qglBegin(GL_TRIANGLE_STRIP);
					}

					for (j = 0; j < tri; j++, ptricmds = (short*)(((mstudiotrivert_t*)ptricmds) + 1))
					{
						qglTexCoord2f((float)chrome[g_NormalIndex[((mstudiotrivert_t*)ptricmds)->vertindex]][0] * s, (float)chrome[g_NormalIndex[((mstudiotrivert_t*)ptricmds)->vertindex]][1] * t);
						qglVertex3f(pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[0], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[1], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[2]);
					}

					qglEnd();
				}
			}
			else
			{
				while ((tri = *(ptricmds++)) != 0)
				{
					if (tri < 0)
					{
						tri = -tri;
						qglBegin(GL_TRIANGLE_FAN);
					}
					else
					{
						qglBegin(GL_TRIANGLE_STRIP);
					}

					for (j = 0; j < tri && pvlightvalues_size > ((mstudiotrivert_t*)ptricmds)->normindex; j++, ptricmds = (short*)(((mstudiotrivert_t*)ptricmds) + 1))
					{
						qglTexCoord2f((float)chrome[((mstudiotrivert_t*)ptricmds)->normindex][0] * s, (float)chrome[((mstudiotrivert_t*)ptricmds)->normindex][1] * t);

						R_LightLambert(lightpos[((mstudiotrivert_t*)ptricmds)->vertindex], pstudionorms[((mstudiotrivert_t*)ptricmds)->normindex], pvlightvalues[((mstudiotrivert_t*)ptricmds)->normindex], fl);

						qglColor4f(fl[0], fl[1], fl[2], r_blend);
						qglVertex3f(pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[0], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[1], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[2]);

					}

					qglEnd();
				}
			}
		}
		else
		{
			while ((tri = *(ptricmds++)) != 0)
			{
				if (tri < 0)
				{
					tri = -tri;
					qglBegin(GL_TRIANGLE_FAN);
				}
				else
				{
					qglBegin(GL_TRIANGLE_STRIP);
				}

				for (j = 0; j < tri && pvlightvalues_size > ((mstudiotrivert_t*)ptricmds)->normindex; j++, ptricmds = (short*)(((mstudiotrivert_t*)ptricmds) + 1))
				{
					qglTexCoord2f((float)((mstudiotrivert_t*)ptricmds)->s * s, (float)((mstudiotrivert_t*)ptricmds)->t * t);

					lv = &pvlightvalues[((mstudiotrivert_t*)ptricmds)->normindex];

					if (iFlippedVModel == 1)
					{
						vNormal[0] = pstudionorms[((mstudiotrivert_t*)ptricmds)->normindex][0];
						vNormal[1] = -pstudionorms[((mstudiotrivert_t*)ptricmds)->normindex][1];
						vNormal[2] = pstudionorms[((mstudiotrivert_t*)ptricmds)->normindex][2];

						R_LightLambert(lightpos[((mstudiotrivert_t*)ptricmds)->vertindex], vNormal, *lv, fl);

						qglColor4f(fl[0], fl[1], fl[2], r_blend);
						qglVertex3f(pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[0], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[1], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[2]);
					}
					else
					{
						R_LightLambert(lightpos[((mstudiotrivert_t*)ptricmds)->vertindex], pstudionorms[((mstudiotrivert_t*)ptricmds)->normindex], *lv, fl);

						qglColor4f(fl[0], fl[1], fl[2], r_blend);
						qglVertex3f(pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[0], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[1], pauxverts[((mstudiotrivert_t*)ptricmds)->vertindex].fv[2]);
					}
				}

				qglEnd();
			}
		}

		if (flags & STUDIO_NF_MASKED)
		{
			qglAlphaFunc(GL_NOTEQUAL, 0.0f);
			qglDisable(GL_ALPHA_TEST);
		}

		if ((flags & STUDIO_NF_ADDITIVE) && currententity->curstate.rendermode != kRenderNormal)
		{
			qglDisable(GL_BLEND);
			qglDepthMask(1);
			qglShadeModel(GL_FLAT);
		}
	}

	qglEnable(GL_CULL_FACE);
}

void R_StudioDrawPoints(void)
{
	R_GLStudioDrawPoints();
}
#else
// отрисовка текстурированных треугольников в параметрической системе координат st
void R_StudioDrawPoints(void)
{
	byte *pvbone, *pnbone;
	studiohdr_t* phdr;
	int vertidx;
	short skin, *pstudioskin, *ptri, tri;
	mstudiotexture_t* pstudiotex;
	mstudiomesh_t* pstudiomesh;
	vec3_t *pstudiovert, *pstudionorm;
	finalvert_t *fv, *fv2 = NULL, *fv3 = NULL;
	auxvert_t* pav;
	float* plight;
	int i, j, meshflags, typed, ctr;
	mtriangle_t mtri;
	mstudiotrivert_t* pstudiotri;
	qboolean bIsGlowShell;

	pvbone = ((byte*)pstudiohdr + psubmodel->vertinfoindex);
	pnbone = ((byte*)pstudiohdr + psubmodel->norminfoindex);

	phdr = R_LoadTextures(r_model);

	vertidx = psubmodel->vertindex;
	skin = currententity->curstate.skin;

	pstudiotex = (mstudiotexture_t*)((byte*)phdr + phdr->textureindex);
	pstudiomesh = (mstudiomesh_t*)((byte*)pstudiohdr + psubmodel->meshindex);

	pstudiovert = (vec3_t*)((byte*)pstudiohdr + vertidx);
	pstudionorm = (vec3_t*)((byte*)pstudiohdr + psubmodel->normindex);

	pstudioskin = (short*)((byte*)phdr + phdr->skinindex);

	if (skin != 0 && skin < phdr->numskinfamilies)
		pstudioskin = (short*)((byte*)phdr + phdr->skinindex) + phdr->numskinref * skin;

	fv = pfinalverts;
	r_anumverts = psubmodel->numverts;
	pav = pauxverts;
	plight = plightvalues;

	bIsGlowShell = currententity->curstate.renderfx == kRenderFxGlowShell;

	if (bIsGlowShell)
	{
		BuildNormalIndexTable();
		BuildGlowShellVerts(pstudiovert, pav);
	}
	else
	{
		for (i = 0; i < r_anumverts; i++, pstudiovert++, fv++, pav++, pvbone++)
		{
			R_StudioTransformAuxVert(pav, *pvbone, *pstudiovert);

			fv->flags = 0;

			if (pav->fv[2] < ALIAS_Z_CLIP_PLANE)
				fv->flags = ALIAS_Z_CLIP;
			else
			{
				R_StudioProjectFinalVert(fv, pav);

				if (fv->v[0] < r_refdef.aliasvrect.x)
					fv->flags |= ALIAS_LEFT_CLIP;
				if (fv->v[1] < r_refdef.aliasvrect.y)
					fv->flags |= ALIAS_TOP_CLIP;
				if (fv->v[0] > r_refdef.aliasvrectright)
					fv->flags |= ALIAS_RIGHT_CLIP;
				if (fv->v[1] > r_refdef.aliasvrectbottom)
					fv->flags |= ALIAS_BOTTOM_CLIP;
			}
		}
	}

	drawstyle = r_fullbright.value > 1;

	for (i = 0; i < psubmodel->nummesh; i++)
	{
		meshflags = g_ForcedFaceFlags | pstudiotex[pstudioskin[pstudiomesh[i].skinref]].flags;

		for (j = 0; j < pstudiomesh[i].numnorms; j++, pnbone++, pstudionorm++, plight++)
		{
			R_StudioLighting(plight, *pnbone, meshflags, *pstudionorm);
			if (meshflags & STUDIO_NF_CHROME)
				R_StudioChrome(chrome[plight - plightvalues], *pnbone, *pstudionorm);
		}
	}

	pstudionorm = (vec3_t*)((byte*)pstudiohdr + psubmodel->normindex);
	pstudiovert = (vec3_t*)((byte*)pstudiohdr + vertidx);
	pvbone = ((byte*)pstudiohdr + psubmodel->vertinfoindex);

	for (i = 0; i < r_anumverts; i++, pvbone++, pstudiovert++)
		R_LightStrength(*pvbone, *pstudiovert, lightpos[i]);

	r_affinetridesc.numtriangles = 1;

	for (i = 0; i < psubmodel->nummesh; i++)
	{
		if (r_fullbright.value < 0 || r_fullbright.value > 1)
			drawstyle = 1;
		else
			drawstyle = pstudiotex[pstudioskin[pstudiomesh[i].skinref]].flags & STUDIO_NF_CHROME;

		if (g_ForcedFaceFlags & STUDIO_NF_CHROME)
			drawstyle = 2;

		R_StudioSetupSkin(phdr, pstudioskin[pstudiomesh[i].skinref]);

		ptri = (short*)((byte*)pstudiohdr + pstudiomesh[i].triindex);
		r_affinetridesc.drawtype = 0;

		// формат размещени€:
		// 00 short - vert count
		// 02 count * mstudiotrivert_t (8 bytes)
		// парс до тех пор пока число вершин не будет равно нулю

		while ((tri = *(ptri++)) != NULL)
		{
			mtri.facesfront = 1;

			if (tri < 0)
			{
				tri = -tri;
				typed = 0;
			}
			else
				typed = 1;

			pstudiotri = (mstudiotrivert_t*)(ptri);
			for (ctr = 0; ctr < tri; ctr++, pstudiotri++, ptri = (short*)pstudiotri)
			{
				mtri.vertindex[2] = pstudiotri->vertindex;
				fv = pfinalverts + mtri.vertindex[2];

				if (drawstyle == 0)
				{
					fv->v[2] = pstudiotri->s << 16;
					fv->v[3] = pstudiotri->t << 16;

					fv->v[4] = plightvalues[pstudiotri->normindex];
					R_LightLambert(lightpos[pstudiotri->vertindex], pstudionorm[pstudiotri->normindex], &fv->v[4]);
				}
				else if (drawstyle == 1)
				{
					fv->v[2] = fv->v[3] = 0;

					fv->v[4] = plightvalues[pstudiotri->normindex];
					R_LightLambert(lightpos[pstudiotri->vertindex], pstudionorm[pstudiotri->normindex], &fv->v[4]);
				}
				else if (drawstyle == 2)
				{
					fv->v[2] = chrome[pstudiotri->normindex][0];
					fv->v[3] = chrome[pstudiotri->normindex][1];

					fv->v[4] = plightvalues[pstudiotri->normindex];
					R_LightLambert(lightpos[pstudiotri->vertindex], pstudionorm[pstudiotri->normindex], &fv->v[4]);
				}

				if (ctr >= 2)
				{
					if (!(fv3->flags & fv2->flags & fv->flags & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP)))
					{
						if ((fv3->flags | fv2->flags | fv->flags) & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))
						{
							R_AliasClipTriangle(&mtri);
						}
						else
						{
							r_affinetridesc.pfinalverts = pfinalverts;
							r_affinetridesc.ptriangles = &mtri;
							D_PolysetDraw();
						}
					}
				}

				if (typed == 1)
				{
					if (!(ctr & 1))
					{
						fv2 = fv;
						mtri.vertindex[0] = mtri.vertindex[2];
					}
					else
					{
						mtri.vertindex[1] = mtri.vertindex[2];
						fv3 = fv;
					}
				}
				else
				{
					if (!ctr)
					{
						fv2 = fv;
						mtri.vertindex[0] = mtri.vertindex[2];
					}
					mtri.vertindex[1] = mtri.vertindex[2];
					fv3 = fv;
				}

			}
		}
	}
}
#endif

void R_StudioAbsBB(void)
{
	mstudioseqdesc_t* desc;
	vec_t p[8][3];
	vec3_t tmp;
	float lv;

	desc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;
	tri.RenderMode(kRenderTransAdd);
	tri.SpriteTexture(cl_sprite_white, 0);

	for (int i = 0; i < 8; i++)
	{
		if (i & 1)
			p[i][0] = desc->bbmin[0];
		else
			p[i][0] = desc->bbmax[0];

		if (i & 2)
			p[i][1] = desc->bbmin[1];
		else
			p[i][1] = desc->bbmax[1];

		if (i & 4)
			p[i][2] = desc->bbmin[2];
		else
			p[i][2] = desc->bbmax[2];

		VectorAdd(p[i], currententity->origin, p[i]);
	}

	tri.Begin(TRI_QUADS);
	tri.Color4f(0.5f, 0.5f, 1.0f, 1.0f);

	for (int j = 0; j < 6; j++)
	{
		tmp[0] = tmp[1] = tmp[2] = 0;
		tmp[j % 3] = (j >= 3) ? -1.0f : 1.0f;
		R_StudioLighting(&lv, -1, 0, tmp);
#if !defined(GLQUAKE)
		// 1 / (1023 * 64)
		lv /= 0x10000;
#endif
		tri.Brightness(lv);
		tri.Vertex3fv(p[boxpnt[j][0]]);
		tri.Vertex3fv(p[boxpnt[j][1]]);
		tri.Vertex3fv(p[boxpnt[j][2]]);
		tri.Vertex3fv(p[boxpnt[j][3]]);
	}

	tri.End();
	tri.RenderMode(kRenderNormal);
}

void R_StudioRenderFinal(void)
{
#if defined(GLQUAKE)
	int i;
	int rendermode;

	// studioapi_SetupRenderer w/o g_rendermode
	pauxverts = auxverts;

	pvlightvalues = lightvalues;
	pvlightvalues_size = 2048;

	shadevector[0] = 1.0f;
	shadevector[1] = -0.0f;
	shadevector[2] = 1.0f;

	VectorNormalize(shadevector);

	GL_DisableMultitexture();

	qglPushMatrix();

	qglShadeModel(GL_SMOOTH);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value != 0.0f)
		qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	rendermode = g_ForcedFaceFlags ? kRenderTransAdd : currententity->curstate.rendermode;

	if (r_drawentities.value == 2)
	{
		R_StudioDrawBones();
	}
	else if (r_drawentities.value == 3)
	{
		R_StudioDrawHulls();
	}
	else
	{
		for (i = 0; i < pstudiohdr->numbodyparts; i++)
		{
			pbodypart = (mstudiobodyparts_t*)((byte*)pstudiohdr + pstudiohdr->bodypartindex) + i;
			psubmodel = (mstudiomodel_t*)((byte*)pstudiohdr + pbodypart->modelindex) + (currententity->curstate.body / pbodypart->base % pbodypart->nummodels);

			if (r_dointerp)
			{
				// interpolation messes up bounding boxes.
				currententity->trivial_accept = 0;
			}

			studioapi_GL_SetRenderMode(rendermode);
			R_StudioDrawPoints();
			studioapi_GL_StudioDrawShadow();
		}
	}

	if (r_drawentities.value == 4)
	{
		tri.RenderMode(kRenderTransAdd);
		R_StudioDrawHulls();
		tri.RenderMode(kRenderNormal);
	}

	if (rendermode != kRenderNormal)
		qglDisable(GL_BLEND);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	qglShadeModel(GL_FLAT);

	if (gl_affinemodels.value != 0.0)
		qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	qglPopMatrix();

#else
	int i;
	int seq;

	R_StudioVertBuffer();

	seq = currententity->curstate.sequence;
	if (seq < 0 || seq >= pstudiohdr->numseq)
	{
		Con_DPrintf(const_cast<char*>("R_StudioRenderFinal:  sequence %i/%i out of range for model %s\n"), seq, pstudiohdr->numseq, pstudiohdr->name);
		currententity->curstate.sequence = 0;
	}

	zishift = 0.0f;

	ziscale = (float)(INFINITE_DISTANCE * ZISCALE);
	// если рисуема€ модель - вид (wvp -> v)
	if (&cl.viewent == currententity)
		ziscale = (float)(INFINITE_DISTANCE * ZISCALE) * 3;

	r_affinetridesc.drawtype = 0;

	if (r_drawentities.value == 2)
	{
		R_StudioDrawBones();
	}
	else if (r_drawentities.value == 3)
	{
		R_StudioDrawHulls();
	}
	else
	{
		for (i = 0; i < pstudiohdr->numbodyparts; i++)
		{
			R_StudioSetupModel(i);
			R_StudioDrawPoints();
		}
	}

	if (r_drawentities.value == 4)
	{
		tri.RenderMode(kRenderTransAdd);
		R_StudioDrawHulls();
		tri.RenderMode(kRenderNormal);
	}

	if (r_drawentities.value == 5)
	{
		R_StudioAbsBB();
	}

	//studioapi_RestoreRenderer();
#endif
}

void R_StudioRenderModel(void)
{
	VectorCopy(r_origin, g_ChromeOrigin);
	g_ForcedFaceFlags = 0;

	if (currententity->curstate.renderfx == kRenderFxGlowShell)
	{
		currententity->curstate.renderfx = kRenderFxNone;
		R_StudioRenderFinal();

#if !defined(GLQUAKE)
		tri.RenderMode(kRenderTransAdd);
#endif

		g_ForcedFaceFlags = STUDIO_NF_CHROME;

		tri.SpriteTexture(cl_sprite_shell, 0);
		currententity->curstate.renderfx = kRenderFxGlowShell;

		R_StudioRenderFinal();
#if !defined(GLQUAKE)
		tri.RenderMode(kRenderNormal);
#endif
	}
	else
	{
		R_StudioRenderFinal();
	}
}

int R_StudioDrawModel(int flags)
{
	alight_t lighting;
	vec3_t dir;

	if (currententity->curstate.renderfx == kRenderFxDeadPlayer)
	{
		entity_state_t deadplayer;

		int result;
		int save_interp;

		if (currententity->curstate.renderamt <= 0 || currententity->curstate.renderamt > cl.maxclients)
			return 0;

		// get copy of player
		deadplayer = cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->curstate.renderamt - 1];

		// clear weapon, movement state
		deadplayer.number = currententity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy(currententity->curstate.angles, deadplayer.angles);
		VectorCopy(currententity->curstate.origin, deadplayer.origin);

		save_interp = r_dointerp;
		r_dointerp = 0;

		// draw as though it were a player
		result = R_StudioDrawPlayer(flags, &deadplayer);

		r_dointerp = save_interp;
		return result;
	}

	r_model = currententity->model;
	pstudiohdr = (studiohdr_t*)Mod_Extradata(r_model);

	R_StudioSetUpTransform(0);

	if (flags & STUDIO_RENDER)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (!R_StudioCheckBBox ())
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if (pstudiohdr->numbodyparts == 0)
			return 1;
	}

	if (currententity->curstate.movetype == MOVETYPE_FOLLOW)
	{
		R_StudioMergeBones(r_model);
	}
	else
	{
		R_StudioSetupBones();
	}
	R_StudioSaveBones();

	if (flags & STUDIO_EVENTS)
	{
		R_StudioCalcAttachments();
		R_StudioClientEvents();
		// copy attachments into global entity array
		if (currententity->index > 0)
		{
			Q_memcpy(cl_entities[currententity->index].attachment, currententity->attachment, sizeof(vec3_t) * 4);
		}
	}

	if (flags & STUDIO_RENDER)
	{
		lighting.plightvec = dir;
		R_StudioDynamicLight(currententity, &lighting);

		R_StudioEntityLight(&lighting);

		// model and frame independant
		R_StudioSetupLighting(&lighting);

		// get remap colors
		r_topcolor = currententity->curstate.colormap & 0xFF;
		r_bottomcolor = (currententity->curstate.colormap & 0xFF00) >> 8;

		R_StudioRenderModel();
	}

	return 1;
}

void R_StudioChangePlayerModel(void)
{
	for (int i = 0; i < 11; i++)
	{
		skin_t* skin = R_StudioGetSkin(currententity->index, i);
		skin->topcolor = skin->bottomcolor = -1;
	}
}

void R_StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch)
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);
	if (*pBlend < pseqdesc->blendstart[0])
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if (*pBlend > pseqdesc->blendend[0])
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if (pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1) // catch qc error
			*pBlend = 127;
		else
			*pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

void R_StudioEstimateGait(entity_state_t* pplayer)
{
	float dt;
	vec3_t est_velocity;

	dt = (cl.time - cl.oldtime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	if (dt == 0 || r_playerinfo->renderframe == r_framecount)
	{
		r_gaitmovement = 0;
		return;
	}

	// VectorAdd( pplayer->velocity, pplayer->prediction_error, est_velocity );
	if (cl_gaitestimation.value)
	{
		VectorSubtract(currententity->origin, r_playerinfo->prevgaitorigin, est_velocity);
		VectorCopy(currententity->origin, r_playerinfo->prevgaitorigin);
		r_gaitmovement = Length(est_velocity);
		if (dt <= 0 || r_gaitmovement / dt < 5)
		{
			r_gaitmovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy(pplayer->velocity, est_velocity);
		r_gaitmovement = Length(est_velocity) * dt;
	}

	if (est_velocity[1] == 0 && est_velocity[0] == 0)
	{
		float flYawDiff = currententity->angles[YAW] - r_playerinfo->gaityaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		r_playerinfo->gaityaw += flYawDiff;
		r_playerinfo->gaityaw = r_playerinfo->gaityaw - (int)(r_playerinfo->gaityaw / 360) * 360;

		r_gaitmovement = 0;
	}
	else
	{
		r_playerinfo->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (r_playerinfo->gaityaw > 180)
			r_playerinfo->gaityaw = 180;
		if (r_playerinfo->gaityaw < -180)
			r_playerinfo->gaityaw = -180;
	}
}

void R_StudioProcessGait(entity_state_t* pplayer)
{
	mstudioseqdesc_t* pseqdesc;
	float dt;
	int iBlend;
	float flYaw;	 // view direction relative to movement

	if (currententity->curstate.sequence >= pstudiohdr->numseq)
	{
		currententity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;

	R_StudioPlayerBlend(pseqdesc, &iBlend, &currententity->angles[PITCH]);

	currententity->latched.prevangles[PITCH] = currententity->angles[PITCH];
	currententity->curstate.blending[0] = iBlend;
	currententity->latched.prevblending[0] = currententity->curstate.blending[0];
	currententity->latched.prevseqblending[0] = currententity->curstate.blending[0];

	// Con_DPrintf("%f %d\n", currententity->angles[PITCH], currententity->blending[0] );

	dt = (cl.time - cl.oldtime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	R_StudioEstimateGait(pplayer);

	// Con_DPrintf("%f %f\n", currententity->angles[YAW], currententity->gaityaw );

	// calc side to side turning
	flYaw = currententity->angles[YAW] - r_playerinfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180)
		flYaw = flYaw + 360;
	if (flYaw > 180)
		flYaw = flYaw - 360;

	if (flYaw > 120)
	{
		r_playerinfo->gaityaw = r_playerinfo->gaityaw - 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw = flYaw - 180;
	}
	else if (flYaw < -120)
	{
		r_playerinfo->gaityaw = r_playerinfo->gaityaw + 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	currententity->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	currententity->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	currententity->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	currententity->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
	currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
	currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
	currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

	currententity->angles[YAW] = r_playerinfo->gaityaw;
	if (currententity->angles[YAW] < -0)
		currententity->angles[YAW] += 360;
	currententity->latched.prevangles[YAW] = currententity->angles[YAW];

	if (pplayer->gaitsequence >= pstudiohdr->numseq)
	{
		pplayer->gaitsequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
	{
		r_playerinfo->gaitframe += (r_gaitmovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		r_playerinfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	r_playerinfo->gaitframe = r_playerinfo->gaitframe - (int)(r_playerinfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (r_playerinfo->gaitframe < 0)
		r_playerinfo->gaitframe += pseqdesc->numframes;
}

bool WeaponHasAttachments(entity_state_t* pplayer)
{
	if (!pplayer)
		return false;

	model_t* pweaponmodel = CL_GetModelByIndex(pplayer->weaponmodel);
	studiohdr_t* modelheader = (studiohdr_t*)Mod_Extradata(pweaponmodel);

	return (modelheader->numattachments != 0);
}

int R_StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
	alight_t lighting;
	vec3_t dir;

	r_playerindex = pplayer->number - 1;

	if (r_playerindex < 0 || r_playerindex >= cl.maxclients)
		return 0;

	if (!developer.value && Host_IsSinglePlayerGame() || !cl.players[r_playerindex].model[0])
	{
		if (DM_PlayerState[r_playerindex].model != currententity->model)
		{
			DM_PlayerState[r_playerindex].model = currententity->model;
			R_StudioChangePlayerModel();
		}
	}
	else if (Q_strcmp(DM_PlayerState[r_playerindex].name, cl.players[r_playerindex].model))
	{
		Q_strncpy(DM_PlayerState[r_playerindex].name, cl.players[r_playerindex].model, sizeof(DM_PlayerState[r_playerindex].name) - 1);
		DM_PlayerState[r_playerindex].name[sizeof(DM_PlayerState[r_playerindex].name) - 1] = '\0';

		_snprintf(DM_PlayerState[r_playerindex].modelname, sizeof(DM_PlayerState[r_playerindex].modelname), "models/player/%s/%s.mdl",
			cl.players[r_playerindex].model, cl.players[r_playerindex].model);

		DM_PlayerState[r_playerindex].model = Mod_ForName(DM_PlayerState[r_playerindex].modelname, false, true);

		if (DM_PlayerState[r_playerindex].model == NULL)
			DM_PlayerState[r_playerindex].model = currententity->model;

		R_StudioChangePlayerModel();
	}

	r_model = DM_PlayerState[r_playerindex].model;

	if (r_model == NULL)
		return 0;

	pstudiohdr = (studiohdr_t*)Mod_Extradata(r_model);

	if (pplayer->gaitsequence)
	{
		vec3_t orig_angles;
		r_playerinfo = &cl.players[r_playerindex];

		VectorCopy(currententity->angles, orig_angles);

		R_StudioProcessGait(pplayer);

		r_playerinfo->gaitsequence = pplayer->gaitsequence;
		r_playerinfo = NULL;

		R_StudioSetUpTransform(0);
		VectorCopy(orig_angles, currententity->angles);
	}
	else
	{
		currententity->curstate.controller[0] = 127;
		currententity->curstate.controller[1] = 127;
		currententity->curstate.controller[2] = 127;
		currententity->curstate.controller[3] = 127;
		currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
		currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
		currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
		currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

		r_playerinfo = &cl.players[r_playerindex];
		r_playerinfo->gaitsequence = 0;

		R_StudioSetUpTransform(0);
	}

	if (flags & STUDIO_RENDER)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (!R_StudioCheckBBox())
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if (pstudiohdr->numbodyparts == 0)
			return 1;
	}

	r_playerinfo = &cl.players[r_playerindex];
	R_StudioSetupBones();
	R_StudioSaveBones();
	r_playerinfo->renderframe = r_framecount;

	r_playerinfo = NULL;

	if (flags & STUDIO_EVENTS)
	{
		R_StudioCalcAttachments();
		R_StudioClientEvents();
		// copy attachments into global entity array
		if (currententity->index > 0)
		{
			Q_memcpy(cl_entities[currententity->index].attachment, currententity->attachment, sizeof(vec3_t) * 4);
		}
	}

	if (flags & STUDIO_RENDER)
	{
		if (cl_himodels.value && DM_PlayerState[r_playerindex].model != currententity->model)
		{
			// show highest resolution multiplayer model
			currententity->curstate.body = 255;
		}

		if (!(developer.value == 0 && Host_IsSinglePlayerGame()) && DM_PlayerState[r_playerindex].name[0] && (DM_PlayerState[r_playerindex].model == currententity->model))
		{
			currententity->curstate.body = 1; // force helmet
		}

		lighting.plightvec = dir;
		R_StudioDynamicLight(currententity, &lighting);

		R_StudioEntityLight(&lighting);

		// model and frame independant
		R_StudioSetupLighting(&lighting);

		r_playerinfo = &cl.players[r_playerindex];

		// get remap colors
		r_topcolor = r_playerinfo->topcolor;
		r_bottomcolor = r_playerinfo->bottomcolor;

		// bounds check
		if (r_topcolor < 0)
			r_topcolor = 0;
		if (r_topcolor > 360)
			r_topcolor = 360;
		if (r_bottomcolor < 0)
			r_bottomcolor = 0;
		if (r_bottomcolor > 360)
			r_bottomcolor = 360;

		R_StudioRenderModel();
		r_playerinfo = NULL;

		if (pplayer->weaponmodel)
		{
			cl_entity_t saveent = *currententity;

			model_t* pweaponmodel = CL_GetModelByIndex(pplayer->weaponmodel);

			pstudiohdr = (studiohdr_t*)Mod_Extradata(pweaponmodel);

			R_StudioMergeBones(pweaponmodel);

			R_StudioSetupLighting(&lighting);

			R_StudioRenderModel();

			R_StudioCalcAttachments();

			*currententity = saveent;
		}
	}

	return 1;
}

void R_UpdateLatchedVars(cl_entity_t* ent)
{
	VectorCopy(ent->prevstate.origin, ent->latched.prevorigin);
	VectorCopy(ent->prevstate.angles, ent->latched.prevangles);
	ent->latched.prevanimtime = ent->prevstate.animtime;
	if (ent->curstate.sequence != ent->prevstate.sequence)
	{
		ent->latched.prevsequence = ent->prevstate.sequence;
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevseqblending[0] = ent->prevstate.blending[0];
		ent->latched.prevseqblending[1] = ent->prevstate.blending[1];
	}
	ent->latched.prevblending[0] = ent->prevstate.blending[0];
	ent->latched.prevblending[1] = ent->prevstate.blending[1];
	ent->latched.prevcontroller[0] = ent->prevstate.controller[0];
	ent->latched.prevcontroller[1] = ent->prevstate.controller[1];
	ent->latched.prevcontroller[2] = ent->prevstate.controller[2];
	ent->latched.prevcontroller[3] = ent->prevstate.controller[3];
}

// 25.12.23
float CL_GetEstimatedFrame(cl_entity_t* ent)
{
	studiohdr_t* pstudiohdr;
	mstudioseqdesc_t* pseqdesc;
	cl_entity_t* oldce;
	float time = 0.0f;

	oldce = currententity;

	if (ent->model && ent->model->type == mod_studio)
	{
		pstudiohdr = (studiohdr_t*)Mod_Extradata(ent->model);

		if (pstudiohdr != NULL)
		{
			currententity = ent;
			pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex) + currententity->curstate.sequence;
			time = CL_StudioEstimateFrame(pseqdesc);
			currententity = oldce;
		}
	}

	return time;
}

void R_ResetLatched(cl_entity_t* ent, qboolean full_reset)
{
	entity_state_t* p_prevstate;
	entity_state_t* p_curstate;

	if (full_reset)
	{
		Q_memcpy(ent->latched.prevcontroller, ent->curstate.controller, 4);
		Q_memcpy(ent->latched.prevblending, ent->curstate.blending, 2);
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevframe = CL_GetEstimatedFrame(ent);
		p_prevstate = &ent->prevstate;
		p_curstate = &ent->curstate;
		*p_prevstate = *p_curstate;
	}
	ent->latched.prevsequence = ent->curstate.sequence;
	ent->curstate.animtime = ent->latched.prevanimtime = cl.mtime[0];
	VectorCopy(ent->curstate.origin, ent->latched.prevorigin);
	VectorCopy(ent->curstate.angles, ent->latched.prevangles);
}

void R_StudioBoundVertex(vec_t* mins, vec_t* maxs, int* vertcount, const vec_t* point)
{
	if (*vertcount)
	{
		for (int i = 0; i < 3; i++)
		{
			if (mins[i] > point[i])
				mins[i] = point[i];

			if (maxs[i] < point[i])
				maxs[i] = point[i];
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			mins[i] = point[i];
			maxs[i] = point[i];
		}
	}

	++(*vertcount);
}

void R_StudioBoundBone(vec_t* mins, vec_t* maxs, int* bonecount, const vec_t* point)
{
	if (*bonecount)
	{
		for (int i = 0; i < 3; i++)
		{
			if (mins[i] > point[i])
				mins[i] = point[i];

			if (maxs[i] < point[i])
				maxs[i] = point[i];
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			mins[i] = point[i];
			maxs[i] = point[i];
		}
	}

	++(*bonecount);
}

void R_StudioAccumulateBoneVerts(vec_t* mins, vec_t* maxs, int* vertcount, vec_t* bone_mins, vec_t* bone_maxs, int* bonecount)
{
	vec3_t delta;
	vec3_t point;

	if (*bonecount > 0)
	{
		VectorSubtract(bone_maxs, bone_mins, delta);
		VectorScale(delta, 0.5, point);
		R_StudioBoundVertex(mins, maxs, vertcount, point);
		VectorScale(delta, -0.5, point);
		R_StudioBoundVertex(mins, maxs, vertcount, point);

		for (int i = 0; i < 3; i++)
			bone_mins[i] = bone_maxs[i] = 0;

		*bonecount = 0;
	}
}

int R_StudioComputeBounds(unsigned char* pBuffer, float* mins, float* maxs)
{
	vec3_t bone_mins;
	vec3_t bone_maxs;
	vec3_t vert_mins;
	vec3_t vert_maxs;

	for (int i = 0; i < 3; i++)
		bone_mins[i] = bone_maxs[i] = vert_mins[i] = vert_maxs[i] = 0;

	studiohdr_t* header = (studiohdr_t*)pBuffer;
	mstudiobodyparts_t* bodyparts = (mstudiobodyparts_t*)((char*)header + header->bodypartindex);

	int nummodels = 0;
	for (int i = 0; i < header->numbodyparts; i++)
		nummodels += bodyparts[i].nummodels;

	int vert_count = 0;
	mstudiomodel_t* pmodel = (mstudiomodel_t*)&bodyparts[header->numbodyparts];
	for (int model = 0; model < nummodels; model++, pmodel++)
	{
		int num_verts = pmodel->numverts;
		vec3_t* pverts = (vec3_t*)((char*)header + pmodel->vertindex);
		for (int v = 0; v < num_verts; v++)
			R_StudioBoundVertex(vert_mins, vert_maxs, &vert_count, pverts[v]);
	}

	int bone_count = 0;
	mstudiobone_t* pbones = (mstudiobone_t*)((char*)header + header->boneindex);
	mstudioseqdesc_t* pseq = (mstudioseqdesc_t*)((char*)header + header->seqindex);
	for (int sequence = 0; sequence < header->numseq; sequence++, pseq++)
	{
		int num_frames = pseq->numframes;
		mstudioanim_t* panim = (mstudioanim_t*)((char*)header + pseq->animindex);
		for (int bone = 0; bone < header->numbones; bone++)
		{
			for (int f = 0; f < num_frames; f++)
			{
				vec3_t bonepos;
				R_StudioCalcBonePosition(f, 0.0, &pbones[bone], panim, NULL, bonepos);
				R_StudioBoundBone(bone_mins, bone_maxs, &bone_count, bonepos);
			}
		}

		R_StudioAccumulateBoneVerts(vert_mins, vert_maxs, &vert_count, bone_mins, bone_maxs, &bone_count);
	}

	for (int i = 0; i < 3; i++)
	{
		mins[i] = vert_mins[i];
		maxs[i] = vert_maxs[i];
	}

	return 1;
}

int R_GetStudioBounds(const char* filename, float* mins, float* maxs)
{
	int iret = 0;
	qboolean usingReadBuffer = 0;

	for (int i = 0; i < 3; i++)
		mins[0] = maxs[0] = vec3_origin[i];

	if (!strstr(filename, "models") || !strstr(filename, ".mdl"))
		return 0;

	FileHandle_t fp = FS_Open(filename, "rb");
	if (!fp)
		return 0;

	int length;
	char* pBuffer = (char*)FS_GetReadBuffer(fp, &length);
	if (pBuffer)
		usingReadBuffer = 1;
	else
		pBuffer = (char*)COM_LoadFile((char*)filename, 5, 0);

	if (pBuffer)
	{
		if (LittleLong(*(unsigned int*)pBuffer) == IDSTUDIOHEADER)
			iret = R_StudioComputeBounds((unsigned char*)pBuffer, mins, maxs);
		else
			COM_FreeFile(pBuffer);
	}

	if (usingReadBuffer)
		FS_ReleaseReadBuffer(fp, pBuffer);
	else
		COM_FreeFile(pBuffer);

	FS_Close(fp);

	return iret;
}

hull_t* SV_HullForStudioModel(const edict_t* pEdict, const vec_t* mins, const vec_t* maxs, vec_t* offset, int* pNumHulls)
{
	qboolean useComplexHull = FALSE;
	vec3_t size;
	float factor = 0.5;
	int bSkipShield = 0;
	VectorSubtract(maxs, mins, size);
	if (VectorCompare(vec3_origin, size))
	{
		if (!(gGlobalVariables.trace_flags & FTRACE_SIMPLEBOX))
		{
			useComplexHull = TRUE;
			if (pEdict->v.flags & FL_CLIENT)
			{
				if (sv_clienttrace.value == 0.0)
				{
					useComplexHull = FALSE;
				}
				else
				{
					size[2] = 1.0f;
					size[1] = 1.0f;
					size[0] = 1.0f;
					factor = sv_clienttrace.value * 0.5f;
				}
			}
		}
	}
	if (pEdict->v.gamestate == 1 && (g_bIsTerrorStrike || g_bIsCStrike || g_bIsCZero))
		bSkipShield = 1;

	if ((sv.models[pEdict->v.modelindex]->flags & FL_ONGROUND) || useComplexHull)
	{
		VectorScale(size, factor, size);
		offset[0] = 0;
		offset[1] = 0;
		offset[2] = 0;
		if (pEdict->v.flags & FL_CLIENT)
		{
			pstudiohdr = (studiohdr_t*)Mod_Extradata(sv.models[pEdict->v.modelindex]);

			mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((char*)pstudiohdr + pstudiohdr->seqindex);
			pseqdesc += pEdict->v.sequence;

			vec3_t angles;
			VectorCopy(pEdict->v.angles, angles);

			int iBlend;
			R_StudioPlayerBlend(pseqdesc, &iBlend, angles);

			unsigned char blending = (unsigned char)iBlend;
			unsigned char controller[4] = { 0x7F, 0x7F, 0x7F, 0x7F };
			return R_StudioHull(
				sv.models[pEdict->v.modelindex],
				pEdict->v.frame,
				pEdict->v.sequence,
				angles,
				pEdict->v.origin,
				size,
				controller,
				&blending,
				pNumHulls,
				pEdict,
				bSkipShield);
		}
		else
		{
			return R_StudioHull(
				sv.models[pEdict->v.modelindex],
				pEdict->v.frame,
				pEdict->v.sequence,
				pEdict->v.angles,
				pEdict->v.origin,
				size,
				pEdict->v.controller,
				pEdict->v.blending,
				pNumHulls,
				pEdict,
				bSkipShield);
		}
	}
	else
	{
		*pNumHulls = 1;
		return SV_HullForEntity((edict_t*)pEdict, mins, maxs, offset);
	}
}

qboolean DoesSphereIntersect(float* vSphereCenter, float fSphereRadiusSquared, float* vLinePt, float* vLineDir)
{
	vec3_t P;
	float a;
	float b;
	float c;
	float D;

	VectorSubtract(vLinePt, vSphereCenter, P);
	a = DotProduct(vLineDir, vLineDir);
	b = DotProduct(P, vLineDir) * 2;
	c = DotProduct(P, P) - fSphereRadiusSquared;
	D = b * b - 4.f * a * c;
	// объекты пересекаютс€, если дискриминант больше 0
	return D > 0.000001;
}

qboolean SV_CheckSphereIntersection(edict_t* ent, const vec_t* start, const vec_t* end)
{
	studiohdr_t* studiohdr;
	mstudioseqdesc_t* pseqdesc;

	vec3_t traceOrg;
	vec3_t traceDir;
	float radiusSquared;
	vec3_t maxDim;

	radiusSquared = 0.0f;
	if (!(ent->v.flags & FL_CLIENT))
		return 1;

	VectorCopy(start, traceOrg);
	VectorSubtract(end, start, traceDir);

	studiohdr = (studiohdr_t*)Mod_Extradata(sv.models[ent->v.modelindex]);
	pseqdesc = (mstudioseqdesc_t*)((char*)studiohdr + studiohdr->seqindex);
	pseqdesc += ent->v.sequence;
	for (int i = 0; i < 3; i++)
	{
		maxDim[i] = max(fabs(pseqdesc->bbmax[i]), fabs(pseqdesc->bbmin[i]));
	}
	radiusSquared = DotProduct(maxDim, maxDim);
	return DoesSphereIntersect(ent->v.origin, radiusSquared, traceOrg, traceDir) != 0;
}

void AnimationAutomove(const edict_t* pEdict, float flTime)
{
}

void GetBonePosition(const edict_t* pEdict, int iBone, float* rgflOrigin, float* rgflAngles)
{
	pstudiohdr = (studiohdr_t*)Mod_Extradata(sv.models[pEdict->v.modelindex]);
	g_pSvBlendingAPI->SV_StudioSetupBones(
		sv.models[pEdict->v.modelindex],
		pEdict->v.frame,
		pEdict->v.sequence,
		pEdict->v.angles,
		pEdict->v.origin,
		pEdict->v.controller,
		pEdict->v.blending,
		iBone,
		pEdict
	);

	if (rgflOrigin)
	{
		rgflOrigin[0] = bonetransform[iBone][0][3];
		rgflOrigin[1] = bonetransform[iBone][1][3];
		rgflOrigin[2] = bonetransform[iBone][2][3];
	}
}

void GetAttachment(const edict_t* pEdict, int iAttachment, float* rgflOrigin, float* rgflAngles)
{
	mstudioattachment_t* pattachment;
	vec3_t angles;

	angles[0] = -pEdict->v.angles[0];
	angles[1] = pEdict->v.angles[1];
	angles[2] = pEdict->v.angles[2];

	pstudiohdr = (studiohdr_t*)Mod_Extradata(sv.models[pEdict->v.modelindex]);
	pattachment = (mstudioattachment_t*)((char*)pstudiohdr + pstudiohdr->attachmentindex);
	pattachment += iAttachment;

	g_pSvBlendingAPI->SV_StudioSetupBones(
		sv.models[pEdict->v.modelindex],
		pEdict->v.frame,
		pEdict->v.sequence,
		angles,
		pEdict->v.origin,
		pEdict->v.controller,
		pEdict->v.blending,
		pattachment->bone,
		pEdict
	);

	if (rgflOrigin)
		VectorTransform(pattachment->org, bonetransform[pattachment->bone], rgflOrigin);
}



model_t* legacy_Mod_ForName(const char* model, int crash)
{
	return Mod_ForName(model, (qboolean)crash, false);
}

model_t* studioapi_GetModelByIndex(int index)
{
	return CL_GetModelByIndex(index);
}

cl_entity_t* studioapi_GetCurrentEntity()
{
	return currententity;
}

player_info_t* studioapi_PlayerInfo(int index)
{
	if (index >= 0 && index < cl.maxclients)
		return &cl.players[index];

	return NULL;
}

entity_state_t* studioapi_GetPlayerState(int index)
{
	if (index >= 0 && index < cl.maxclients)
		return &cl.frames[cl.parsecountmod].playerstate[index];

	return NULL;
}

cl_entity_t* studioapi_GetViewEntity()
{
	return &cl.viewent;
}

void studioapi_GetTimes(int* framecount, double* cl_time, double* cl_oldtime)
{
	*framecount = r_framecount;
	*cl_time = cl.time;
	*cl_oldtime = cl.oldtime;
}

cvar_t* studioapi_GetCvar(const char* name)
{
	return Cvar_FindVar(name);
}

void studioapi_GetViewInfo(float* origin, float* upv, float* rightv, float* vpnv)
{
	VectorCopy(r_origin, origin);
	VectorCopy(vup, upv);
	VectorCopy(vright, rightv);
	VectorCopy(vpn, vpnv);
}

model_t* studioapi_GetChromeSprite()
{
	return cl_sprite_shell;
}

void studioapi_GetModelCounters(int** s, int** a)
{
	*s = &r_smodels_total;
	*a = &r_amodels_drawn;
}

void studioapi_GetAliasScale(float* x, float* y)
{
#if defined(GLQUAKE)
	*y = 1.0f;
	*x = 1.0f;
#else
	*y = aliasyscale;
	*x = aliasxscale;
#endif
}

float**** studioapi_StudioGetBoneTransform()
{
	return (float****)bonetransform;
}

float**** studioapi_StudioGetLightTransform()
{
	return (float****)lighttransform;
}

float*** studioapi_StudioGetAliasTransform()
{
#if defined(GLQUAKE)
	return NULL;
#else
	return (float***)aliastransform;
#endif
}

float*** studioapi_StudioGetRotationMatrix()
{
	return (float***)rotationmatrix;
}

void studioapi_SetupModel(int bodypart, void** ppbodypart, void** ppsubmodel)
{
	if (pstudiohdr->numbodyparts < bodypart)
	{
		Con_DPrintf(const_cast<char*>("R_StudioSetupModel: no such bodypart %d\n"), bodypart);
		bodypart = 0;
	}

	pbodypart = (mstudiobodyparts_t*)((byte*)pstudiohdr + pstudiohdr->bodypartindex) + bodypart;
	psubmodel = (mstudiomodel_t*)((byte*)pstudiohdr + pbodypart->modelindex) + (currententity->curstate.body / pbodypart->base % pbodypart->nummodels);

	*ppbodypart = &pbodypart;
	*ppsubmodel = &psubmodel;
}

void studioapi_StudioSetRemapColors(int top, int bottom)
{
	r_topcolor = top;
	r_bottomcolor = bottom;
}

model_t* studioapi_SetupPlayerModel(int playerindex)
{
	model_t* mod = NULL;

	if (!developer.value && Host_IsSinglePlayerGame() || !cl.players[playerindex].model[0])
	{
		if (DM_PlayerState[playerindex].model != currententity->model)
		{
			DM_PlayerState[playerindex].model = currententity->model;
			R_StudioChangePlayerModel();
		}
	}
	else if (Q_strcmp(DM_PlayerState[playerindex].name, cl.players[playerindex].model))
	{
		Q_strncpy(DM_PlayerState[playerindex].name, cl.players[playerindex].model, sizeof(DM_PlayerState[playerindex].name) - 1);
		DM_PlayerState[playerindex].name[sizeof(DM_PlayerState[playerindex].name) - 1] = '\0';

		_snprintf(DM_PlayerState[playerindex].modelname, sizeof(DM_PlayerState[playerindex].modelname), "models/player/%s/%s.mdl",
			cl.players[playerindex].model, cl.players[playerindex].model);

		mod = Mod_ForName(DM_PlayerState[playerindex].modelname, false, true);

		DM_PlayerState[playerindex].model = mod != NULL ? mod : currententity->model;

		R_StudioChangePlayerModel();
	}
	
	mod = DM_PlayerState[playerindex].model;

	return mod;
}

int studioapi_GetForceFaceFlags()
{
	return g_ForcedFaceFlags;
}

void studioapi_SetForceFaceFlags(int flags)
{
	g_ForcedFaceFlags = flags;
}

void studioapi_StudioSetHeader(void* header)
{
	pstudiohdr = (studiohdr_t*)header;
}

void studioapi_SetRenderModel(model_t* model)
{
	r_model = model;
}

void studioapi_SetupRenderer(int rendermode)
{
#if defined(GLQUAKE)
	pauxverts = auxverts;
	
	pvlightvalues = lightvalues;
	pvlightvalues_size = 2048;

	shadevector[0] = 1.0f;
	shadevector[1] = -0.0f;
	shadevector[2] = 1.0f;
	
	VectorNormalize(shadevector);
	
	GL_DisableMultitexture();
	
	qglPushMatrix();

	qglShadeModel(GL_SMOOTH);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value != 0.0f)
		qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	g_rendermode = rendermode;
#else
	int seq;

	R_StudioVertBuffer();

	seq = currententity->curstate.sequence;
	if (seq < 0 || seq >= pstudiohdr->numseq)
	{
		Con_DPrintf(const_cast<char*>("R_StudioRenderFinal:  sequence %i/%i out of range for model %s\n"), seq, pstudiohdr->numseq, pstudiohdr->name);
		currententity->curstate.sequence = 0;
	}

	zishift = 0.0f;

	ziscale = (float)(INFINITE_DISTANCE * ZISCALE); // 2147483600.0f;
	// если рисуема€ модель - вид (wvp -> v)
	if (&cl.viewent == currententity)
		ziscale = (float)(INFINITE_DISTANCE * ZISCALE) * 3; // 6442450900.0f

	r_affinetridesc.drawtype = 0;
#endif
}

void studioapi_RestoreRenderer()
{
	// в софтваре нечего ресторить!
#if defined(GLQUAKE)
	if (g_rendermode)
		qglDisable(GL_BLEND);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	qglShadeModel(GL_FLAT);

	if (gl_affinemodels.value != 0.0)
		qglHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	qglPopMatrix();
#endif
}

void studioapi_SetChromeOrigin()
{
	VectorCopy(r_origin, g_ChromeOrigin);
}

int IsHardware()
{
	// 0 - Software
	// 1 - OpenGL
	// 2 - Direct3D
#if defined(GLQUAKE)
	return gD3DMode == 0 ? 1 : 2;
#else
	return false;
#endif
}

#if defined(GLQUAKE)
void GLR_StudioDrawShadow()
{
	GLfloat point[10];

	mstudiomesh_t* pmesh = (mstudiomesh_t*)((byte*)pstudiohdr + psubmodel->meshindex);

	for (int k = 0; k < psubmodel->nummesh; k++, pmesh++)
	{
		short* ptricmds, tri;
		mstudiotrivert_t* ptrivert;

		float height = lightspot[2] + 1.0f;

		c_alias_polys += pmesh->numtris;

		// mstudiotrivert_t в количестве n сразу же после задани€ n (2 байта)
		ptricmds = (short*)((byte*)pstudiohdr + pmesh->triindex);

		while (((tri = *(ptricmds++)) != 0))
		{
			if (tri < 0)
			{
				tri = -tri;
				qglBegin(GL_TRIANGLE_FAN);
			}
			else
			{
				qglBegin(GL_TRIANGLE_STRIP);
			}

			ptrivert = (mstudiotrivert_t*)(ptricmds);
			for (int j = 0; j < tri; j++, ptricmds = (short*)++ptrivert)
			{
				auxvert_t* pvert = &pauxverts[ptrivert->vertindex];

				VectorCopy(pvert->fv, point);

				point[0] -= shadevector[0] * (point[2] - lightspot[2]);
				point[1] -= shadevector[1] * (point[2] - lightspot[2]);
				point[2] = height;

				qglVertex3fv(point);
			}
			qglEnd();
		}
	}
}
#endif

void studioapi_GL_StudioDrawShadow()
{
#if defined(GLQUAKE)
	float color;

	qglDepthMask(1);

	if (r_shadows.value != 0.0 && g_rendermode != kRenderTransAdd && (r_model->flags & STUDIO_DYNAMIC_LIGHT) == 0)
	{
		color = 1.0f - 0.5f * r_blend;

		qglDisable(GL_TEXTURE_2D);

		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		qglEnable(GL_BLEND);


		qglColor4f(0.0f, 0.0f, 0.0f, 1.0f - color);

		if (gl_ztrick.value == 0.0f || gldepthmin < 0.5f)
			qglDepthFunc(GL_LESS);
		else
			qglDepthFunc(GL_GREATER);

		GLR_StudioDrawShadow();

		if (gl_ztrick.value == 0.0f || gldepthmin < 0.5f)
			qglDepthFunc(GL_LEQUAL);
		else
			qglDepthFunc(GL_GEQUAL);

		qglEnable(GL_TEXTURE_2D);
		qglDisable(GL_BLEND);

		qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		qglShadeModel(GL_SMOOTH);
	}
#endif
}

void studioapi_GL_SetRenderMode(int rendermode)
{
#if defined(GLQUAKE)
	if (rendermode)
	{
		if (rendermode == kRenderTransColor)
		{
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
		}
		else
		{
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			if (rendermode == kRenderTransAdd)
			{
				qglBlendFunc(1, 1);

				qglColor4f(r_blend, r_blend, r_blend, 1.0f);
				
				qglDepthMask(0);
			}
			else
			{
				qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				qglColor4f(1.0f, 1.0f, 1.0f, r_blend);

				qglDepthMask(1);
			}
		}

		qglEnable(GL_BLEND);
	}
#endif
}

void studioapi_StudioSetRenderamt(int iRenderamt)
{
	currententity->curstate.renderamt = iRenderamt;
	r_blend = int((float)CL_FxBlend(currententity) / 255.0f);
}

void studioapi_SetBackFaceCull(int iCull)
{
	g_iBackFaceCull = iCull;
}

void studioapi_RenderShadow(int iSprite, float* p1, float* p2, float* p3, float* p4)
{
	// почему софтвар не может?
#if defined (GLQUAKE)
	model_t* mod;

	if (iSprite > 0 && iSprite < cl.model_precache_count)
	{
		mod = CL_GetModelByIndex(iSprite);
		if (mod)
		{
			if (tri.SpriteTexture(mod, 0))
			{
				tri.RenderMode(kRenderTransAlpha);
				tri.Color4f(0.0, 0.0, 0.0, 1.0);
				tri.Begin(TRI_QUADS);
				tri.TexCoord2f(0.0, 0.0);
				tri.Vertex3fv(p1);
				tri.TexCoord2f(0.0, 1.0);
				tri.Vertex3fv(p2);
				tri.TexCoord2f(1.0, 1.0);
				tri.Vertex3fv(p3);
				tri.TexCoord2f(1.0, 0.0);
				tri.Vertex3fv(p4);
				tri.End();
				tri.RenderMode(kRenderNormal);
			}
		}
	}
#endif
}

engine_studio_api_t engine_studio_api = {
#ifdef _DEBUG
	& Dummy_Calloc,
#else
	& Mem_Calloc,
#endif
	& Cache_Check,
	&COM_LoadCacheFile,
	&legacy_Mod_ForName,
	&Mod_Extradata,
	&studioapi_GetModelByIndex,
	&studioapi_GetCurrentEntity,
	&studioapi_PlayerInfo,
	&studioapi_GetPlayerState,
	&studioapi_GetViewEntity,
	&studioapi_GetTimes,
	&studioapi_GetCvar,
	&studioapi_GetViewInfo,
	&studioapi_GetChromeSprite,
	&studioapi_GetModelCounters,
	&studioapi_GetAliasScale,
	&studioapi_StudioGetBoneTransform,
	&studioapi_StudioGetLightTransform,
	&studioapi_StudioGetAliasTransform,
	&studioapi_StudioGetRotationMatrix,
	&studioapi_SetupModel,
	&R_StudioCheckBBox,
	&R_StudioDynamicLight,
	&R_StudioEntityLight,
	&R_StudioSetupLighting,
	&R_StudioDrawPoints,
	&R_StudioDrawHulls,
	&R_StudioAbsBB,
	&R_StudioDrawBones,
	(decltype(engine_studio_api_t::StudioSetupSkin))&R_StudioSetupSkin,
	&studioapi_StudioSetRemapColors,
	&studioapi_SetupPlayerModel,
	&R_StudioClientEvents,
	&studioapi_GetForceFaceFlags,
	&studioapi_SetForceFaceFlags,
	&studioapi_StudioSetHeader,
	&studioapi_SetRenderModel,
	&studioapi_SetupRenderer,
	&studioapi_RestoreRenderer,
	&studioapi_SetChromeOrigin,
	&IsHardware,
	&studioapi_GL_StudioDrawShadow,
	&studioapi_GL_SetRenderMode,
	&studioapi_StudioSetRenderamt,
	&studioapi_SetBackFaceCull,
	&studioapi_RenderShadow
};
