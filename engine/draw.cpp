#include "quakedef.h"
#include "draw.h"
#include "vid.h"
#include "vgui2/text_draw.h"
#include "color.h"
#include "d_iface.h"
#include "view.h"
#include "decals.h"
#include "model.h"
#include "host.h"
#include "buildnum.h"

typedef struct {
	vrect_t	rect;
	int		width;
	int		height;
	byte* ptexbytes;
	int		rowbytes;
} rectdesc_t;

// static rectdesc_t	r_rectdesc;

// byte* draw_chars;				// 8*8 graphic characters
qpic_t* draw_disc;
qpic_t* draw_backtile;

qboolean m_bDrawInitialized = false;
int firsttime = 1;

//=============================================================================
/* Support Routines */

#define	MAX_CACHED_PICS		16
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

int scissor_x = 0;
int scissor_y = 0;
int scissor_x2 = 0;
int scissor_y2 = 0;
bool giScissorTest = false;
extern wrect_t scissor_rect;

unsigned int hlRGB32(word* p, int i)
{
	return 0xFF000000 | RGBPAL888(p, i);
}

EXTERN_C short hlRGB(word* p, int i)
{
	return is15bit ? RGBPAL555(p, i) : RGBPAL(p, i);
}

short PutRGB(colorVec* pcv)
{
	return is15bit ? PACKEDRGB555(pcv->r, pcv->g, pcv->b) : PACKEDRGB565(pcv->r, pcv->g, pcv->b);
}

unsigned int PutRGB32(colorVec* pcv)
{
	return 0xFF000000 | PACKEDRGB888(pcv->r, pcv->g, pcv->b);
}

unsigned int RGBToPacked32(byte* p)
{
	return 0xFF000000 | PACKEDRGB888(p[0], p[1], p[2]);
}

short RGBToPacked(byte* p)
{
	return is15bit ? PACKEDRGB555(p[0], p[1], p[2]) : PACKEDRGB565(p[0], p[1], p[2]);
}

unsigned int PackedRGB32(byte* p, unsigned char i)
{
	return 0xFF000000 | PACKEDRGB888(p[i * 3 + 0], p[i * 3 + 1], p[i * 3 + 2]);
}

short PackedRGB(byte* p, unsigned char i)
{
	return is15bit ? PACKEDRGB555(p[i * 3 + 0], p[i * 3 + 1], p[i * 3 + 2]) : PACKEDRGB565(p[i * 3 + 0], p[i * 3 + 1], p[i * 3 + 2]);
}

void GetRGB(short s, colorVec* pcv)
{
	if (is15bit)
	{
		pcv->r = RGB_RED555(s);
		pcv->g = RGB_GREEN555(s);
		pcv->b = RGB_BLUE555(s);
	}
	else
	{
		pcv->r = RGB_RED565(s);
		pcv->g = RGB_GREEN565(s);
		pcv->b = RGB_BLUE565(s);
	}
}

