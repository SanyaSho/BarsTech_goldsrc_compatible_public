#include "quakedef.h"
#include "winquake.h"

#include "vid.h"
#include "d_iface.h"
// софтварная дичь
#include "d_iface.h"
#include "d_local.h"
#include "sound.h"
#include "host.h"
#include "render.h"
#include "color.h"

qboolean scr_skipupdate = false;
qboolean scr_skiponeupdate = false;

int            minimum_memory;
qboolean       is15bit;
int            vid_stretched;

byte* vid_surfcache;
int		vid_surfcachesize;
static int		VID_highhunkmark;

static float s_fXMouseAspectAdjustment = 1.0;
static float s_fYMouseAspectAdjustment = 1.0;

cvar_t gl_vsync = { const_cast<char*>("gl_vsync"), const_cast<char*>("1") };

RECT window_rect = {};

int window_center_x = 0;
int window_center_y = 0;

float GetXMouseAspectRatioAdjustment()
{
	return s_fXMouseAspectAdjustment;
}

float GetYMouseAspectRatioAdjustment()
{
	return s_fYMouseAspectAdjustment;
}

void VID_UpdateWindowVars(RECT* prc, int x, int y)
{
    window_rect = *prc;

    if (pmainwindow)
    {
        GetWindowRect(*((HWND*)pmainwindow), prc);

        window_center_x = (prc->left + prc->right) / 2;
        window_center_y = (prc->top + prc->bottom) / 2;
    }
    else
    {
        window_center_x = x;
        window_center_y = y;
    }
}

/*
================
VID_AllocBuffers
================
*/
bool VID_AllocBuffers(void)
{
    int        tsize, tbuffersize;

    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    vid.colormap = host_colormap;
    vid.fullbright = 256;

    is15bit = vid.is15bit;

    if (r_pixbytes == 1)
        tbuffersize = (vid.width * sizeof(*d_pzbuffer)) * vid.height;
    else
        tbuffersize = (vid.width * sizeof(*d_pzbuffer32)) * vid.height;

    tsize = D_SurfaceCacheForRes(vid.width, vid.height);

    tbuffersize += tsize;

    // see if there's enough memory, allowing for the normal mode 0x13 pixel,
    // z, and surface buffers
    if ((host_parms.memsize - tbuffersize + SURFCACHE_SIZE_AT_320X200 +
        0x10000 * 3) < minimum_memory)
    {
        Con_SafePrintf("Not enough memory for video mode\n");
        return false;        // not enough memory for mode
    }

    vid_surfcachesize = tsize;

    if (r_pixbytes == 1)
    {
        if (d_pzbuffer)
        {
            D_FlushCaches();
            Hunk_FreeToHighMark(VID_highhunkmark);
            d_pzbuffer = NULL;
        }

        VID_highhunkmark = Hunk_HighMark();

        d_pzbuffer = (short*)Hunk_HighAllocName(tbuffersize, const_cast<char*>("video"));

        vid_surfcache = (byte*)(d_pzbuffer + vid.width * vid.height);
    }
    else
    {
        if (d_pzbuffer32)
        {
            D_FlushCaches();
            Hunk_FreeToHighMark(VID_highhunkmark);
            d_pzbuffer32 = NULL;
        }

        VID_highhunkmark = Hunk_HighMark();

        d_pzbuffer32 = (int*)Hunk_HighAllocName(tbuffersize, const_cast<char*>("video"));

        vid_surfcache = (byte*)(d_pzbuffer32 + vid.width * vid.height);
    }

    D_InitCaches();

    return true;
}

int VID_Init(unsigned short* palette)
{
    host_colormap = (unsigned char*)palette;

    if (!VID_AllocBuffers())
	{
        return false;
	}

	S_Init();
    return true;
}

void VID_TakeSnapshot(const char* destfile)
{
    FileHandle_t f = FS_Open(destfile, "wb");
    if (f == nullptr)
        Sys_Error("Couldn't create file for snapshot.\n");
    BITMAPFILEHEADER bmf{};

    bmf.bfType = 0x4D42;
    bmf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmf.bfSize = vid.width * vid.height * 3 + bmf.bfOffBits;

    if (FS_Write(&bmf, sizeof(bmf), 1, f) != sizeof(bmf))
        Sys_Error("Couldn't write file header to snapshot.\n");

    BITMAPINFOHEADER bmi{};
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biWidth = vid.width;
    bmi.biHeight = vid.height;
    bmi.biCompression = BI_RGB;
    bmi.biBitCount = 24;
    bmi.biPlanes = 1;

    if (FS_Write(&bmi, sizeof(bmi), 1, f) != sizeof(bmi))
        Sys_Error("Couldn't write bitmap header to snapshot.\n");

    color24 row[2048];
    for (int i = vid.height - 1; i >= 0 ; i--)
    {
        if (vid.width)
        {
            if (r_pixbytes == 1)
            {
                unsigned short* start = (WORD*)&vid.buffer[i * vid.rowbytes];
                for (int j = 0; j < vid.width; j++)
                {
                    // запись в формате bgr
                    if (is15bit)
                    {
                        row[j].b = (*start >> 7) & 0b11111000;
                        row[j].g = (*start >> 2) & 0b11111000;
                    }
                    else
                    {
                        row[j].b = (*start >> 8) & 0b11111000;
                        row[j].g = (*start >> 3) & 0b11111100;
                    }
                    // синий цвет для обоих форматов выделяется по одной формуле
                    row[j].r = (*start << 3) & 0xFF;
                    start++;
                }
            }
            else
            {
                unsigned int* start = (unsigned int*)&vid.buffer[i * vid.rowbytes];
                for (int j = 0; j < vid.width; j++)
                {
                    row[j].r = RGB_BLUE888(*start);
                    row[j].g = RGB_GREEN888(*start);
                    row[j].b = RGB_RED888(*start);
                    start++;
                }
            }
        }
        // ширина BM всегда кратна 4
        int compatible_size = vid.width;
        if (vid.width & 3)
            compatible_size += 4 - (vid.width & 3);
        if (FS_Write(row, compatible_size * 3, 1, f) != compatible_size * 3)
            Sys_Error("Couldn't write bitmap data snapshot.\n");
    }
    FS_Close(f);
}

void VID_WriteBuffer(const char* destfile)
{
    char format[MAX_PATH];
    static char basefilename[MAX_PATH];
    static int filenumber;
    if (destfile)
    {
        Q_strncpy(basefilename, (char*)destfile, sizeof(basefilename) - 1);
        format[sizeof(format) - 1] = 0;
        filenumber = 1;
    }

    snprintf(format, sizeof(format) - 1, "%s%05d.bmp", basefilename, filenumber);
    format[sizeof(format) - 1] = 0;
    
    VID_TakeSnapshot(format);

    filenumber++;
}