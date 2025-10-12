#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"
#include "d_iface.h"
#include "r_studio.h"
#include "color.h"
#include "draw.h"
#include "render.h"

// автор оригинальных функций - хуесос

typedef struct {
	int		isflattop;
	int		numleftedges;
	int		*pleftedgevert0;
	int		*pleftedgevert1;
	int		*pleftedgevert2;
	int		numrightedges;
	int		*prightedgevert0;
	int		*prightedgevert1;
	int		*prightedgevert2;
} edgetable;

int			d_aflatcolor;

EXTERN_C
{
int	r_p0[6], r_p1[6], r_p2[6];

byte		*d_pcolormap;

int			d_xdenom;

edgetable	*pedgetable = nullptr;

edgetable	edgetables[12] = {
	{ 0, 1, r_p0, r_p2, NULL, 2, r_p0, r_p1, r_p2 },
	{ 0, 2, r_p1, r_p0, r_p2, 1, r_p1, r_p2, NULL },
	{ 1, 1, r_p0, r_p2, NULL, 1, r_p1, r_p2, NULL },
	{ 0, 1, r_p1, r_p0, NULL, 2, r_p1, r_p2, r_p0 },
	{ 0, 2, r_p0, r_p2, r_p1, 1, r_p0, r_p1, NULL },
	{ 0, 1, r_p2, r_p1, NULL, 1, r_p2, r_p0, NULL },
	{ 0, 1, r_p2, r_p1, NULL, 2, r_p2, r_p0, r_p1 },
	{ 0, 2, r_p2, r_p1, r_p0, 1, r_p2, r_p0, NULL },
	{ 0, 1, r_p1, r_p0, NULL, 1, r_p1, r_p2, NULL },
	{ 1, 1, r_p2, r_p1, NULL, 1, r_p0, r_p1, NULL },
	{ 1, 1, r_p1, r_p0, NULL, 1, r_p2, r_p0, NULL },
	{ 0, 1, r_p0, r_p2, NULL, 1, r_p0, r_p1, NULL },
};

// FIXME: some of these can become statics
int				a_sstepxfrac, a_tstepxfrac, r_lstepx, a_ststepxwhole;
int				r_sstepx, r_tstepx, r_lstepy, r_sstepy, r_tstepy;
int				r_zistepx, r_zistepy;
int				d_aspancount, d_countextrastep;

spanpackage_t			*a_spans;
spanpackage_t			*d_pedgespanpackage;
static int				ystart;
byte					*d_pdest, *d_ptex;
short					*d_pz;
int						d_sfrac, d_tfrac, d_light, d_zi;
int						d_ptexextrastep, d_sfracextrastep;
int						d_tfracextrastep, d_lightextrastep, d_pdestextrastep;
int						d_lightbasestep, d_pdestbasestep, d_ptexbasestep;
int						d_sfracbasestep, d_tfracbasestep;
int						d_ziextrastep, d_zibasestep;
int						d_pzextrastep, d_pzbasestep;

typedef struct {
	int		quotient;
	int		remainder;
} adivtab_t;

static adivtab_t	adivtab[32 * 32] = {
#include "adivtab.h"
};

byte	*skintable[MAX_LBM_HEIGHT];
int		skinwidth;
byte	*skinstart;
};

extern "C" void D_PolysetCalcGradients(int skinwidth);
extern "C" void D_PolysetScanLeftEdge(int height);
extern "C" void D_PolysetSetEdgeTable(void);
extern "C" void D_RasterizeAliasPolySmooth(void);


/*
================
D_PolysetCalcGradients
================
*/
void D_PolysetCalcGradients(int skinwidth)
{
	float	xstepdenominv, ystepdenominv, t0, t1;
	float	p01_minus_p21, p11_minus_p21, p00_minus_p20, p10_minus_p20;

	p00_minus_p20 = r_p0[0] - r_p2[0];
	p01_minus_p21 = r_p0[1] - r_p2[1];
	p10_minus_p20 = r_p1[0] - r_p2[0];
	p11_minus_p21 = r_p1[1] - r_p2[1];

	xstepdenominv = 1.0 / (float)d_xdenom;

	ystepdenominv = -xstepdenominv;

	// ceil () for light so positive steps are exaggerated, negative steps
	// diminished,  pushing us away from underflow toward overflow. Underflow is
	// very visible, overflow is very unlikely, because of ambient lighting
	t0 = r_p0[4] - r_p2[4];
	t1 = r_p1[4] - r_p2[4];
	r_lstepx = (int)
		ceil((t1 * p01_minus_p21 - t0 * p11_minus_p21) * xstepdenominv);
	r_lstepy = (int)
		ceil((t1 * p00_minus_p20 - t0 * p10_minus_p20) * ystepdenominv);

	t0 = r_p0[2] - r_p2[2];
	t1 = r_p1[2] - r_p2[2];
	r_sstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
		xstepdenominv);
	r_sstepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
		ystepdenominv);

	t0 = r_p0[3] - r_p2[3];
	t1 = r_p1[3] - r_p2[3];
	r_tstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
		xstepdenominv);
	r_tstepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
		ystepdenominv);

	t0 = r_p0[5] - r_p2[5];
	t1 = r_p1[5] - r_p2[5];
	r_zistepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
		xstepdenominv);
	r_zistepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
		ystepdenominv);

	a_sstepxfrac = r_sstepx & 0xFFFF;
	a_tstepxfrac = r_tstepx & 0xFFFF;

	a_ststepxwhole = skinwidth * (r_tstepx >> 16) + (r_sstepx >> 16);
}

