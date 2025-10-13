#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"
#include "color.h"
#include "draw.h"
#include "r_studio.h"
#include "client.h"
#include "pr_cmds.h"
#include "render.h"

short* gWaterTexOld = watertex2;
short* gWaterTex = watertex;

float gWaterFrame;
int gDropFrame;

word gWaterTextureBuffer[CYCLE * CYCLE], gWaterPalette[512];
unsigned int gWaterTextureBuffer32[CYCLE * CYCLE], gWaterPalette32[512];

struct
{
	int sMask;
	int tMask;
	int tShift;
} gTilemap;

void TilingSetup(int sMask, int tMask, int tShift)
{
	gTilemap.sMask = sMask;
	gTilemap.tMask = tMask;
	gTilemap.tShift = tShift;
}

double WaterTextureClear()
{
	gDropFrame = gWaterFrame = cl.time - 0.1;

	return 0.1;
}

void WaterTextureSwap()
{
	short* lpTemp;

	lpTemp = gWaterTexOld;
	gWaterTexOld = gWaterTex;
	gWaterTex = lpTemp;
}

void WaterTextureDrop(int x, int y, int amount)
{
	gWaterTexOld[(x & (CYCLE - 1)) + (y & (CYCLE - 1)) * CYCLE] += amount;
	gWaterTexOld[((x + 1) & (CYCLE - 1)) + (y & (CYCLE - 1)) * CYCLE] += amount >> 2;
	gWaterTexOld[((x - 1) & (CYCLE - 1)) + (y & (CYCLE - 1)) * CYCLE] += amount >> 2;
	gWaterTexOld[(x & (CYCLE - 1)) + ((y + 1) & (CYCLE - 1)) * CYCLE] += amount >> 2;
	gWaterTexOld[(x & (CYCLE - 1)) + ((y - 1) & (CYCLE - 1)) * CYCLE] += amount >> 2;
}

void WaterTexturePalette(short* pPalette, word* pSourcePalette)
{
	for (int i = 0; i < 512; i++)
	{
		if (i < 256)
		{
			if (is15bit)
			{
				pPalette[i] = PACKEDRGB555(
					ScaleToColor(pSourcePalette[10], pSourcePalette[6], 0xffff, i),
					ScaleToColor(pSourcePalette[9], pSourcePalette[5], 0xffff, i),
					ScaleToColor(pSourcePalette[8], pSourcePalette[4], 0xffff, i)
				);
			}
			else
			{
				pPalette[i] = PACKEDRGB565(
					ScaleToColor(pSourcePalette[10], pSourcePalette[6], 0xffff, i),
					ScaleToColor(pSourcePalette[9], pSourcePalette[5], 0xffff, i),
					ScaleToColor(pSourcePalette[8], pSourcePalette[4], 0xffff, i)
				);
			}
		}
		else
		{
			if (is15bit)
			{
				pPalette[i] = PACKEDRGB555(
					ScaleToColor(pSourcePalette[6], pSourcePalette[2], 0xffff, i - 256),
					ScaleToColor(pSourcePalette[5], pSourcePalette[1], 0xffff, i - 256),
					ScaleToColor(pSourcePalette[4], pSourcePalette[0], 0xffff, i - 256)
				);
			}
			else
			{
				pPalette[i] = PACKEDRGB565(
					ScaleToColor(pSourcePalette[6], pSourcePalette[2], 0xffff, i - 256),
					ScaleToColor(pSourcePalette[5], pSourcePalette[1], 0xffff, i - 256),
					ScaleToColor(pSourcePalette[4], pSourcePalette[0], 0xffff, i - 256)
				);
			}
		}
	}

	D_SetFadeColor(pSourcePalette[14], pSourcePalette[13], pSourcePalette[12], pSourcePalette[18]);
}

void WaterTextureSmooth(short* pOldBuffer, short* pNewBuffer)
{
	for (int i = 0; i < CYCLE * CYCLE; i++)
	{
		pNewBuffer[i] = ((pOldBuffer[(i + CYCLE) & ((CYCLE * CYCLE) - 1)] + pOldBuffer[((i + CYCLE) - (CYCLE - 1)) & ((CYCLE * CYCLE) - 1)] +
					pOldBuffer[((i + CYCLE) - (CYCLE + 1)) & ((CYCLE * CYCLE) - 1)] + pOldBuffer[((i + CYCLE) - (CYCLE * 2)) & ((CYCLE * CYCLE) - 1)]) >> 1) - pNewBuffer[i] -
				((((pOldBuffer[(i + CYCLE) & ((CYCLE * CYCLE) - 1)] + pOldBuffer[((i + CYCLE) - (CYCLE - 1)) & ((CYCLE * CYCLE) - 1)] +
					pOldBuffer[((i + CYCLE) - (CYCLE + 1)) & ((CYCLE * CYCLE) - 1)] + pOldBuffer[((i + CYCLE) - (CYCLE * 2)) & ((CYCLE * CYCLE) - 1)]) >> 1) - pNewBuffer[i]) >> 6);
	}
}

