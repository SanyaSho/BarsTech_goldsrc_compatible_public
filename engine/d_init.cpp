// d_init.c: rasterization driver initialization

#include "quakedef.h"
#include "d_local.h"
#include "client.h"

#define NUM_MIPS	4

cvar_t	d_subdiv16 = { const_cast<char*>("d_subdiv16"), const_cast<char*>("1") };
cvar_t	d_mipcap = { const_cast<char*>("d_mipcap"), const_cast<char*>("0") };
cvar_t	d_mipscale = { const_cast<char*>("d_mipscale"), const_cast<char*>("1") };
cvar_t	d_spriteskip = { const_cast<char*>("d_spriteskip"), const_cast<char*>("0"), FCVAR_ARCHIVE };

surfcache_t* d_initial_rover;
qboolean		d_roverwrapped;
int				d_minmip;
float			d_scalemip[NUM_MIPS - 1];

static float	basemip[NUM_MIPS - 1] = { 1.0, 0.5 * 0.8, 0.25 * 0.8 };

extern int			d_aflatcolor;

void (*d_drawspans) (espan_t* pspan);

float d_fadetime, d_fadestart;
int d_fader, d_fadeg, d_fadeb, d_fadelevel, d_depth;

word * gWaterLastPalette;
short watertex[CYCLE * CYCLE], watertex2[CYCLE * CYCLE];

colorVec d_fogtable[64][64];
colorVec d_fogtable32[64][256];

void D_BuildFogTable(qboolean blend)
{
	if (d_fadelevel != 0)
	{
		float time;
		int startFog, minFog, blendFog;
		int startR, startG, startB;
		int rchanged, bchanged, gchanged;

		if (d_fadetime > 0)
			time = max(1.0f - float(cl.time - d_fadestart) / d_fadetime, 0.0f);
		else 
			time = 1.0f;

		int level = d_fadelevel * (int)time;

		if (level == 0)
			d_fadelevel = 0;

		int levelCount;

		if (d_depth)
		{
			levelCount = 64;
			// zero shift right by one?
			startFog = char(d_depth >> 1);
			minFog = 32l;
			blendFog = (d_depth * (512 - startFog)) >> 8;
		}
		else
		{
			levelCount = 1;
			startFog = level;
			minFog = blendFog = 0;
		}

		rchanged = bchanged = gchanged = 0;

		for (int i = 0; i < levelCount; i++)
		{
			if (d_depth != NULL)
				level = clamp(startFog + ((blendFog * (64 - i)) / 64), minFog, 255);

			// 16b warp unit
			if (blend != 0)
			{
				int colorinc = level * (-8), colorinc_g = level * (-4), colorinc32 = -level;
				startR = level * d_fader;
				startG = level * d_fadeg;
				startB = level * d_fadeb;

				if (r_pixbytes == 1)
				{
					if (is15bit)
					{
						for (int j = 0; j < 32; j++)
						{
							d_fogtable[i][j].r = ((j * 8 + ((startR + j * colorinc) >> 8)) >> 3) << 10;
							d_fogtable[i][j].g = ((j * 8 + ((startG + j * colorinc) >> 8)) >> 3) << 5;
							d_fogtable[i][j].b = ((j * 8 + ((startB + j * colorinc) >> 8)) >> 3) << 0;
						}
					}
					else
					{
						for (int j = 0; j < 32; j++)
						{
							d_fogtable[i][j].r = ((j * 8 + ((startR + j * colorinc) >> 8)) >> 3) << 11;
							d_fogtable[i][j].b = ((j * 8 + ((startB + j * colorinc) >> 8)) >> 3) << 0;
						}

						for (int j = 0; j < 64; j++)
						{
							d_fogtable[i][j].g = ((j * 4 + ((startG + j * colorinc_g) >> 8)) >> 2) << 5;
						}
					}
				}
				else
				{
					for (int j = 0; j < 256; j++)
					{
						d_fogtable32[i][j].r = ((j + ((startR + j * colorinc32) >> 8)) & 0xFF);
						d_fogtable32[i][j].g = ((j + ((startG + j * colorinc32) >> 8)) & 0xFF);
						d_fogtable32[i][j].b = ((j + ((startB + j * colorinc32) >> 8)) & 0xFF);
					}
				}
			}
			// zero (debug?) build-mode - warp unit 32b
			else
			{
				startR = (255 * (255 - level) + level * d_fader) >> 8;
				startG = (255 * (255 - level) + level * d_fadeg) >> 8;
				startB = (255 * (255 - level) + level * d_fadeb) >> 8;

				if (r_pixbytes == 1)
				{
					if (is15bit)
					{
						for (int j = 0; j < 32; j++)
						{
							d_fogtable[i][j].r = (((8 * j * startR) >> 8) >> 3) << 10;
							d_fogtable[i][j].g = (((8 * j * startG) >> 8) >> 3) << 5;
							d_fogtable[i][j].b = (((8 * j * startB) >> 8) >> 3) << 0;
						}
					}
					else
					{
						for (int j = 0; j < 32; j++)
						{
							d_fogtable[i][j].r = (((8 * j * startR) >> 8) >> 3) << 11;
							d_fogtable[i][j].b = (((8 * j * startB) >> 8) >> 3) << 0;
						}

						for (int j = 0; j < 64; j++)
						{
							d_fogtable[i][j].g = (((4 * j * startG) >> 8) >> 2) << 5;
						}
					}
				}
				else
				{
					for (int j = 0; j < 256; j++)
					{
						d_fogtable32[i][j].r = (((j * startR) >> 8) & 0xFF);
						d_fogtable32[i][j].g = (((j * startG) >> 8) & 0xFF);
						d_fogtable32[i][j].b = (((j * startB) >> 8) & 0xFF);
					}
				}

				rchanged = gchanged = bchanged = 1;
			}
		}

		if (rchanged)
			d_fader = startR;

		if (gchanged)
			d_fadeg = startG;

		if (bchanged)
			d_fadeb = startB;

		if (d_fadetime <= 0.0f)
			d_fadelevel = 0;
	}
}

