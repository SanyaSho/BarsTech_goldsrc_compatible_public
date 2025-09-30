#ifndef ENGINE_VID_H
#define ENGINE_VID_H

/**
*	@file
*
*	Video driver defs
*/

#include "tier0/platform.h"

#define VID_CBITS	6

enum VidTypes
{
    VT_None = 0,
    VT_Software,
    VT_OpenGL,
    VT_Direct3D,
};

// a pixel can be one, two, or four bytes
typedef byte pixel_t;

struct viddef_t
{
    pixel_t*        buffer;            // invisible rgba
    pixel_t*        colormap;        // 256 * VID_GRADES size
    unsigned short* colormap16;        // 256 * VID_GRADES size
    int                fullbright;        // index of first fullbright color
    int                bits;
    int                is15bit;
    unsigned        rowbytes;        // may be > width if displayed in a window
    unsigned        width;
    unsigned        height;
    float            aspect;            // width / height -- < 0 is taller than wide
    int                numpages;
    int                recalc_refdef;    // if true, recalc vid-based stuff
    pixel_t*        conbuffer;
    int                conrowbytes;
    unsigned        conwidth;
    unsigned        conheight;
    unsigned         maxwarpwidth;
    unsigned         maxwarpheight;
    pixel_t*        direct;            // direct drawing to framebuffer, if not
                                        //  NULL
    VidTypes        vidtype;
};

extern	viddef_t	vid;				// global video state
extern qboolean scr_skipupdate;
extern qboolean scr_skiponeupdate;
extern qboolean       is15bit;
extern RECT window_rect;

#if !defined(GLQUAKE)
extern int vid_stretched;
#endif

extern int window_center_x;
extern int window_center_y;

void VID_WriteBuffer(const char* destfile);
void VID_TakeSnapshot(const char* destfile);

#endif //ENGINE_VID_H