void GetRGB32(unsigned int c, colorVec* pcv)
{
	pcv->r = RGB_RED888(c);
	pcv->g = RGB_GREEN888(c);
	pcv->b = RGB_BLUE888(c);
	pcv->a = 255;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Set the scissor
//  the coordinate system for gl is upsidedown (inverted-sheight) as compared to software, so the
//  specified clipping rect must be flipped
// Input  : swidth - 
//			sheight - 
//			screenwidth - 
//			sheight - 
//-----------------------------------------------------------------------------
void EnableScissorTest(int x, int y, int width, int height)
{
	x = clamp(x, 0, (int)vid.width);
	scissor_x = x;
	scissor_rect.left = x;
	y = clamp(y, 0, (int)vid.height);
	scissor_y = y;
	scissor_rect.top = y;
	width = clamp(x + width, 0, (int)vid.width);
	scissor_x2 = width;
	scissor_rect.right = width;
	height = clamp(y + height, 0, (int)vid.height);
	scissor_y2 = height;
	scissor_rect.bottom = height;

	giScissorTest = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DisableScissorTest(void)
{
	scissor_x = 0;
	scissor_x2 = 0;
	scissor_y = 0;
	scissor_y2 = 0;

	scissor_rect.left = 0;
	scissor_rect.top = 0;
	scissor_rect.right = 0;
	scissor_rect.bottom = 0;

	giScissorTest = false;
}

//-----------------------------------------------------------------------------
// Purpose: Verify that this is globalAlphaScale valid, properly ordered rectangle.
// Input  : *prc - 
// Output : int
//-----------------------------------------------------------------------------
static int ValidateWRect(const wrect_t* prc)
{

	if (!prc)
		return false;

	if ((prc->left >= prc->right) || (prc->top >= prc->bottom))
	{
		//!!!UNDONE Dev only warning msg
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: classic interview question
// Input  : *prc1 - 
//			*prc2 - 
//			*prc - 
// Output : int
//-----------------------------------------------------------------------------
static int IntersectWRect(const wrect_t* prc1, const wrect_t* prc2, wrect_t* prc)
{
	wrect_t rc;

	if (!prc)
		prc = &rc;

	prc->left = max(prc1->left, prc2->left);
	prc->right = min(prc1->right, prc2->right);

	if (prc->left < prc->right)
	{
		prc->top = max(prc1->top, prc2->top);
		prc->bottom = min(prc1->bottom, prc2->bottom);

		if (prc->top < prc->bottom)
			return 1;

	}

	return 0;
}

void	Draw_TransPic(int x, int y, qpic_t* pic)
{
	byte* dest, * source;
	unsigned short* pusdest;
	unsigned int* puidest;
	int				v, u;


	if ((x < 0) ||
		(x + pic->width > vid.width) ||
		(y < 0) ||
		(y + pic->height > vid.height))
	{
		Sys_Error("Draw_TransPic: bad coordinates");
	}

	source = pic->data;

	pusdest = (unsigned short*)vid.buffer + y * (vid.rowbytes >> 1) + x;
	puidest = (unsigned int*)vid.buffer + y * (vid.rowbytes >> 2) + x;

	for (v = 0; v < pic->height; v++)
	{
		for (u = 0; u < pic->width; u++)
		{
			/*
			* 00 screenwidth
			* 04 sheight
			* 08 data ->
			* 08	colorsused
			* 0A	pPalette
			*/
			if (r_pixbytes == 1)
			{
				if (source[u] != TRANSPARENT_COLOR)
					pusdest[u] = PackedRGB(&pic->data[pic->width * pic->height + 2], source[u]);
			}
			else
			{
				if (source[u] != TRANSPARENT_COLOR)
					puidest[u] = PackedRGB32(&pic->data[pic->width * pic->height + 2], source[u]);
			}
		}

		if (r_pixbytes == 1)
			pusdest += vid.rowbytes >> 1;
		else
			puidest += vid.rowbytes >> 2;
		source += pic->width;
	}

}

void Draw_GetDefaultColor(void)
{
	int r, g, b;

	if (sscanf(con_color.string, "%i %i %i", &r, &g, &b) == 3)
		VGUI2_Draw_SetTextColor(r, g, b);
}

void Draw_ResetTextColor(void)
{
	Draw_GetDefaultColor();
}

int Draw_String(int x, int y, char* str)
{
	const auto iWidth = VGUI2_DrawString(x, y, str, VGUI2_GetConsoleFont());

	Draw_ResetTextColor();

	return iWidth + x;
}

void Draw_SetTextColor(float r, float g, float b)
{
	RecEngDrawSetTextColor(r, g, b);

	VGUI2_Draw_SetTextColor(
		static_cast<int>(r * 255.0),
		static_cast<int>(g * 255.0),
		static_cast<int>(b * 255.0)
	);
}

// Draw_SurfChar - idb
void Draw_RGBAImage(int x, int y, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba, int istext, byte textr, byte textg, byte textb, byte globalAlphaScale)
{
	if (rgba == NULL || globalAlphaScale == 0)
		return;
	
	if ((x < 0 || x + srcw > vid.width) || (y < 0 || y + srch > vid.height))
		return;
	
	int coeff = 0;
	int cx = x, cy = y;
	
	if (giScissorTest)
	{
		if (x < scissor_x)
		{
			srcw -= min(scissor_x - x, srcw);
			coeff = (scissor_x - x) * 4;
			cx = scissor_x;
		}
		
		if (y < scissor_y)
		{
			srch -= min(scissor_y - y, srch);
			coeff += texw * (scissor_y - y) * 4;
			cy = scissor_y;
		}
		
		if (cy + srch > scissor_y2)
		{
			if (scissor_y2 - y < 0)
					return;
				srch = scissor_y2 - y;
		}
		
		if (cx + srcw > scissor_x2)
		{
			if (scissor_x2 - x < 0)
					return;
				srcw = scissor_x2 - x;
		}
	}
	
	for (int v = 0; v < srch; v++)
	{
		for (int u = 0; u < srcw; u++)
		{
			byte* src = &rgba[4 * (srcx + u) + 4 * (srcy + v) * texw + coeff];
			word* pusdest = (word*)&vid.buffer[2 * (x + u) + 2 * (y + v) * (vid.rowbytes >> 1)];
			unsigned int* puidest = (unsigned int*)&vid.buffer[4 * (x + u) + 4 * (y + v) * (vid.rowbytes >> 2)];
			
			if (*(unsigned*)src != 0)
			{
				byte blend_a = (src[3] * globalAlphaScale) >> 8;

				if (r_pixbytes == 1)
				{
					word r = (256 - blend_a) * RGB_RED565(*pusdest);
					word g = (256 - blend_a) * RGB_GREEN565(*pusdest);
					word b = (256 - blend_a) * RGB_BLUE565(*pusdest);
					
					if (istext)
					{
						*pusdest = ( ((long)(r)+(long)(textr >> 3) * (unsigned char)blend_a) >> 8 ) << 11 |
							( ((long)(g)+(long)(textg >> 2) * (unsigned char)blend_a) >> 8 ) << 5 |
							( ((long)(b)+(long)(textb >> 3) * (unsigned char)blend_a) >> 8 );
					}
					else
					{
						*pusdest = ( ((long)(r)+(long)(src[0] >> 3) * (unsigned char)blend_a) >> 8 ) << 11 |
							( ((long)(g)+(long)(src[1] >> 2) * (unsigned char)blend_a) >> 8 ) << 5 |
							( ((long)(b)+(long)(src[2] >> 3) * (unsigned char)blend_a) >> 8 );
					}
				}
				else
				{
					int r = (256 - blend_a) * RGB_RED888(*puidest);
					int g = (256 - blend_a) * RGB_GREEN888(*puidest);
					int b = (256 - blend_a) * RGB_BLUE888(*puidest);

					if (istext)
					{
						*puidest = 0xFF000000 | 
							(((long)(r)+(long)(textr) * (unsigned char)blend_a) >> 8) << 16 |
							(((long)(g)+(long)(textg) * (unsigned char)blend_a) >> 8) << 8 |
							(((long)(b)+(long)(textb) * (unsigned char)blend_a) >> 8);
					}
					else
					{
						*puidest = 0xFF000000 | 
							(((long)(r)+(long)(src[0]) * (unsigned char)blend_a) >> 8) << 16 |
							(((long)(g)+(long)(src[1]) * (unsigned char)blend_a) >> 8) << 8 |
							(((long)(b)+(long)(src[2]) * (unsigned char)blend_a) >> 8);
					}
				}
			}
		}
	}
}


// idb Draw_SurfRect
void Draw_ScaledRGBAImage(int x, int y, int wide, int tall, int srcx, int srxy, int srcw, int srch, int texw, int texh, byte* rgba)
{
	if (rgba != nullptr && x < vid.width && y < vid.height && wide != 0 && tall != 0)
	{
		int dx = 0, dy = 0;

		if (x + wide > vid.width)
			wide = vid.width - x;
		else
		{
			if (x < 0)
			{
				if (x + wide < 0)
					return;

				dx = -x;
			}
		}

		if (y + tall > vid.height)
			tall = vid.height - y;
		else
		{
			if (y < 0)
			{
				if (y + tall < 0)
					return;

				dy = -y;
			}
		}

		float xcoeff = ((float)srcw) / ((float)wide);
		float ycoeff = ((float)srch) / ((float)tall);

		byte* source = &rgba[(srcx + texw * srxy) * 4];
		unsigned short* pusdest = &((word*)vid.buffer)[(x + dx) + (y + dy) * (vid.rowbytes >> 1)];
		unsigned int* puidest = &((unsigned int*)vid.buffer)[(x + dx) + (y + dy) * (vid.rowbytes >> 2)];

		int uOffset = (1.0f / xcoeff) * (float)dx;
		int vOffset = (1.0f / ycoeff) * (float)dy;

		for (int i = vOffset; i < tall; i++)
		{
			for (int j = uOffset; j < wide; j++)
			{
				if (*(unsigned*)&source[4 * texw * int(i * ycoeff) + 4 * (int(j * (xcoeff * 256.0f)) >> 8)])
				{
					byte *pix = &source[4 * texw * int(i * ycoeff) + 4 * (int(j * (xcoeff * 256.0f)) >> 8)];
					if (pix[3] != 0)
					{
						if (r_pixbytes == 1)
							pusdest[(i - vOffset) * (vid.rowbytes >> 1) + (j - uOffset)] = RGBToPacked(pix);
						else
							puidest[(i - vOffset) * (vid.rowbytes >> 2) + (j - uOffset)] = RGBToPacked32(pix);
					}
				}
			}
		}
	}
}

void Draw_RGBAImageAdd32(int x, int y, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba, int istext, byte textr, byte textg, byte textb, byte globalAlphaScale)
{
	if (rgba == NULL || globalAlphaScale == 0)
		return;

	if (x < 0 || x + srcw > vid.width || y < 0 || y + srch > vid.height)
		return;

	unsigned int* pScreen32 = &((unsigned int*)vid.buffer)[x + y * (vid.rowbytes >> 2)];
	byte* sourceLine = &rgba[(srcx + texw * srcy) * 4];

	for (int i = 0; i < srch; i++)
	{
		for (int j = 0; j < srcw; j++)
		{
			byte* colors = &sourceLine[(j + i * texw) * 4];
			if (colors[3] != 0)
			{
				// c*a+dest*(1-a)
				int srcB = (colors[0] * textr) >> 8;
				int srcG = (colors[1] * textg) >> 8;
				int srcR = (colors[2] * textb) >> 8;

				int a = colors[3]; // (colors[3] * globalAlphaScale) >> 8;

				if (srcR || srcG || srcB)
				{
					color32* pScreenRGB = (color32*)&pScreen32[i * (vid.rowbytes >> 2) + j];

					pScreenRGB->r = (srcR * a + pScreenRGB->r * (255 - a)) / 255;
					pScreenRGB->g = (srcG * a + pScreenRGB->g * (255 - a)) / 255;
					pScreenRGB->b = (srcB * a + pScreenRGB->b * (255 - a)) / 255;
					pScreenRGB->a = 255;
				}
			}
		}
	}
}

void Draw_RGBAImageAdd15(int x, int y, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba, int istext, byte textr, byte textg, byte textb, byte globalAlphaScale)
{
	if (rgba == NULL || globalAlphaScale == 0)
		return;

	if (x < 0 || x + srcw > vid.width || y < 0 || y + srch > vid.height)
		return;

	word* pScreen = &((word*)vid.buffer)[x + y * (vid.rowbytes >> 1)];
	byte* sourceLine = &rgba[(srcx + texw * srcy) * 4];

	for (int i = 0; i < srch; i++)
	{
		for (int j = 0; j < srcw; j++)
		{
			byte* colors = &sourceLine[(j + i * texw) * 4];
			if (colors[3] != 0)
			{
				int alphachan = (colors[3] * 192) & 0xFF00;
				word finalsrc =
					red_64klut[alphachan + ((colors[0] * textr) >> 8)] |
					blue_64klut[alphachan + ((colors[2] * textb) >> 8)] |
					green_64klut[alphachan + ((colors[1] * textg) >> 8)];

				if (finalsrc != 0)
				{
					word bufcolor = pScreen[i * (vid.rowbytes >> 1) + j];
					word destr = min(AddColor(finalsrc, bufcolor, 0x7C00), (long)0x7C00);
					word destg = min(AddColor(finalsrc, bufcolor, 0x3E0), (long)0x3E0);
					word destb = min(AddColor(finalsrc, bufcolor, 0x1F), (long)0x1F);
					pScreen[i * (vid.rowbytes >> 1) + j] = destr | destg | destb;
				}
			}
		}
	}
}

void Draw_RGBAImageAdd16(int x, int y, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba, int istext, byte textr, byte textg, byte textb, byte globalAlphaScale)
{
	if (rgba == NULL || globalAlphaScale == 0)
		return;

	if (x < 0 || x + srcw > vid.width || y < 0 || y + srch > vid.height)
		return;

	word* pScreen = &((word*)vid.buffer)[x + y * (vid.rowbytes >> 1)];
	byte* sourceLine = &rgba[(srcx + texw * srcy) * 4];

	for (int i = 0; i < srch; i++)
	{
		for (int j = 0; j < srcw; j++)
		{
			byte* colors = &sourceLine[(j + i * texw) * 4];
			if (colors[3] != 0)
			{
				int alphachan = (colors[3] * 192) & 0xFF00;
				word finalsrc = 
					red_64klut[alphachan + ((colors[0] * textr) >> 8)] |
					blue_64klut[alphachan + ((colors[2] * textb) >> 8)] |
					green_64klut[alphachan + ((colors[1] * textg) >> 8)];

				if (finalsrc != 0)
				{
					word bufcolor = pScreen[i * (vid.rowbytes >> 1) + j];
					word destr = min(AddColor(finalsrc, bufcolor, 0xF800), (long)0xF800);
					word destg = min(AddColor(finalsrc, bufcolor, 0x7E0), (long)0x7E0);
					word destb = min(AddColor(finalsrc, bufcolor, 0x1F), (long)0x1F);
					pScreen[i * (vid.rowbytes >> 1) + j] = destr | destg | destb;
				}
			}
		}
	}
}

void Draw_RGBAImageAdditive(int x, int y, int srcx, int srcy, int srcw, int srch, int texw, int texh, byte* rgba, int istext, byte textr, byte textg, byte textb, byte globalAlphaScale)
{
	if (r_pixbytes != 1)
		return Draw_RGBAImageAdd32(x, y, srcx, srcy, srcw, srch, texw, texh, rgba, istext, textr, textg, textb, globalAlphaScale);

	if (is15bit)
		return Draw_RGBAImageAdd15(x, y, srcx, srcy, srcw, srch, texw, texh, rgba, istext, textr, textg, textb, globalAlphaScale);
	else
		return Draw_RGBAImageAdd16(x, y, srcx, srcy, srcw, srch, texw, texh, rgba, istext, textr, textg, textb, globalAlphaScale);
}

// idb Draw_FillRect
void Draw_FillRGB(int x, int y, int width, int height, int r, int g, int b)
{
	wrect_t rectborders{ x, x + width, y, y + height }, screenborders{0, vid.width, 0, vid.height};
	if (IntersectWRect(&rectborders, &screenborders, &screenborders))
	{
		if (!giScissorTest || IntersectWRect(&scissor_rect, &screenborders, &screenborders))
		{
			int step = vid.rowbytes >> 1;
			colorVec color{ r, g, b, 255 };

			unsigned short* pdestbuf = &((WORD*)vid.buffer)[screenborders.left + screenborders.top * step];
			unsigned int* pdestbuf32 = &((unsigned int*)vid.buffer)[screenborders.left + screenborders.top * step / 2];

			int dx = screenborders.right - screenborders.left, dy = screenborders.bottom - screenborders.top;

			if (r_pixbytes == 1)
			{
				for (int i = 0; i < dy; i++)
				{
					for (int j = 0; j < dx; j++)
						pdestbuf[i * step + j] = PutRGB(&color);
				}
			}
			else
			{
				for (int i = 0; i < dy; i++)
				{
					for (int j = 0; j < dx; j++)
						pdestbuf32[i * step / 2 + j] = PutRGB32(&color);
				}
			}
		}
	}
}

void Draw_FillRGBABlend(int x, int y, int width, int height, int r, int g, int b, int a)
{
	wrect_t rectborders{ x, x + width, y, y + height }, screenborders{ 0, vid.width, 0, vid.height };
	int dalpha = 255 - a;
	if (IntersectWRect(&rectborders, &screenborders, &screenborders))
	{
		if (!giScissorTest || IntersectWRect(&scissor_rect, &screenborders, &screenborders))
		{
			if (a != 0)
			{
				int alphachan = (192 * a) & 0xFF00;
				int step = vid.rowbytes >> 1;
				unsigned short* pdestbuf = &((WORD*)vid.buffer)[screenborders.left + screenborders.top * step];
				unsigned int* pdestbuf32 = &((unsigned int*)vid.buffer)[screenborders.left + screenborders.top * step / 2];
				int dx = screenborders.right - screenborders.left, dy = screenborders.bottom - screenborders.top;

				if (r_pixbytes == 1)
				{
					word newcolor = blue_64klut[alphachan + b] | green_64klut[alphachan + g] | red_64klut[alphachan + r];
					if (is15bit)
					{
						for (int i = 0; i < dy; i++)
						{
							for (int j = 0; j < dx; j++)
							{
								word srcpix = pdestbuf[i * step + j];
								pdestbuf[i * step + j] = newcolor + (
									((dalpha * (srcpix & 0x7C00)) >> 8) & 0x7C00 |	// red
									((dalpha * (srcpix & 0x1F)) >> 8) & 0x1F |		// blue
									((dalpha * (srcpix & 0x3E0)) >> 8) & 0x3E0		// green
									);
							}
						}
					}
					else
					{
						for (int i = 0; i < dy; i++)
						{
							for (int j = 0; j < dx; j++)
							{
								word srcpix = pdestbuf[i * step + j];
								pdestbuf[i * step + j] = newcolor + (
									((dalpha * (srcpix & 0xF800)) >> 8) & 0xF800 |	// red
									((dalpha * (srcpix & 0x1F)) >> 8) & 0x1F |		// blue
									((dalpha * (srcpix & 0x7E0)) >> 8) & 0x7E0		// green
									);
							}
						}
					}
				}
				else
				{
					for (int i = 0; i < dy; i++)
					{
						for (int j = 0; j < dx; j++)
						{
							color32* pScreenRGB = (color32*)&pdestbuf32[i * step / 2 + j];

							// src + dst * (1 - alpha)
							//pScreenRGB->r = min(255, (r * a) / 255 + (pScreenRGB->r * dalpha) / 255);
							//pScreenRGB->g = min(255, (g * a) / 255 + (pScreenRGB->g * dalpha) / 255);
							//pScreenRGB->b = min(255, (b * a) / 255 + (pScreenRGB->b * dalpha) / 255);
							pScreenRGB->r = min(255, b + (pScreenRGB->r * dalpha) / 255);
							pScreenRGB->g = min(255, g + (pScreenRGB->g * dalpha) / 255);
							pScreenRGB->b = min(255, r + (pScreenRGB->b * dalpha) / 255);
							pScreenRGB->a = 255;
						}
					}
				}
			}
		}
	}
}

void Draw_LineRGB(int x1, int y1, int x2, int y2, int r, int g, int b)
{
	int dx = abs(x2 - x1), dy = abs(y2 - y1);

	int axisalign = 2 * (x1 <= x2) - 1;
	int yinc = (y1 <= y2) ? (vid.rowbytes >> 1) : -((int)vid.rowbytes >> 1);

	wrect_t rectborders{ x1 >= x2 ? x2 : x1, (x1 > x2 ? x1 : x2) + 1, y1 >= y2 ? y2 : y1, (y1 > y2 ? y1 : y2) + 1 }, screenborders{ 0, vid.width, 0, vid.height };

	if (IntersectWRect(&rectborders, &screenborders, &screenborders))
	{
		if (!giScissorTest || IntersectWRect(&scissor_rect, &screenborders, &screenborders))
		{
			word* pdestbuf = &((word*)vid.buffer)[x1 + y1 * (vid.rowbytes >> 1)];
			unsigned int* pdestbuf32 = &((unsigned int*)vid.buffer)[x1 + y1 * (vid.rowbytes >> 2)];
			colorVec lineColor{ r, g, b, 255 };

			if (dx < dy)
			{
				int queue = dx << 1 - dy;
				for (int i = 0; i < dy; i++)
				{
					if (r_pixbytes == 1)
						pdestbuf[i * (queue <= 0 ? yinc : axisalign + yinc)] = PutRGB(&lineColor);
					else
						pdestbuf32[i * (queue <= 0 ? yinc : axisalign + yinc)] = PutRGB32(&lineColor);
					queue += (queue <= 0 ? dx : dx - dy) << 1;
				}
			}
			else
			{
				int queue = dy << 1 - dx;
				for (int i = 0; i < dx; i++)
				{
					if (r_pixbytes == 1)
						pdestbuf[i * (queue <= 0 ? axisalign : axisalign + yinc)] = PutRGB(&lineColor);
					else
						pdestbuf32[i * (queue <= 0 ? axisalign : axisalign + yinc)] = PutRGB32(&lineColor);
					queue += (queue <= 0 ? dy : dy - dx) << 1;
				}
			}
		}
	}
}

void Draw_FillRGBA(int x, int y, int width, int height, int r, int g, int b, int a)
{
	RecEngDraw_FillRGBA(x, y, width, height, r, g, b, a);

	wrect_t rectborders{ x, x + width, y, y + height }, screenborders{ 0, vid.width, 0, vid.height };

	if (IntersectWRect(&rectborders, &screenborders, &screenborders))
	{
		if (!giScissorTest || IntersectWRect(&scissor_rect, &screenborders, &screenborders))
		{
			int step = vid.rowbytes >> 1;
			word* pdestbuf = &((word*)vid.buffer)[screenborders.left + screenborders.top * step];
			unsigned int* pdestbuf32 = &((unsigned int*)vid.buffer)[screenborders.left + screenborders.top * step / 2];
			int dx = screenborders.right - screenborders.left, dy = screenborders.bottom - screenborders.top;
			int alphachan = (192 * a) & 0xFF00;

			if (r_pixbytes == 1)
			{
				word newcolor = blue_64klut[alphachan + b] | green_64klut[alphachan + g] | red_64klut[alphachan + r];

				if (is15bit)
				{
					for (int i = 0; i < dy; i++)
					{
						for (int j = 0; j < dx; j++)
						{
							word srcpix = pdestbuf[i * step + j];
							pdestbuf[i * step + j] = min(AddColor(srcpix, newcolor, 0x7C00), (long)0x7C00) | min(AddColor(srcpix, newcolor, 0x3E0), (long)0x3E0) | min(AddColor(srcpix, newcolor, 0x1F), (long)0x1F);
						}
					}
				}
				else
				{
					for (int i = 0; i < dy; i++)
					{
						for (int j = 0; j < dx; j++)
						{
							word srcpix = pdestbuf[i * step + j];
							pdestbuf[i * step + j] = min(AddColor(srcpix, newcolor, 0xF800), (long)0xF800) | min(AddColor(srcpix, newcolor, 0x7E0), (long)0x7E0) | min(AddColor(srcpix, newcolor, 0x1F), (long)0x1F);
						}
					}
				}
			}
			else
			{
				int final_b = (r * a) / 255;
				int final_g = (g * a) / 255;
				int final_r = (b * a) / 255;

				for (int i = 0; i < dy; i++)
				{
					for (int j = 0; j < dx; j++)
					{
						color32* pScreenRGB = (color32*)&pdestbuf32[i * step / 2 + j];

						pScreenRGB->r = min(255, pScreenRGB->r + final_r);
						pScreenRGB->g = min(255, pScreenRGB->g + final_g);
						pScreenRGB->b = min(255, pScreenRGB->b + final_b);
						pScreenRGB->a = 255;
					}
				}
			}
		}
	}
}

void Draw_Pic(int x, int y, qpic_t* pic)
{
	byte *source;
	pixel_t* dest;
	pixel_t* dest32;
	int				v, u;


	if ((x < 0) ||
		(x + pic->width > vid.width) ||
		(y < 0) ||
		(y + pic->height > vid.height))
	{
		Sys_Warning("Draw_Pic: bad coordinates");
		return;
	}

	source = pic->data;

	dest = &vid.buffer[(y * (vid.rowbytes >> 1) + x) * 2];
	dest32 = &vid.buffer[(y * (vid.rowbytes >> 2) + x) * 4];

	for (v = 0; v < pic->height; v++)
	{
		for (u = 0; u < pic->width; u++)
			if (r_pixbytes == 1)
				*(word*)&dest[u] = PackedRGB((byte*)&pic[pic->width * pic->height + 2], source[u]);
			else
				*(unsigned int*)&dest32[u] = PackedRGB32((byte*)&pic[pic->width * pic->height + 2], source[u]);

		if (r_pixbytes == 1)
			dest += vid.rowbytes;
		else
			dest32 += vid.rowbytes;
		source += pic->width;
	}
}

void Draw_MiptexTexture(cachewad_t* wad, unsigned char* data)
{
	texture_t* tex;
	miptex_t* mip;
	miptex_t tmp;
	int i;
	int pix;
	int paloffset;
	int mipsize;
	word colorsused;
	byte* pal;

	if (wad->cacheExtra != DECAL_EXTRASIZE)
		Sys_Error(__FUNCTION__ ": Bad cached wad %s\n", wad->name);

	tex = (texture_t*)data;
	mip = (miptex_t*)(data + wad->cacheExtra);
	tmp = *mip;

	tex->width = LittleLong(tmp.width);
	tex->height = LittleLong(tmp.height);
	tex->anim_max = 0;
	tex->anim_min = 0;
	tex->anim_total = 0;
	tex->alternate_anims = 0;
	tex->anim_next = 0;

	for (i = 0; i < MIPLEVELS; i++)
		tex->offsets[i] = wad->cacheExtra + LittleLong(tmp.offsets[i]);

	pix = tex->width * tex->height;
	mipsize = (pix >> 2) + (pix >> 4) + (pix >> 6) + pix;
	paloffset = tex->offsets[0] + mipsize + sizeof(word);
	colorsused = *(word*)((byte*)(&mip[1]) + mipsize);
	pal = (byte*)tex + paloffset;
	tex->paloffset = paloffset;

	if (gfCustomBuild)
	{
		Q_strncpy(tex->name, szCustName, 15);
		tex->name[15] = 0;
	}

	if (pal[765] || pal[766] || pal[767] != 0xFF)
		tex->name[0] = '}';
	else
		tex->name[0] = '{';

	for (i = 0; i < colorsused; i++)
	{
		pal[3 * i] = texgammatable[pal[3 * i] & 0xFF];
		pal[3 * i + 1] = texgammatable[pal[3 * i + 1] & 0xFF];
		pal[3 * i + 2] = texgammatable[pal[3 * i + 2] & 0xFF];
	}
}

int AdjustSubRect(mspriteframe_t *pFrame, int *pw, int *ph, const wrect_t *prcSubRect)
{
	wrect_t rc;
	float f;

	if (!ValidateWRect(prcSubRect))
		return 0;

	// clip sub rect to sprite

	rc.top = rc.left = 0;
	rc.right = *pw;
	rc.bottom = *ph;

	if (!IntersectWRect(prcSubRect, &rc, &rc))
		return 0;

	*pw = rc.right - rc.left;
	*ph = rc.bottom - rc.top;

	return rc.left + rc.top * pFrame->width;
}

int SpriteFrameClip(mspriteframe_t* pFrame, int *x, int *y, int *w, int *h, const wrect_t *prcSubRect)
{
	if (pFrame == NULL)
		return 0;

	*w = pFrame->width;
	*h = pFrame->height;

	int row = 0;

	if (prcSubRect)
	{
		row = AdjustSubRect(pFrame, w, h, prcSubRect);
	}
	else if (giScissorTest)
	{
		if (*x < scissor_x)
		{
			*w = pFrame->width - min(scissor_x - *x, pFrame->width);
			row = scissor_x - *x;
			*x = scissor_x;
		}

		if (*y < scissor_y)
		{
			*h = pFrame->height - min(scissor_y - *y, pFrame->height);
			row += pFrame->width * (scissor_y - *y);
			*y = scissor_y;
		}

		// align by 2
		if (*y + *h > scissor_y2)
			*h = (scissor_y2 - *y) & ((scissor_y2 - (*y < 0)) - 1);
		if (*x + *w > scissor_x2)
			*w = (scissor_x2 - *x) & ((scissor_x2 - (*x < 0)) - 1);
	}

	if (*x < 0)
	{
		*w += *x;
		row -= *x;
		*x = 0;
	}

	if (*y < 0)
	{
		*h += *y;
		row -= *y * pFrame->width;
		*y = 0;
	}

	if (*y + *h > vid.height)
		*h = vid.height - *y;

	if (*x + *w > vid.width)
		*w = vid.width - *x;

	if (*h < 0)
		*h = 0;

	return row;
}

void Draw_SpriteFrame(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect)
{
	int width, height;
	byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, prcSubRect)];
	int step = vid.rowbytes >> 1;
	pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 2];
	
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
			*(word*)&pdestbuf[(j + i * step) * 2] = pPalette[pframedata[j + i * pFrame->width]];
	}
}

