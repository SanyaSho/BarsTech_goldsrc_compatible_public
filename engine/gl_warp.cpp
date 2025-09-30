// gl_warp.cpp -- sky and water polygons

#include "quakedef.h"
#include "sys.h"
#include "gl_model.h"
#include "cl_spectator.h"
#include "draw.h"
#include "sys_getmodes.h"
#include "r_studio.h"
#include "r_triangle.h"
#include "bitmap_win.h"
#include "view.h"
#include "pm_shared/pm_movevars.h"

int			skytexturenum; // GL_WARP.C:33

colorVec gWaterColor; // GL_WARP.C:38

msurface_t* warpface;

cshift_t cshift_water;

#define SUBDIVIDE_SIZE	64.0

cvar_t	gl_wireframe = { const_cast<char*>("gl_wireframe"), const_cast<char*>("0"), FCVAR_SPONLY }; // draw warframe of the map?

extern cvar_t gl_dither;
extern cvar_t gl_palette_tex;

extern model_t* loadmodel;

void R_DrawSkyBox( void );

void BoundPoly( int numverts, float* verts, vec3_t mins, vec3_t maxs )
{
	int		i, j;
	float* v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;

	v = verts;

	for (i = 0; i < numverts; i++)
	{
		for (j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;

			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

void SubdividePolygon( int numverts, float* verts )
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float* v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t* poly;
	float	s, t;

	if (numverts > 60)
		Sys_Error("numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = SUBDIVIDE_SIZE * floor(m / SUBDIVIDE_SIZE + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy(v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy(v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0))
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	poly = (glpoly_t*)Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	poly->flags = warpface->flags;
	warpface->polys = poly;
	poly->numverts = numverts;

	for (i = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy(verts, poly->verts[i]);
		s = DotProduct(verts, warpface->texinfo->vecs[0]);
		t = DotProduct(verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface( msurface_t* fa )
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float* vec;

	warpface = fa;

	if (fa->numedges > 64)
		Sys_Error("Too many edges in model %s\n", loadmodel->name);

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i = 0; i < fa->numedges; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;

		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon(numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
float turbsin[]
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

void D_SetFadeColor( int r, int g, int b, int fog )
{
	gWaterColor.r = r;
	gWaterColor.g = g;
	gWaterColor.b = b;

	cshift_water.destcolor[0] = r;
	cshift_water.destcolor[1] = g;
	cshift_water.destcolor[2] = b;
	cshift_water.percent = fog;
}

/*
=============
EmitWireFrameWaterPolys
=============
*/
void EmitWireFrameWaterPolys( msurface_t* fa, int direction )
{
	glpoly_t* p;
	float* v;
	int			i;
	float		scale;
	float		tempVert[3];
	
	if (!gl_wireframe.value)
		return;

	if (r_refdef.vieworg[2] <= fa->polys->verts[0][2])
		scale = -currententity->curstate.scale;
	else
		scale = currententity->curstate.scale;

	if (gl_wireframe.value == 2.0)
		qglDisable(GL_DEPTH_TEST);

	qglColor3f(1, 1, 1);

	for (p = fa->polys; p; p = p->next)
	{
		qglBegin(GL_LINE_LOOP);

		if (direction)
			v = p->verts[p->numverts - 1];
		else
			v = (float*)p->verts;

		if (direction)
		{
			for (i = 0; i < p->numverts; i++, v -= VERTEXSIZE)
			{
				tempVert[0] = v[0];
				tempVert[1] = v[1];
				tempVert[2] = (turbsin[(byte)(cl.time * 160.0 + v[0] + v[1])] + 8.0 +
					(turbsin[(byte)(cl.time * 171.0 + v[0] * 5.0 - v[1])] + 8.0) * 0.8) * scale + v[2];

				qglVertex3fv(tempVert);
			}
		}
		else
		{
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
			{
				tempVert[0] = v[0];
				tempVert[1] = v[1];
				tempVert[2] = (turbsin[(byte)(cl.time * 160.0 + v[0] + v[1])] + 8.0 +
					(turbsin[(byte)(cl.time * 171.0 + v[0] * 5.0 - v[1])] + 8.0) * 0.8) * scale + v[2];

				qglVertex3fv(tempVert);
			}
		}

		qglEnd();
	}

	if (gl_wireframe.value == 2.0)
		qglEnable(GL_DEPTH_TEST);
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys( msurface_t* fa, int direction )
{
	glpoly_t* p;
	float* v;
	int			i;
	float		s, t, os, ot, scale;
	float		tempVert[3];
	byte* pal;

	pal = fa->texinfo->texture->pPal;

	D_SetFadeColor(pal[9], pal[10], pal[11], pal[12]);

	scale = currententity->curstate.scale;

	if (r_refdef.vieworg[2] <= fa->polys->verts[0][2])
		scale = -scale;

	for (p = fa->polys; p; p = p->next)
	{
		qglBegin(GL_POLYGON);

		if (direction)
			v = p->verts[p->numverts - 1];
		else
			v = (float*)p->verts;

		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			tempVert[0] = v[0];
			tempVert[1] = v[1];
			tempVert[2] = (turbsin[(byte)(cl.time * 160.0 + v[0] + v[1])] + 8.0 +
				(turbsin[(byte)(cl.time * 171.0 + v[0] * 5.0 - v[1])] + 8.0) * 0.8) * scale + v[2];

			s = os + turbsin[(int)((ot * 0.125 + cl.time) * TURBSCALE) & 255];
			s *= (1.0 / 64);
			
			t = ot + turbsin[(int)((os * 0.125 + cl.time) * TURBSCALE) & 255];
			t *= (1.0 / 64);
			
			qglTexCoord2f(s, t);
			qglVertex3fv(tempVert);

			if (direction)
			{
				v -= 14;
			}
		}
		qglEnd();
	}

	// Draw water wireframe
	EmitWireFrameWaterPolys(fa, direction);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


TargaHeader	targa_header;

int fgetLittleShort( unsigned char** p )
{
	byte	b1, b2;

	b1 = (*p)[0];
	b2 = (*p)[1];

	*p += 2;

	return (short)(b1 + b2 * 256);
}

int fgetLittleLong( unsigned char** p )
{
	byte	b1, b2, b3, b4;

	b1 = (*p)[0];
	b2 = (*p)[1];
	b3 = (*p)[2];
	b4 = (*p)[3];

	*p += 4;

	return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
}


/*
=============
LoadTGA2
=============
*/
qboolean LoadTGA2( char* szFilename, unsigned char* buffer, int bufferSize, int* width, int* height, int errFail )
{
	unsigned char* pfilebuf;
	unsigned char* pfiledata;
	unsigned char* end;

	int				columns, rows, numPixels;
	int				size;
	byte* pixbuf;
	int				row, column;

	int fileLen;
	pfilebuf = COM_LoadFile(szFilename, 5, &fileLen);

	if (!pfilebuf)
		return false;

	pfiledata = pfilebuf;

	end = &pfilebuf[fileLen];

	if (width)
		*width = 0;
	if (height)
		*height = 0;

	// Check the file size
	if (fileLen < sizeof(TargaHeader))
	{
		if (errFail)
			Sys_Error("LoadTGA: file shorter than TGA file header length\n");

		COM_FreeFile(pfilebuf);
		Con_Printf(const_cast<char*>("LoadTGA: file shorter than TGA file header length\n"));
		return false;
	}

	targa_header.id_length = *pfilebuf;
	pfilebuf++;

	targa_header.colormap_type = *pfilebuf;
	pfilebuf++;

	targa_header.image_type = *pfilebuf;
	pfilebuf++;

	targa_header.colormap_index = fgetLittleShort(&pfilebuf);
	targa_header.colormap_length = fgetLittleShort(&pfilebuf);
	targa_header.colormap_size = *pfilebuf;
	pfilebuf++;

	targa_header.x_origin = fgetLittleShort(&pfilebuf);
	targa_header.y_origin = fgetLittleShort(&pfilebuf);

	targa_header.width = fgetLittleShort(&pfilebuf);
	targa_header.height = fgetLittleShort(&pfilebuf);

	targa_header.pixel_size = *pfilebuf;
	pfilebuf++;

	targa_header.attributes = *pfilebuf;
	pfilebuf++;

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
	{
		if (errFail)
			Sys_Error("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

		COM_FreeFile(pfilebuf);
		Con_Printf(const_cast<char*>("LoadTGA: Only type 2 and 10 targa RGB images supported\n"));
		return false;
	}

	if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
	{
		if (errFail)
			Sys_Error("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

		COM_FreeFile(pfilebuf);
		Con_Printf(const_cast<char*>("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n"));
		return false;
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (numPixels > 0x1FFFFFFF)
		return false;

	size = numPixels * 4;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	if (size > bufferSize)
	{
		if (errFail)
			Sys_Error("LoadTGA: texture too large (%ux%u>256x256)\n", columns, rows);

		COM_FreeFile(pfiledata);
		Con_Printf(const_cast<char*>("LoadTGA: texture too large (%ux%u>256x256)\n"), columns, rows);
		return false;
	}

	if (targa_header.id_length != 0)
		pfilebuf += targa_header.id_length;  // skip TARGA image comment

	if (targa_header.image_type == 2)	// Uncompressed, RGB images
	{
		int pos;
	
		if (targa_header.pixel_size == 24)
			pos = numPixels * 3;
		else
			pos = numPixels * 4;

		if (&pfilebuf[pos] > end)
		{
			if (errFail)
				Sys_Error("LoadTGA: file shorter than length of texture\n");

			COM_FreeFile(pfiledata);
			Con_Printf(const_cast<char*>("LoadTGA: file shorter than length of texture\n"));
			return false;
		}
		
		for (row = rows - 1; row >= 0; row--)
		{
			pixbuf = buffer + row * columns * 4;

			for (column = 0; column < columns; column++)
			{
				unsigned char red, green, blue, alphabyte;

				switch (targa_header.pixel_size)
				{
					case 24:
						blue = pfilebuf[0];
						green = pfilebuf[1];
						red = pfilebuf[2];

						pfilebuf += 3;

						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;

					case 32:
						blue = pfilebuf[0];
						green = pfilebuf[1];
						red = pfilebuf[2];
						alphabyte = pfilebuf[3];

						pfilebuf += 4;

						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						break;
				}
			}
		}
	}
	else if (targa_header.image_type == 10)	// Runlength encoded RGB images
	{  
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;

		for (row = rows - 1; row >= 0; row--)
		{
			pixbuf = buffer + row * columns * 4;

			for (column = 0; column < columns; )
			{
				packetHeader = *pfilebuf;
				pfilebuf++;

				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80)	// run-length packet
				{
					switch (targa_header.pixel_size)
					{
						case 24:
							blue = pfilebuf[0];
							green = pfilebuf[1];
							red = pfilebuf[2];
							pfilebuf += 3;

							alphabyte = 255;
							break;

						case 32:
							blue = pfilebuf[0];
							green = pfilebuf[1];
							red = pfilebuf[2];
							alphabyte = pfilebuf[3];
							pfilebuf += 4;
							break;
					}

					for (j = 0; j < packetSize; j++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;

						column++;

						if (column == columns)
						{
							// run spans across rows
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							pixbuf = buffer + row * columns * 4;
						}
					}
				}
				else
				{
					// non run-length packet
					for (j = 0; j < packetSize; j++)
					{
						switch (targa_header.pixel_size)
						{
							case 24:
								blue = pfilebuf[0];
								green = pfilebuf[1];
								red = pfilebuf[2];
								pfilebuf += 3;

								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = 255;
								break;

							case 32:
								blue = pfilebuf[0];
								green = pfilebuf[1];
								red = pfilebuf[2];
								alphabyte = pfilebuf[3];
								pfilebuf += 4;

								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;
								break;
						}

						column++;

						if (column == columns)	// pixel packet run spans across rows
						{
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							pixbuf = buffer + row * columns * 4;
						}
					}
				}
			}
		breakOut:;
		}
	}

	COM_FreeFile(pfiledata);

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	return true;
}

/*
=============
LoadTGA
=============
*/
qboolean LoadTGA( char* szFilename, unsigned char* buffer, int bufferSize, int* width, int* height )
{
	return LoadTGA2(szFilename, buffer, bufferSize, width, height, FALSE);
}

/*
================
R_LoadSkys

Loads up and setup sky textures
================
*/
// extensions come after skyname_, used to determine/store the side of sky cube
const char* suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

int gLoadSky;
GLuint gSkyTexNumber[6];

#define MAX_SKY_TGA_RESOLUTION	(256 * 256) * 4

void R_LoadSkys( void )
{
	int		i, j, palCount;
	int		bmpWidth, bmpHeight;	// width and height of loaded BMP texture
	int		tgaWidth, tgaHeight;	// width and height of loaded TGA texture

	bool	bTGAsky;	// if true, skip BMP texture loading and load only TGA
	bool	printed = false; // was skyloading data printed to console?
	bool	paletted = false;

	FileHandle_t hFile;
	char	name[MAX_QPATH];

	byte* pImage = NULL;
	byte* buffer = NULL; // TGA Buffer
	byte* packPalette = NULL;

	if (!gLoadSky)
	{
		for (i = 0; i < 6; i++)
		{
			if (gSkyTexNumber[i])
			{
				qglDeleteTextures(GL_ONE, &gSkyTexNumber[i]);
				gSkyTexNumber[i] = 0;
			}
		}
	}

	if (qglColorTableEXT && gl_palette_tex.value)
	{
		GL_PaletteClearSky();
		bTGAsky = false;
	}
	else
	{
		bTGAsky = true;
	}

	// 256 * 256 resolution
	buffer = (byte*)Mem_Malloc(MAX_SKY_TGA_RESOLUTION);

	for (i = 0; i < 6; i++)
	{
		paletted = false;
		snprintf(name, sizeof(name), "gfx/env/%s%s.bmp", movevars.skyName, suf[i]);

		if (!bTGAsky)
		{
			// Look up for bitmap texture file
			hFile = FS_Open(name, "rb");

			if (hFile)
			{
				// Load BMP texture
				LoadBMP8(hFile, &packPalette, &palCount, &pImage, &bmpWidth, &bmpHeight);

				if (bmpHeight * bmpWidth > 0)
				{
					if (GL_PaletteAdd(packPalette, true) >= 0)
					{
						if (!gSkyTexNumber[i])
							gSkyTexNumber[i] = GL_GenTexture();
						
						GL_Bind(gSkyTexNumber[i]);
						qglTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_COLOR_INDEX8_EXT, GL_ACCUM, GL_ACCUM, GL_ZERO, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, pImage);
						paletted = true;
					}

					Mem_Free(pImage);
					Mem_Free(packPalette);
				}

				if (paletted)
				{
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

					if (!printed)
					{
						Con_DPrintf(const_cast<char*>("SKY:  "));
						printed = true;
					}

					if (!paletted)
					{
						Con_DPrintf(const_cast<char*>("%s%s, "), movevars.skyName, suf[i]);
					}
					else
					{
						Con_DPrintf(const_cast<char*>("%s%s - paletted, "), movevars.skyName, suf[i]);
					}
					continue;
				}		
			}
		}

		if (!paletted)
		{
			bTGAsky = true; // mark sky as TGA-textured
			snprintf(name, sizeof(name), "gfx/env/%s%s.tga", movevars.skyName, suf[i]);

			// Loads up, setups, and get the information about an actual targa image.
			if (!LoadTGA(name, buffer, MAX_SKY_TGA_RESOLUTION, &tgaWidth, &tgaHeight))
			{
				Con_Printf(const_cast<char*>("R_LoadSkys: Couldn't load %s\n"), name);

				if (i == 0 && Q_strcmp(movevars.skyName, "desert"))
				{
					Q_strcpy(movevars.skyName, "desert");
					i = 0;
				}

				continue;
			}

			if (gl_dither.value)
			{
				byte* pBuf = buffer;

				for (j = 0; j < MAX_SKY_TGA_RESOLUTION; j += 4)
				{
					if (pBuf[0] <= 0xFB)
					{
						pBuf[0] = pBuf[0] | (pBuf[0] >> 6);
						pBuf[0] = texgammatable[pBuf[0]];
					}

					if (pBuf[1] <= 0xFB)
					{
						pBuf[1] = pBuf[1] | (pBuf[1] >> 6);
						pBuf[1] = texgammatable[pBuf[1]];
					}

					if (pBuf[2] <= 0xFB)
					{
						pBuf[2] = pBuf[2] | (pBuf[2] >> 6);
						pBuf[2] = texgammatable[pBuf[2]];
					}

					pBuf += 4;
				}
			}

			if (!gSkyTexNumber[i])
				gSkyTexNumber[i] = GL_GenTexture();

			GL_Bind(gSkyTexNumber[i]);

			// Get information about current videomode.
			int w, h, bpp;
			VideoMode_GetCurrentVideoMode(&w, &h, &bpp);

			if (bpp == 32)
				qglTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RGBA8_EXT, GL_ACCUM, GL_ACCUM, GL_ZERO, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
			else
				qglTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RGB5_A1_EXT, GL_ACCUM, GL_ACCUM, GL_ZERO, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (!printed)
			{
				Con_DPrintf(const_cast<char*>("SKY:  "));
				printed = true;
			}

			if (!paletted)
			{
				Con_DPrintf(const_cast<char*>("%s%s, "), movevars.skyName, suf[i]);
			}
			else
			{
				Con_DPrintf(const_cast<char*>("%s%s - paletted, "), movevars.skyName, suf[i]);
			}
		}
	}

	// we are done with sky loading
	if (printed)
		Con_DPrintf(const_cast<char*>("done\n"));

	Mem_Free(buffer);

	gLoadSky = false; // mark as loaded
}

vec3_t skyclip[6] =
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down

//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}

	//	{-1,2,3},
	//	{1,2,-3}
};

float	skymins[2][6], skymaxs[2][6];
float	sky_min, sky_max;
// REVISIT: Do we want to spend this much CPU to save fill rate?
void DrawSkyPolygon( int nump, vec_t* vecs )
{
	vec3_t	v, av;
	int		i, j;
	float	s, t, dv;
	int		axis;
	float* vp;

	c_sky++;

	// decide which face it maps to
	VectorCopy(vec3_origin, v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
	{
		VectorAdd(vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}


#define	MAX_CLIP_VERTS	64

void ClipSkyPolygon( int nump, vec_t* vecs, int stage )
{
	float* norm = NULL;
	float* v = NULL;

	bool	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS - 2)
		Sys_Error("ClipSkyPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{
		// fully clipped, so draw it
		DrawSkyPolygon(nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		d = DotProduct(v, norm);

		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;

		dists[i] = d;
	}

	if (!front || !back)
	{
		// not clipped
		ClipSkyPolygon(nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy(vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
			case SIDE_FRONT:
				VectorCopy(v, newv[0][newc[0]]);
				newc[0]++;
				break;

			case SIDE_BACK:
				VectorCopy(v, newv[1][newc[1]]);
				newc[1]++;
				break;

			case SIDE_ON:
				VectorCopy(v, newv[0][newc[0]]);
				newc[0]++;
				VectorCopy(v, newv[1][newc[1]]);
				newc[1]++;
				break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i + 1]);

		for (j = 0; j < 3; j++)
		{
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}

		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon(newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon(newc[1], newv[1][0], stage + 1);
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain( msurface_t* s )
{
	vec3_t verts[MAX_CLIP_VERTS];

	msurface_t* fa;

	if (CL_IsDevOverviewMode())
		return;

	c_sky = 0;

	for (fa = s; fa; fa = fa->texturechain)
	{
		for (glpoly_t* p = fa->polys; p; p = p->next)
		{
			for (int i = 0; i < p->numverts; i++)
				VectorSubtract(p->verts[i], r_origin, verts[i]);

			ClipSkyPolygon(p->numverts, verts[0], 0);
		}
	}

	R_DrawSkyBox();
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox( void )
{
	int		i;

	for (i = 0; i < 6; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

#define SQRT3INV		(0.57735f)		// a little less than 1 / sqrt(3)

void MakeSkyVec( float s, float t, int axis )
{
	vec3_t		v, b;
	int			j, k;

	float width = movevars.zmax * SQRT3INV;

	if (s < -1)
		s = -1;
	else if (s > 1)
		s = 1;
	if (t < -1)
		t = -1;
	else if (t > 1)
		t = 1;

	b[0] = s * width;
	b[1] = t * width;
	b[2] = width;

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	if (s < 1.0 / 512)
		s = 1.0 / 512;
	else if (s > 511.0 / 512)
		s = 511.0 / 512;
	if (t < 1.0 / 512)
		t = 1.0 / 512;
	else if (t > 511.0 / 512)
		t = 511.0 / 512;

	t = 1.0 - t;
	qglTexCoord2f(s, t);
	qglVertex3fv(v);
}

/*
=================
R_DrawSkyBox
=================
*/
int skytexorder[6] = { 0, 2, 1, 3, 4, 5 };
#define SIGN(d)				((d)<0?-1:1)
static int	gFakePlaneType[6] = { 1, -1, 2, -2, 3, -3 };
void R_DrawSkyBox( void )
{
	int		i;
	vec3_t	normal;

	float r = 1.0;
	float g = 1.0;
	float b = 1.0;

	bool fog = false;

	if (filterMode)
	{
		r = filterColorRed * filterBrightness;
		g = filterColorGreen * filterBrightness;
		b = filterBrightness * filterColorBlue;
	}

	GL_DisableMultitexture();

	if (!g_bFogSkybox)
	{
		if (qglIsEnabled(GL_FOG) == GL_TRUE)
		{
			fog = true;
			qglDisable(GL_FOG);
		}
	}

	for (i = 0; i < 6; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		VectorCopy(vec3_origin, normal);

		switch (gFakePlaneType[i])
		{
		case 1:
			normal[0] = 1;
			break;

		case -1:
			normal[0] = -1;
			break;

		case 2:
			normal[1] = 1;
			break;

		case -2:
			normal[1] = -1;
			break;

		case 3:
			normal[2] = 1;
			break;

		case -3:
			normal[2] = -1;
			break;
		}

		// Hack, try to find backfacing surfaces on the inside of the cube to avoid binding their texture
		if (DotProduct(vpn, normal) < (-1 + 0.70710678))
			continue;

		GL_Bind(gSkyTexNumber[skytexorder[i]]);

		qglColor3f(r, g, b);

		qglBegin(GL_QUADS);
		MakeSkyVec(skymins[0][i], skymins[1][i], i);
		MakeSkyVec(skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec(skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec(skymaxs[0][i], skymins[1][i], i);
		qglEnd();
	}

	if (fog)
		qglEnable(GL_FOG);
}

/*
==================
R_InitSky
==================
*/
void R_InitSky( void )
{
	gLoadSky = true;
}