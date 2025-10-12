// d_part.c: software driver module for drawing particles

#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"
#include "host.h"

/*
==============
D_EndParticles
==============
*/
void D_EndParticles(void)
{
	// not used by software driver
}


/*
==============
D_StartParticles
==============
*/
void D_StartParticles(void)
{
	// not used by software driver
}

/*
==============
D_DrawParticle
==============
*/
void D_DrawParticle(particle_t *pparticle)
{
	vec3_t	local, transformed;
	float	zi;
	byte	*pdest;
	unsigned int* pdest32;
	short	*pz;
	int		*pz32;
	int		i, izi, izi32, pix, count, u, v;
	short packed;
	unsigned int packed32;

	// transform point
	VectorSubtract(pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

	// project the point
	// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if (pparticle->type == pt_vox_slowgrav || pparticle->type == pt_vox_grav)
	{
		if ((v > d_vrectbottom_vox) ||
			(u > d_vrectright_vox) ||
			(v < d_vrecty) ||
			(u < d_vrectx))
		{
			return;
		}
	}
	else
	{
		if ((v > d_vrectbottom_particle) ||
			(u > d_vrectright_particle) ||
			(v < d_vrecty) ||
			(u < d_vrectx))
		{
			return;
		}
	}

	packed = pparticle->packedColor;

	if (r_pixbytes == 1)
	{
		pz = zspantable[v] + u;
		pdest = d_viewbuffer + d_scantable[v] + u * sizeof(word);
	}
	else
	{
		packed32 = hlRGB32(host_basepal, pparticle->color);
		pz32 = zspantable32[v] + u;
		pdest32 = (unsigned int*)(d_viewbuffer + d_scantable[v] + u * sizeof(unsigned int));
	}

	izi = (int)(zi * ZISCALE);
	izi32 = (int)(zi * ZISCALE * INFINITE_DISTANCE);

	pix = izi >> d_pix_shift;

	if (pparticle->type == pt_vox_slowgrav || pparticle->type == pt_vox_grav)
	{
		if (pix < d_vox_min)
			pix = d_vox_min;
		else if (pix > d_vox_max)
			pix = d_vox_max;
	}
	else
	{
		if (pix < d_pix_min)
			pix = d_pix_min;
		else if (pix > d_pix_max)
			pix = d_pix_max;
	}

	if (r_pixbytes == 1)
	{
		switch (pix)
		{
		case 1:
			count = 1 << d_y_aspect_shift;

			for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
			{
				if (pz[0] <= izi)
				{
					pz[0] = izi;
					((WORD*)pdest)[0] = pparticle->packedColor;
				}
			}
			break;

		case 2:
			count = 2 << d_y_aspect_shift;

			for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
			{
				if (pz[0] <= izi)
				{
					pz[0] = izi;
					((WORD*)pdest)[0] = pparticle->packedColor;
				}

				if (pz[1] <= izi)
				{
					pz[1] = izi;
					((WORD*)pdest)[1] = pparticle->packedColor;
				}
			}
			break;

		case 3:
			count = 3 << d_y_aspect_shift;

			for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
			{
				if (pz[0] <= izi)
				{
					pz[0] = izi;
					((WORD*)pdest)[0] = pparticle->packedColor;
				}

				if (pz[1] <= izi)
				{
					pz[1] = izi;
					((WORD*)pdest)[1] = pparticle->packedColor;
				}

				if (pz[2] <= izi)
				{
					pz[2] = izi;
					((WORD*)pdest)[2] = pparticle->packedColor;
				}
			}
			break;

		case 4:
			count = 4 << d_y_aspect_shift;

			for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
			{
				if (pz[0] <= izi)
				{
					pz[0] = izi;
					((WORD*)pdest)[0] = pparticle->packedColor;
				}

				if (pz[1] <= izi)
				{
					pz[1] = izi;
					((WORD*)pdest)[1] = pparticle->packedColor;
				}

				if (pz[2] <= izi)
				{
					pz[2] = izi;
					((WORD*)pdest)[2] = pparticle->packedColor;
				}

				if (pz[3] <= izi)
				{
					pz[3] = izi;
					((WORD*)pdest)[3] = pparticle->packedColor;
				}
			}
			break;

		default:
			count = pix << d_y_aspect_shift;

			for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
			{
				for (i=0 ; i<pix ; i++)
				{
					if (pz[i] <= izi)
					{
						pz[i] = izi;
						((WORD*)pdest)[i] = pparticle->packedColor;
					}
				}
			}
			break;
		}
	}
	else
	{
		switch (pix)
		{
		case 1:
			count = 1 << d_y_aspect_shift;

			for ( ; count ; count--, pz32 += d_zwidth, pdest32 += screenwidth >> 2)
			{
				if (pz32[0] <= izi32)
				{
					pz32[0] = izi32;
					pdest32[0] = packed32;
				}
			}
			break;

		case 2:
			count = 2 << d_y_aspect_shift;

			for ( ; count ; count--, pz32 += d_zwidth, pdest32 += screenwidth >> 2)
			{
				if (pz32[0] <= izi32)
				{
					pz32[0] = izi32;
					pdest32[0] = packed32;
				}

				if (pz32[1] <= izi32)
				{
					pz32[1] = izi32;
					pdest32[1] = packed32;
				}
			}
			break;

		case 3:
			count = 3 << d_y_aspect_shift;

			for ( ; count ; count--, pz32 += d_zwidth, pdest32 += screenwidth >> 2)
			{
				if (pz32[0] <= izi32)
				{
					pz32[0] = izi32;
					pdest32[0] = packed32;
				}

				if (pz32[1] <= izi32)
				{
					pz32[1] = izi32;
					pdest32[1] = packed32;
				}

				if (pz32[2] <= izi32)
				{
					pz32[2] = izi32;
					pdest32[2] = packed32;
				}
			}
			break;

		case 4:
			count = 4 << d_y_aspect_shift;

			for ( ; count ; count--, pz32 += d_zwidth, pdest32 += screenwidth >> 2)
			{
				if (pz32[0] <= izi32)
				{
					pz32[0] = izi32;
					pdest32[0] = packed32;
				}

				if (pz32[1] <= izi32)
				{
					pz32[1] = izi32;
					pdest32[1] = packed32;
				}

				if (pz32[2] <= izi32)
				{
					pz32[2] = izi32;
					pdest32[2] = packed32;
				}

				if (pz32[3] <= izi32)
				{
					pz32[3] = izi32;
					pdest32[3] = packed32;
				}
			}
			break;

		default:
			count = pix << d_y_aspect_shift;

			for ( ; count ; count--, pz32 += d_zwidth, pdest32 += screenwidth >> 2)
			{
				for (i=0 ; i<pix ; i++)
				{
					if (pz32[i] <= izi32)
					{
						pz32[i] = izi32;
						pdest32[i] = packed32;
					}
				}
			}
			break;
		}
	}
}
