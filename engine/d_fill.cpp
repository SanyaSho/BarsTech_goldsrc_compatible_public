#include "quakedef.h"
#include "vid.h"
#include "sound.h"

void D_FillRect(vrect_t* vrect, color24 color)
{
	WORD colordest;

	if (is15bit)
		colordest = (color.b >> 3) | ((color.g & 0xF8 | ((color.r & 0xF8) << 5)) << 2);
	else
		colordest = (color.b >> 3) | ((color.g & 0xFC | ((color.r & 0xF8) << 5)) << 3);

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
		for (int i = 0; i < lr.height; i++)
		{
			if (lr.x > 0)
			{

				for (int j = 0; j < lr.width; j++)
				{
					*(WORD*)&start[j * sizeof(colordest)] = colordest;
				}
			}
			start += vid.rowbytes;
		}
	}
}

void D_GrayScreen()
{
	S_ExtraUpdate();
	for (int y = 0; y < vid.height; y++)
	{
		for (int x = 0; x < vid.width; x++)
		{
			WORD pix = ((WORD*)&vid.buffer[y * vid.rowbytes])[x];
			pix >>= 1;
			if (is15bit)
				pix &= 0x756F;
			else
				pix &= 0x7BEF;
			((WORD*)&vid.buffer[y * vid.rowbytes])[x] = pix;
		}
	}
	S_ExtraUpdate();
}