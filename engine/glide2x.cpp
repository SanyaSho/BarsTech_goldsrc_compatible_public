#include "winsani_in.h"
#include <windows.h>
#include "winsani_out.h"
#include <cstdint>
#include <quakedef.h>

#define GR_BUFFER_FRONTBUFFER       0

static HINSTANCE glideDLLInst = NULL;

/*
** basic data types
*/
typedef uint32_t  FxU32;
typedef int       FxBool;
typedef float     FxFloat;
typedef double    FxDouble;
typedef uintptr_t FxU;
typedef intptr_t  FxI;

typedef struct GrSstPerfStats_s {
    FxU32  pixelsIn;              /* # pixels processed (minus buffer clears) */
    FxU32  chromaFail;            /* # pixels not drawn due to chroma key */
    FxU32  zFuncFail;             /* # pixels not drawn due to Z comparison */
    FxU32  aFuncFail;             /* # pixels not drawn due to alpha comparison */
    FxU32  pixelsOut;             /* # pixels drawn (including buffer clears) */
} GrSstPerfStats_t;

typedef int (*PFNGRLFBREADREGIONFUNC)(int* src_buffer, int src_x, int src_y, int src_width, int src_height, int dst_stride, void* dst_data);

void LoadGlide( void )
{
    if (!glideDLLInst)
    {
        // get glide module handle
        glideDLLInst = GetModuleHandle("glide2x");
    }
}

FxU32 Glide_Init( void )
{
    void (*grSstPerfStats)(GrSstPerfStats_t*);
    void (*grSstResetPerfStats)();
    void (*grSstIdle)();
    void (*grTriStats)(FxU32*, FxU32*);
    void (*grResetTriStats)();

    LoadGlide();

    if (!glideDLLInst)
        return 0;

    grSstPerfStats = (decltype(grSstPerfStats))GetProcAddress(glideDLLInst, "_grSstPerfStats@4");
    grSstResetPerfStats = (decltype(grSstResetPerfStats))GetProcAddress(glideDLLInst, "_grSstResetPerfStats@0");
    grSstIdle = (decltype(grSstIdle))GetProcAddress(glideDLLInst, "_grSstIdle@0");
    grTriStats = (decltype(grTriStats))GetProcAddress(glideDLLInst, "_grTriStats@8");
    grResetTriStats = (decltype(grResetTriStats))GetProcAddress(glideDLLInst, "_grResetTriStats@0");

    if (!grSstPerfStats || !grSstIdle || !grSstResetPerfStats || !grTriStats || !grResetTriStats)
    {
        return 0;
    }

    grSstIdle();

    GrSstPerfStats_t stats;
    grSstPerfStats(&stats);

    FxU32 trisProcessed;
    grTriStats(&trisProcessed, NULL);

    grSstResetPerfStats();
    grResetTriStats();

    return stats.pixelsIn;
}

qboolean Glide_ReadPixels(int x, int y, int width, int height, byte* pBuffer)
{
    PFNGRLFBREADREGIONFUNC grLfbReadRegion;
    int                        i;
    GLushort* data;

    LoadGlide();

    if (!glideDLLInst)
        return FALSE;

    grLfbReadRegion = (PFNGRLFBREADREGIONFUNC)GetProcAddress(glideDLLInst, "_grLfbReadRegion@28");
    if (!grLfbReadRegion)
        return FALSE;

    if (!grLfbReadRegion(GR_BUFFER_FRONTBUFFER, x, y, width, height, 2 * width, pBuffer))
        return FALSE;

    data = (GLushort*)pBuffer;
    /* we only need to swap bytes */
    for (i = 0; i < width / 2; i++)
    {
        GLushort tmp;

        tmp = data[2 * i];
        data[2 * i] = data[2 * i + 1];
        data[2 * i + 1] = tmp;

        data[2 * i] = (((data[2 * i] & 0x00FF) << 8) | ((data[2 * i] & 0xFF00) >> 8));
        data[2 * i + 1] = (((data[2 * i + 1] & 0x00FF) << 8) | ((data[2 * i + 1] & 0xFF00) >> 8));
    }
#if 0
    /* Fix odd pixels! */
    if (src_width & 1)
    {
        data[src_width - 1] = (((data[src_width - 1] & 0x00FF) << 8) | ((data[src_width - 1] & 0xFF00) >> 8));
    }
#endif

    for (i = 0; i < width; i++)
    {
        int red, green, blue;
        int offset[3] = { 11, 5, 0 };
        GLushort out;

        out = data[i];
        red = (out & 0x001F) >> offset[2];
        green = (out & 0x07E0) >> offset[1];
        blue = (out & 0xF800) >> offset[0];

        /* We wan't it to output: BGR!!! */
        out = 0;
        out |= red << offset[2];
        out |= green << offset[1];
        out |= blue << offset[0];

        data[i] = out;
    }
}