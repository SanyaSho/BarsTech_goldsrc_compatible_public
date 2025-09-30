/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#ifndef GL_MODEL_H
#define GL_MODEL_H

#ifdef _WIN32
#pragma once
#endif

#include "modelgen.h"
#include "bspfile.h"
#include "spritegn.h"
#include "wad.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

#include "studio.h"

#define STUDIO_RENDER 1
#define STUDIO_EVENTS 2

#define MAX_CLIENTS			32
#define	MAX_EDICTS			900

#define MAX_MODEL_NAME		64
#define MAX_MAP_HULLS		4
#define	MIPLEVELS			4
#define	NUM_AMBIENTS		4		// automatic ambient sounds
#define	MAXLIGHTMAPS		4
#define	PLANE_ANYZ			5

#define ALIAS_Z_CLIP_PLANE	5

// flags in finalvert_t.flags
#define ALIAS_LEFT_CLIP				0x0001
#define ALIAS_TOP_CLIP				0x0002
#define ALIAS_RIGHT_CLIP			0x0004
#define ALIAS_BOTTOM_CLIP			0x0008
#define ALIAS_Z_CLIP				0x0010
#define ALIAS_ONSEAM				0x0020
#define ALIAS_XY_CLIP_MASK			0x000F

#define	ZISCALE	((float)0x8000)

#define CACHE_SIZE	32		// used to align key data structures

#define ALIAS_MODEL_VERSION		0x006

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
// normalizing factor so player model works out to about
//  1 pixel per triangle
#define MAX_LBM_HEIGHT			480
#define MAX_ALIAS_MODEL_VERTS	2000

//struct msurface_t;
//struct decal_t;

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/

//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
//struct mvertex_t
//{
//	vec3_t position;
//};

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2
#define	SIDE_CROSS	-2


// plane_t structure
// !!! if this is changed, it must be changed in asm_i386.h too !!!
//struct mplane_t
//{
//	vec3_t normal;
//	float dist;
//	byte type;			// for texture axis selection and fast side tests
//	byte signbits;		// signx + signy<<1 + signz<<1
//	byte pad[2];
//};

//struct texture_t
//{
//	char		name[16];
//	unsigned int width;
//	unsigned int height;
//	int			gl_texturenum;
//	msurface_t* texturechain;
//	int			anim_total;
//	int			anim_min;
//	int			anim_max;
//	texture_t*	anim_next;
//	texture_t*	alternate_anims;
//	unsigned int offsets[MIPLEVELS];
//	byte*		pPal;
//};

#define SURF_PLANEBACK			2
#define SURF_DRAWSKY			4
#define SURF_DRAWSPRITE			8
#define SURF_DRAWTURB			0x10
#define SURF_DRAWTILED			0x20
#define SURF_DRAWBACKGROUND		0x40
#define SURF_UNDERWATER			0x80
#define SURF_DONTWARP			0x100

// !!! if this is changed, it must be changed in asm_draw.h too !!!
//struct medge_t
//{
//	unsigned short v[2];
//	unsigned int cachededgeoffset;
//};

//struct mtexinfo_t
//{
//	float		vecs[2][4];
//	float		mipadjust;
//	texture_t*	texture;
//	int			flags;
//};

//#define	VERTEXSIZE	7
//
//struct glpoly_t
//{
//	glpoly_t*	next;
//	glpoly_t*	chain;
//	int			numverts;
//	int			flags;					// for SURF_UNDERWATER
//	float		verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
//};
//
//struct msurface_t
//{
//	int			visframe;		// should be drawn when node is crossed
//
//	mplane_t*	plane;
//	int			flags;
//
//	int			firstedge;	// look up in model->surfedges[], negative numbers
//	int			numedges;	// are backwards edges
//
//	short		texturemins[2]; // smallest s/t position on the surface.
//	short		extents[2];	// ?? s/t texture size, 1..256 for all non-sky surfaces
//
//	int			light_s, light_t;	// gl lightmap coordinates
//
//	glpoly_t*	polys;			// multiple if warped
//	msurface_t* texturechain;
//
//	mtexinfo_t* texinfo;
//
//// lighting info
//	int			dlightframe;
//	int			dlightbits;
//
//	int			lightmaptexturenum;
//	byte		styles[MAXLIGHTMAPS]; // index into d_lightstylevalue[] for animated lights 
//									  // no one surface can be effected by more than 4 
//									  // animated lights.
//
//	int			cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
//	qboolean	cached_dlight;				// true if dynamic light in cache
//	color24*	samples;		// [numstyles*surfsize]
//	decal_t*	pdecals;
//};

