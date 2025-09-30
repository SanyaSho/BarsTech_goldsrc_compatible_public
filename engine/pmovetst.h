#ifndef ENGINE_PMOVETST_H
#define ENGINE_PMOVETST_H

#include "pmtrace.h"

int PM_PointContents( vec_t* p, int* truecontents );

int PM_WaterEntity( vec_t* p );

pmtrace_t* PM_TraceLine( float* start, float* end, int flags, int usehull, int ignore_pe );

pmtrace_t PM_PlayerTrace( vec_t* start, vec_t* end, int traceFlags, int ignore_pe );
void PM_InitBoxHull(void);
int PM_TestPlayerPosition(float *pos, pmtrace_t *ptrace);
int PM_TestPlayerPositionEx(vec_t *pos, pmtrace_t *ptrace, int(*pfnIgnore)(physent_t *));
int PM_TruePointContents(float *p);
int PM_HullPointContents(hull_t *hull, int num, vec_t *p);
float PM_TraceModel(physent_t *pEnt, float *start, float *end, trace_t *trace);
int PM_GetModelType(struct model_s *mod);
void* PM_HullForBsp(physent_t *pe, vec_t *offset);
void PM_GetModelBounds(struct model_s *mod, float *mins, float *maxs);
pmtrace_t PM_PlayerTraceEx(vec_t *start, vec_t *end, int traceFlags, int(*pfnIgnore)(physent_t *));
struct pmtrace_s* PM_TraceLineEx(float *start, float *end, int flags, int usehulll, int(*pfnIgnore)(physent_t *pe));

#endif //ENGINE_PMOVETST_H