/*
===================
D_PolysetScanLeftEdge
====================
*/
void D_PolysetScanLeftEdge (int height)
{

	do
	{
		d_pedgespanpackage->pdest = d_pdest;
		d_pedgespanpackage->pz = d_pz;
		d_pedgespanpackage->count = d_aspancount;
		d_pedgespanpackage->ptex = d_ptex;

		d_pedgespanpackage->sfrac = d_sfrac;
		d_pedgespanpackage->tfrac = d_tfrac;

	// FIXME: need to clamp l, s, t, at both ends?
		d_pedgespanpackage->light = d_light;
		d_pedgespanpackage->zi = d_zi;

		d_pedgespanpackage++;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_pdest += d_pdestextrastep;
			d_pz += d_pzextrastep;
			d_aspancount += d_countextrastep;
			d_ptex += d_ptexextrastep;
			d_sfrac += d_sfracextrastep;
			d_ptex += d_sfrac >> 16;

			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracextrastep;
			if (d_tfrac & 0x10000)
			{
				d_ptex += r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}
			d_light += d_lightextrastep;
			d_zi += d_ziextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_pdest += d_pdestbasestep;
			d_pz += d_pzbasestep;
			d_aspancount += ubasestep;
			d_ptex += d_ptexbasestep;
			d_sfrac += d_sfracbasestep;
			d_ptex += d_sfrac >> 16;
			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracbasestep;
			if (d_tfrac & 0x10000)
			{
				d_ptex += r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}
			d_light += d_lightbasestep;
			d_zi += d_zibasestep;
		}
	} while (--height);
}

void D_PolysetDrawFinalVerts(finalvert_t* fv, int numverts)
{
	for (int i = 0; i < numverts; i++)
	{
		if (fv->v[0] >= r_refdef.vrectright)
			continue;

		if (fv->v[1] >= r_refdef.vrectbottom)
			continue;

		short* zbuf = &zspantable[fv->v[1]][fv->v[0]];
		short z = fv->v[5] >> 16;

		if (*zbuf <= z)
		{
			*zbuf = z;

			word* lpdest = (word*)&d_viewbuffer[sizeof(word) * fv->v[0] + d_scantable[fv->v[1]]];
			PackedColorVec* pcolor = &((PackedColorVec*)r_palette)[skintable[fv->v[3] >> 16][fv->v[2] >> 16]];
			short llight = fv->v[4] & 0xFFFF;

			int r = r_lut[pcolor->r + llight];
			int g = r_lut[pcolor->g + llight];
			int b = r_lut[pcolor->b + llight];

			if (is15bit)
				*lpdest = PACKEDRGB555(r, g, b);
			else
				*lpdest = PACKEDRGB565(r, g, b);
		}
	}
}

/*
================
D_PolysetRecursiveTriangle
================
*/
void D_PolysetRecursiveTriangle(int* lp1, int* lp2, int* lp3)
{
	int* temp;
	int		d;
	int		_new[6];
	int		z;
	short* zbuf;

	d = lp2[0] - lp1[0];
	if (d < -1 || d > 1)
		goto split;
	d = lp2[1] - lp1[1];
	if (d < -1 || d > 1)
		goto split;

	d = lp3[0] - lp2[0];
	if (d < -1 || d > 1)
		goto split2;
	d = lp3[1] - lp2[1];
	if (d < -1 || d > 1)
		goto split2;

	d = lp1[0] - lp3[0];
	if (d < -1 || d > 1)
		goto split3;
	d = lp1[1] - lp3[1];
	if (d < -1 || d > 1)
	{
	split3:
		temp = lp1;
		lp1 = lp3;
		lp3 = lp2;
		lp2 = temp;

		goto split;
	}

	return;			// entire tri is filled

split2:
	temp = lp1;
	lp1 = lp2;
	lp2 = lp3;
	lp3 = temp;

split:
	// split this edge
	_new[0] = (lp1[0] + lp2[0]) >> 1;
	_new[1] = (lp1[1] + lp2[1]) >> 1;
	_new[2] = (lp1[2] + lp2[2]) >> 1;
	_new[3] = (lp1[3] + lp2[3]) >> 1;
	_new[5] = (lp1[5] + lp2[5]) >> 1;

	// draw the point if splitting a leading edge
	if (lp2[1] > lp1[1])
		goto nodraw;
	if ((lp2[1] == lp1[1]) && (lp2[0] < lp1[0]))
		goto nodraw;


	z = _new[5] >> 16;
	zbuf = zspantable[_new[1]] + _new[0];
	if (z >= *zbuf)
	{
		int		pix;

		*zbuf = z;
		pix = d_pcolormap[skintable[_new[3] >> 16][_new[2] >> 16]];
		((word*)d_viewbuffer)[d_scantable[_new[1]] + _new[0]] = hlRGB(r_palette, pix);
	}

nodraw:
	// recursively continue
	D_PolysetRecursiveTriangle(lp3, lp1, _new);
	D_PolysetRecursiveTriangle(lp3, _new, lp2);
}

