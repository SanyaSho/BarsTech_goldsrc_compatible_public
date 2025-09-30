#ifndef ENGINE_MODEL_H
#define ENGINE_MODEL_H

#define MODEL_MAX_PVS 1024

void Mod_Init();

void CM_FreePAS();

unsigned char *Mod_DecompressVis(unsigned char *in, model_t *model);
unsigned char *Mod_LeafPVS(mleaf_t *leaf, model_t *model);
void CM_DecompressPVS(unsigned char *in, unsigned char *decompressed, int byteCount);
unsigned char *CM_LeafPVS(int leafnum);
unsigned char *CM_LeafPAS(int leafnum);
void CM_CalcPAS(model_t *pModel);
qboolean CM_HeadnodeVisible(mnode_t *node, unsigned char *visbits, int *first_visible_leafnum);


#endif //ENGINE_MODEL_H