void Draw_SpriteFrame32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect)
{
	int width, height;
	byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, prcSubRect)];
	int step = vid.rowbytes >> 2;
	pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 4];
	
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
			*(unsigned int*)&pdestbuf[(j + i * step) * 4] = pPalette[pframedata[j + i * pFrame->width]];
	}
}

void Draw_SpriteFrameHoles(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect)
{
	int width, height;
	byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, prcSubRect)];
	int step = vid.rowbytes >> 1;
	pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 2];

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			if (pframedata[j + i * pFrame->width] != TRANSPARENT_COLOR)
				*(word*)&pdestbuf[(j + i * step) * 2] = pPalette[pframedata[j + i * pFrame->width]];
		}
	}
}

void Draw_SpriteFrameHoles32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect)
{
	int width, height;
	byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, prcSubRect)];
	int step = vid.rowbytes >> 2;
	pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 4];

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			if (pframedata[j + i * pFrame->width] != TRANSPARENT_COLOR)
				*(unsigned int*)&pdestbuf[(j + i * step) * 4] = pPalette[pframedata[j + i * pFrame->width]];
		}
	}
}

void Draw_SpriteFrameAdd15(byte* pSource, word* pPalette, word* pScreen, int width, int height, int screenwidth, int sourcewidth)
{
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			word sprpix = pPalette[pSource[j + sourcewidth * i]];
			if (sprpix)
			{
				word oldpix = pScreen[j + i * screenwidth];
				unsigned carrybits = ((sprpix & mask_g_15) | (sprpix & (mask_r_15 | mask_b_15)) << 16) +
					((oldpix & mask_g_15) | (oldpix & (mask_r_15 | mask_b_15)) << 16);
				if (carrybits & overflow15)
					carrybits |= overflow15 - ((carrybits & overflow15) >> 5);
				pScreen[j + i * screenwidth] = carrybits & mask_g_15 | (carrybits >> 16) & (mask_r_15 | mask_b_15);
			}
		}
	}
}