/*
================
D_DrawSubdiv
================
*/
void D_DrawSubdiv(void)
{
	mtriangle_t* ptri;
	finalvert_t* pfv, * index0, * index1, * index2;
	int				i;
	int				lnumtriangles;

	pfv = r_affinetridesc.pfinalverts;
	ptri = r_affinetridesc.ptriangles;
	lnumtriangles = r_affinetridesc.numtriangles;

	for (i = 0; i < lnumtriangles; i++)
	{
		index0 = pfv + ptri[i].vertindex[0];
		index1 = pfv + ptri[i].vertindex[1];
		index2 = pfv + ptri[i].vertindex[2];

		if (((index0->v[1] - index1->v[1]) *
			(index0->v[0] - index2->v[0]) -
			(index0->v[0] - index1->v[0]) *
			(index0->v[1] - index2->v[1])) >= 0)
		{
			continue;
		}

		d_pcolormap = &((byte*)acolormap)[index0->v[4] & 0xFF00];

		if (ptri[i].facesfront)
		{
			D_PolysetRecursiveTriangle(index0->v, index1->v, index2->v);
		}
		else
		{
			int		s0, s1, s2;

			s0 = index0->v[2];
			s1 = index1->v[2];
			s2 = index2->v[2];

			if (index0->flags & ALIAS_ONSEAM)
				index0->v[2] += r_affinetridesc.seamfixupX16;
			if (index1->flags & ALIAS_ONSEAM)
				index1->v[2] += r_affinetridesc.seamfixupX16;
			if (index2->flags & ALIAS_ONSEAM)
				index2->v[2] += r_affinetridesc.seamfixupX16;

			D_PolysetRecursiveTriangle(index0->v, index1->v, index2->v);

			index0->v[2] = s0;
			index1->v[2] = s1;
			index2->v[2] = s2;
		}
	}
}


/*
================
D_DrawNonSubdiv
================
*/
void D_DrawNonSubdiv(void)
{
	mtriangle_t* ptri;
	finalvert_t* pfv, * index0, * index1, * index2;
	int				i;
	int				lnumtriangles;

	pfv = r_affinetridesc.pfinalverts;
	ptri = r_affinetridesc.ptriangles;
	lnumtriangles = r_affinetridesc.numtriangles;

	for (i = 0; i < lnumtriangles; i++, ptri++)
	{
		index0 = pfv + ptri->vertindex[0];
		index1 = pfv + ptri->vertindex[1];
		index2 = pfv + ptri->vertindex[2];

		d_xdenom = (index0->v[1] - index1->v[1]) *
			(index0->v[0] - index2->v[0]) -
			(index0->v[0] - index1->v[0]) * (index0->v[1] - index2->v[1]);

		if (d_xdenom >= 0)
		{
			continue;
		}

		r_p0[0] = index0->v[0];		// u
		r_p0[1] = index0->v[1];		// v
		r_p0[2] = index0->v[2];		// s
		r_p0[3] = index0->v[3];		// t
		r_p0[4] = index0->v[4];		// light
		r_p0[5] = index0->v[5];		// iz

		r_p1[0] = index1->v[0];
		r_p1[1] = index1->v[1];
		r_p1[2] = index1->v[2];
		r_p1[3] = index1->v[3];
		r_p1[4] = index1->v[4];
		r_p1[5] = index1->v[5];

		r_p2[0] = index2->v[0];
		r_p2[1] = index2->v[1];
		r_p2[2] = index2->v[2];
		r_p2[3] = index2->v[3];
		r_p2[4] = index2->v[4];
		r_p2[5] = index2->v[5];

		if (!ptri->facesfront)
		{
			if (index0->flags & ALIAS_ONSEAM)
				r_p0[2] += r_affinetridesc.seamfixupX16;
			if (index1->flags & ALIAS_ONSEAM)
				r_p1[2] += r_affinetridesc.seamfixupX16;
			if (index2->flags & ALIAS_ONSEAM)
				r_p2[2] += r_affinetridesc.seamfixupX16;
		}

		D_PolysetSetEdgeTable();
		D_RasterizeAliasPolySmooth();
	}
}

