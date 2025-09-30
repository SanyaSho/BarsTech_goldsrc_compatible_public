#ifndef ENGINE_RENDER_H
#define ENGINE_RENDER_H

#include "tier0/platform.h"

struct refdef_t
{
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
	// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible 
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably allways be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	color24		ambientlight;

	bool		onlyClientDraws;
};

qboolean LoadTGA2( char *szFilename, byte *buffer, int bufferSize, int *width, int *height, int errFail );
qboolean LoadTGA( char *szFilename, byte *buffer, int bufferSize, int *width, int *height );

extern "C" refdef_t r_refdef;

void R_Init(void);
void R_InitTextures(void);
void R_InitEfrags(void);
void R_RenderView(void);		// must set r_refdef first
void R_ViewChanged(vrect_t* pvrect, int lineadj, float aspect);

BOOL ScreenTransform(vec_t* point, vec_t* screen);

// called whenever r_refdef or vid change

#if !defined(GLQUAKE)
//
// surface cache related
//
extern	int		reinit_surfcache;	// if 1, surface cache is currently empty and
extern qboolean	r_cache_thrash;	// set if thrashing the surface cache

extern byte* vid_surfcache;
extern int		vid_surfcachesize;
extern int filterMode;
extern float filterBrightness, filterColorRed, filterColorGreen, filterColorBlue;

void D_FlushCaches(void);
void D_DeleteSurfaceCache(void);
void D_InitCaches();
void R_SetVrect(vrect_t* pvrect, vrect_t* pvrectin, int lineadj);

float GetXMouseAspectRatioAdjustment();

float GetYMouseAspectRatioAdjustment();

BOOL ScreenTransform(vec_t* point, vec_t* screen);
#endif

#endif //ENGINE_RENDER_H
