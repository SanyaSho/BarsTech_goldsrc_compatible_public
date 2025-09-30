#include "quakedef.h"
#include "r_triangle.h"
#include "r_shared.h"
#include "render.h"
#include "r_studio.h"
#include "draw.h"
#include "color.h"
#include "pr_cmds.h"
#include "client.h"

#if defined(GLQUAKE)
#include "vgui2/text_draw.h"
#include "glquake.h"
#include "cl_tent.h"

int gRenderMode;
float gGlR, gGlG, gGlB, gGlW;
int g_GL_Modes[7] = { GL_TRIANGLES, GL_TRIANGLE_FAN, GL_QUADS, GL_POLYGON, GL_LINES, GL_TRIANGLE_STRIP, GL_QUAD_STRIP };
BOOL g_bFogSkybox = TRUE;
BOOL g_bUserFogOn;
GLfloat flFogStart;
GLfloat flFogDensity;
GLfloat flFogEnd;
float flFinalFogColor[4];
extern cvar_t gl_fog;

triangleapi_t tri =
{
	TRI_API_VERSION,
	&tri_GL_RenderMode,
	&tri_GL_Begin,
	&tri_GL_End,
	&tri_GL_Color4f,
	&tri_GL_Color4ub,
	&tri_GL_TexCoord2f,
	&tri_GL_Vertex3fv,
	&tri_GL_Vertex3f,
	&tri_GL_Brightness,
	&tri_GL_CullFace,
	&R_TriangleSpriteTexture,
	&tri_ScreenTransform,
	&R_RenderFog,
	&tri_WorldTransform,
	&tri_GetMatrix,
	&tri_BoxinPVS,
	&tri_LightAtPoint,
	&tri_GL_Color4fRendermode,
	&R_FogParams
};
#else

#include "r_local.h"
#include "d_local.h"

triangleapi_t tri =
{
	TRI_API_VERSION,
	&tri_Soft_RenderMode,
	&tri_Soft_Begin,
	&tri_Soft_End,
	&tri_Soft_Color4f,
	&tri_Soft_Color4ub,
	&tri_Soft_TexCoord2f,
	&tri_Soft_Vertex3fv,
	&tri_Soft_Vertex3f,
	&tri_Soft_Brightness,
	&tri_Soft_CullFace,
	&R_TriangleSpriteTexture,
	&tri_ScreenTransform,
	&R_RenderFog,
	&tri_WorldTransform,
	&tri_GetMatrix,
	&tri_BoxinPVS,
	&tri_LightAtPoint,
	&tri_Soft_Color4fRendermode,
	&R_FogParams
};

int gPrimitiveCode;
TRICULLSTYLE gFaceRule;

void R_TriangleSetTexture(void* pixels, word width, word height, word* palette);
#endif

BOOL ScreenTransform(vec_t* point, vec_t* screen)
{
	float w;

#if defined(GLQUAKE)
	screen[0] = gWorldToScreen[0] * point[0] + gWorldToScreen[4] * point[1] + gWorldToScreen[8] * point[2] + gWorldToScreen[12];
	screen[1] = gWorldToScreen[1] * point[0] + gWorldToScreen[5] * point[1] + gWorldToScreen[9] * point[2] + gWorldToScreen[13];
	//	z		 = worldToScreen[2][0] * point[0] + worldToScreen[2][1] * point[1] + worldToScreen[2][2] * point[2] + worldToScreen[2][3];
	w = gWorldToScreen[3] * point[0] + gWorldToScreen[7] * point[1] + gWorldToScreen[11] * point[2] + gWorldToScreen[15];
#else
	vec3_t local;
	VectorSubtract(point, r_origin, local);
	TransformVector(local, screen);

	w = screen[2];
#endif

	if (w == .0f)
		return w <= .0f;

	float invw = 1.0f / w;
	screen[0] *= invw;
	screen[1] *= invw;

	return invw <= .0f;
}

