#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"
#include "render.h"
#include "color.h"
#include "draw.h"
#include "r_studio.h"

static int		sprite_height;
static int		minindex, maxindex;
static sspan_t	*sprite_spans;

void (*spritedraw)(sspan_t* pspan) = D_SpriteDrawSpans;

/*
=====================
D_SpriteDrawSpans
=====================
*/
void D_SpriteDrawSpans(sspan_t* pspan)
{
	int			count, spancount;
	int			izi;
	byte* pbase;
	word *pdest;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, z, du, dv, spancountminus1;
	byte		btemp;
	short* pz;

	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = cacheblock;

	z = (float)INFINITE_DISTANCE / d_ziorigin;
	d_spanzi = (int)(d_ziorigin * (float)ZISCALE);

	izi = d_spanzi;

	do
	{
		pdest = (word*)&d_viewbuffer[sizeof(word) * pspan->u + screenwidth * pspan->v];
		pz = &zspantable[pspan->v][pspan->u];

		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		d_spanzi = (int)(sdivz * z);

		s = d_spanzi + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = (int)(tdivz * z);

		t = d_spanzi + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		spancount = count;

		// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
		// can't step off polygon), clamp, calculate s and t steps across
		// span by division, biasing steps low so we don't run off the
		// texture
		spancountminus1 = (float)(spancount - 1);
		sdivz += d_sdivzstepu * spancountminus1;
		tdivz += d_tdivzstepu * spancountminus1;
		d_spanzi = (int)(sdivz * z);

		snext = d_spanzi + sadjust;
		if (snext > bbextents)
			snext = bbextents;
		else if (snext < 8)
			snext = 8;	// prevent round-off error on <0 steps from
		//  from causing overstepping & running off the
		//  edge of the texture

		d_spanzi = (int)(tdivz * z);

		tnext = d_spanzi + tadjust;
		if (tnext > bbextentt)
			tnext = bbextentt;
		else if (tnext < 8)
			tnext = 8;	// guard against round-off error on <0 steps

		if (spancount > 1)
		{
			sstep = (snext - s) / (spancount - 1);
			tstep = (tnext - t) / (spancount - 1);
		}

		do
		{
			btemp = pbase[(s >> 16) + (t >> 16) * cachewidth];
			if (btemp != 0xFF)
			{
				if (*pz <= izi)
				{
					*pz = izi;
					*pdest = r_palette[btemp];
				}
			}

			pdest++;
			pz++;
			s += sstep;
			t += tstep;
		} while (--spancount > 0);

	NextSpan:
		pspan++;

	} while (pspan->count != DS_SPAN_LIST_END);
	restore_fpu_cw();
}

void D_SpriteDrawSpansTrans(sspan_t* pspan)
{
	int			count, spancount;
	int			izi;
	byte		*pbase;
	word		*pdest;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float		sdivz8stepu, tdivz8stepu, zi8stepu;
	byte		btemp;
	short		*pz;

	set_fpu_cw();

	sstep = 0;
	tstep = 0;
	pbase = cacheblock;

	z = (float)INFINITE_DISTANCE / d_ziorigin;
	d_spanzi = (int)(d_ziorigin * (float)ZISCALE);

	izi = d_spanzi;

	do
	{
		pdest = (word*)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u * sizeof(word));
		pz = &zspantable[pspan->v][pspan->u];

		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;

		d_spanzi = sdivz * z;
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

		spancount = count;

		// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
		// can't step off polygon), clamp, calculate s and t steps across
		// span by division, biasing steps low so we don't run off the
		// texture
		spancountminus1 = (float)(spancount - 1);
		sdivz += d_sdivzstepu * spancountminus1;
		tdivz += d_tdivzstepu * spancountminus1;

		d_spanzi = (int)(sdivz * z);
		snext = (int)(sdivz * z) + sadjust;
		if (snext > bbextents)
			snext = bbextents;
		else if (snext < 8)
			snext = 8;	// prevent round-off error on <0 steps from
		//  from causing overstepping & running off the
		//  edge of the texture

		d_spanzi = (int)(tdivz * z);
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

		do
		{
			btemp = pbase[(s >> 16) + (t >> 16) * cachewidth];
			if (btemp != TRANSPARENT_COLOR && *pz <= izi)
			{
				WORD rblend, gblend, bblend;
				if (is15bit)
				{
					rblend = ScaleToColor(*pdest, r_palette[btemp], mask_r_15, r_blend) & mask_r_15;
					gblend = ScaleToColor(*pdest, r_palette[btemp], mask_g_15, r_blend) & mask_g_15;
				}
				else
				{
					rblend = ScaleToColor(*pdest, r_palette[btemp], mask_r_16, r_blend) & mask_r_16;
					gblend = ScaleToColor(*pdest, r_palette[btemp], mask_g_16, r_blend) & mask_g_16;
				}

				bblend = ScaleToColor(*pdest, r_palette[btemp], mask_b_16, r_blend) & mask_b_16;
				*pdest = rblend | gblend | bblend;
			}

			pdest++;
			pz++;
			s += sstep;
			t += tstep;
		} while (--spancount > 0);

	NextSpan:
		pspan++;

	} while (pspan->count != DS_SPAN_LIST_END);
	restore_fpu_cw();
}

