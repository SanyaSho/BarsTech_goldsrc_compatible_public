#include "quakedef.h"
#include "d_local.h"
#include "render.h"
#include "d_iface.h"

EXTERN_C
{
int	d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

int	d_y_aspect_shift, d_pix_min, d_pix_max, d_pix_shift;

int		d_scantable[MAXHEIGHT];
short* zspantable[MAXHEIGHT];
int* zspantable32[MAXHEIGHT];

// вертикальные коэффициенты
int d_vox_min, d_vox_max, d_vrectright_vox, d_vrectbottom_vox;
};

/*
================
D_Patch
================
*/
void D_Patch(void)
{
}


/*
================
D_ViewChanged
================
*/
void D_ViewChanged(void)
{
	int rowbytes;

	if (r_dowarp)
	{
		if (r_pixbytes == 1)
			rowbytes = WARP_WIDTH * sizeof(word);
		else
			rowbytes = WARP_WIDTH * r_pixbytes;
	}
	else
		rowbytes = vid.rowbytes;

	scale_for_mip = xscale;
	if (yscale > xscale)
		scale_for_mip = yscale;

	d_zrowbytes = vid.width * sizeof(short);
	d_zwidth = vid.width;

	d_pix_min = r_refdef.vrect.width / WARP_WIDTH;
	if (d_pix_min < 1)
		d_pix_min = 1;

	d_pix_max = (int)((float)r_refdef.vrect.width / (WARP_WIDTH / 4.0) + 0.5);
	d_pix_shift = 8 - (int)((float)r_refdef.vrect.width / WARP_WIDTH + 0.5);
	d_vox_min = r_refdef.vrect.width / WARP_HEIGHT;
	d_vox_max = (int)((float)r_refdef.vrect.width / ((WARP_WIDTH / 2) / 9.0) + 0.5);
	if (d_pix_max < 1)
		d_pix_max = 1;

	if (pixelAspect > 1.4)
		d_y_aspect_shift = 1;
	else
		d_y_aspect_shift = 0;

	d_vrectx = r_refdef.vrect.x;
	d_vrecty = r_refdef.vrect.y;
	d_vrectright_particle = r_refdef.vrectright - d_pix_max;
	d_vrectright_vox = r_refdef.vrectright - d_vox_max;
	d_vrectbottom_particle =
		r_refdef.vrectbottom - (d_pix_max << d_y_aspect_shift);
	d_vrectbottom_vox = r_refdef.vrectbottom - (d_vox_max << d_y_aspect_shift);

	{
		int		i;

		for (i = 0; i < vid.height; i++)
		{
			d_scantable[i] = i * rowbytes;
			if (r_pixbytes == 1)
				zspantable[i] = d_pzbuffer + i * d_zwidth;
			else
				zspantable32[i] = d_pzbuffer32 + i * d_zwidth;
		}
	}

	D_Patch();
}
