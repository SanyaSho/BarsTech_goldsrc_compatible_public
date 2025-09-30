#ifndef _MODEL_H
#define _MODEL_H

#include "const.h"
#include "modelgen.h"
#include "spritegn.h"
#include "bspfile.h"
#include "crc.h"
#include "..\common\com_model.h"

#define MAX_KNOWN_MODELS	1024

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail

// values for model_t's needload
#define NL_PRESENT		0
#define NL_NEEDS_LOADED	1
#define NL_UNREFERENCED	2

#define OVERVIEW_FRAME_WIDTH		128
#define OVERVIEW_FRAME_HEIGHT		128
#define OVERVIEW_SPRITE_MAX_FRAMES	48

typedef struct mspriteframe_t
{
	int				width;
	int				height;
	void* pcachespot;
	float			up, down, left, right;
	byte			pixels[4];
} mspriteframe_s;

typedef struct mspritegroup_s
{
	int				numframes;
	float* intervals;
	mspriteframe_t* frames[1];
} mspritegroup_t;

typedef struct mspriteframedesc_s
{
	spriteframetype_t type;
	mspriteframe_t* frameptr;
} mspriteframedesc_t;

typedef struct msprite_s
{
	short int		type;
	short int		texFormat;
	int				maxwidth, maxheight;
	int				numframes;
	int				paloffset;
	float			beamlength;
	void* cachespot;
	mspriteframedesc_t frames[1];
} msprite_t;

/*typedef struct cachepic_s
{
	char			name[64];
	cache_user_t	cache;
} cachepic_t;

typedef struct cachewad_s cachewad_t;

typedef void(*PFNCACHE)(cachewad_t*, unsigned char*);

typedef struct cachewad_s
{
	char* name;
	cachepic_t* cache;
	int				cacheCount;
	int				cacheMax;
	lumpinfo_t*		lumps;
	int				lumpCount;
	int				cacheExtra;
	PFNCACHE		pfnCacheBuild;
	int				numpaths;
	char** basedirs;
	int* lumppathindices;
	int				tempWad;
} cachewad_t;*/

typedef struct mod_known_info_s
{
	qboolean		shouldCRC;
	qboolean		firstCRCDone;
	CRC32_t			initialCRC;
} mod_known_info_t;

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct
{
	aliasframetype_t	type;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	aliasskintype_t		type;
	void				*pcachespot;
	int					skin;
} maliasskindesc_t;

typedef struct
{
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
} maliasgroupframedesc_t;

typedef struct
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

typedef struct
{
	int					numskins;
	int					intervals;
	maliasskindesc_t	skindescs[1];
} maliasskingroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;

typedef struct {
	int					model;
	int					stverts;
	int					skindesc;
	int					triangles;
	int					palette;
	maliasframedesc_t	frames[1];
} aliashdr_t;

//===================================================================

void Mod_UnloadSpriteTextures(model_t* pModel);
model_t* Mod_ForName(const char* name, qboolean crash, qboolean trackCRC);
void* Mod_Extradata(model_t* mod);
void Mod_LoadStudioModel(model_t* mod, void* buffer);
void Mod_MarkClient(model_t* pModel);
void Mod_ClearAll(void);
model_t* Mod_LoadModel(model_t* mod, qboolean crash, qboolean trackCRC);
mleaf_t* Mod_PointInLeaf(vec_t* p, model_t* model);
void Mod_NeedCRC(char* name, qboolean needCRC);
model_t* Mod_FindName(qboolean trackCRC, char* name);
qboolean Mod_ValidateCRC(char* name, CRC32_t crc);
void Mod_Print(void);

#endif