//struct mnode_t
//{
//// common with leaf
//	int			contents;		// 0, to differentiate from leafs
//	int			visframe;		// node needs to be traversed if current
//
//	float		minmaxs[6];		// for bounding box culling
//
//
//	mnode_t* parent;
//
//// node specific
//	mplane_t* plane;
//	mnode_t* children[2];
//
//	unsigned short		firstsurface;
//	unsigned short		numsurfaces;
//};

// JAY: Compress this as much as possible
//struct decal_t
//{
//	decal_t*	pnext;			// linked list for each surface
//	msurface_t* psurface;		// Surface id for persistence / unlinking
//	float		dx;				// Offsets into surface texture (in texture coordinates, so we don't need floats)
//	float		dy;
//	float		scale;			// scale
//	short		texture;		// Decal texture
//	short		flags;			// Decal flags
//
//	short		entityIndex;	// Entity this is attached to
//};

//struct DECALLIST
//{
//	char* name;
//	short entityIndex;
//
//	byte depth;
//	byte flags;
//	vec3_t position;
//};

//struct mleaf_t
//{
//// common with node
//	int			contents;		// will be a negative contents number
//	int			visframe;		// node needs to be traversed if current
//
//	float		minmaxs[6];		// for bounding box culling
//
//	mnode_t*	parent;
//
//// leaf specific
//	byte*		compressed_vis;
//	efrag_t*	efrags;
//
//	msurface_t** firstmarksurface;
//	int			nummarksurfaces;
//	int			key;			// BSP sequence number for leaf's contents
//	byte ambient_sound_level[NUM_AMBIENTS];
//};

// !!! if this is changed, it must be changed in asm_i386.h too !!!
//struct hull_t
//{
//	dclipnode_t*	clipnodes;
//	mplane_t*		planes;
//	int				firstclipnode;
//	int				lastclipnode;
//	vec3_t			clip_mins;
//	vec3_t			clip_maxs;
//};

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

// FIXME: shorten these?
struct mspriteframe_t
{
	int		width;
	int		height;
	float	up, down, left, right;
	int		gl_texturenum;
};

struct mspritegroup_t
{
	int				numframes;
	float* intervals;
	mspriteframe_t* frames[1];
};

struct mspriteframedesc_t
{
	spriteframetype_t	type;
	mspriteframe_t* frameptr;
};

struct msprite_t
{
	short				type;
	short				texFormat;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	int					paloffset;
	float				beamlength;		// remove?
	void* cachespot;		// remove?
	mspriteframedesc_t	frames[1];
};

enum GL_TEXTURETYPE
{
	GLT_SYSTEM,
	GLT_DECAL,
	GLT_HUDSPRITE,
	GLT_STUDIO,
	GLT_WORLD,
	GLT_SPRITE
};

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

struct maliasframedesc_t
{
	int					firstpose;
	int					numposes;
	float				interval;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	char				name[16];
};

struct maliasgroupframedesc_t
{
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
};

