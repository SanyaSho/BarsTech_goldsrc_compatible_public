#ifndef ENGINE_WORLD_H
#define ENGINE_WORLD_H
#if defined(GLQUAKE)
#include "gl_model.h"
#else
#include "model.h"
#endif

typedef struct areanode_s
{
	int axis;
	float dist;
	struct areanode_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
} areanode_t;

#define AREA_DEPTH 4
#define AREA_NODES 32

typedef struct moveclip_s	// TODO: Move it to world.cpp someday
{
	vec3_t boxmins;
	vec3_t boxmaxs;
	const float *mins;
	const float *maxs;
	vec3_t mins2;
	vec3_t maxs2;
	const float *start;
	const float *end;
	trace_t trace;
	short int type;
	short int ignoretrans;
	edict_t *passedict;
	qboolean monsterClipBrush;
} moveclip_t;

#define CONTENTS_NONE       0 // no custom contents specified

#define MOVE_NORMAL         0 // normal trace
#define MOVE_NOMONSTERS     1 // ignore monsters (edicts with flags (FL_MONSTER|FL_FAKECLIENT|FL_CLIENT) set)
#define MOVE_MISSILE        2 // extra size for monsters

#define FMOVE_IGNORE_GLASS  0x100
#define FMOVE_SIMPLEBOX     0x200

typedef dclipnode_t box_clipnodes_t[6];
typedef mplane_t box_planes_t[6];
typedef mplane_t beam_planes_t[6];

qboolean SV_RecursiveHullCheck( hull_t* hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t* trace );
hull_t *SV_HullForEntity(edict_t *ent, const vec_t *mins, const vec_t *maxs, vec_t *offset);
void SV_ClearWorld();
void SV_UnlinkEdict(edict_t *ent);
void SV_LinkEdict(edict_t *ent, qboolean touch_triggers);
hull_t *SV_HullForBsp(edict_t *ent, const vec_t *mins, const vec_t *maxs, vec_t *offset);
trace_t SV_ClipMoveToEntity(edict_t *ent, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end);
trace_t SV_Move(const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int type, edict_t *passedict, qboolean monsterClipBrush);
int SV_PointContents(const vec_t *p);
edict_t *SV_TestEntityPosition(edict_t *ent);
trace_t SV_MoveNoEnts(const vec_t *start, vec_t *mins, vec_t *maxs, const vec_t *end, int type, edict_t *passedict);
int SV_HullPointContents(hull_t* hull, int num, const vec_t* p);

extern areanode_t sv_areanodes[AREA_NODES];

#endif //ENGINE_WORLD_H
