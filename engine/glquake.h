// disable data conversion warnings

#ifndef GLQUAKE_H
#define GLQUAKE_H
#pragma once

#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#include "render.h"

void GL_BeginRendering( int* x, int* y, int* width, int* height );
void GL_EndRendering( void );

extern	float	gldepthmin, gldepthmax;

void GL_Upload32( unsigned int* data, int width, int height, qboolean mipmap, int iType, int filter );
void GL_Upload16( unsigned char* data, int width, int height, qboolean mipmap, int iType, unsigned char* pPal, int filter );
int GL_LoadTexture( char* identifier, GL_TEXTURETYPE textureType, int width, int height, unsigned char* data, int mipmap, int iType, unsigned char* pPal );
int GL_LoadTexture2( char* identifier, GL_TEXTURETYPE textureType, int width, int height, unsigned char* data, qboolean mipmap, int iType, unsigned char* pPal, int filter );
void GL_LoadFilterTexture( float r, float g, float b, float brightness );

void GL_UnloadTexture( char* identifier );
void GL_UnloadTextures( void );

//qboolean IsPowerOfTwo( int value );

GLuint GL_GenTexture( void );
void GL_SelectTexture( GLenum target );

void GL_PaletteClearSky( void );
short GL_PaletteAdd( unsigned char* pPal, qboolean isSky );

extern	int glx, gly, glwidth, glheight;

#define NUMVERTEXNORMALS	162

#define BACKFACE_EPSILON	0.01

#define TEX_TYPE_NONE				0
#define TEX_TYPE_ALPHA				1
#define TEX_TYPE_LUM				2
#define TEX_TYPE_ALPHA_GRADIENT		3
#define TEX_TYPE_RGBA				4

#define TEX_IS_ALPHA(type)		((type) == TEX_TYPE_ALPHA || (type) == TEX_TYPE_ALPHA_GRADIENT || (type) == TEX_TYPE_RGBA)

void R_TimeRefresh_f( void );
void R_ReadPointFile_f( void );
texture_t* R_TextureAnimation( msurface_t* s );


//====================================================

extern cl_entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern cl_entity_t* currententity;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys;


//
// view origin
//
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

//
// screen size info
//
extern refdef_t	r_refdef;
extern mleaf_t* r_viewleaf, * r_oldviewleaf;
extern texture_t* r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean envmap;
extern	int	currenttexture;
extern	int	cnttextures[2];
extern	int	particletexture;
extern	int	playertextures[16];
extern particle_t* free_particles, *active_particles;

extern	int	skytexturenum;		// index in cl.loadmodel, not gl texture object

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_wadtextures;
extern	cvar_t	r_decals;
extern	cvar_t	mp_decals;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_monolights;
extern	cvar_t	gl_polyoffset;
extern	cvar_t	gl_overbright;
extern	cvar_t	gl_alphamin;
extern	cvar_t	gl_wateramp;
extern	cvar_t	gl_zmax;
extern	cvar_t	gl_spriteblend;
extern	cvar_t	gl_wireframe;
extern	cvar_t	gl_ztrick;
extern	cvar_t	gl_vsync;

extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_lightholes;

extern	cvar_t	gl_envmapsize;
extern	cvar_t	gl_flipmatrix;

extern	int		gl_lightmap_format;

extern	cvar_t	gl_ansio;
extern	cvar_t	gl_max_size;

extern	int		mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean mirror;
extern	mplane_t* mirror_plane;

extern	float	r_world_matrix[16];
extern	float	r_base_world_matrix[16];
extern	float	gProjectionMatrix[16];
extern	float	gWorldToScreen[16];
extern	float	gScreenToWorld[16];

extern float r_avertexnormals[NUMVERTEXNORMALS][3];

extern	const char* gl_vendor;
extern	const char* gl_renderer;
extern	const char* gl_version;
extern	const char* gl_extensions;

extern	qboolean atismoothing;
extern	vec3_t		lightspot;

void R_TranslatePlayerSkin( int playernum );
void GL_Bind( int texnum );

// Multitexture
#define QGL_TEXTURE0_SGIS 0x835E
#define QGL_TEXTURE1_SGIS 0x835F
#define QGL_TEXTURE2_SGIS 0x8360

extern int TEXTURE0_SGIS;
extern int TEXTURE1_SGIS;
extern int TEXTURE2_SGIS;

extern int gl_mtexable;

extern int r_dlightframecount;
extern int r_dlightchanged;
extern int r_dlightactive;

extern qboolean filterMode;
extern float filterColorRed;
extern float filterColorGreen;
extern float filterColorBlue;
extern float filterBrightness;

void GL_DisableMultitexture( void );
void GL_EnableMultitexture( void );
void R_DrawParticles(void);
void R_InitParticles(void);

//
// gl_warp.cpp
//
void GL_SubdivideSurface( msurface_t* fa );
void EmitWaterPolys( msurface_t* fa, int direction );
void R_DrawSkyChain( msurface_t* s );
void R_ClearSkyBox(void);
void R_LoadSkys(void);

//
// gl_draw.cpp
//
int GL_LoadPicTexture( qpic_t* pic, char* pszName );

//
// gl_rmisc.cpp
//
void D_FlushCaches(void);
void R_NewMap( void );

//
// gl_rmain.cpp
//
qboolean R_CullBox( vec_t* mins, vec_t* maxs );
void R_RotateForEntity( vec_t* origin, cl_entity_t* e );
void AllowFog( int allowed );
void R_ForceCVars( qboolean mp );
float CalcFov( float fov_x, float width, float height );

//
// gl_rlight.cpp
//
void R_MarkLights( dlight_t* light, int bit, mnode_t* node );
void R_AnimateLight( void );
void R_RenderDlights( void );
colorVec R_LightPoint( vec_t* p );
colorVec R_LightVec( vec_t* start, vec_t* end );
colorVec RecursiveLightPoint( mnode_t* node, vec3_t start, vec3_t end );
void R_PushDlights(void);

//
// gl_refrag.cpp
//
void R_StoreEfrags( efrag_t** ppefrag );

//
// gl_mesh.cpp
//
void GL_MakeAliasModelDisplayLists( model_t* m, aliashdr_t* hdr );

//
// gl_rsurf.cpp
//
void R_DrawBrushModel( cl_entity_t* e );
void R_DrawWorld( void );
void GL_BuildLightmaps( void );
void R_DecalInit( void );
int DecalListCreate( struct DECALLIST* pList );
void R_MarkLeaves(void);

//
// water shifting
//

typedef struct
{
	int		destcolor[3];
	int		percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

#endif // GLQUAKE_H