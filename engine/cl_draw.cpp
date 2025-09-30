#include "quakedef.h"
#include "client.h"
#include "cl_draw.h"
#if defined(GLQUAKE)
#include "gl_model.h"
#include "gl_draw.h"
#include "gl_rmisc.h"
#include "vid.h"
#include "cl_tent.h"
#else
#include "model.h"
#include "r_local.h"
#include "draw.h"
#endif
#include "color.h"
#include "r_studio.h"

#define SPR_MAX_SPRITES 256

HSPRITE ghCrosshair = 0;
wrect_t gCrosshairRc = {};
int gCrosshairR = 0;
int gCrosshairG = 0;
int gCrosshairB = 0;

qboolean gSpriteMipMap = TRUE;

static msprite_t* gpSprite = nullptr;

static int gSpriteCount = 0;
static SPRITELIST* gSpriteList = nullptr;

word gSpritePalette[256];

void SetCrosshair(HSPRITE hspr, wrect_t rc, int r, int g, int b)
{
	RecEngSetCrosshair(hspr, rc, r, g, b);

	ghCrosshair = hspr;
	gCrosshairRc = rc;
	gCrosshairR = r;
	gCrosshairG = g;
	gCrosshairB = b;
}

void DrawCrosshair(int x, int y)
{
	if (ghCrosshair == NULL)
		return;

	SPR_Set(ghCrosshair, gCrosshairR, gCrosshairG, gCrosshairB);
	SPR_DrawHoles(0, x - (gCrosshairRc.right - gCrosshairRc.left) / 2, y - (gCrosshairRc.bottom - gCrosshairRc.top) / 2, &gCrosshairRc);
}

void SPR_Init()
{
	if (gSpriteList)
		return;

	ghCrosshair = NULL;
	gSpriteCount = SPR_MAX_SPRITES;
	gSpriteList = reinterpret_cast<SPRITELIST*>( Mem_ZeroMalloc( SPR_MAX_SPRITES * sizeof( SPRITELIST ) ) );
	gpSprite = NULL;
}

void SPR_Shutdown()
{
	int i;

	if (!host_initialized)
		return;

	if (gSpriteList)
	{
		for (i = 0; i < gSpriteCount; i++)
		{
			if (gSpriteList[i].pSprite != NULL)
				Mod_UnloadSpriteTextures(gSpriteList[i].pSprite);

			if (gSpriteList[i].pName != NULL)
				Mem_Free(gSpriteList[i].pName);
		}

		Mem_Free(gSpriteList);
	}

	gpSprite = NULL;
	gSpriteList = NULL;
	gSpriteCount = 0;
	ghCrosshair = 0;
}

void SPR_Shutdown_NoModelFree()
{
	int i;

	if (!host_initialized)
		return;

	if (gSpriteList)
	{
		for (i = 0; i < gSpriteCount; i++)
		{
			if (gSpriteList[i].pName != NULL)
				Mem_Free(gSpriteList[i].pName);
		}

		Mem_Free(gSpriteList);
	}

	gpSprite = NULL;
	gSpriteList = NULL;
	gSpriteCount = 0;
	ghCrosshair = 0;
}

HSPRITE SPR_Load( const char* pTextureName )
{
	RecEngSPR_Load(pTextureName);

	if (!pTextureName || !gSpriteList || gSpriteCount <= 0)
		return 0;

	for (int i = 0; i < gSpriteCount; i++)
	{
		if (gSpriteList[i].pSprite == NULL)
		{
			gSpriteList[i].pName = (char*)Mem_Malloc(Q_strlen((char*)pTextureName) + 1);
			Q_strcpy(gSpriteList[i].pName, (char*)pTextureName);
		}

		if (!Q_strcasecmp(pTextureName, gSpriteList[i].pName))
		{
			gSpriteMipMap = FALSE;
			gSpriteList[i].pSprite = Mod_ForName(pTextureName, 0, 1);
			gSpriteMipMap = TRUE;

			if (gSpriteList[i].pSprite != NULL)
			{
				gSpriteList[i].frameCount = ModelFrameCount(gSpriteList[i].pSprite);
				return i + 1;
			}
		}
	}

	Sys_Error("cannot allocate more than %d HUD sprites\n", SPR_MAX_SPRITES);

	return 0;
}