void D_SetFadeColor(int r, int g, int b, int fog)
{
	if (cl.sf.fadeEnd <= cl.time && (d_fader != r || d_fadeg != g || d_fadeb != b || fog != d_depth))
	{
		d_fadetime = 0.0;
		d_fadestart = cl.time;
		d_fadelevel = 128;
		d_fader = r;
		d_fadeg = g;
		d_fadeb = b;
		d_depth = fog;

		D_BuildFogTable(1);
	}
}

void D_SetScreenFade(int r, int g, int b, int alpha, int type)
{
	d_fadelevel = alpha;
	d_fadetime = 0.0;
	d_fadestart = cl.time;
	d_depth = 0;
	d_fader = r;
	d_fadeg = g;
	d_fadeb = b;
	D_BuildFogTable(type);
}

void D_InitFade()
{
	d_fadetime = 0.0f;
	d_fadestart = 0.0f;
	d_fadelevel = 128;
	d_fader = 32;
	d_fadeg = 32;
	d_fadeb = 128;
	D_BuildFogTable(1);
}

/*
===============
D_Init
===============
*/
void D_Init(void)
{

//	r_skydirect = 1;

	Cvar_RegisterVariable(&d_mipcap);
	Cvar_RegisterVariable(&d_subdiv16);
	Cvar_RegisterVariable(&d_mipscale);

	r_drawpolys = false;
	r_worldpolysbacktofront = false;
	r_aliasuvscale = 1.0;
	r_recursiveaffinetriangles = true;
	//r_pixbytes = 4;

	D_InitFade();
}

void D_UpdateRects(vrect_t *prect)
{
	
}

/*
===============
D_TurnZOn
===============
*/
void D_TurnZOn(void)
{
	// not needed for software version
}

/*
===============
D_SetupFrame
===============
*/
void D_SetupFrame(void)
{
	int		i;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = vid.buffer;

	if (r_dowarp)
	{
		if (r_pixbytes == 1)
			screenwidth = WARP_WIDTH * sizeof(word);
		else
			screenwidth = WARP_WIDTH * sizeof(unsigned int);
	}
	else
		screenwidth = vid.rowbytes;

	d_roverwrapped = FALSE;
	d_initial_rover = sc_rover;

	d_minmip = d_mipcap.value;
	if (d_minmip > 3)
		d_minmip = 3;
	else if (d_minmip < 0)
		d_minmip = 0;

	for (i = 0; i < (NUM_MIPS - 1); i++)
		d_scalemip[i] = basemip[i] * d_mipscale.value;

	d_drawspans = D_DrawSpans8;

	d_aflatcolor = 0;
}