void Draw_SpriteFrameAdd16(byte* pSource, word* pPalette, word* pScreen, int width, int height, int screenwidth, int sourcewidth)
{
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			word sprpix = pPalette[pSource[j + sourcewidth * i]];
			if (sprpix)
			{
				word oldpix = pScreen[j + i * screenwidth];

				unsigned newcolor = ((sprpix & mask_g_16) | (sprpix & (mask_r_16 | mask_b_16)) << 16);
				unsigned prevcolor = newcolor + ((oldpix & mask_g_16) | (oldpix & (mask_r_16 | mask_b_16)) << 16);

				unsigned carrybits = prevcolor & overflow16 | newcolor > prevcolor;

				if (carrybits)
				{
					carrybits = ROR32(carrybits);
					// saturate
					prevcolor |= ((carrybits | ROR32(overflow16)) - (carrybits >> 5)) << 1;
				}

				// extract green(6 bits) and merge with high part (5 & 5)
				pScreen[j + i * screenwidth] = (prevcolor >> 16) & (mask_r_16 | mask_b_16) | prevcolor & mask_g_16;
			}
		}
	}
}

void Draw_SpriteFrameAdd32(byte* pSource, unsigned int* pPalette, unsigned int* pScreen, int width, int height, int screenwidth, int sourcewidth)
{
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			color32* sprpix = (color32*)&pPalette[pSource[j + sourcewidth * i]];
			if (pPalette[pSource[j + sourcewidth * i]] & 0x00FFFFFF)
			{
				color32* oldpix = (color32*)&pScreen[j + i * screenwidth];

				int r_result = sprpix->r + oldpix->r;
				int g_result = sprpix->g + oldpix->g;
				int b_result = sprpix->b + oldpix->b;

				if (r_result > 255 || g_result > 255 || b_result > 255) 
				{
					int max_comp = max(max(r_result, g_result), b_result);
					float scale = 255.0f / (float)max_comp;
					// saturate
					r_result = (int)((float)r_result * scale);
					g_result = (int)((float)g_result * scale);
					b_result = (int)((float)b_result * scale);
				}

				oldpix->r = r_result;
				oldpix->g = g_result;
				oldpix->b = b_result;
				oldpix->a = 255;
			}
		}
	}
}