/*
=============
WaterTextureUpdate
=============
*/
void WaterTextureUpdate(word * pPalette, float dropTime, texture_t* texture)
{
	float time;
	int amount;
	int widthdeg;
	int i;

	time = cl.time - gWaterFrame;

	if (time < 0)
	{
		time = WaterTextureClear();
	}
	else
	{
		if (time < 0.05)
			time = 0;
	}

	gWaterFrame += time;

	if (time > 0)
	{
		WaterTextureSwap();

		if (gWaterFrame > gDropFrame)
		{
			gDropFrame = gWaterFrame;

			amount = RandomLong(0, 1023);

			WaterTextureDrop(RandomLong(0, 0x7FFF), RandomLong(0, 0x7FFF), amount);
		}

		WaterTextureSmooth(gWaterTex, gWaterTexOld);
	}

	if (cl.waterlevel > 2)
		D_SetFadeColor(pPalette[14], pPalette[13], pPalette[12], pPalette[18]);

	i = 0;

	if (r_pixbytes == 1)
	{
		if (gWaterLastPalette != pPalette)
		{
			for (; i < 256; i++)
			{
				gWaterPalette[i] = hlRGB(pPalette, i);
			}
			i = 1;
			gWaterLastPalette = pPalette;
		}

		widthdeg = Q_log2(texture->width);
		if (time > 0 || i != 0)
		{
			byte* samples = (byte*)texture + texture->offsets[0];
			for (int j = 0; j < CYCLE * CYCLE; j++)
				gWaterTextureBuffer[j] = gWaterPalette[
					samples[
						(((texture->height - 1) & ((j / CYCLE) - (gWaterTexOld[j] >> 4))) << widthdeg)
							+ ((texture->width - 1) & (j + (gWaterTexOld[j] >> 4)))
					]
				];

			// this variant is incorrent but looks better
			/*gWaterTextureBuffer[j] = gWaterPalette[
				samples[
					((texture->height - 1) & ((j / CYCLE) - (gWaterTexOld[j] >> 4)) << widthdeg)
					+ (texture->width - 1) & (j + (gWaterTexOld[j] >> 4))
				]
			];*/
		}

		cacheblock = (pixel_t*)gWaterTextureBuffer;
	}
	else
	{
		if (gWaterLastPalette != pPalette)
		{
			for (; i < 256; i++)
			{
				gWaterPalette32[i] = hlRGB32(pPalette, i);
			}
			i = 1;
			gWaterLastPalette = pPalette;
		}

		widthdeg = Q_log2(texture->width);
		if (time > 0 || i != 0)
		{
			byte* samples = (byte*)texture + texture->offsets[0];
			for (int j = 0; j < CYCLE * CYCLE; j++)
			{
				unsigned int color = gWaterPalette32[
					samples[
						(((texture->height - 1) & ((j / CYCLE) - (gWaterTexOld[j] >> 4))) << widthdeg)
							+ ((texture->width - 1) & (j + (gWaterTexOld[j] >> 4)))
					]
				];

				gWaterTextureBuffer32[j] = 0xFF000000 | PACKEDRGB888(RGB_BLUE888(color), RGB_GREEN888(color), RGB_RED888(color));
			}
		}

		cacheblock = (pixel_t*)gWaterTextureBuffer32;
	}

	cachewidth = CYCLE;

	// 2^7 = CYCLE
	TilingSetup(CYCLE - 1, CYCLE - 1, 7);
}

void D_DrawZSpans32(espan_t* pspan)
{
	int				count, izistep;
	int				izi;
	int* pdest;
	unsigned		ltemp;
	double			zi;
	float			du, dv;

	// FIXME: check for clamping/range problems
	// we count on FP exceptions being turned off to avoid range problems
	izistep = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);

	do
	{
		pdest = &zspantable32[pspan->v][pspan->u];

		count = pspan->count;

		// calculate the initial 1/z
		du = (float)pspan->u;
		dv = (float)pspan->v;

		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		// we count on FP exceptions being turned off to avoid range problems
		izi = (int)(zi * ZISCALE * INFINITE_DISTANCE);

		do
		{
			ltemp = izi;
			izi += izistep;
			*pdest++ = ltemp;
		} while (--count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}

void D_DrawZSpans(espan_t* pspan)
{
	int				count, doublecount, izistep;
	int				izi;
	short* pdest;
	unsigned		ltemp;
	double			zi;
	float			du, dv;
	short* zbuf;

	if (r_pixbytes != 1)
	{
		D_DrawZSpans32(pspan);
		return;
	}

	// FIXME: check for clamping/range problems
	// we count on FP exceptions being turned off to avoid range problems
	izistep = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);

	do
	{
		pdest = &zspantable[pspan->v][pspan->u];

		count = pspan->count;

		// calculate the initial 1/z
		du = (float)pspan->u;
		dv = (float)pspan->v;

		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		// we count on FP exceptions being turned off to avoid range problems
		izi = (int)(zi * ZISCALE * INFINITE_DISTANCE);

		if ((uintptr_t)pdest & 0x02)
		{
			*pdest++ = (short)(izi >> 16);
			izi += izistep;
			count--;
		}

		if ((doublecount = count >> 1) > 0)
		{
			do
			{
				ltemp = izi >> 16;
				izi += izistep;
				ltemp |= izi & 0xFFFF0000;
				izi += izistep;
				*(int*)pdest = ltemp;
				pdest += 2;
			} while (--doublecount > 0);
		}

		if (count & 1)
			*pdest = (short)(izi >> 16);

	} while ((pspan = pspan->pnext) != NULL);
}

