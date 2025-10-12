#include "quakedef.h"
#include "vid.h"
#include "sound.h"
#include "color.h"
#include "d_iface.h"

void D_FillRect(vrect_t* vrect, color24 color)
{
	WORD colordest;
	unsigned int colordest32;

	if (is15bit)
		colordest = (color.b >> 3) | ((color.g & 0xF8 | ((color.r & 0xF8) << 5)) << 2);
	else
		colordest = (color.b >> 3) | ((color.g & 0xFC | ((color.r & 0xF8) << 5)) << 3);

	colordest32 = 0xFF000000 | PACKEDRGB888(color.r, color.g, color.b);

	vrect_t lr = *vrect;
	if (lr.x < 0)
	{
		lr.width += lr.x;
		lr.x = 0;
	}

	if (lr.y < 0)
	{
		lr.height += lr.y;
		lr.y = 0;
	}

	if (lr.width + lr.x > vid.width)
		lr.width = vid.width - lr.x;

	if (lr.height + lr.y > vid.height)
		lr.height = vid.height - lr.y;

	if (lr.width >= 1 && lr.height >= 1)
	{
		pixel_t* start = &vid.buffer[lr.y * vid.rowbytes + sizeof(colordest) * lr.x];
		pixel_t* start32 = &vid.buffer[lr.y * vid.rowbytes + sizeof(colordest32) * lr.x];
		for (int i = 0; i < lr.height; i++)
		{
			if (lr.x > 0)
			{

				for (int j = 0; j < lr.width; j++)
				{
					if (r_pixbytes == 1)
						*(WORD*)&start[j * sizeof(colordest)] = colordest;
					else
						*(unsigned int*)&start32[j * sizeof(colordest32)] = colordest32;
				}
			}
			
			if (r_pixbytes == 1)
				start += vid.rowbytes;
			else
				start32 += vid.rowbytes;
		}
	}
}