/*
================
D_PolysetDraw
================
*/
void D_PolysetDraw(void)
{
	spanpackage_t	spans[DPS_MAXSPANS + 1 +
		((CACHE_SIZE - 1) / sizeof(spanpackage_t)) + 1];
	// one extra because of cache line pretouching

	a_spans = (spanpackage_t*)
		(((uintptr_t)&spans[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));

	if (r_affinetridesc.drawtype)
	{
		D_DrawSubdiv();
	}
	else
	{
		D_DrawNonSubdiv();
	}
}

// poly spans
void (*polysetdraw)(spanpackage_t *pspanpackage) = D_PolysetDrawSpans8;

void D_PolysetDrawSpans32(spanpackage_t *pspanpackage)
{
	int		lcount;
	unsigned int *lpdest;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	int		*lpz;
	byte r, g, b;

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			lpdest = (unsigned int*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = (int*)pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				if (lzi >= *lpz)
				{
					int step1_r = (((PackedColorVec*)r_palette)[*lptex].r * ((r_icolormix.r & 0xFF00) >> 8)) / 255;
					int step1_g = (((PackedColorVec*)r_palette)[*lptex].g * ((r_icolormix.g & 0xFF00) >> 8)) / 255;
					int step1_b = (((PackedColorVec*)r_palette)[*lptex].b * ((r_icolormix.b & 0xFF00) >> 8)) / 255;

					int lightcomp = (llight & 0xFF00) >> 8;

					r = (step1_r * lightcomp) / 255;
					g = (step1_g * lightcomp) / 255;
					b = (step1_b * lightcomp) / 255;

					*lpdest = mask_a_32 | PACKEDRGB888(r, g, b);

					*lpz = lzi;
				}
				llight += r_lstepx;
				lzi += r_zistepx;
				lpdest++;
				lpz++;

				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;

				ltfrac += a_tstepxfrac;
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

void D_PolysetDrawSpans8(spanpackage_t *pspanpackage)
{
	int		lcount;
	word	*lpdest;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	short	*lpz;
	byte r, g, b;

	if (r_pixbytes != 1)
	{
		D_PolysetDrawSpans32(pspanpackage);
		return;
	}

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			lpdest = (word*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				// TODO: fix z-buffer | 20.08.2025
				if ((lzi >> 16) >= *lpz)
				{
					r = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].r + r_icolormix.r] + (llight & 0xFF00)];
					g = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].g + r_icolormix.g] + (llight & 0xFF00)];
					b = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].b + r_icolormix.b] + (llight & 0xFF00)];

					*lpdest = is15bit ? PACKEDRGB555(r, g, b) : PACKEDRGB565(r, g, b);

					*lpz = lzi >> 16;
				}
				llight += r_lstepx;
				lzi += r_zistepx;
				lpdest++;
				lpz++;

				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;

				ltfrac += a_tstepxfrac;
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

void D_PolysetDrawHole32(spanpackage_t *pspanpackage)
{
	int		lcount;
	unsigned int *lpdest;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	int		*lpz;
	byte r, g, b;

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			lpdest = (unsigned int*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = (int*)pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				if (lzi >= *lpz && *lptex != TRANSPARENT_COLOR)
				{
					int step1_r = (((PackedColorVec*)r_palette)[*lptex].r * ((r_icolormix.r & 0xFF00) >> 8)) / 255;
					int step1_g = (((PackedColorVec*)r_palette)[*lptex].g * ((r_icolormix.g & 0xFF00) >> 8)) / 255;
					int step1_b = (((PackedColorVec*)r_palette)[*lptex].b * ((r_icolormix.b & 0xFF00) >> 8)) / 255;

					int lightcomp = (llight & 0xFF00) >> 8;

					r = (step1_r * lightcomp) / 255;
					g = (step1_g * lightcomp) / 255;
					b = (step1_b * lightcomp) / 255;

					*lpdest = mask_a_32 | PACKEDRGB888(r, g, b);

					*lpz = lzi;
				}
				llight += r_lstepx;
				lzi += r_zistepx;
				lpdest++;
				lpz++;

				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;

				ltfrac += a_tstepxfrac;
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

void D_PolysetDrawHole(spanpackage_t *pspanpackage)
{
	int		lcount;
	word	*lpdest;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	short	*lpz;
	byte r, g, b;

	if (r_pixbytes != 1)
	{
		D_PolysetDrawHole32(pspanpackage);
		return;
	}

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			lpdest = (word*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				if ((lzi >> 16) >= *lpz && *lptex != TRANSPARENT_COLOR)
				{
					r = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].r + r_icolormix.r] + (llight & 0xFF00)];
					g = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].g + r_icolormix.g] + (llight & 0xFF00)];
					b = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].b + r_icolormix.b] + (llight & 0xFF00)];

					*lpdest = is15bit ? PACKEDRGB555(r, g, b) : PACKEDRGB565(r, g, b);
					*lpz = lzi >> 16;
				}
				llight += r_lstepx;
				lzi += r_zistepx;
				lpdest++;
				lpz++;

				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;

				ltfrac += a_tstepxfrac;
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