void Draw_SpriteFrameAdditive(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect)
{
	int width, height;
	byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, prcSubRect)];
	int step = vid.rowbytes >> 1;
	pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 2];

	if (is15bit)
		Draw_SpriteFrameAdd15(pframedata, pPalette, (word*)pdestbuf, width, height, step, pFrame->width);
	else 
		Draw_SpriteFrameAdd16(pframedata, pPalette, (word*)pdestbuf, width, height, step, pFrame->width);
}

void Draw_SpriteFrameAdditive32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect)
{
	int width, height;
	byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, prcSubRect)];
	int step = vid.rowbytes >> 2;
	pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 4];

	Draw_SpriteFrameAdd32(pframedata, pPalette, (unsigned int*)pdestbuf, width, height, step, pFrame->width);
}

void Draw_SpriteFrameGeneric(mspriteframe_t* pFrame, unsigned short* pPalette, int x, int y, const wrect_t* prcSubRect, int src, int dest, int swidth, int sheight)
{
	int numRows = ceil((float)sheight / (float)pFrame->height);
	int numCols = ceil((float)swidth / (float)pFrame->width);

	int clipHeight = sheight % pFrame->height;
	int clipWidth = swidth % pFrame->width;

	wrect_t subRect;

	subRect.left = 0;
	subRect.top = 0;

	for (int i = 0; i < numRows; i++)
	{
		for (int j = 0; j < numCols; j++)
		{
			if (j == numCols - 1)
				subRect.right = clipWidth;
			else subRect.right = pFrame->width;

			if (i == numRows - 1)
				subRect.bottom = clipHeight;
			else subRect.bottom = pFrame->height;

			int width, height;
			byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, &subRect)];
			int step = vid.rowbytes >> 1;
			pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 2];

			if (is15bit)
				Draw_SpriteFrameAdd15(pframedata, pPalette, (word*)pdestbuf, width, height, step, pFrame->width);
			else
				Draw_SpriteFrameAdd16(pframedata, pPalette, (word*)pdestbuf, width, height, step, pFrame->width);
		}
	}
}