struct maliasgroup_t
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
};

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s
{
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;


#define	MAX_SKINS	32

struct aliashdr_t
{
	int					ident;
	int					version;
	vec3_t				scale;
	vec3_t				scale_origin;
	float				boundingradius;
	vec3_t				eyeposition;
	int					numskins;
	int					skinwidth;
	int					skinheight;
	int					numverts;
	int					numtris;
	int					numframes;
	synctype_t			synctype;
	int					flags;
	float				size;

	int					numposes;
	int					poseverts;
	int					posedata;	// numposes*poseverts trivert_t
	int					commands;	// gl command list with embedded s/t
	int					gl_texturenum[MAX_SKINS];
	maliasframedesc_t	frames[1];	// variable sized
};

//struct cachepic_t
//{
//	char name[64];
//	qpic_t pic;
//	byte padding[32];
//};

#define	MAX_KNOWN_MODELS	1024
#define	MAXALIASVERTS		1024
#define	MAXALIASFRAMES		256
#define	MAXALIASTRIS		2048
extern	aliashdr_t* pheader;
extern	stvert_t	stverts[MAXALIASVERTS];
extern	mtriangle_t	triangles[MAXALIASTRIS];
extern	trivertx_t* poseverts[MAXALIASFRAMES];

//===================================================================

//
// Whole model
//

//enum modtype_t
//{
//	mod_bad = -1,
//	mod_brush,
//	mod_sprite,
//	mod_alias,
//	mod_studio
//};

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail

enum NeedLoad : int
{
	NL_PRESENT = 0,		// The model is already present
	NL_NEEDS_LOADED,	// The model needs to be loaded
	NL_UNREFERENCED		// The model is unreferenced
};

//struct model_t
//{
//	char		name[MAX_QPATH];
//	qboolean	needload;		// bmodels and sprites don't cache normally
//
//	modtype_t	type;
//	int			numframes;
//	synctype_t	synctype;
//
//	int			flags;
//
////
//// volume occupied by the model graphics
////	
//	vec3_t		mins, maxs;
//	float		radius;
//
////
//// brush model
////
//	int			firstmodelsurface, nummodelsurfaces;
//
//	int			numsubmodels;
//	dmodel_t*	submodels;
//
//	int			numplanes;
//	mplane_t*	planes;
//
//	int			numleafs;		// number of visible leafs, not counting 0
//	mleaf_t*	leafs;
//
//	int			numvertexes;
//	mvertex_t*	vertexes;
//
//	int			numedges;
//	medge_t*	edges;
//
//	int			numnodes;
//	mnode_t*	nodes;
//
//	int			numtexinfo;
//	mtexinfo_t* texinfo;
//
//	int			numsurfaces;
//	msurface_t* surfaces;
//
//	int			numsurfedges;
//	int*		surfedges;
//
//	int			numclipnodes;
//	dclipnode_t* clipnodes;
//
//	int			nummarksurfaces;
//	msurface_t** marksurfaces;
//
//	hull_t		hulls[MAX_MAP_HULLS];
//
//	int			numtextures;
//	texture_t** textures;
//
//	byte*		visdata;
//	color24*	lightdata;
//	char*		entities;
//
////
//// additional model data
////
//	cache_user_t cache;		// only access through Mod_Extradata
//};

//struct alight_t
//{
//	int			ambientlight;	// clip at 128
//	int			shadelight;		// clip at 192 - ambientlight
//	vec3_t		color;
//	float*		plightvec;
//};

//struct auxvert_t
//{
//	float fv[3];
//};

#include "custom.h"

//#define	MAX_INFO_STRING			256
//#define	MAX_SCOREBOARDNAME		32
//struct player_info_t
//{
//	// User id on server
//	int		userid;
//
//	// User info string
//	char	userinfo[MAX_INFO_STRING];
//
//	// Name
//	char	name[MAX_SCOREBOARDNAME];
//
//	// Spectator or not, unused
//	int		spectator;
//
//	int		ping;
//	int		packet_loss;
//
//	// skin information
//	char	model[MAX_QPATH];
//	int		topcolor;
//	int		bottomcolor;
//
//	// last frame rendered
//	int		renderframe;
//
//	// Gait frame estimation
//	int		gaitsequence;
//	float	gaitframe;
//	float	gaityaw;
//	vec3_t	prevgaitorigin;
//
//	customization_t customdata;
//
//	char	hashedcdkey[16];
//	uint64	m_nSteamID;
//};

//============================================================================

void Mod_ClearAll( void );

mleaf_t* Mod_PointInLeaf( vec_t* p, model_t* model );

model_t* Mod_LoadModel( model_t* mod, const bool crash, const bool trackCRC );
void Mod_LoadStudioModel( model_t* mod, void* buffer );
model_t* Mod_FindName( bool trackCRC, const char* name );

model_t* Mod_ForName( const char* name, bool crash, bool trackCRC );

void Mod_MarkClient( model_t* pModel );

void Mod_UnloadSpriteTextures( model_t* pModel );

bool Mod_ValidateCRC( const char* name, CRC32_t crc );

void Mod_FillInCRCInfo( bool trackCRC, int model_number );
void Mod_NeedCRC( const char* name, bool needCRC );
void Mod_SetParent( mnode_t* node, mnode_t* parent );
void* Mod_Extradata(model_t* mod);
void Mod_Print(void);

#endif // GL_MODEL_H