void D_PolysetDrawSpansTrans32(spanpackage_t *pspanpackage)
{
	int		lcount;
	unsigned int *oldcolor;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	int	*lpz;
	byte r, g, b;

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			oldcolor = (unsigned int*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = (int*)pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				if (lzi >= *lpz)
				{
					int step1_r = (((PackedColorVec*)r_palette)[*lptex].r * ((r_icolormix.r & 0xFF00) >> 8)) / 255;
					int step1_g = (((PackedColorVec*)r_palette)[*lptex].g * ((r_icolormix.g & 0xFF00) >> 8)) / 255;
					int step1_b = (((PackedColorVec*)r_palette)[*lptex].b * ((r_icolormix.b & 0xFF00) >> 8)) / 255;

					int lightcomp = (llight & 0xFF00) >> 8;

					r = (step1_r * lightcomp) / 255;
					g = (step1_g * lightcomp) / 255;
					b = (step1_b * lightcomp) / 255;

					r = ScaleToColor(RGB_RED888(*oldcolor), r, 0xFF, r_blend) & 0xFF;
					g = ScaleToColor(RGB_GREEN888(*oldcolor), g, 0xFF, r_blend) & 0xFF;
					b = ScaleToColor(RGB_BLUE888(*oldcolor), b, 0xFF, r_blend) & 0xFF;

					*oldcolor = mask_a_32 | PACKEDRGB888(r, g, b);
					*lpz = lzi;
				}
				llight += r_lstepx;
				lzi += r_zistepx;
				oldcolor++;
				lpz++;

				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;

				ltfrac += a_tstepxfrac;
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

void D_PolysetDrawSpansTrans(spanpackage_t *pspanpackage)
{
	int		lcount;
	word	*oldcolor;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	short	*lpz;
	byte r, g, b;
	word newcolor, scaledr, scaledg, scaledb;

	if (r_pixbytes != 1)
	{
		D_PolysetDrawSpansTrans32(pspanpackage);
		return;
	}

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			oldcolor = (word*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				if ((lzi >> 16) >= *lpz)
				{
					r = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].r + r_icolormix.r] + (llight & 0xFF00)];
					g = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].g + r_icolormix.g] + (llight & 0xFF00)];
					b = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].b + r_icolormix.b] + (llight & 0xFF00)];

					// возможна замена макросом ScaleToColor
					if (is15bit)
					{
						newcolor = PACKEDRGB555(r, g, b);
						scaledr = ScaleToColor(*oldcolor, newcolor, mask_r_15, r_blend) & mask_r_15;
						scaledg = ScaleToColor(*oldcolor, newcolor, mask_g_15, r_blend) & mask_g_15;
					}
					else
					{
						// красный канал в newColor и итоговый scaledR обрезался по 0xFFFFF800, но эффект не имеет отличия от F800
						newcolor = PACKEDRGB565(r, g, b);
						scaledr = ScaleToColor(*oldcolor, newcolor, mask_r_16, r_blend) & mask_r_16;
						scaledg = ScaleToColor(*oldcolor, newcolor, mask_g_16, r_blend) & mask_g_16;
					}

					scaledb = ScaleToColor(*oldcolor, newcolor, mask_b_16, r_blend) & mask_b_16;

					*oldcolor = scaledr | scaledg | scaledb;
					*lpz = lzi >> 16;
				}
				llight += r_lstepx;
				lzi += r_zistepx;
				oldcolor++;
				lpz++;

				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;

				ltfrac += a_tstepxfrac;
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