#if defined(GLQUAKE)
void tri_GL_RenderMode(int mode)
{
	switch (mode)
	{
	case kRenderNormal:
		qglDisable(GL_BLEND);
		qglDepthMask(GL_TRUE);
		qglShadeModel(GL_FLAT);
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable(GL_BLEND);
		qglShadeModel(GL_SMOOTH);
		break;
	case kRenderTransAlpha:
		qglEnable(GL_BLEND);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglShadeModel(GL_SMOOTH);
		qglDepthMask(GL_FALSE);
		break;
	case kRenderTransAdd:
		qglBlendFunc(1, GL_ONE);
		qglEnable(GL_BLEND);
		qglDepthMask(GL_FALSE);
		qglShadeModel(GL_SMOOTH);
		break;
	default:
		break;
	}
	
	gRenderMode = mode;
}

void tri_GL_Begin(int primitiveCode)
{
	VGUI2_ResetCurrentTexture();
	qglBegin(g_GL_Modes[primitiveCode]);
}

void tri_GL_End(void)
{
	qglEnd();
}

void tri_GL_Color4f(float x, float y, float z, float w)
{
	if (gRenderMode == kRenderTransAlpha)
	{
		qglColor4ub(255.9f * x, 255.9f * y, 255.9f * z, 255.0f * w);
	}
	else
	{
		qglColor4f(x * w, y * w, z * w, 1.0f);
	}

	gGlR = x;
	gGlG = y;
	gGlB = z;
	gGlW = w;
}

void tri_GL_Color4ub(byte r, byte g, byte b, byte a)
{
	gGlR = (float)r / 255.0f;
	gGlG = (float)g / 255.0f;
	gGlB = (float)b / 255.0f;
	gGlW = (float)a / 255.0f;

	qglColor4f(gGlR, gGlG, gGlB, 1.0f);
}

void tri_GL_TexCoord2f(float u, float v)
{
	qglTexCoord2f(u, v);
}

void tri_GL_Vertex3fv(float* worldPnt)
{
	qglVertex3fv(worldPnt);
}

void tri_GL_Vertex3f(float x, float y, float z)
{
	qglVertex3f(x, y, z);
}

void tri_GL_Brightness(float x)
{
	qglColor4f(gGlR * x * gGlW, gGlG * x * gGlW, gGlB * x * gGlW, 1.0f);
}

void tri_GL_CullFace(TRICULLSTYLE style)
{
	if (style == TRI_FRONT)
	{
		qglEnable(GL_CULL_FACE);
		qglCullFace(GL_FRONT);
		
	}
	else if(style == TRI_NONE)
	{
		qglDisable(GL_CULL_FACE);
	}
}

int R_TriangleSpriteTexture(model_t* pSpriteModel, int frame)
{
	mspriteframe_t* pSpriteFrame;

	VGUI2_ResetCurrentTexture();

	pSpriteFrame = R_GetSpriteFrame((msprite_t*)pSpriteModel->cache.data, frame);

	if (pSpriteFrame == NULL)
		return FALSE;
	
	GL_Bind(pSpriteFrame->gl_texturenum);
	return TRUE;
}

#else
void tri_Soft_RenderMode( int mode )
{

	switch (mode)
	{
	case kRenderNormal:
		polysetdraw = D_PolysetDrawSpans8;
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		polysetdraw = D_PolysetDrawSpansTrans;
		break;
	case kRenderTransAlpha:
		polysetdraw = D_PolysetDrawHole;
		break;
	case kRenderTransAdd:
		polysetdraw = D_PolysetDrawSpansTransAdd;
		break;
	default:
		break;
	}
}

