#ifndef ENGINE_DETAILTEXTURE_H
#define ENGINE_DETAILTEXTURE_H

void DT_Initialize();
void DT_LoadDetailMapFile(char* levelName);
void DT_LoadDetailTexture(char* diffuseName, int diffuseId);
int DT_SetRenderState(int diffuseId);
void DT_SetTextureCoordinates(float u, float v);
void DT_ClearRenderState();

#endif //ENGINE_DETAILTEXTURE_H