int SPR_Frames( HSPRITE hSprite )
{
	SPRITELIST* pList;

	RecEngSPR_Frames(hSprite);

	pList = SPR_Get(hSprite);

	if (pList)
		return pList->frameCount;
	return 0;
}

int SPR_Height( HSPRITE hSprite, int frame )
{
	SPRITELIST* pList;
	mspriteframe_t* pFrame;

	RecEngSPR_Height(hSprite, frame);

	pList = SPR_Get(hSprite);

	if (pList)
	{
		pFrame = R_GetSpriteFrame(SPR_Pointer(pList), frame);

		if (pFrame)
			return pFrame->height;
	}
	return 0;
}

int SPR_Width( HSPRITE hSprite, int frame )
{
	SPRITELIST* pList;
	mspriteframe_t* pFrame;

	RecEngSPR_Width(hSprite, frame);

	pList = SPR_Get(hSprite);

	if (pList)
	{
		pFrame = R_GetSpriteFrame(SPR_Pointer(pList), frame);

		if (pFrame)
			return pFrame->width;
	}
	return 0;
}

void UnpackPalette(unsigned short* pDest, word* pSource, int r, int g, int b)
{
#if !defined ( GLQUAKE )
	word* rangeR = &red_64klut[(r * 192) & 0xFF00];
	word* rangeG = &green_64klut[(g * 192) & 0xFF00];
	word* rangeB = &blue_64klut[(b * 192) & 0xFF00];

	for (int i = 0; i < 256; i++)
	{
		pDest[i] = rangeR[((PackedColorVec*)pSource)[i].r] | rangeG[((PackedColorVec*)pSource)[i].g] | rangeB[((PackedColorVec*)pSource)[i].b];
	}
#endif
}

msprite_t* SPR_Pointer(SPRITELIST* pList)
{
	return (msprite_t*)pList->pSprite->cache.data;
}

void SPR_Set( HSPRITE hSprite, int r, int g, int b )
{
	SPRITELIST* pList;

	RecEngSPR_Set(hSprite, r, g, b);

	pList = SPR_Get(hSprite);

	if (pList)
	{
		gpSprite = SPR_Pointer(pList);

		if (gpSprite)
		{
#if defined ( GLQUAKE )
			qglColor4f((GLfloat)r / 255.0, (GLfloat)g / 255.0, (GLfloat)b / 255.0, 1.0);
#else
			UnpackPalette(gSpritePalette, (word*)((byte*)gpSprite + gpSprite->paloffset), r, g, b);
#endif
		}
	}
}

void SPR_EnableScissor( int x, int y, int width, int height )
{
	RecEngSPR_EnableScissor(x, y, width, height);
	EnableScissorTest(x, y, width, height);
}

void SPR_DisableScissor()
{
	RecEngSPR_DisableScissor();
	DisableScissorTest();
}

void SPR_Draw( int frame, int x, int y, const wrect_t* prc )
{
	mspriteframe_t* pFrame;

	RecEngSPR_Draw(frame, x, y, prc);

	if (gpSprite == NULL)
		return;

	if (x >= (int)vid.width || y >= (int)vid.height)
		return;

	pFrame = R_GetSpriteFrame(gpSprite, frame);
	if (pFrame == NULL)
	{
		Con_DPrintf(const_cast<char*>("Client.dll SPR_Draw error:  invalid frame\n"));
		return;
	}

	Draw_SpriteFrame(pFrame, gSpritePalette, x, y, prc);
}

void SPR_DrawHoles( int frame, int x, int y, const wrect_t* prc )
{
	mspriteframe_t* pFrame;

	RecEngSPR_DrawHoles(frame, x, y, prc);

	if (gpSprite == NULL)
		return;

	if (x >= (int)vid.width || y >= (int)vid.height)
		return;

	pFrame = R_GetSpriteFrame(gpSprite, frame);
	if (pFrame == NULL)
	{
		Con_DPrintf(const_cast<char*>("Client.dll SPR_DrawHoles error:  invalid frame\n"));
		return;
	}

	Draw_SpriteFrameHoles(pFrame, gSpritePalette, x, y, prc);
}

