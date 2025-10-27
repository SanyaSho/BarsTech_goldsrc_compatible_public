#ifndef ENGINE_R_STUDIO_H
#define ENGINE_R_STUDIO_H

#include "studio.h"
#include "r_studioint.h"
#include "sound.h"

#define STUDIO_DYNAMIC_LIGHT	0x0100	// dynamically get lighting from floor or ceil (flying monsters)
#define STUDIO_TRACE_HITBOX		0x0200	// always use hitbox trace instead of bbox
#define STUDIO_NUM_HULLS	128
#define STUDIO_NUM_PLANES	(STUDIO_NUM_HULLS * 6)
#define STUDIO_CACHE_SIZE	16
#define STUDIO_CACHEMASK	(STUDIO_CACHE_SIZE - 1)

// bonecontroller types
#define STUDIO_MOUTH	4	// hardcoded

// sequence flags
#define STUDIO_LOOPING	0x0001

typedef struct r_studiocache_s
{
	float frame;
	int sequence;
	vec3_t angles;
	vec3_t origin;
	vec3_t size;
	unsigned char controller[4];
	unsigned char blending[2];
	model_t *pModel;
	int nStartHull;
	int nStartPlane;
	int numhulls;
} r_studiocache_t;

typedef struct
{
	int keynum;
	int topcolor;
	int bottomcolor;
	struct model_s *model;
#if !defined(GLQUAKE)
	word palette[1024];
#else
	char name[260];
	int index;
	int source;
	int width;
	int height;
	int gl_index;
#endif
} skin_t;

typedef struct
{
	char		name[MAX_OSPATH];
	char		modelname[MAX_OSPATH];
	model_t		*model;
} player_model_t;

extern engine_studio_api_t engine_studio_api;
extern r_studio_interface_t studio;
extern r_studio_interface_t* pStudioAPI;

#if defined(GLQUAKE)
extern float r_blend;
#else
extern int r_blend;
#endif
extern vec3_t r_colormix;
extern colorVec r_icolormix;
extern colorVec r_icolormix32;
extern int r_dointerp;

extern cvar_t* cl_righthand;

extern cvar_t r_cachestudio;

extern sv_blending_interface_t *g_pSvBlendingAPI;
extern server_studio_api_t server_studio_api;
extern float rotationmatrix[3][4];
extern float bonetransform[STUDIO_NUM_HULLS][3][4];

void R_ResetStudio();

sfx_t* CL_LookupSound( const char* pName );
int SV_HitgroupForStudioHull(int index);
hull_t *R_StudioHull(model_t *pModel, float frame, int sequence, const vec_t *angles, const vec_t *origin, const vec_t *size, const unsigned char *pcontroller, const unsigned char *pblending, int *pNumHulls, const edict_t *pEdict, int bSkipShield);
int R_GetStudioBounds(const char *filename, float *mins, float *maxs);
hull_t *SV_HullForStudioModel(const edict_t *pEdict, const vec_t *mins, const vec_t *maxs, vec_t *offset, int *pNumHulls);
qboolean SV_CheckSphereIntersection(edict_t *ent, const vec_t *start, const vec_t *end);
float CL_StudioEstimateFrame(mstudioseqdesc_t *pseqdesc);
void R_UpdateLatchedVars(cl_entity_t *ent);
void CL_ResetLatchedState(int pnum, packet_entities_t *pack, cl_entity_t *ent);
void R_ResetLatched(cl_entity_t *ent, qboolean full_reset);
void R_ResetSvBlending();
void AnimationAutomove(const edict_t* pEdict, float flTime);
void GetBonePosition(const edict_t* pEdict, int iBone, float* rgflOrigin, float* rgflAngles);
void GetAttachment(const edict_t* pEdict, int iAttachment, float* rgflOrigin, float* rgflAngles);

#endif //ENGINE_R_STUDIO_H