void Draw_SpriteFrameGeneric32(mspriteframe_t* pFrame, unsigned int* pPalette, int x, int y, const wrect_t* prcSubRect, int src, int dest, int swidth, int sheight)
{
	int numRows = ceil((float)sheight / (float)pFrame->height);
	int numCols = ceil((float)swidth / (float)pFrame->width);

	int clipHeight = sheight % pFrame->height;
	int clipWidth = swidth % pFrame->width;

	wrect_t subRect;

	subRect.left = 0;
	subRect.top = 0;

	for (int i = 0; i < numRows; i++)
	{
		for (int j = 0; j < numCols; j++)
		{
			if (j == numCols - 1)
				subRect.right = clipWidth;
			else subRect.right = pFrame->width;

			if (i == numRows - 1)
				subRect.bottom = clipHeight;
			else subRect.bottom = pFrame->height;

			int width, height;
			byte* pframedata = &pFrame->pixels[SpriteFrameClip(pFrame, &x, &y, &width, &height, &subRect)];
			int step = vid.rowbytes >> 2;
			pixel_t* pdestbuf = &vid.buffer[(x + y * step) * 4];

			Draw_SpriteFrameAdd32(pframedata, pPalette, (unsigned int*)pdestbuf, width, height, step, pFrame->width);
		}
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar(char num)
{
}

qpic_t* Draw_SetupConback( int screenwidth )
{
	static char szTryFile[MAX_PATH];

	qpic_t* pic;
	const char* picname;

	if (!firsttime)
	{
		return Draw_CachePic(szTryFile);
	}

	firsttime = 1;

	if (console.value)
		picname = "conback";
	else
		picname = "loading";

	if (screenwidth >= 640)
		snprintf(szTryFile, sizeof(szTryFile), "gfx/%s640.lmp", picname);
	else
		snprintf(szTryFile, sizeof(szTryFile), "gfx/%s400.lmp", picname);

	pic = Draw_CachePic(szTryFile);
	if (pic)
	{
		return pic;
	}

	snprintf(szTryFile, sizeof(szTryFile), "gfx/%s.lmp", picname);
	pic = Draw_CachePic(szTryFile);

	if (!pic)
	{
		Sys_Error("Draw_SetupConback: Couldn't load conback.lmp");
	}

	return pic;
}

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground(int lines)
{
	int				i, x, y, v;
	pixel_t* src, * dest;
	int				f, fstep;
	qpic_t* pConBack;
	short colors[256];
	unsigned int colors32[256];
	char			ver[100];

	pConBack = Draw_SetupConback(vid.width);

	for (i = 0; i < 256; i++)
	{
		colors[i] = PackedRGB(&pConBack->data[pConBack->width * pConBack->height + 2], i);
		colors32[i] = PackedRGB32(&pConBack->data[pConBack->width * pConBack->height + 2], i);
	}

	// draw the pic
	dest = vid.conbuffer;

	for (y = 0; y < lines; y++)
	{
		v = (vid.conheight - lines) * pConBack->height;
		src = &pConBack->data[pConBack->width * (v / vid.conheight)];

		f = 0;
		fstep = (pConBack->width << 16) / vid.conwidth;

		if (r_pixbytes == 1)
		{
			for (x = 0; x < vid.conwidth; x += 4)
			{
				((word*)dest)[x] = colors[src[f >> 16]];
				f += fstep;
				((word*)dest)[x + 1] = colors[src[f >> 16]];
				f += fstep;
				((word*)dest)[x + 2] = colors[src[f >> 16]];
				f += fstep;
				((word*)dest)[x + 3] = colors[src[f >> 16]];
				f += fstep;
			}
		}
		else
		{
			for (x = 0; x < vid.conwidth; x += 4)
			{
				((unsigned int*)dest)[x] = 0xFF000000 | colors32[src[f >> 16]];
				f += fstep;
				((unsigned int*)dest)[x + 1] = 0xFF000000 | colors32[src[f >> 16]];
				f += fstep;
				((unsigned int*)dest)[x + 2] = 0xFF000000 | colors32[src[f >> 16]];
				f += fstep;
				((unsigned int*)dest)[x + 3] = 0xFF000000 | colors32[src[f >> 16]];
				f += fstep;
			}
		}

		v += pConBack->height;
		dest += vid.conrowbytes;
	}

	snprintf(ver, sizeof(ver), "Half-Life %i/%s (sw build %d)", PROTOCOL_VERSION, gpszVersionString, build_number());

	if (console.value && (giSubState & 4) == 0)
		Draw_String(vid.conwidth - Draw_StringLen(ver, VGUI2_GetConsoleFont()), 0, ver);
}

/*
===============
Draw_MessageCharacterAdd

Draws globalAlphaScale single character using the specified font, using additive rendering
===============
*/
int Draw_MessageCharacterAdd(int x, int y, int num, int rr, int gg, int bb, unsigned int font)
{
	return VGUI2_Draw_CharacterAdd(x, y, num, rr, gg, bb, font);
}

void Draw_TileClear(int x, int y, int w, int h)
{
}

int Draw_StringLen(const char* psz, unsigned int font)
{
	return VGUI2_Draw_StringLen(psz, font);
}

int Draw_Character(int x, int y, int num, unsigned int font)
{
	return VGUI2_Draw_Character(x, y, num, font);
}

/*
================
Draw_CachePic
================
*/
qpic_t* Draw_CachePic(char* path)
{
	return (qpic_t*)Draw_CacheGet(menu_wad, Draw_CacheIndex(menu_wad, path));
}

/*
===============
Draw_Init
===============
*/
void Draw_Init(void)
{
	Draw_Shutdown();

	VGUI2_Draw_Init();

	m_bDrawInitialized = true;
	firsttime = 1;

	menu_wad = (cachewad_t*)Mem_Malloc(sizeof(cachewad_t));
	Q_memset(menu_wad, 0, sizeof(cachewad_t));

	Draw_CacheWadInit(const_cast<char*>("cached.wad"), 16, menu_wad);
	Draw_CacheWadHandler(&custom_wad, Draw_MiptexTexture, DECAL_EXTRASIZE);

	Q_memset(decal_names, 0, sizeof(decal_names));

	//
	// get the other pics we need
	//
	draw_disc = (qpic_t*)W_GetLumpName(0, "lambda");

	Draw_GetDefaultColor();
}

void Draw_Fill(int x, int y, int w, int h, int c)
{
	word* dest;
	unsigned int* dest32;
	int				u, v;

	dest = (word*)vid.buffer + x + y * (vid.rowbytes >> 1);
	dest32 = (unsigned int*)vid.buffer + x + y * (vid.rowbytes >> 2);

	for (v = 0; v < h; v++)
	{
		if (r_pixbytes == 1)
		{
			for (u = 0; u < w; u++)
			{
				dest[u] = c;
			}

			dest += vid.rowbytes >> 1;
		}
		else
		{
			for (u = 0; u < w; u++)
			{
				dest32[u] = 0xFF000000 | c;
			}

			dest32 += vid.rowbytes >> 2;
		}
	}
}

qpic_t* Draw_PicFromWad(char* name)
{
	if (name)
		return (qpic_t*)W_GetLumpName(0, name);

	return NULL;
}

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen(void)
{
	int			x, y;
	pixel_t* pbuf;

	S_ExtraUpdate();

	for (y = 0; y < vid.height; y++)
	{
		pbuf = vid.buffer + vid.rowbytes * y;

		for (x = 0; x < vid.width; x++)
		{
			if (r_pixbytes == 1)
			{
				if (is15bit)
					*(word*)&pbuf[2 * x] = (*(word*)&pbuf[2 * x] & 0b1110101011011110) >> 1;
				else
					*(word*)&pbuf[2 * x] = (*(word*)&pbuf[2 * x] & 0b1111011111011110) >> 1;
			}
			else
			{
				color32* pScreenRGB = (color32*)&pbuf[4 * x];

				pScreenRGB->r = pScreenRGB->r >> 1;
				pScreenRGB->g = pScreenRGB->g >> 1;
				pScreenRGB->b = pScreenRGB->b >> 1;
				pScreenRGB->a = 255;
			}
		}
	}

	S_ExtraUpdate();
}

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc(void)
{
}

/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc(void)
{
}
