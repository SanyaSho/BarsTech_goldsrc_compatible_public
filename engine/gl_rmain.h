#ifndef ENGINE_GL_RMAIN_H
#define ENGINE_GL_RMAIN_H

#include "render.h"
#include "mathlib.h"
#include "cl_entity.h"

extern int currenttexture;
extern int			r_framecount;
extern int cnttextures[ 2 ];
extern int			d_lightstylevalue[256];
extern vec3_t		r_plightvec;
extern vec3_t	shadevector;

extern vec3_t	vup;
extern vec3_t	vpn;
extern vec3_t	vright;
extern vec3_t	r_origin;

extern vec3_t r_origin;

extern refdef_t r_refdef;

void AllowFog( bool allowed );
void R_SetStackBase(void);
void R_DrawSpriteModel(cl_entity_t* e);
void R_DrawAliasModel(cl_entity_t* e);
int SignbitsForPlane(mplane_t* out);

#endif //ENGINE_GL_RMAIN_H