void SPR_DrawAdditive( int frame, int x, int y, const wrect_t* prc )
{
	mspriteframe_t* pFrame;

	RecEngSPR_DrawAdditive(frame, x, y, prc);

	if (gpSprite == NULL)
		return;

	if (x >= (int)vid.width || y >= (int)vid.height)
		return;

	pFrame = R_GetSpriteFrame(gpSprite, frame);
	if (pFrame == NULL)
	{
		Con_DPrintf(const_cast<char*>("Client.dll SPR_DrawAdditive error:  invalid frame\n"));
		return;
	}

	Draw_SpriteFrameAdditive(pFrame, gSpritePalette, x, y, prc);
}

void SPR_DrawGeneric( int frame, int x, int y, const wrect_t* prc, int src, int dest, int width, int height )
{
	mspriteframe_t* pFrame;

	RecEngSPR_DrawGeneric(frame, x, y, prc, src, dest, width, height);

	if (gpSprite == NULL)
		return;

	if (x >= (int)vid.width || y >= (int)vid.height)
		return;

	pFrame = R_GetSpriteFrame(gpSprite, frame);
	if (pFrame == NULL)
	{
		Con_DPrintf(const_cast<char*>("Client.dll SPR_DrawGeneric error: invalid frame\n"));
		return;
	}

	Draw_SpriteFrameGeneric(pFrame, gSpritePalette, x, y, prc, src, dest, width, height);
}

client_sprite_t* SPR_GetList( char* psz, int* piCount )
{
	RecEngSPR_GetList( psz, piCount );

	char* pszData = reinterpret_cast<char*>( COM_LoadTempFile( psz, nullptr ) );

	if (pszData == NULL)
		return NULL;

	pszData = COM_Parse(pszData);
	int iNumSprites = atoi(com_token);

	if (iNumSprites == 0)
		return NULL;

	client_sprite_t* pList = reinterpret_cast<client_sprite_t*>(calloc(sizeof(client_sprite_t) * iNumSprites, 1));

	if (pList != NULL)
	{
		client_sprite_t* pSprite = pList;
		for (int i = 0; i < iNumSprites; i++, pSprite++)
		{
			pszData = COM_Parse(pszData);
			Q_strncpy(pSprite->szName, com_token, sizeof(pSprite->szName) - 1);
			pSprite->szName[sizeof(pSprite->szName) - 1] = 0;

			pszData = COM_Parse(pszData);
			pSprite->iRes = atoi(com_token);

			pszData = COM_Parse(pszData);
			Q_strncpy(pSprite->szSprite, com_token, sizeof(pSprite->szSprite) - 1);
			pSprite->szSprite[sizeof(pSprite->szSprite) - 1] = 0;

			pszData = COM_Parse(pszData);
			pSprite->rc.left = atoi(com_token);

			pszData = COM_Parse(pszData);
			pSprite->rc.top = atoi(com_token);

			pszData = COM_Parse(pszData);
			pSprite->rc.right = pSprite->rc.left + atoi(com_token);

			pszData = COM_Parse(pszData);
			pSprite->rc.bottom = pSprite->rc.top + atoi(com_token);
		}

		if (piCount)
			*piCount = iNumSprites;
	}

	return pList;
}

SPRITELIST* SPR_Get( HSPRITE hSprite )
{
	HSPRITE iIndex = hSprite - 1;

	if( iIndex >= 0 && iIndex < gSpriteCount )
		return &gSpriteList[ iIndex ];

	return NULL;
}

model_t* SPR_GetModelPointer( HSPRITE hSprite )
{
	SPRITELIST* pList = SPR_Get( hSprite );

	if( pList )
		return pList->pSprite;

	return NULL;
}

void SetFilterMode( int mode )
{
	RecEngSetFilterMode(mode);
	filterMode = mode;
}

void SetFilterColor( float r, float g, float b )
{
	RecEngSetFilterColor(r, g, b);

	filterColorRed = r;
	filterColorGreen = g;
	filterColorBlue = b;
}

void SetFilterBrightness( float brightness )
{
	RecEngSetFilterBrightness(brightness);

	filterBrightness = brightness;
}