void D_PolysetDrawSpansTransAdd32(spanpackage_t *pspanpackage)
{
	int		lcount;
	unsigned int *lpdest;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	int		*lpz;
	byte r, g, b;

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			lpdest = (unsigned int*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = (int*)pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				if (lzi >= *lpz)
				{
					int step1_r = (((PackedColorVec*)r_palette)[*lptex].r * ((r_icolormix.r & 0xFF00) >> 8)) / 255;
					int step1_g = (((PackedColorVec*)r_palette)[*lptex].g * ((r_icolormix.g & 0xFF00) >> 8)) / 255;
					int step1_b = (((PackedColorVec*)r_palette)[*lptex].b * ((r_icolormix.b & 0xFF00) >> 8)) / 255;

					int lightcomp = (llight & 0xFF00) >> 8;

					r = (step1_r * lightcomp) / 255;
					g = (step1_g * lightcomp) / 255;
					b = (step1_b * lightcomp) / 255;

					int blendalpha = r_blend & lowcleanmask(4);

					int r_result = (r * blendalpha) / 255 + RGB_RED888(*lpdest);
					int g_result = (g * blendalpha) / 255 + RGB_GREEN888(*lpdest);
					int b_result = (b * blendalpha) / 255 + RGB_BLUE888(*lpdest);

					if (r_result > 255 || g_result > 255 || b_result > 255) 
					{
						int max_comp = max(max(r_result, g_result), b_result);
						float scale = 255.0f / (float)max_comp;
						r_result = (int)((float)r_result * scale);
						g_result = (int)((float)g_result * scale);
						b_result = (int)((float)b_result * scale);
					}

					*lpdest = mask_a_32 | PACKEDRGB888(r_result, g_result, b_result);

					*lpz = lzi;
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
				llight += r_lstepx;
				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;
				ltfrac += a_tstepxfrac;
				
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

void D_PolysetDrawSpansTransAdd(spanpackage_t *pspanpackage)
{
	int		lcount;
	word	*lpdest;
	byte	*lptex;
	int		lsfrac, ltfrac;
	int		llight;
	int		lzi;
	short	*lpz;
	byte r, g, b;

	if (r_pixbytes != 1)
	{
		D_PolysetDrawSpansTransAdd32(pspanpackage);
		return;
	}

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			lpdest = (word*)pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight = pspanpackage->light;
			lzi = pspanpackage->zi;

			do
			{
				if ((lzi >> 16) >= *lpz)
				{
					r = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].r + r_icolormix.r] + (llight & 0xFF00)];
					g = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].g + r_icolormix.g] + (llight & 0xFF00)];
					b = r_lut[r_lut[((PackedColorVec*)r_palette)[*lptex].b + r_icolormix.b] + (llight & 0xFF00)];

					// performing dst = src * alpha + dst * (1 - alpha) without fpu
					if (is15bit)
					{
						// input: rgb888
						// output (r << 26) | (b << 16) | (g << 5) after masking
						// _rrrrr0000_ggggg00000_bbbbb00000
						// O111110000O1111100000O1111100000
						// hex: 0x7C1F03E0
						// where O - overflow bits
						// inverted overflow: 10000000001000000000010000000000
						// hex: 0x80200400
						unsigned int oldcolor = ((*lpdest & (mask_r_15 | mask_b_15)) << 16) | *lpdest & mask_g_15;
						unsigned int newcolor = ((r << 23) + (g << 2) + (b << 13)) & (((mask_r_15 | mask_b_15) << 16) | mask_g_15);

						unsigned int blendbits = r_blend & (BYTE)(0xff & lowcleanmask(4)), carrybits = 0;

						if (blendbits < (BYTE)(0xff & lowcleanmask(4)))
						{
							for (unsigned int mask = 1 << 8, deltacolor = newcolor >> 1; blendbits != 0; deltacolor >>= 1)
							{
								mask >>= 1;
								deltacolor &= ~(overflow15 >> 1);
								oldcolor &= ~(overflow15);

								
								if (blendbits & mask)
								{
									blendbits ^= mask;
									oldcolor += deltacolor;
									carrybits |= oldcolor & (overflow15);
								}
							}
						}
						else
						{
							oldcolor += newcolor;
							carrybits = oldcolor & (overflow15);
						}

						if (carrybits)
							oldcolor |= (overflow15) - (carrybits >> 5);

						*lpdest = oldcolor & mask_g_15 | (oldcolor >> 16) & (mask_r_15 | mask_b_15);
					}
					else
					{
						// input: rgb888
						// output (r << 27) | (b << 16) | (g << 5) after masking
						// rrrrr00000_bbbbb0000_gggggg00000
						// 1111100000O111110000O11111100000
						// hex: 0xF81F07E0
						// where O - overflow bits
						// inverted overflow: 00000000001000000000100000000000
						// hex: 0x200800
						// shifted color for handling R overflow: 10000000000100000000010000000000
						// hex: 0x80100400
						unsigned int newcolor = ((r << 24) + (b << 13) + (g << 3)) & (((mask_r_16 | mask_b_16) << 16) | mask_g_16);
						unsigned int oldcolor = ((*lpdest & (mask_r_16 | mask_b_16)) << 16) | *lpdest & mask_g_16;
						
						int blendalpha = r_blend & (BYTE)(0xff & lowcleanmask(4));
						
						unsigned int carrybits, summary;

						if (blendalpha >= (BYTE)(0xff & lowcleanmask(4)))
						{
							summary = oldcolor + newcolor;
							carrybits = ((summary & (overflow16)) | (oldcolor > summary));
							oldcolor = summary;
						}
						else
						{
							int maskBit = 1 << 8;
							newcolor >>= 1;
							carrybits = 0;
							do
							{
								maskBit >>= 1;
								// бит 0 сдвинулся на позицию 31
								newcolor &= ~(overflow16withred);
								oldcolor &= ~(overflow16);
								if ((maskBit & blendalpha) != 0)
								{
									summary = newcolor + oldcolor;
									blendalpha ^= maskBit;
									carrybits |= ((summary & (overflow16)) | (oldcolor > summary));
									oldcolor += newcolor;
								}
								newcolor >>= 1;
							} while (blendalpha);
						}

						if (carrybits)
						{
							carrybits = ROR32(carrybits);
							oldcolor |= ((carrybits | ROR32(overflow16)) - (carrybits >> 5)) << 1;
						}
						*lpdest = (oldcolor >> 16) & (mask_r_16 | mask_b_16) | oldcolor & mask_g_16;

					}

					*lpz = lzi >> 16;
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
				llight += r_lstepx;
				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += (unsigned int)lsfrac >> 16;
				lsfrac &= 0xFFFF;
				ltfrac += a_tstepxfrac;
				
				// поиск переполнения
				if (ltfrac & 0xFFFF0000)
				{
					lptex += r_affinetridesc.skinwidth;
					// сброс переполнения
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != -999999);
}

/*
================
D_PolysetUpdateTables
================
*/
void D_PolysetUpdateTables(void)
{
	int		i;
	byte	*s;

	if (r_affinetridesc.skinwidth != skinwidth ||
		r_affinetridesc.pskin != skinstart)
	{
		skinwidth = r_affinetridesc.skinwidth;
		skinstart = (byte*)r_affinetridesc.pskin;
		s = skinstart;
		for (i = 0; i<MAX_LBM_HEIGHT; i++, s += skinwidth)
			skintable[i] = s;
	}
}

/*
===================
D_PolysetSetUpForLineScan
====================
*/
void D_PolysetSetUpForLineScan(fixed8_t startvertu, fixed8_t startvertv,
	fixed8_t endvertu, fixed8_t endvertv)
{
	double		dm, dn;
	int			tm, tn;
	adivtab_t	*ptemp;

	// TODO: implement x86 version

	errorterm = -1;

	tm = endvertu - startvertu;
	tn = endvertv - startvertv;

	if (((tm <= 16) && (tm >= -15)) &&
		((tn <= 16) && (tn >= -15)))
	{
		ptemp = &adivtab[((tm + 15) << 5) + (tn + 15)];
		ubasestep = ptemp->quotient;
		erroradjustup = ptemp->remainder;
		erroradjustdown = tn;
	}
	else
	{
		dm = (double)tm;
		dn = (double)tn;

		FloorDivMod(dm, dn, &ubasestep, &erroradjustup);

		erroradjustdown = dn;
	}
}

