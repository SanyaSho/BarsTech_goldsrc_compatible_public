#ifndef ENGINE_CL_PMOVE_H
#define ENGINE_CL_PMOVE_H

msurface_t* SurfaceAtPoint(model_t *pModel, mnode_t *node, vec_t *start, vec_t *end);
const char* PM_CL_TraceTexture( int ground, float* vstart, float* vend );
void PM_ParticleLine(vec_t *start, vec_t *end, int pcolor, float life, float vert);
void PM_DrawBBox(vec_t *mins, vec_t *maxs, vec_t *origin, int pcolor, float life);
void PM_DrawRectangle(vec_t *tl, vec_t *bl, vec_t *tr, vec_t *br, int pcolor, float life);
void CL_SetSolidEntities();
void PM_CL_PlaySound(int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch);
void PM_CL_PlaybackEventFull(int flags, int clientindex, word eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);

#endif //ENGINE_CL_PMOVE_H