/*
=====================
D_SpriteDrawSpansAdd
=====================
*/
void D_SpriteDrawSpansAdd(sspan_t* pspan)
{
	int			count, spancount;
	int			izi;
	byte* pbase;
	word *pdest;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, z, du, dv, spancountminus1;
	byte		btemp;
	short       color;
	short* pz;

	//    new_cw = old_cw | 0xC00;
	set_fpu_cw();

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = cacheblock;

	z = (float)INFINITE_DISTANCE / d_ziorigin;
	d_spanzi = (int)(d_ziorigin * (float)ZISCALE);

	izi = d_spanzi;

	do
	{
		pdest = (word*)&d_viewbuffer[sizeof(word) * pspan->u + screenwidth * pspan->v];
		pz = &zspantable[pspan->v][pspan->u];

		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv * d_sdivzstepv + du * d_sdivzstepu;
		tdivz = d_tdivzorigin + dv * d_tdivzstepv + du * d_tdivzstepu;
		d_spanzi = (int)(sdivz * z);

		s = d_spanzi + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		d_spanzi = (int)(tdivz * z);

		t = d_spanzi + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		spancount = count;

		// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
		// can't step off polygon), clamp, calculate s and t steps across
		// span by division, biasing steps low so we don't run off the
		// texture
		spancountminus1 = (float)(spancount - 1);
		sdivz += d_sdivzstepu * spancountminus1;
		tdivz += d_tdivzstepu * spancountminus1;
		d_spanzi = (int)(sdivz * z);

		snext = d_spanzi + sadjust;
		if (snext > bbextents)
			snext = bbextents;
		else if (snext < 8)
			snext = 8;	// prevent round-off error on <0 steps from
		//  from causing overstepping & running off the
		//  edge of the texture

		d_spanzi = (int)(tdivz * z);

		tnext = d_spanzi + tadjust;
		if (tnext > bbextentt)
			tnext = bbextentt;
		else if (tnext < 8)
			tnext = 8;	// guard against round-off error on <0 steps

		if (spancount > 1)
		{
			sstep = (snext - s) / (spancount - 1);
			tstep = (tnext - t) / (spancount - 1);
		}

		if (is15bit)
		{
			do
			{
				btemp = pbase[(s >> 16) + (t >> 16) * cachewidth];
				color = r_palette[btemp];
				if (color)
				{
					if (*pz <= izi)
					{
						// mix green in 0..15, red and blue in 16..31
						unsigned int oldcolor = colorsplit(color, mask_r_15, mask_g_15, mask_b_15) + colorsplit(*pdest, mask_r_15, mask_g_15, mask_b_15);

						// flip overflow bits
						unsigned int carrybits = (oldcolor & (1 << 31 | overflow15));
						if (carrybits != 0)
							oldcolor |= (1 << 31 | overflow15) - (carrybits >> 5);	// saturate

						// merge parts and mask it
						*pdest = (oldcolor & 0xFFFF) & mask_g_15 | (oldcolor >> 16) & (mask_r_15 | mask_b_15);
					}
				}

				pdest++;
				pz++;
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
		}
		else
		{
			do
			{
				btemp = pbase[(s >> 16) + (t >> 16) * cachewidth];
				color = r_palette[btemp];
				if (color)
				{
					if (*pz <= izi)
					{
						// mix green in 0..15, red and blue in 16..31
						unsigned int prevcolor = colorsplit(*pdest, mask_r_16, mask_g_16, mask_b_16);
						unsigned int oldcolor = (prevcolor & splitmost16) + colorsplit(color, mask_r_16, mask_g_16, mask_b_16);

						// make bitmask
						unsigned int carrybits = (oldcolor & overflow16 << 1) | prevcolor > oldcolor;

						if (carrybits)
						{
							// move low bit(1) into high pos and shr other bits
							if (carrybits & 1)
							{
								carrybits &= ~1;
								carrybits >>= 1;
								carrybits |= (1 << 31);
							}
							else carrybits >>= 1;

							// flip overflow bits
							oldcolor |= ((carrybits | overflow16) - (carrybits >> 5)) << 1;
						}

						// extract green(6 bits) and merge with high part (5 & 5)
						*pdest = (oldcolor >> 16) & (mask_r_16 | mask_b_16) | oldcolor & mask_g_16;
					}
				}

				pdest++;
				pz++;
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
		}

	NextSpan:
		pspan++;

	} while (pspan->count != DS_SPAN_LIST_END);
	restore_fpu_cw();
}

void D_SpriteDrawSpansGlow(sspan_t* pspan)
{
	int			count, spancount;
	int			izi;
	byte		*pbase;
	word		*pdest;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float		sdivz8stepu, tdivz8stepu, zi8stepu;
	word		btemp;

	set_fpu_cw();

	sstep = 0;
	tstep = 0;
	pbase = cacheblock;

	z = (float)INFINITE_DISTANCE / d_ziorigin;
	d_spanzi = (int)(d_ziorigin * (float)ZISCALE);

	izi = d_spanzi;

	do
	{
		pdest = (word*)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u * sizeof(word));
		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;

		d_spanzi = sdivz * z;
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

		spancount = count;

		// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
		// can't step off polygon), clamp, calculate s and t steps across
		// span by division, biasing steps low so we don't run off the
		// texture
		spancountminus1 = (float)(spancount - 1);
		sdivz += d_sdivzstepu * spancountminus1;
		tdivz += d_tdivzstepu * spancountminus1;

		d_spanzi = (int)(sdivz * z);
		snext = (int)(sdivz * z) + sadjust;
		if (snext > bbextents)
			snext = bbextents;
		else if (snext < 8)
			snext = 8;	// prevent round-off error on <0 steps from
		//  from causing overstepping & running off the
		//  edge of the texture

		d_spanzi = (int)(tdivz * z);
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

		if (is15bit)
		{
			do
			{
				btemp = r_palette[pbase[(s >> 16) + (t >> 16) * cachewidth]];
				if (btemp != 0)
				{
					unsigned int oldcolor = (lowfrac16(btemp, mask_g_15) | highfrac16(btemp, mask_r_15 | mask_b_15)) +
						(lowfrac16(*pdest, mask_g_15) | highfrac16(*pdest, mask_r_15 | mask_b_15));

					unsigned int carrybits = oldcolor & ((1 << 31) | overflow15);
					if (carrybits)
						oldcolor |= ((1 << 31) | overflow15) - (carrybits >> 5); // насыщение цвета

					*pdest = oldcolor & mask_g_15 | (oldcolor >> 16) & (mask_r_15 | mask_b_15);
				}

				pdest++;
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
		}
		else
		{
			do
			{
				btemp = r_palette[pbase[(s >> 16) + (t >> 16) * cachewidth]];
				if (btemp != 0)
				{
					unsigned int prevcolor = *pdest & mask_g_16 | ((*pdest & (mask_r_16 | mask_b_16)) << 16);
					unsigned int oldcolor = (((btemp & (mask_r_16 | mask_b_16)) << 16) | btemp & mask_g_16) + prevcolor;
					unsigned int carrybits = (oldcolor & (overflow16 << 1)) | (prevcolor > oldcolor);

					if (carrybits)
					{
						carrybits = _rotr(carrybits, 1);
						oldcolor |= ((carrybits | overflow16) - (carrybits >> 5)) << 1;
					}

					*pdest = (oldcolor >> 16) & (mask_r_16 | mask_b_16) | oldcolor & mask_g_16;
				}

				pdest++;
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
		}

	NextSpan:
		pspan++;

	} while (pspan->count != DS_SPAN_LIST_END);
	restore_fpu_cw();
}

void D_SpriteDrawSpansAlpha(sspan_t* pspan)
{
	int			count, spancount;
	int			izi;
	byte		*pbase;
	word		*pdest;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float		sdivz8stepu, tdivz8stepu, zi8stepu;
	byte		btemp;
	short		*pz;
	WORD redblue, green;
	DWORD transparent_split;
	int newcolor;
	unsigned int redpart, newcolor15bit;

	redblue = is15bit != 0 ? mask_r_15 | mask_b_15 : mask_r_16 | mask_b_16;
	green = is15bit != 0 ? mask_g_15 : mask_g_16;

	// текстуры спрайтов имеют отличный от текстур брашей (2*4) формат палитры (2*1), 510 оффсет +2 - последний
	transparent_split = lowfrac16(r_palette[255], green) | highfrac16(r_palette[255], redblue);

	redpart = transparent_split & (mask_r_16 << 16);
	newcolor15bit = transparent_split | (overflow15 | (1 << 31));
	newcolor = transparent_split | (overflow16 << 1);

	set_fpu_cw();

	sstep = 0;
	tstep = 0;
	pbase = cacheblock;

	z = (float)INFINITE_DISTANCE / d_ziorigin;
	d_spanzi = (int)(d_ziorigin * (float)ZISCALE);

	izi = d_spanzi;

	do
	{
		pdest = (word*)((byte *)d_viewbuffer + (screenwidth * pspan->v) + pspan->u * sizeof(word));
		pz = &zspantable[pspan->v][pspan->u];

		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;

		d_spanzi = sdivz * z;
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

		spancount = count;

		// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
		// can't step off polygon), clamp, calculate s and t steps across
		// span by division, biasing steps low so we don't run off the
		// texture
		spancountminus1 = (float)(spancount - 1);
		sdivz += d_sdivzstepu * spancountminus1;
		tdivz += d_tdivzstepu * spancountminus1;

		d_spanzi = (int)(sdivz * z);
		snext = (int)(sdivz * z) + sadjust;
		if (snext > bbextents)
			snext = bbextents;
		else if (snext < 8)
			snext = 8;	// prevent round-off error on <0 steps from
		//  from causing overstepping & running off the
		//  edge of the texture

		d_spanzi = (int)(tdivz * z);
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

		if (is15bit)
		{
			do
			{
				btemp = pbase[(s >> 16) + (t >> 16) * cachewidth];
				if (btemp != 0 && *pz <= izi)
				{
					int oldcolor;
					unsigned int deltacolor;
					int blendbits;
					unsigned int signbits;

					blendbits = btemp & 0xF0;
					oldcolor = ((*pdest & redblue) << 16) | *pdest & green;
					unsigned int cyclebits = 256;
					deltacolor = (unsigned int)(newcolor15bit - oldcolor) >> 1;
					signbits = ~deltacolor & ((1 << 31 | overflow15) >> 1);
					do
					{
						cyclebits >>= 1;
						oldcolor &= ~(1 << 31 | overflow15);
						unsigned int diff = signbits | deltacolor & ~((1 << 31 | overflow15) >> 1);
						if ((blendbits & cyclebits) != 0)
						{
							blendbits ^= cyclebits;
							oldcolor += diff;
						}
						deltacolor = diff >> 1;
					} while (blendbits != 0);
					*pdest = (oldcolor >> 16) & redblue | oldcolor & green;
				}

				pdest++;
				pz++;
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
		}
		else
		{
			do
			{
				btemp = pbase[(s >> 16) + (t >> 16) * cachewidth];
				if (btemp != 0 && *pz <= izi)
				{
					int oldcolor;
					unsigned int deltacolor;
					int blendbits;
					unsigned int signbits;

					oldcolor = ((*pdest & redblue) << 16) | *pdest & green;
					deltacolor = (unsigned int)(newcolor - oldcolor) >> 1;

					unsigned int cyclebits = 0b100000000;

					// handle overflow
					if (redpart >= (oldcolor & (mask_r_16 << 16)))
						deltacolor = ((unsigned int)(newcolor - oldcolor) >> 1) | (1 << 31);

					blendbits = btemp & 0xF0;

					signbits = ~deltacolor & (1 << 31 | overflow16);

					do
					{
						cyclebits >>= 1;
						oldcolor &= ~(overflow16 << 1);
						unsigned int diff = signbits | deltacolor & ~(1 << 31 | overflow16);
						if ((cyclebits & blendbits) != 0)
						{
							blendbits ^= cyclebits;
							oldcolor += diff;
						}
						deltacolor = diff >> 1;
					} while (blendbits != 0);

					*pdest = (oldcolor >> 16) & redblue | oldcolor & green;
				}

				pdest++;
				pz++;
				s += sstep;
				t += tstep;
			} while (--spancount > 0);
		}

	NextSpan:
		pspan++;

	} while (pspan->count != DS_SPAN_LIST_END);
	restore_fpu_cw();
}

/*
=====================
D_SpriteScanLeftEdge
=====================
*/
void D_SpriteScanLeftEdge(void)
{
	int			i, v, itop, ibottom, lmaxindex;
	emitpoint_t	*pvert, *pnext;
	sspan_t		*pspan;
	float		du, dv, vtop, vbottom, slope;
	fixed16_t	u, u_step;

	pspan = sprite_spans;
	i = minindex;
	if (i == 0)
		i = r_spritedesc.nump;

	lmaxindex = maxindex;
	if (lmaxindex == 0)
		lmaxindex = r_spritedesc.nump;

	vtop = ceil(r_spritedesc.pverts[i].v);

	do
	{
		pvert = &r_spritedesc.pverts[i];
		pnext = pvert - 1;

		vbottom = ceil(pnext->v);

		if (vtop < vbottom)
		{
			du = pnext->u - pvert->u;
			dv = pnext->v - pvert->v;
			slope = du / dv;
			u_step = (int)(slope * MAX_TEXTURE_PIXELS);
			// adjust u to ceil the integer portion
			u = (int)((pvert->u + (slope * (vtop - pvert->v))) * MAX_TEXTURE_PIXELS) +
				(MAX_TEXTURE_PIXELS - 1);
			itop = (int)vtop;
			ibottom = (int)vbottom;

			for (v = itop; v<ibottom; v++)
			{
				pspan->u = u >> 16;
				pspan->v = v;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;

		i--;
		if (i == 0)
			i = r_spritedesc.nump;

	} while (i != lmaxindex);
}


/*
=====================
D_SpriteScanRightEdge
=====================
*/
void D_SpriteScanRightEdge(void)
{
	int			i, v, itop, ibottom;
	emitpoint_t	*pvert, *pnext;
	sspan_t		*pspan;
	float		du, dv, vtop, vbottom, slope, uvert, unext, vvert, vnext;
	fixed16_t	u, u_step;

	pspan = sprite_spans;
	i = minindex;

	vvert = r_spritedesc.pverts[i].v;
	if (vvert < r_refdef.fvrecty_adj)
		vvert = r_refdef.fvrecty_adj;
	if (vvert > r_refdef.fvrectbottom_adj)
		vvert = r_refdef.fvrectbottom_adj;

	vtop = ceil(vvert);

	do
	{
		pvert = &r_spritedesc.pverts[i];
		pnext = pvert + 1;

		vnext = pnext->v;
		if (vnext < r_refdef.fvrecty_adj)
			vnext = r_refdef.fvrecty_adj;
		if (vnext > r_refdef.fvrectbottom_adj)
			vnext = r_refdef.fvrectbottom_adj;

		vbottom = ceil(vnext);

		if (vtop < vbottom)
		{
			uvert = pvert->u;
			if (uvert < r_refdef.fvrectx_adj)
				uvert = r_refdef.fvrectx_adj;
			if (uvert > r_refdef.fvrectright_adj)
				uvert = r_refdef.fvrectright_adj;

			unext = pnext->u;
			if (unext < r_refdef.fvrectx_adj)
				unext = r_refdef.fvrectx_adj;
			if (unext > r_refdef.fvrectright_adj)
				unext = r_refdef.fvrectright_adj;

			du = unext - uvert;
			dv = vnext - vvert;
			slope = du / dv;
			u_step = (int)(slope * MAX_TEXTURE_PIXELS);
			// adjust u to ceil the integer portion
			u = (int)((uvert + (slope * (vtop - vvert))) * MAX_TEXTURE_PIXELS) +
				(MAX_TEXTURE_PIXELS - 1);
			itop = (int)vtop;
			ibottom = (int)vbottom;

			for (v = itop; v<ibottom; v++)
			{
				pspan->count = (u >> 16) - pspan->u;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;
		vvert = vnext;

		i++;
		if (i == r_spritedesc.nump)
			i = 0;

	} while (i != maxindex);

	pspan->count = DS_SPAN_LIST_END;	// mark the end of the span list 
}


/*
=====================
D_SpriteCalculateGradients
=====================
*/
void D_SpriteCalculateGradients(void)
{
	vec3_t		p_normal, p_saxis, p_taxis, p_temp1;
	float		distinv;

//	TransformVector(r_spritedesc.vpn, p_normal);
	TransformVector(r_spritedesc.vright, p_saxis);
	TransformVector(r_spritedesc.vup, p_taxis);

	VectorScale(p_saxis, 1.0f / r_spritedesc.scale, p_saxis);
	VectorScale(p_taxis, -(1.0f / r_spritedesc.scale), p_taxis);

//	VectorInverse(p_taxis);

	distinv = 1.0 / (-DotProduct(modelorg, r_spritedesc.vpn));

	d_sdivzstepu = p_saxis[0] * xscaleinv;
	d_tdivzstepu = p_taxis[0] * xscaleinv;

	d_sdivzstepv = -p_saxis[1] * yscaleinv;
	d_tdivzstepv = -p_taxis[1] * yscaleinv;

	d_zistepu = 0;// p_normal[0] * xscaleinv * distinv;
	d_zistepv = 0;// -p_normal[1] * yscaleinv * distinv;

	d_sdivzorigin = p_saxis[2] - xcenter * d_sdivzstepu -
		ycenter * d_sdivzstepv;
	d_tdivzorigin = p_taxis[2] - xcenter * d_tdivzstepu -
		ycenter * d_tdivzstepv;
	d_ziorigin = distinv;/* p_normal[2] * distinv - xcenter * d_zistepu -
		ycenter * d_zistepv;*/

	TransformVector(modelorg, p_temp1);

	sadjust = ((fixed16_t)(DotProduct(p_temp1, p_saxis) * MAX_TEXTURE_PIXELS + 0.5)) -
		(-(cachewidth >> 1) << 16);
	tadjust = ((fixed16_t)(DotProduct(p_temp1, p_taxis) * MAX_TEXTURE_PIXELS + 0.5)) -
		(-(sprite_height >> 1) << 16);

	// -1 (-epsilon) so we never wander off the edge of the texture
	bbextents = (cachewidth << 16) - 1;
	bbextentt = (sprite_height << 16) - 1;
}

void D_SpriteSpanSkip(sspan_t* pspan, int skipCount)
{
	sspan_t* cur = pspan;
	int i = skipCount / 2;
	
	if (pspan->count == DS_SPAN_LIST_END)
		return;

	do
	{
		if (i >= skipCount)
		{
			cur->count = 0;
			i = 0;
		}
		else
			i++;

		cur++;
	} while (cur->count != DS_SPAN_LIST_END);
}

/*
=====================
D_DrawSprite
=====================
*/
void D_DrawSprite(void)
{
	int			i, nump;
	float		ymin, ymax;
	emitpoint_t	*pverts;
	sspan_t		spans[MAXHEIGHT + 1];
	sspan_t		*pspan;
	void(*sspandrawer) (sspan_t* pspan);

	sprite_spans = spans;

	// find the top and bottom vertices, and make sure there's at least one scan to
	// draw
	ymin = 999999.9;
	ymax = -999999.9;
	pverts = r_spritedesc.pverts;

	for (i = 0; i<r_spritedesc.nump; i++)
	{
		if (pverts->v < ymin)
		{
			ymin = pverts->v;
			minindex = i;
		}

		if (pverts->v > ymax)
		{
			ymax = pverts->v;
			maxindex = i;
		}

		pverts++;
	}

	ymin = ceil(ymin);
	ymax = ceil(ymax);

	if (ymin >= ymax)
		return;		// doesn't cross any scans at all

	cachewidth = r_spritedesc.pspriteframe->width;
	sprite_height = r_spritedesc.pspriteframe->height;
	cacheblock = (byte *)&r_spritedesc.pspriteframe->pixels[0];

	// copy the first vertex to the last vertex, so we don't have to deal with
	// wrapping
	nump = r_spritedesc.nump;
	pverts = r_spritedesc.pverts;
	pverts[nump] = pverts[0];

	D_SpriteCalculateGradients();
	D_SpriteScanLeftEdge();
	D_SpriteScanRightEdge();

	pspan = sprite_spans;
	sspandrawer = spritedraw;

	if (d_spriteskip.value != 0.0 && spritedraw != D_SpriteDrawSpans)
		D_SpriteSpanSkip(pspan, d_spriteskip.value);

	if (pspan != NULL && pspan->count != DS_SPAN_LIST_END)
		sspandrawer(pspan);
}
