#ifndef ENGINE_DECALS_H
#define ENGINE_DECALS_H

#if defined(GLQUAKE)
#include "gl_draw.h"
#else
#include "draw.h"
#endif

#define MAX_DECALS			4096
#define MAX_DECALNAMES		512
#define DECAL_EXTRASIZE		sizeof(texture_t) - sizeof(miptex_t)

#define FDECAL_PERMANENT			0x01		// This decal should not be removed in favor of any new decals
#define FDECAL_REFERENCE			0x02		// This is a decal that's been moved from another level
#define FDECAL_CUSTOM               0x04        // This is a custom clan logo and should not be saved/restored
#define FDECAL_HFLIP				0x08		// Flip horizontal (U/S) axis
#define FDECAL_VFLIP				0x10		// Flip vertical (V/T) axis
#define FDECAL_CLIPTEST				0x20		// Decal needs to be clip-tested
#define FDECAL_NOCLIP				0x40		// Decal is not clipped by containing polygon
#define FDECAL_DONTSAVE				0x80		// Decal was loaded from adjacent level, don't save out to save file for this level

extern qboolean gfCustomBuild;
extern char szCustName[10];

#include <pshpack1.h>
typedef struct decalname_s
{
	char name[16];
	unsigned char ucFlags;
} decalname_t;
#include <poppack.h>

typedef struct lumplist_s
{
	lumpinfo_t *lump;
	qboolean breplaced;
	lumplist_s *next;
} lumplist_t;

struct DECALLIST
{
	char *name;
	short entityIndex;
	byte depth;
	byte flags;
	vec3_t position;
};

extern cvar_t sp_decals, mp_decals;

extern cachewad_t* menu_wad;
extern cachewad_t custom_wad;
extern char decal_names[MAX_DECALNAMES][16];

// функции для загрузки

void Draw_DecalShutdown();

void Decal_Init();

int Draw_DecalIndex( int id );

int Draw_DecalIndexFromName( char* name );

const char *Draw_DecalName(int number);
int Draw_DecalCount(void);
int Draw_DecalSize(int number);
texture_t *Draw_DecalTexture(int index);
void Draw_CacheWadHandler(cachewad_t* wad, PFNCACHE fn, int extraDataSize);
void Draw_CacheWadInit(char *name, int cacheMax, cachewad_t *wad);
void* Draw_CacheGet(cachewad_t* wad, int index);
int Draw_CacheIndex(cachewad_t* wad, char* path);
void* Draw_CustomCacheGet(cachewad_t* wad, void* raw, int rawsize, int index);
qboolean CustomDecal_Validate(void *raw, int nFileSize);
qboolean CustomDecal_Init(struct cachewad_s *wad, void *raw, int nFileSize, int playernum);
void Draw_DecalSetName(int decal, char *name);

// процедуры для поверхностей
void R_DecalShoot(int textureIndex, int entity, int modelIndex, float * position, int flags);
void R_CustomDecalShoot(texture_t* ptexture, int playernum, int entity, int modelIndex, vec_t* position, int flags);
void R_FireCustomDecal(int textureIndex, int entity, int modelIndex, vec_t* position, int flags, float scale);
void R_DecalRemoveNonPermanent(int textureIndex);
void R_DecalRemoveAll(int textureIndex);
void R_DecalInit(void);

// функции для репликации
int DecalListCreate(DECALLIST* pList);
int DecalListAdd(DECALLIST* pList, int count);

#endif //ENGINE_DECALS_H