void R_StudioVertBuffer(void)
{
	static finalvert_t finalverts[2049]{};

	// cache align
	pfinalverts = (finalvert_t *)
		(((uintptr_t)&finalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = &auxverts[0];
	plightvalues = &lightvalues[0][0];
}

void tri_Soft_Begin( int primitiveCode )
{
	r_affinetridesc.numtriangles = 0;
	r_anumverts = 0;
	R_StudioVertBuffer();
	gPrimitiveCode = primitiveCode;
	pfinalverts->v[2] = 0;		// s
	pfinalverts->v[3] = 0;		// t
	pfinalverts->v[4] = 0xFFFF;	// l
}

int R_TriangleFakeTexture(float r, float g, float b, float a)
{
	static word fakePal[4];
	static byte fakeTex[1];

	fakePal[0] = (word)(b * a * 255.0f);
	fakePal[1] = (word)(g * a * 255.0f);
	fakePal[2] = (word)(r * a * 255.0f);
	fakePal[2] = 0;

	R_TriangleSetTexture(fakeTex, 1, 1, fakePal);

	return 1;
}

void R_TriangleDraw(int i0, int i1, int i2)
{
	finalvert_t s, t, l;
	int clipflags;
	mtriangle_t mtri;

	s = pfinalverts[i0], t = pfinalverts[i1], l = pfinalverts[i2];

	if (s.flags & t.flags & l.flags & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))
		return;		// completely clipped

	clipflags = s.flags | t.flags | l.flags;
	if ((clipflags & ALIAS_Z_CLIP) == 0)
	{
		r_affinetridesc.numtriangles = 1;
		mtri.facesfront = 1;

		if (gFaceRule == TRI_NONE && ((s.v[0] - l.v[0]) * (s.v[1] - t.v[1]) - (s.v[0] - t.v[0]) * (s.v[1] - l.v[1])) >= 0)
			mtri.vertindex[0] = i2, mtri.vertindex[1] = i1, mtri.vertindex[2] = i0;
		else
			mtri.vertindex[0] = i0, mtri.vertindex[1] = i1, mtri.vertindex[2] = i2;

		if (!((s.flags | t.flags | l.flags) &
			(ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP)))
		{	// totally unclipped
			r_affinetridesc.pfinalverts = pfinalverts;
			r_affinetridesc.ptriangles = &mtri;
			D_PolysetDraw();
		}
		else
		{	// partially clipped
			R_AliasClipTriangle(&mtri);
		}
	}

}

void tri_Soft_End()
{
	switch (gPrimitiveCode)
	{
	case TRI_TRIANGLES:
		for (int i = 0; i < r_anumverts; i+= 3)
			R_TriangleDraw(i, i + 1, i + 2);

		r_anumverts = 0;
		break;
	case TRI_TRIANGLE_FAN:
		for (int i = 0; i < r_anumverts; i += 2)
			R_TriangleDraw(0, i, i + 1);

		r_anumverts = 0;
		break;
	case TRI_QUADS:
		for (int i = 0; i < r_anumverts; i += 4)
		{
			R_TriangleDraw(i, i + 1, i + 2);
			R_TriangleDraw(i, i + 2, i + 3);
		}

		r_anumverts = 0;
		break;
	case TRI_TRIANGLE_STRIP:
		for (int s = 2, t = 1, l = 0; s < r_anumverts; s++)
		{
			R_TriangleDraw(s, t, l);
			if (s < r_anumverts - 1)
			{
				l = s++;
				R_TriangleDraw(s, t, l);
				t = s;
			}
		}
		r_anumverts = 0;
		break;
	case TRI_QUAD_STRIP:
		for (int i = 2; i < r_anumverts; i++)
		{
			R_TriangleDraw(i - 2, i - 1, i);
			R_TriangleDraw(i - 1, i + 1, i);
		}
		r_anumverts = 0;
		break;
	default:
		r_anumverts = 0;
		break;
	}
}

void tri_Soft_Color4f( float r, float g, float b, float a )
{
	if (r_fullbright.value == 1)
	{
		r = g = b = 1;
	}
	else if (filterMode)
	{
		r = filterBrightness * filterColorRed;
		g = filterBrightness * filterColorGreen;
		b = filterBrightness * filterColorBlue;
	}
	r_icolormix.r = (int)(r * 49280.0f) & 0xFF00;
	r_icolormix.g = (int)(g * 49280.0f) & 0xFF00;
	r_icolormix.b = (int)(b * 49280.0f) & 0xFF00;
	r_blend = (int)(a * 255.0f);

}

void tri_Soft_Color4ub( byte r, byte g, byte b, byte a )
{
	r_icolormix.r = (r * 192) & 0xFF00;
	r_icolormix.g = (g * 192) & 0xFF00;
	r_icolormix.b = (b * 192) & 0xFF00;
	r_blend = a;
}

void tri_Soft_TexCoord2f( float u, float v )
{
	if (u < 0.0f || v < 0.0f)
		Con_DPrintf(const_cast<char*>("Underflow: %f %f\n"), u, v);

	pfinalverts[r_anumverts].v[2] = (int)((float)(r_affinetridesc.skinwidth - 1) * u) << 16;
	pfinalverts[r_anumverts].v[3] = (int)((float)(r_affinetridesc.skinheight - 1) * v) << 16;
}

void R_TriangleProjectFinalVert(finalvert_t* fv, vec_t* av)
{
	float zi;

	if (av[2] == 0.0f)
		zi = 0;
	else
		zi = 1.0f / av[2];

	fv->v[5] = (int)(zi * ZISCALE * INFINITE_DISTANCE);
	fv->v[0] = xcenter + xscale * av[0] * zi;
	fv->v[1] = ycenter - yscale * av[1] * zi;
}

void tri_Soft_Vertex3fv(float* worldPnt)
{
	finalvert_t* pfv;
	vec3_t localPnt, screen;
	int tmax, i;
	float fInvAspect, fAspect;

	VectorSubtract(worldPnt, r_origin, localPnt);
	TransformVector(localPnt, screen);
	pfv = &pfinalverts[r_anumverts];
	pfv->flags = 0;

	if (screen[2] < ALIAS_Z_CLIP_PLANE)
	{
		pfv->flags = ALIAS_Z_CLIP;
	}
	else
	{
		R_TriangleProjectFinalVert(pfv, screen);
		if (pfv->v[0] < r_refdef.aliasvrect.x)
			pfv->flags |= ALIAS_LEFT_CLIP;
		if (pfv->v[1] < r_refdef.aliasvrect.y)
			pfv->flags |= ALIAS_TOP_CLIP;
		if (pfv->v[0] > r_refdef.aliasvrectright)
			pfv->flags |= ALIAS_RIGHT_CLIP;
		if (pfv->v[1] > r_refdef.aliasvrectbottom)
			pfv->flags |= ALIAS_BOTTOM_CLIP;
	}
	

	r_anumverts++;

	if (gPrimitiveCode == TRI_QUADS && 			// рисуемая фигура - квадрат
		(r_anumverts & 3) == 0 && 				// число вершин кратно 4
		(pfv->flags & ALIAS_Z_CLIP) == 0)		// последняя вершина видима игроку
	{
		if ((pfinalverts[r_anumverts - 1].flags & ALIAS_Z_CLIP) == 0 && 
			(pfinalverts[r_anumverts - 2].flags & ALIAS_Z_CLIP) == 0 &&
			(pfinalverts[r_anumverts - 3].flags & ALIAS_Z_CLIP) == 0)
		{
			// (height - 1), расположенный в старших 16 битах
			tmax = (r_affinetridesc.skinheight - 1) << 16;
			if (pfinalverts[r_anumverts - 1].v[3] >= tmax)
			{
				pfinalverts[r_anumverts + 3] = pfinalverts[r_anumverts - 1];
				pfinalverts[r_anumverts + 2] = pfinalverts[r_anumverts - 2];

				fAspect = float(tmax - pfinalverts[r_anumverts - 4].v[3]) / float(pfinalverts[r_anumverts - 1].v[3] - pfinalverts[r_anumverts - 4].v[3]);
				fInvAspect = 1.0 - fAspect;

				for (i = 0; i < 6; i++)
				{
					pfinalverts[r_anumverts + 0].v[i] = int((float)pfinalverts[r_anumverts - 4].v[i] * fInvAspect) + int((float)pfinalverts[r_anumverts - 1].v[i] * fAspect);
					pfinalverts[r_anumverts + 1].v[i] = int((float)pfinalverts[r_anumverts - 3].v[i] * fInvAspect) + int((float)pfinalverts[r_anumverts - 2].v[i] * fAspect);
				}

				pfinalverts[r_anumverts - 1] = pfinalverts[r_anumverts];
				pfinalverts[r_anumverts - 2] = pfinalverts[r_anumverts + 1];


				pfv = &pfinalverts[r_anumverts - 2];
				for (i = 0; i < 4; i++, pfv++)
				{
					pfv->flags = 0;

					if (pfv->v[0] < r_refdef.aliasvrect.x)
						pfv->flags |= ALIAS_LEFT_CLIP;
					if (pfv->v[1] < r_refdef.aliasvrect.y)
						pfv->flags |= ALIAS_TOP_CLIP;
					if (pfv->v[0] > r_refdef.aliasvrectright)
						pfv->flags |= ALIAS_RIGHT_CLIP;
					if (pfv->v[1] > r_refdef.aliasvrectbottom)
						pfv->flags |= ALIAS_BOTTOM_CLIP;
				}

				pfinalverts[r_anumverts + 3].v[3] = (pfinalverts[r_anumverts + 3].v[3] - tmax) & 0xffff0000;
				pfinalverts[r_anumverts + 2].v[3] = (pfinalverts[r_anumverts + 2].v[3] - tmax) & 0xffff0000;

				pfinalverts[r_anumverts + 1].v[3] = 0;
				pfinalverts[r_anumverts + 0].v[3] = 0;

				pfinalverts[r_anumverts - 1].v[3] = tmax - 1;
				pfinalverts[r_anumverts - 2].v[3] = tmax - 1;

				r_anumverts += 4;
			}
		}
	}

	pfinalverts[r_anumverts].v[2] = pfinalverts[r_anumverts - 1].v[2];
	pfinalverts[r_anumverts].v[3] = pfinalverts[r_anumverts - 1].v[3];
	pfinalverts[r_anumverts].v[4] = pfinalverts[r_anumverts - 1].v[4];
}

void tri_Soft_Vertex3f( float x, float y, float z )
{
	float world[3] = { x, y, z };
	tri_Soft_Vertex3fv(world);
}

void tri_Soft_Brightness( float brightness )
{
	if (filterMode)
		pfinalverts[r_anumverts].v[4] = filterBrightness * 49280.0f;
	else if (r_fullbright.value == 1)
		pfinalverts[r_anumverts].v[4] = 49280.0f;
	else
		pfinalverts[r_anumverts].v[4] = min((int)(brightness * 49280.0f), MAXWORD);
}

void tri_Soft_CullFace( TRICULLSTYLE style )
{
	gFaceRule = style;
}

void R_TriangleSetTexture(void* pixels, word width, word height, word* palette)
{
	r_affinetridesc.pskin = pixels;
	r_affinetridesc.pskindesc = NULL;

	r_affinetridesc.skinwidth = width;
	r_affinetridesc.seamfixupX16 = (width >> 1) << 16;
	r_affinetridesc.skinheight = height;
	r_palette = palette;
	D_PolysetUpdateTables();
}

int R_TriangleSpriteTexture( model_t *pSpriteModel, int frame )
{
	msprite_t* pSpr = (msprite_t*)pSpriteModel->cache.data;
	mspriteframe_t* pFrame = R_GetSpriteFrame(pSpr, frame);
	if (pFrame == NULL)
		return false;

	R_TriangleSetTexture(pFrame->pixels, pFrame->width, pFrame->height, (word*)((byte*)pSpr + pSpr->paloffset));
	return true;
}
#endif

int tri_ScreenTransform( float* world, float* screen )
{
	return ScreenTransform(world, screen);
}

void R_RenderFinalFog()
{
#if defined(GLQUAKE)
	extern int gD3DMode;
	if (gD3DMode != 1)
	{
		qglEnable(GL_FOG);
		qglFogi(GL_FOG_MODE, GL_EXP2);
		qglFogf(GL_FOG_DENSITY, flFogDensity);
		qglHint(GL_FOG_HINT, GL_NICEST);
		qglFogfv(GL_FOG_COLOR, flFinalFogColor);
		qglFogf(GL_FOG_START, flFogStart);
		qglFogf(GL_FOG_END, flFogEnd);
	}
#endif
}

void R_RenderFog( float* flFogColor, float flStart, float flEnd, int bOn )
{
#if defined(GLQUAKE)
	if (bOn != 0 && gl_fog.value > 0.0)
	{
		g_bUserFogOn = TRUE;
		flFinalFogColor[0] = flFogColor[0] / 255.0f;
		flFinalFogColor[1] = flFogColor[1] / 255.0f;
		flFinalFogColor[2] = flFogColor[2] / 255.0f;
		flFinalFogColor[3] = 1.0f;
		flFogStart = flStart;
		flFogEnd = flEnd;
	}
	else
	{
		g_bUserFogOn = FALSE;
	}
#endif
}

void WorldTransform(vec_t* screen, vec_t* point)
{
#if defined(GLQUAKE)

	point[0] = gWorldToScreen[0] * screen[0] + gWorldToScreen[4] * screen[1] + gWorldToScreen[8] * screen[2] + gWorldToScreen[12];
	point[1] = gWorldToScreen[1] * screen[0] + gWorldToScreen[5] * screen[1] + gWorldToScreen[9] * screen[2] + gWorldToScreen[13];
	point[2] = gWorldToScreen[2] * screen[0] + gWorldToScreen[6] * screen[1] + gWorldToScreen[10] * screen[2] + gWorldToScreen[14];
	float w = gWorldToScreen[3] * screen[0] + gWorldToScreen[7] * screen[1] + gWorldToScreen[11] * screen[2] + gWorldToScreen[15];

	if (w != 0.0f)
	{
		point[0] *= (1.0f / w);
		point[1] *= (1.0f / w);
		point[2] *= (1.0f / w);
	}
#endif
}

void tri_WorldTransform( float* screen, float* world )
{
	WorldTransform(screen, world);
}

void tri_GetMatrix( const int pname, float* matrix )
{
#if defined(GLQUAKE)
	qglGetFloatv(pname, matrix);
#endif
}

int tri_BoxinPVS( float* mins, float* maxs )
{
	return PVSNode(cl.worldmodel->nodes, mins, maxs) != 0;
}

void tri_LightAtPoint( float* vPos, float* value )
{
	colorVec color;
	color = R_LightPoint(vPos);

	value[0] = color.r;
	value[1] = color.g;
	value[2] = color.b;
}

#if !defined(GLQUAKE)
void tri_Soft_Color4fRendermode( float r, float g, float b, float a, int rendermode )
{
	if (r_fullbright.value == 1)
	{
		r = g = b = 1;
	} 
	else if (filterMode)
	{
		r = filterColorRed;
		g = filterColorGreen;
		b = filterColorBlue;
	}

	r_icolormix.r = (int)(r * 49280.0f) & 0xFF00;
	r_icolormix.g = (int)(g * 49280.0f) & 0xFF00;
	r_icolormix.b = (int)(b * 49280.0f) & 0xFF00;

	r_blend = a * 255;
}
#else
void tri_GL_Color4fRendermode(float x, float y, float z, float w, int rendermode)
{
	if (gRenderMode == kRenderTransAlpha)
	{
		gGlW = w / 255.0f;
		qglColor4ub(x, z, y, w);
	}
	else
	{
		qglColor4f(x * w, y * w, z * w, 1.0f);
	}
}
#endif

void R_FogParams( float flDensity, int iFogSkybox )
{
#if defined(GLQUAKE)
	flFogDensity = flDensity;
	g_bFogSkybox = iFogSkybox;
#endif
}