/*
================
D_RasterizeAliasPolySmooth
================
*/
extern "C" void D_RasterizeAliasPolySmooth(void)
{
	int				initialleftheight, initialrightheight;
	int* plefttop, * prighttop, * pleftbottom, * prightbottom;
	int				working_lstepx, originalcount;

	plefttop = pedgetable->pleftedgevert0;
	prighttop = pedgetable->prightedgevert0;

	pleftbottom = pedgetable->pleftedgevert1;
	prightbottom = pedgetable->prightedgevert1;

	initialleftheight = pleftbottom[1] - plefttop[1];
	initialrightheight = prightbottom[1] - prighttop[1];

	//
	// set the s, t, and light gradients, which are consistent across the triangle
	// because being a triangle, things are affine
	//
	D_PolysetCalcGradients(r_affinetridesc.skinwidth);

	//
	// rasterize the polygon
	//

	//
	// scan out the top (and possibly only) part of the left edge
	//
	d_pedgespanpackage = a_spans;

	ystart = plefttop[1];
	d_aspancount = plefttop[0] - prighttop[0];

	d_ptex = (byte*)r_affinetridesc.pskin + (plefttop[2] >> 16) +
		(plefttop[3] >> 16) * r_affinetridesc.skinwidth;

	d_sfrac = plefttop[2] & 0xFFFF;
	d_tfrac = plefttop[3] & 0xFFFF;

	d_light = plefttop[4];
	d_zi = plefttop[5];

	if (r_pixbytes == 1)
	{
		d_pdest = (byte*)d_viewbuffer +
			ystart * screenwidth + plefttop[0] * sizeof(word);
		d_pz = zspantable[ystart] + plefttop[0];
	}
	else
	{
		d_pdest = (byte*)d_viewbuffer +
			ystart * screenwidth + plefttop[0] * sizeof(unsigned int);
		d_pz = (short*)(zspantable32[ystart] + plefttop[0]);
	}

	if (initialleftheight == 1)
	{
		a_spans->pdest = d_pdest;
		d_pedgespanpackage->pz = d_pz;

		d_pedgespanpackage->count = d_aspancount;
		d_pedgespanpackage->ptex = d_ptex;

		d_pedgespanpackage->sfrac = d_sfrac;
		d_pedgespanpackage->tfrac = d_tfrac;

		// FIXME: need to clamp l, s, t, at both ends?
		d_pedgespanpackage->light = d_light;
		d_pedgespanpackage->zi = d_zi;

		d_pedgespanpackage++;
	}
	else
	{
		D_PolysetSetUpForLineScan(plefttop[0], plefttop[1],
			pleftbottom[0], pleftbottom[1]);

		if (r_pixbytes == 1)
		{
			d_pzbasestep = d_zwidth + ubasestep;
			d_pzextrastep = d_pzbasestep + 1;

			d_pdestbasestep = screenwidth + sizeof(word) * ubasestep;
			d_pdestextrastep = d_pdestbasestep + sizeof(word) * 1;
		}
		else
		{
			d_pzbasestep = (d_zwidth + ubasestep) * (sizeof(unsigned int) / sizeof(word));
			d_pzextrastep = d_pzbasestep + 1 * (sizeof(unsigned int) / sizeof(word));

			d_pdestbasestep = screenwidth + sizeof(unsigned int) * ubasestep;
			d_pdestextrastep = d_pdestbasestep + sizeof(unsigned int) * 1;
		}

		// TODO: can reuse partial expressions here

		// for negative steps in x along left edge, bias toward overflow rather than
		// underflow (sort of turning the floor () we did in the gradient calcs into
		// ceil (), but plus a little bit)	
		if (ubasestep < 0 && r_lstepx)
			working_lstepx = r_lstepx - 1;
		else
			working_lstepx = r_lstepx;

		d_countextrastep = ubasestep + 1;

		d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
		d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;

		d_lightbasestep = r_lstepy + working_lstepx * ubasestep;
		d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
			((r_tstepy + r_tstepx * ubasestep) >> 16) *
			r_affinetridesc.skinwidth;

		d_zibasestep = r_zistepy + r_zistepx * ubasestep;

		d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
			((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
			r_affinetridesc.skinwidth;

		d_sfracextrastep = (r_sstepy + r_sstepx * d_countextrastep) & 0xFFFF;
		d_tfracextrastep = (r_tstepy + r_tstepx * d_countextrastep) & 0xFFFF;

		d_lightextrastep = d_lightbasestep + working_lstepx;
		d_ziextrastep = d_zibasestep + r_zistepx;

		D_PolysetScanLeftEdge(initialleftheight);
	}

	//
	// scan out the bottom part of the left edge, if it exists
	//
	if (pedgetable->numleftedges == 2)
	{
		int		height;

		plefttop = pleftbottom;
		pleftbottom = pedgetable->pleftedgevert2;

		height = pleftbottom[1] - plefttop[1];

		// TODO: make this a function; modularize this function in general

		ystart = plefttop[1];
		d_aspancount = plefttop[0] - prighttop[0];
		d_ptex = (byte*)r_affinetridesc.pskin + (r_affinetridesc.skinwidth * (plefttop[3] >> 16) + (plefttop[2] >> 16));
		d_sfrac = 0;
		d_tfrac = 0;
		d_light = plefttop[4];
		d_zi = plefttop[5];

		if (r_pixbytes == 1)
		{
			d_pdest = &d_viewbuffer[sizeof(word) * plefttop[0] + ystart * screenwidth];
			d_pz = &zspantable[ystart][plefttop[0]];
		}
		else
		{
			d_pdest = &d_viewbuffer[sizeof(unsigned int) * plefttop[0] + ystart * screenwidth];
			d_pz = (short*)&zspantable32[ystart][plefttop[0]];
		}

		if (height == 1)
		{
			d_pedgespanpackage->pdest = d_pdest;
			d_pedgespanpackage->pz = d_pz;

			d_pedgespanpackage->count = d_aspancount;
			d_pedgespanpackage->ptex = d_ptex;

			d_pedgespanpackage->sfrac = d_sfrac;
			d_pedgespanpackage->tfrac = d_tfrac;

			// FIXME: need to clamp l, s, t, at both ends?
			d_pedgespanpackage->light = d_light;
			d_pedgespanpackage->zi = d_zi;
		}
		else
		{
			D_PolysetSetUpForLineScan(plefttop[0], plefttop[1],
				pleftbottom[0], pleftbottom[1]);

			if (r_pixbytes == 1)
			{
				d_pdestbasestep = screenwidth + sizeof(word) * ubasestep;
				d_pdestextrastep = d_pdestbasestep + sizeof(word);

				d_pzbasestep = d_zwidth + ubasestep;
				d_pzextrastep = d_pzbasestep + 1;
			}
			else
			{
				d_pdestbasestep = screenwidth + sizeof(unsigned int) * ubasestep;
				d_pdestextrastep = d_pdestbasestep + sizeof(unsigned int);

				d_pzbasestep = (d_zwidth + ubasestep) * (sizeof(unsigned int) / sizeof(word));
				d_pzextrastep = d_pzbasestep + 1 * (sizeof(unsigned int) / sizeof(word));
			}

			if (ubasestep < 0 && r_lstepx)
				working_lstepx = r_lstepx - 1;
			else
				working_lstepx = r_lstepx;

			d_countextrastep = ubasestep + 1;
			d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
				((r_tstepy + r_tstepx * ubasestep) >> 16) *
				r_affinetridesc.skinwidth;

			d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
			d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;

			d_lightbasestep = r_lstepy + working_lstepx * ubasestep;
			d_zibasestep = r_zistepy + r_zistepx * ubasestep;

			d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
				((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
				r_affinetridesc.skinwidth;

			d_sfracextrastep = (r_sstepy + r_sstepx * d_countextrastep) & 0xFFFF;
			d_tfracextrastep = (r_tstepy + r_tstepx * d_countextrastep) & 0xFFFF;

			d_lightextrastep = d_lightbasestep + working_lstepx;
			d_ziextrastep = d_zibasestep + r_zistepx;

			D_PolysetScanLeftEdge(height);
		}
	}

	// scan out the top (and possibly only) part of the right edge, updating the
	// count field
	d_pedgespanpackage = a_spans;

	D_PolysetSetUpForLineScan(prighttop[0], prighttop[1],
		prightbottom[0], prightbottom[1]);
	d_aspancount = 0;
	d_countextrastep = ubasestep + 1;
	originalcount = a_spans[initialrightheight].count;
	a_spans[initialrightheight].count = -999999; // mark end of the spanpackages
	polysetdraw(a_spans);

	// scan out the bottom part of the right edge, if it exists
	if (pedgetable->numrightedges == 2)
	{
		int				height;
		spanpackage_t* pstart;

		pstart = &a_spans[initialrightheight];
		pstart->count = originalcount;

		d_aspancount = prightbottom[0] - prighttop[0];

		prighttop = prightbottom;
		prightbottom = pedgetable->prightedgevert2;

		height = prightbottom[1] - prighttop[1];

		D_PolysetSetUpForLineScan(prighttop[0], prighttop[1],
			prightbottom[0], prightbottom[1]);

		d_countextrastep = ubasestep + 1;
		a_spans[initialrightheight + height].count = -999999;
		// mark end of the spanpackages
		polysetdraw(pstart);
	}
}

/*
================
D_PolysetSetEdgeTable
================
*/
extern "C" void D_PolysetSetEdgeTable(void)
{
	int			edgetableindex;

	edgetableindex = 0;	// assume the vertices are already in
	//  top to bottom order

	//
	// determine which edges are right & left, and the order in which
	// to rasterize them
	//
	if (r_p0[1] >= r_p1[1])
	{
		if (r_p0[1] == r_p1[1])
		{
			if (r_p0[1] < r_p2[1])
			{
				pedgetable = &edgetables[2];
			}
			else
			{
				pedgetable = &edgetables[5];
			}

			return;
		}
		else
		{
			edgetableindex = 1;
		}
	}

	if (r_p0[1] == r_p2[1])
	{
		if (edgetableindex)
			pedgetable = &edgetables[8];
		else
			pedgetable = &edgetables[9];

		return;
	}
	else if (r_p1[1] == r_p2[1])
	{
		if (edgetableindex)
		{
			pedgetable = &edgetables[10];
		}
		else
		{
			pedgetable = &edgetables[11];
		}

		return;
	}

	if (r_p0[1] > r_p2[1])
		edgetableindex += 2;

	if (r_p1[1] > r_p2[1])
		edgetableindex += 4;

	pedgetable = &edgetables[edgetableindex];
}