void D_DrawTiled32(espan_t* pspan)
{
	int				count;
	fixed16_t		snext, tnext;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz16stepu, tdivz16stepu, zi16stepu;
	unsigned int	* r_turb_pbase, * r_turb_pdest;
	fixed16_t		r_turb_s, r_turb_t, r_turb_sstep, r_turb_tstep;
	int				r_turb_spancount;

	set_fpu_cw();

	r_turb_pbase = (unsigned int*)cacheblock;

	sdivz16stepu = d_sdivzstepu * 16;
	tdivz16stepu = d_tdivzstepu * 16;
	zi16stepu = d_zistepu * 16;

	do
	{
		r_turb_pdest = (unsigned int*)((byte*)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(unsigned int));

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

		r_turb_s = (int)(sdivz * z) + sadjust;
		d_spanzi = (int)(tdivz * z);
		r_turb_t = (int)(tdivz * z) + tadjust;

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 16)
				r_turb_spancount = 16;
			else
				r_turb_spancount = count;

			count -= r_turb_spancount;

			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
			sdivz += sdivz16stepu;
			tdivz += tdivz16stepu;
			zi += zi16stepu;
			z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

			snext = (int)(sdivz * z) + sadjust;
			d_spanzi = (int)(tdivz * z);
			tnext = (int)(tdivz * z) + tadjust;

			r_turb_sstep = (snext - r_turb_s) >> 4;
			r_turb_tstep = (tnext - r_turb_t) >> 4;

			do
			{
				*r_turb_pdest++ = r_turb_pbase[((gTilemap.tMask & (r_turb_t >> 16)) << gTilemap.tShift) + (gTilemap.sMask & (r_turb_s >> 16))];
				r_turb_s += r_turb_sstep;
				r_turb_t += r_turb_tstep;
			} while (--r_turb_spancount > 0);

			r_turb_s = snext;
			r_turb_t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTiled8(espan_t* pspan)
{
	int				count;
	fixed16_t		snext, tnext;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz16stepu, tdivz16stepu, zi16stepu;
	unsigned short* r_turb_pbase, * r_turb_pdest;
	fixed16_t		r_turb_s, r_turb_t, r_turb_sstep, r_turb_tstep;
	int				r_turb_spancount;

	if (r_pixbytes != 1)
	{
		D_DrawTiled32(pspan);
		return;
	}

	set_fpu_cw();

	r_turb_pbase = (unsigned short*)cacheblock;

	sdivz16stepu = d_sdivzstepu * 16;
	tdivz16stepu = d_tdivzstepu * 16;
	zi16stepu = d_zistepu * 16;

	do
	{
		r_turb_pdest = (unsigned short*)((byte*)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(WORD));

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

		r_turb_s = (int)(sdivz * z) + sadjust;
		d_spanzi = (int)(tdivz * z);
		r_turb_t = (int)(tdivz * z) + tadjust;

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 16)
				r_turb_spancount = 16;
			else
				r_turb_spancount = count;

			count -= r_turb_spancount;

			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
			sdivz += sdivz16stepu;
			tdivz += tdivz16stepu;
			zi += zi16stepu;
			z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

			snext = (int)(sdivz * z) + sadjust;
			d_spanzi = (int)(tdivz * z);
			tnext = (int)(tdivz * z) + tadjust;

			r_turb_sstep = (snext - r_turb_s) >> 4;
			r_turb_tstep = (tnext - r_turb_t) >> 4;

			do
			{
				*r_turb_pdest++ = r_turb_pbase[((gTilemap.tMask & (r_turb_t >> 16)) << gTilemap.tShift) + (gTilemap.sMask & (r_turb_s >> 16))];
				r_turb_s += r_turb_sstep;
				r_turb_t += r_turb_tstep;
			} while (--r_turb_spancount > 0);

			r_turb_s = snext;
			r_turb_t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTiled32Trans(espan_t* pspan)
{
	int				izi;
	int				count;
	fixed16_t		snext, tnext;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz16stepu, tdivz16stepu, zi16stepu, izistep;
	unsigned int  *psource, *pdest;
	fixed16_t		s, t, sstep, tstep;
	int				spancount;
	int*			zbuf;

	sdivz16stepu = d_sdivzstepu * 16;
	tdivz16stepu = d_tdivzstepu * 16;
	zi16stepu = d_zistepu * 16;

	// FIXME: check for clamping/range problems
	// we count on FP exceptions being turned off to avoid range problems
	izistep = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);

	do
	{
		pdest = (unsigned int*)((byte*)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(unsigned int));

		zbuf = &zspantable32[pspan->v][pspan->u];

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		t = (int)(tdivz * z) + tadjust;

		if (zi > 0.9f)
		{
			izi = 0x7F000000;
			izistep = 0;
		}
		else
		{
			izi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 16)
				spancount = 16;
			else
				spancount = count;

			count -= spancount;

			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
			sdivz += sdivz16stepu;
			tdivz += tdivz16stepu;
			zi += zi16stepu;
			z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

			snext = (int)(sdivz * z) + sadjust;
			tnext = (int)(tdivz * z) + tadjust;

			sstep = (snext - s) >> 4;
			tstep = (tnext - t) >> 4;

			do
			{
				if (*zbuf < izi)
				{
					psource = (unsigned int*)cacheblock + ((gTilemap.tMask & (t >> 16)) << gTilemap.tShift) + (gTilemap.sMask & (s >> 16));
					
					*pdest = mask_a_32 |
						ScaleToColor(*pdest, *psource, mask_b_32, r_blend) & mask_b_32 |
						ScaleToColor(*pdest, *psource, mask_r_32, r_blend) & mask_r_32 |
						ScaleToColor(*pdest, *psource, mask_g_32, r_blend) & mask_g_32;
				}
				zbuf++;
				pdest++;
				s += sstep;
				t += tstep;
				izi += izistep;
			} while (--spancount > 0);

			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
}

void D_DrawTiled8Trans(espan_t* pspan)
{
	int				izi;
	int				count;
	fixed16_t		snext, tnext;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz16stepu, tdivz16stepu, zi16stepu, izistep;
	unsigned short  *psource, *pdest;
	fixed16_t		s, t, sstep, tstep;
	int				spancount;
	short*			zbuf;

	if (r_pixbytes != 1)
	{
		D_DrawTiled32Trans(pspan);
		return;
	}

	sdivz16stepu = d_sdivzstepu * 16;
	tdivz16stepu = d_tdivzstepu * 16;
	zi16stepu = d_zistepu * 16;

	// FIXME: check for clamping/range problems
	// we count on FP exceptions being turned off to avoid range problems
	izistep = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);

	do
	{
		pdest = (unsigned short*)((byte*)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(WORD));

		zbuf = &zspantable[pspan->v][pspan->u];

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		t = (int)(tdivz * z) + tadjust;

		if (zi > 0.9f)
		{
			izi = 0x7F000000;
			izistep = 0;
		}
		else
		{
			izi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 16)
				spancount = 16;
			else
				spancount = count;

			count -= spancount;

			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
			sdivz += sdivz16stepu;
			tdivz += tdivz16stepu;
			zi += zi16stepu;
			z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

			snext = (int)(sdivz * z) + sadjust;
			tnext = (int)(tdivz * z) + tadjust;

			sstep = (snext - s) >> 4;
			tstep = (tnext - t) >> 4;

			do
			{
				if (*zbuf < izi >> 16)
				{
					word* psource = (word*)cacheblock + ((gTilemap.tMask & (t >> 16)) << gTilemap.tShift) + (gTilemap.sMask & (s >> 16));
					if (is15bit)
						*pdest = ScaleToColor(*pdest, *psource, mask_b_15, r_blend) & mask_b_15 |
						ScaleToColor(*pdest, *psource, mask_r_15, r_blend) & mask_r_15 |
						ScaleToColor(*pdest, *psource, mask_g_15, r_blend) & mask_g_15;
					else
						*pdest = ScaleToColor(*pdest, *psource, mask_b_16, r_blend) & mask_b_16 |
						ScaleToColor(*pdest, *psource, mask_r_16, r_blend) & mask_r_16 |
						ScaleToColor(*pdest, *psource, mask_g_16, r_blend) & mask_g_16;
				}
				zbuf++;
				pdest++;
				s += sstep;
				t += tstep;
				izi += izistep;
			} while (--spancount > 0);

			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
}

void D_DrawSpans32(espan_t* pspan)
{
	int				count, spancount;
	unsigned int* pbase, * pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned int*)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;

	do
	{
		pdest = (unsigned int*)((byte*)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(unsigned int));

		count = pspan->count;

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			do
			{
				*pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth];
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
}

void D_DrawSpans8(espan_t* pspan)
{
	int				count, spancount;
	unsigned short* pbase, * pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;

	if (r_pixbytes != 1)
	{
		D_DrawSpans32(pspan);
		return;
	}

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned short*)cacheblock;
	//if (pbase == NULL) return;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;

	do
	{
		pdest = (unsigned short*)((byte*)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(word));

		count = pspan->count;

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		zi = d_ziorigin + dv * d_zistepv + du * d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			do
			{
				*pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth];
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
}

void D_DrawTranslucentColor32(espan_t *pspan)
{
	int				count = 0, spancount;
	unsigned int	*pdest;
	float zi, du, dv;
	int* zbuf;
	int zstep, zlim;

	set_fpu_cw();

	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned int *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(unsigned int));

		zbuf = &zspantable32[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		// amt - alpha multiplier
		int alpha = currententity->curstate.renderamt;
		color24 entcolor = currententity->curstate.rendercolor;

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			int beta = 255 - alpha;
			
			int new_r = (alpha * entcolor.r) >> 8;
			int new_g = (alpha * entcolor.g) >> 8;
			int new_b = (alpha * entcolor.b) >> 8;

			do
			{
				WORD oldpix = *pdest;
				if (*zbuf < zlim)
				{
					int old_r = (beta * RGB_RED888(*pdest)) >> 8;
					int old_g = (beta * RGB_GREEN888(*pdest)) >> 8;
					int old_b = (beta * RGB_BLUE888(*pdest)) >> 8;

					*pdest = mask_a_32 | PACKEDRGB888(new_r + old_r, new_g + old_g, new_b + old_b);
				}
				zlim += zstep;
				pdest++;
				zbuf++;
			} while (--spancount > 0);
		} while (count > 0);


	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTranslucentColor(espan_t *pspan)
{
	int				count = 0, spancount;
	unsigned short	*pdest;
	float zi, du, dv;
	short* zbuf;
	int zstep, zlim;

	if (r_pixbytes != 1)
	{
		D_DrawTranslucentColor32(pspan);
		return;
	}

	set_fpu_cw();

	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned short *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(word));

		zbuf = &zspantable[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		// amt - alpha multiplier
		int alpha = currententity->curstate.renderamt;
		color24 entcolor = currententity->curstate.rendercolor;

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			int beta = 255 - alpha;
			WORD newpix = blue_64klut[((192 * alpha) & 0xFF00) + entcolor.b] | green_64klut[((192 * alpha) & 0xFF00) + entcolor.g] | red_64klut[((192 * alpha) & 0xFF00) + entcolor.r];
		
			if (is15bit)
			{
				do
				{
					WORD oldpix = *pdest;
					if (*zbuf < zlim >> 16)
						*pdest = newpix + (((beta * (oldpix & 0x1F)) >> 8) & 0x1F | ((beta * (oldpix & 0x3E0)) >> 8) & 0x3E0 | ((beta * (oldpix & 0x7C00)) >> 8) & 0x7C00);
					zlim += zstep;
					pdest++;
					zbuf++;
				} while (--spancount > 0);
			}
			else
			{
				do
				{
					WORD oldpix = *pdest;
					if (*zbuf < zlim >> 16)
						*pdest = newpix + (((beta * (oldpix & 0x1F)) >> 8) & 0x1F | ((beta * (oldpix & 0x7E0)) >> 8) & 0x7E0 | ((beta * (oldpix & 0xF800)) >> 8) & 0xF800);
					zlim += zstep;
					pdest++;
					zbuf++;
				} while (--spancount > 0);
			}
		} while (count > 0);


	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTranslucentTexture32(espan_t *pspan)
{
	int				count, spancount;
	unsigned int	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;
	int* zbuf;
	int zstep, zlim;

	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned int *)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;
	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned int *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(unsigned int));

		count = pspan->count;

		zbuf = &zspantable32[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point
		d_spanzi = z * sdivz;

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = z * tdivz;
		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			
			
			do
			{
				if (*zbuf < zlim)
				{
					unsigned int srcpix = pbase[(s >> 16) + (t >> 16) * cachewidth];

					int src_r = RGB_RED888(srcpix);
					int src_g = RGB_GREEN888(srcpix);
					int src_b = RGB_BLUE888(srcpix);

					int dst_r = RGB_RED888(*pdest);
					int dst_g = RGB_GREEN888(*pdest);
					int dst_b = RGB_BLUE888(*pdest);

					unsigned int blendalpha = r_blend & lowcleanmask(3);

					unsigned int inv_alpha = 255 - blendalpha;

					int r_result = (src_r * blendalpha + dst_r * inv_alpha) / 255;
					int g_result = (src_g * blendalpha + dst_g * inv_alpha) / 255;
					int b_result = (src_b * blendalpha + dst_b * inv_alpha) / 255;

					*pdest = mask_a_32 | PACKEDRGB888(r_result, g_result, b_result);
				}
				pdest++;
				zbuf++;
				s += sstep;
				t += tstep;
				zlim += zstep;
			} while (--spancount > 0);
			
			s = snext;
			t = tnext;
		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

// кастомные рисовалки текстур хранятся в r_trans.cpp
void D_DrawTranslucentTexture(espan_t *pspan)
{
	int				count, spancount;
	unsigned short	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;
	short* zbuf;
	int zstep, zlim;

	if (r_pixbytes != 1)
	{
		D_DrawTranslucentTexture32(pspan);
		return;
	}

	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned short *)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;
	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned short *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(word));

		count = pspan->count;

		zbuf = &zspantable[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point
		d_spanzi = z * sdivz;

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = z * tdivz;
		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			if (is15bit)
			{
				do
				{
					if (*zbuf < zlim >> 16)
					{
						// число бит в неупакованном альфа-канале
						int bitcycle = 1 << 8;

						WORD srcpix = pbase[(s >> 16) + (t >> 16) * cachewidth];
						unsigned int prevcolor = colorsplit(*pdest, mask_r_15, mask_g_15, mask_b_15);
						// rrrrrr____bbbbbb
						// включение в верхнюю часть первого бита из нижней. ошибка оригинала?
						// сравни конечный результат с colorsplit-реализацией
						unsigned int oldcolor = lowfrac16(srcpix, mask_g_15) | highfrac16(srcpix, ~(mask_g_15 & ~(1 << 5)));
						// с макросом colorsplit результат эквивалентный, однако в двоичном коде у битовой маски выше 15 бита единицы
						// такой результат достигается только при помощи побитового отрицания соответствующего набора
										// colorsplit(srcpix, color6bitmax << 10, mask_g_15, color6bitmax << 0);
						unsigned int blendalpha = r_blend & lowcleanmask(3);
						// разница между исходным и текущим цветами
						unsigned int newcolor = ((oldcolor | (overflow15)) - prevcolor) >> 1;
						// поиск переполнения
						unsigned int signbits = ~newcolor & ((overflow15) >> 1);

						// обработка альфа-бит 
						do
						{
							prevcolor &= ~overflow15;
							// смещение контрольного бита
							bitcycle >>= 1;

							// значение исходного цвета с учётом альфа-канала для текущего бита
							unsigned int alphainc = signbits | newcolor & ~((overflow15) >> 1);

							// проверка его наличия в альфа-канале
							if (blendalpha & bitcycle)
							{
								// парсинг бита завершён
								blendalpha ^= bitcycle;
								// смешение цветов с альфа каналом...без операций умножения
								prevcolor += alphainc;
							}
							newcolor = alphainc >> 1;
						} while (blendalpha);

						*pdest = prevcolor & mask_g_15 | (prevcolor >> 16) & (mask_r_15 | mask_b_15);
					}
					pdest++;
					zbuf++;
					s += sstep;
					t += tstep;
					zlim += zstep;
				} while (--spancount > 0);
			}
			else
			{
				do
				{
					if (*zbuf < zlim >> 16)
					{
						int bitcycle = 1 << 8;

						unsigned int prevcolor = colorsplitcustom(*pdest, mask_g_16);
						unsigned int oldcolor = colorsplitcustom(pbase[(s >> 16) + (t >> 16) * cachewidth], mask_g_16);

						// shr 1 to make visible red overflow
						unsigned int newcolor = (((oldcolor | (overflow16)) - prevcolor) >> 1) |
							((oldcolor & mask_r_16 << 16) >= (highfrac16(*pdest, ~mask_g_16) & mask_r_16 << 16) ? (1 << 31) : 0);

						unsigned int blendalpha = r_blend & lowcleanmask(3);
						unsigned int signbits = ~newcolor & (overflow16withred);

						do
						{
							prevcolor &= ~overflow16;
							bitcycle >>= 1;

							unsigned int alphainc = signbits | newcolor & ~(overflow16withred);

							if (blendalpha & bitcycle)
							{
								blendalpha ^= bitcycle;
								prevcolor += alphainc;
							}
							newcolor = alphainc >> 1;
						} while (blendalpha);

						*pdest = prevcolor & mask_g_16 | (prevcolor >> 16) & (mask_r_16 | mask_b_16);
					}
					pdest++;
					zbuf++;
					s += sstep;
					t += tstep;
					zlim += zstep;
				} while (--spancount > 0);
			}
			s = snext;
			t = tnext;
		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTransHoles32(espan_t *pspan)
{
	int				count, spancount;
	unsigned int	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;
	int* zbuf;
	int zstep, zlim;

	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned int *)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;
	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned int *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(unsigned int));

		count = pspan->count;

		zbuf = &zspantable32[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point
		d_spanzi = z * sdivz;

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = z * tdivz;
		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			do
			{
				if (*zbuf < zlim)
				{
					unsigned int src = pbase[(s >> 16) + (t >> 16) * cachewidth];
					if (src != (mask_a_32 | PACKEDRGB888(0, 0, TRANSPARENT_COLOR)))
					{
						*pdest = src;
						*zbuf = zlim;
					}
				}
				pdest++;
				zbuf++;
				s += sstep;
				t += tstep;
				zlim += zstep;
			} while (--spancount > 0);
			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTransHoles(espan_t *pspan)
{
	int				count, spancount;
	unsigned short	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;
	short* zbuf;
	int zstep, zlim;

	if (r_pixbytes != 1)
	{
		D_DrawTransHoles32(pspan);
		return;
	}

	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned short *)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;
	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned short *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(word));

		count = pspan->count;

		zbuf = &zspantable[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point
		d_spanzi = z * sdivz;

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = z * tdivz;
		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			do
			{
				if (*zbuf < zlim >> 16)
				{
					WORD src = pbase[(s >> 16) + (t >> 16) * cachewidth];
					if (src != TRANSPARENT_COLOR16)
					{
						*pdest = src;
						*zbuf = zlim >> 16;
					}
				}
				pdest++;
				zbuf++;
				s += sstep;
				t += tstep;
				zlim += zstep;
			} while (--spancount > 0);
			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTranslucentAdd32(espan_t *pspan)
{
	int				count, spancount;
	unsigned int	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;
	int* zbuf;
	int zstep, zlim;

	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned int *)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;
	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned int *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(unsigned int));

		count = pspan->count;

		zbuf = &zspantable32[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point
		d_spanzi = z * sdivz;

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = z * tdivz;
		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			
			
			do
			{
				if (*zbuf < zlim)
				{
					unsigned int srcpix = pbase[(s >> 16) + (t >> 16) * cachewidth];

					int src_r = RGB_RED888(srcpix);
					int src_g = RGB_GREEN888(srcpix);
					int src_b = RGB_BLUE888(srcpix);

					int dst_r = RGB_RED888(*pdest);
					int dst_g = RGB_GREEN888(*pdest);
					int dst_b = RGB_BLUE888(*pdest);

					unsigned int blendalpha = r_blend & lowcleanmask(4);

					int r_result = (src_r * blendalpha) / 255 + dst_r;
					int g_result = (src_g * blendalpha) / 255 + dst_g;
					int b_result = (src_b * blendalpha) / 255 + dst_b;

					if (r_result > 255 || g_result > 255 || b_result > 255) 
					{
						int max_comp = max(max(r_result, g_result), b_result);
						float scale = 255.0f / (float)max_comp;
						r_result = (int)((float)r_result * scale);
						g_result = (int)((float)g_result * scale);
						b_result = (int)((float)b_result * scale);
					}

					*pdest = mask_a_32 | PACKEDRGB888(r_result, g_result, b_result);
				}
				pdest++;
				zbuf++;
				s += sstep;
				t += tstep;
				zlim += zstep;
			} while (--spancount > 0);
			
			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

void D_DrawTranslucentAdd(espan_t *pspan)
{
	int				count, spancount;
	unsigned short	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;
	short* zbuf;
	int zstep, zlim;

	if (r_pixbytes != 1)
	{
		D_DrawTranslucentAdd32(pspan);
		return;
	}

	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned short *)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;
	d_spanzi = (int)(d_zistepu * ZISCALE * INFINITE_DISTANCE);
	zstep = d_spanzi;

	do
	{
		pdest = (unsigned short *)((byte *)d_viewbuffer +
			(screenwidth * pspan->v) + pspan->u * sizeof(word));

		count = pspan->count;

		zbuf = &zspantable[pspan->v][pspan->u];

		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point
		d_spanzi = z * sdivz;

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = z * tdivz;
		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		if (zi <= 0.9f)
		{
			d_spanzi = (int)(zi * ZISCALE * INFINITE_DISTANCE);
			zlim = d_spanzi;
		}
		else
		{
			zlim = 0x7F000000;
			zstep = 0;
		}

		do
		{
			// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)INFINITE_DISTANCE / zi;	// prescale to 16.16 fixed-point

				d_spanzi = z * sdivz;
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				d_spanzi = z * tdivz;
				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			if (is15bit)
			{
				do
				{
					if (*zbuf < zlim >> 16)
					{
						WORD srcpix = pbase[(s >> 16) + (t >> 16) * cachewidth];
						unsigned int prevcolor = colorsplit(*pdest, mask_r_15, mask_g_15, mask_b_15);
						unsigned int oldcolor = colorsplit(srcpix, mask_r_15, mask_g_15, mask_b_15);
						unsigned int carrybits = 0;

						// пропуск 4 бит
						unsigned int blendalpha = r_blend & lowcleanmask(4);

						// высокие значения альфа-канала не требуют проходов умножения
						if (blendalpha < (BYTE)(MAXBYTE & lowcleanmask(4)))
						{
							unsigned int blendstart = oldcolor >> 1;
							int bitcycle = 1 << 8;
							unsigned int alphainc;
							do
							{
								bitcycle >>= 1;
								alphainc = blendstart & ~((overflow15) >> 1);
								prevcolor &= ~overflow15;

								if (bitcycle & blendalpha)
								{
									prevcolor += alphainc;
									blendalpha ^= bitcycle;
									carrybits |= prevcolor & (overflow15);
								}
								blendstart = alphainc >> 1;
							} while (blendalpha);
						}
						else
						{
							prevcolor = oldcolor + ((lowfrac16(*pdest, mask_g_15) | highfrac16(*pdest, mask_r_15 | mask_b_15)) & ~overflow15);
							carrybits = prevcolor & (overflow15);
						}

						if (carrybits)
							prevcolor |= (overflow15) - (carrybits >> 5);

						*pdest = prevcolor & mask_g_15 | (prevcolor >> 16) & (mask_r_15 | mask_b_15);
					}
					pdest++;
					zbuf++;
					s += sstep;
					t += tstep;
					zlim += zstep;
				} while (--spancount > 0);
			}
			else
			{
				do
				{
					if (*zbuf < zlim >> 16)
					{
						WORD srcpix = pbase[(s >> 16) + (t >> 16) * cachewidth];
						unsigned int prevcolor = colorsplitcustom(*pdest, mask_g_16);
						unsigned int newcolor = colorsplitcustom(srcpix, mask_g_16);
						unsigned int carrybits = 0;

						unsigned int blendalpha = r_blend & lowcleanmask(4);

						if (blendalpha < (BYTE)(MAXBYTE & lowcleanmask(4)))
						{
							unsigned int blendstart = newcolor >> 1;
							int bitcycle = 1 << 8;
							unsigned int alphainc;
							do
							{
								bitcycle >>= 1;
								alphainc = blendstart & ~(overflow16withred);
								prevcolor &= ~overflow16;

								if (bitcycle & blendalpha)
								{
									unsigned int oldcur = prevcolor;
									prevcolor += alphainc;
									blendalpha ^= bitcycle;
									carrybits |= 
										(prevcolor & overflow16) // b & g overflow
										| (oldcur > prevcolor); // r overflow is not suitable for int32 so detect it by comparing
								}
								blendstart = alphainc >> 1;
							} while (blendalpha);
						}
						else
						{
							prevcolor = newcolor + (lowfrac16(*pdest, mask_g_16) | highfrac16(*pdest, ~mask_g_16) & ~overflow16);
							carrybits = (prevcolor & overflow16) | colorsplitcustom(*pdest, mask_g_16) > prevcolor;
						}

						if (carrybits)
						{
							// transform 200801 to 80100400
							carrybits = ROR32(carrybits);
							prevcolor |= ((carrybits | ROR32(overflow16)) - (carrybits >> 5)) << 1; // shl back because we operated with ror-ed values
						}

						*pdest = prevcolor & mask_g_16 | (prevcolor >> 16) & (mask_r_16 | mask_b_16);
					}
					pdest++;
					zbuf++;
					s += sstep;
					t += tstep;
					zlim += zstep;
				} while (--spancount > 0);
			}
			s = snext;
			t = tnext;
		} while (count > 0);
	} while ((pspan = pspan->pnext) != NULL);
	restore_fpu_cw();
}

/*
=============
D_WarpScreen

// this performs a slight compression of the screen at the same time as
// the sine warp, to keep the edges from wrapping
=============
*/
void D_WarpScreen(void)
{
	static word* rowptr[MAXHEIGHT];
	static word* zrowptr[MAXHEIGHT];
	static unsigned int* rowptr32[MAXHEIGHT];
	static int* zrowptr32[MAXHEIGHT];
	static int column[MAXWIDTH];
	static int cached_width = -1;
	static int cached_height = -1;
	static int cache;
	static byte *cached_buffer;

	int w, h;
	int u, v;
	float	wratio, hratio;
	int c1, c2;
	pixel_t *dest;
	pixel_t *dest32;

	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;

	wratio = w / (float)scr_vrect.width;
	hratio = h / (float)scr_vrect.height;

	if (cached_width != scr_vrect.width || cached_height != scr_vrect.height || cached_buffer != d_viewbuffer)
	{
		if (cache != 0)
		{
			cache = 0;
			return;
		}

		cache = 1;

		cached_width = scr_vrect.width;
		cached_height = scr_vrect.height;
		cached_buffer = d_viewbuffer;

		if (r_pixbytes == 1)
		{
			for (v = 0; v < scr_vrect.height; v++)
			{
				c1 = min(r_refdef.vrect.height - 1, (int)((float)v * hratio + 0.5f));
				rowptr[v] = (word*)d_viewbuffer + WARP_WIDTH * (r_refdef.vrect.y + c1);
				zrowptr[v] = (word*)zspantable[r_refdef.vrect.y + c1];
			}
		}
		else
		{
			for (v = 0; v < scr_vrect.height; v++)
			{
				c1 = min(r_refdef.vrect.height - 1, (int)((float)v * hratio + 0.5f));
				rowptr32[v] = (unsigned int*)d_viewbuffer + WARP_WIDTH * (r_refdef.vrect.y + c1);
				zrowptr32[v] = zspantable32[r_refdef.vrect.y + c1];
			}
		}

		for (u = 0; u<scr_vrect.width; u++)
		{
			c2 = min(r_refdef.vrect.width - 1, (int)((float)u * wratio + 0.5f));
			column[u] = r_refdef.vrect.x + c2;
		}
	}

	dest = (pixel_t*) &vid.buffer[scr_vrect.y * vid.rowbytes + scr_vrect.x * 2];
	dest32 = (pixel_t*) &vid.buffer[scr_vrect.y * vid.rowbytes + scr_vrect.x * 4];

	if (d_depth)
	{
		if (r_pixbytes == 1)
		{
			if (is15bit)
			{
				for (int i = 0; i < scr_vrect.height; i++)
				{
					word* pcol = rowptr[i];
					word* zbuf = zrowptr[i];

					for (int j = 0; j < scr_vrect.width; j++)
					{
						int row = min((int)zbuf[column[j]], 63);
						int col = pcol[column[j]];

						// MARK: в листинге нет битовой маски для r-канала

						((word*)&dest[i * vid.rowbytes])[j] =
							LOWORD(d_fogtable[row][RGB_BLUE555(col)].b) |
							LOWORD(d_fogtable[row][RGB_GREEN555(col)].g) |
							LOWORD(d_fogtable[row][RGB_RED555(col)].r);
					}
				}
			}
			else
			{
				for (int i = 0; i < scr_vrect.height; i++)
				{
					word* pcol = rowptr[i];
					word* zbuf = zrowptr[i];

					for (int j = 0; j < scr_vrect.width; j++)
					{
						int row = min((int)zbuf[column[j]], 63);
						int col = pcol[column[j]];

						// MARK: в листинге нет битовой маски для r-канала

						((word*)&dest[i * vid.rowbytes])[j] =
							LOWORD(d_fogtable[row][RGB_BLUE565(col)].b) |
							LOWORD(d_fogtable[row][RGB_GREEN565(col)].g) |
							LOWORD(d_fogtable[row][RGB_RED565(col)].r);
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < scr_vrect.height; i++)
			{
				unsigned int* pcol = rowptr32[i];
				int* zbuf = zrowptr32[i];

				for (int j = 0; j < scr_vrect.width; j++)
				{
					unsigned int row = min((unsigned int)(zbuf[column[j]] >> 16), (unsigned int)63);
					int col = pcol[column[j]];

					((unsigned int*)&dest32[i * vid.rowbytes])[j] = mask_a_32 |
						PACKEDRGB888(d_fogtable32[row][RGB_RED888(col)].r, d_fogtable32[row][RGB_GREEN888(col)].g, d_fogtable32[row][RGB_BLUE888(col)].b);
				}
			}
		}
	}
	else
	{
		if (r_pixbytes == 1)
		{
			if (is15bit)
			{
				for (int i = 0; i < scr_vrect.height; i++)
				{
					word* pcol = rowptr[i];

					// MARK: R_SetVrect гарантирует кратность ширины области отображения 8 и высоты 2
					for (int j = 0; j < scr_vrect.width; j += 4)
					{
						int cols[] = { pcol[column[j + 0]], pcol[column[j + 1]], pcol[column[j + 2]], pcol[column[j + 3]] };
						int row = 0;

						// MARK: в листинге нет битовой маски для r-канала
						for (int k = 0; k < 4; k++)
							((word*)&dest[i * vid.rowbytes])[j + k] =
							LOWORD(d_fogtable[row][RGB_BLUE555(cols[k])].b) |
							LOWORD(d_fogtable[row][RGB_GREEN555(cols[k])].g) |
							LOWORD(d_fogtable[row][RGB_RED555(cols[k])].r);
					}
				}
			}
			else
			{
				for (int i = 0; i < scr_vrect.height; i++)
				{
					word* pcol = rowptr[i];

					for (int j = 0; j < scr_vrect.width; j += 4)
					{
						int cols[] = { pcol[column[j + 0]], pcol[column[j + 1]], pcol[column[j + 2]], pcol[column[j + 3]] };
						int row = 0;

						// MARK: в листинге нет битовой маски для r-канала
						for (int k = 0; k < 4; k++)
							((word*)&dest[i * vid.rowbytes])[j + k] =
							LOWORD(d_fogtable[row][RGB_BLUE565(cols[k])].b) |
							LOWORD(d_fogtable[row][RGB_GREEN565(cols[k])].g) |
							LOWORD(d_fogtable[row][RGB_RED565(cols[k])].r);
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < scr_vrect.height; i++)
			{
				unsigned int* pcol = rowptr32[i];

				for (int j = 0; j < scr_vrect.width; j += 4)
				{
					int cols[] = { pcol[column[j + 0]], pcol[column[j + 1]], pcol[column[j + 2]], pcol[column[j + 3]] };
					int row = 0;

					for (int k = 0; k < 4; k++)
						((unsigned int*)&dest32[i * vid.rowbytes])[j + k] = mask_a_32 |
						PACKEDRGB888(d_fogtable32[row][RGB_RED888(cols[k])].r, d_fogtable32[row][RGB_GREEN888(cols[k])].g, d_fogtable32[row][RGB_BLUE888(cols[k])].b);
				}
			}
		}
	}
}
