#ifndef ENGINE_TEXTURES_H
#define ENGINE_TEXTURES_H

const int TEX_MAX_WADS = 128;
const int TEX_MAP_MIPTEX = 512;
const int TEX_MAX_MIPTEX_NAME = 64;

qboolean TEX_InitFromWad( char* path );

void TEX_CleanupWadInfo();

int TEX_LoadLump( char* name, byte* dest, int destSize );

void TEX_AddAnimatingTextures();

#endif //ENGINE_TEXTURES_H
