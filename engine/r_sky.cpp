//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// r_sky.cpp

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"
#include "vid.h"
#include "r_shared.h"
#include "bspfile.h"
#include "com_model.h"
#include "pm_shared/pm_movevars.h"
#include "color.h"
#include "bitmap_win.h"
#include "cl_entity.h"

void R_SetupSky( void );
byte* R_LoadTGA(char* szFilename, byte* buffer, int bufferSize, int* width, int* height, qboolean normalSize);

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

short PutDitheredRGB( colorVec* pcv )
{
	pcv->r |= ((unsigned int)pcv->r >> 6) & 3;
	pcv->g |= ((unsigned char)pcv->g >> 6);
	pcv->b |= ((unsigned char)pcv->b >> 6);

	if (is15bit)
		return PACKEDRGB555(pcv->r, pcv->g, pcv->b);
	else
		return PACKEDRGB565(pcv->r, pcv->g, pcv->b);
}

/*
=============
LoadTGA
=============
*/
qboolean LoadTGA(char* szFilename, byte* buffer, int bufferSize, int* width, int* height)
{
	return R_LoadTGA(szFilename, buffer, bufferSize, width, height, TRUE) != NULL;
}

/*
=============
R_LoadTGA
=============
*/
byte* R_LoadTGA(char* szFilename, byte* buffer, int bufferSize, int* width, int* height, qboolean normalSize)
{
	int				columns, rows, numPixels;
	int				targaPixelBytes;
	byte* pixbuf;
	unsigned char* targa_rgba;
	int				row, column;
	TargaHeader		targa_header;
	unsigned char* tgafile_mem_buffer;
	unsigned char* p;
	unsigned char* end;
	int				requiredBufferBytes;
	colorVec		color;

	int fileLen;
	tgafile_mem_buffer = COM_LoadFile(szFilename, 5, &fileLen);

	if (!tgafile_mem_buffer)
		return NULL;

	p = tgafile_mem_buffer;

	end = &tgafile_mem_buffer[fileLen];

	if (width)
		*width = 0;
	if (height)
		*height = 0;

	// Check the file size
	if (fileLen < sizeof(TargaHeader))
	{
		COM_FreeFile(tgafile_mem_buffer);
		Con_DPrintf(const_cast<char*>("R_LoadTGA: file shorter than TGA file header length\n"));
		return NULL;
	}

	targa_header.id_length = *tgafile_mem_buffer;
	tgafile_mem_buffer++;

	targa_header.colormap_type = *tgafile_mem_buffer;
	tgafile_mem_buffer++;

	targa_header.image_type = *tgafile_mem_buffer;
	tgafile_mem_buffer++;

	fgetLittleShort(&tgafile_mem_buffer);
	fgetLittleShort(&tgafile_mem_buffer);

	tgafile_mem_buffer++;

	fgetLittleShort(&tgafile_mem_buffer);
	fgetLittleShort(&tgafile_mem_buffer);

	targa_header.width = fgetLittleShort(&tgafile_mem_buffer);
	targa_header.height = fgetLittleShort(&tgafile_mem_buffer);

	targa_header.pixel_size = *tgafile_mem_buffer;
	tgafile_mem_buffer++;

	tgafile_mem_buffer++;

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
	{
		COM_FreeFile(tgafile_mem_buffer);
		Con_DPrintf(const_cast<char*>("R_LoadTGA: Only type 2 and 10 targa RGB images supported\n"));
		return NULL;
	}

	if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
	{
		COM_FreeFile(tgafile_mem_buffer);
		Con_DPrintf(const_cast<char*>("R_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n"));
		return NULL;
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (targa_header.image_type != 2)
	{
		targaPixelBytes = 2;
	}
	else if (normalSize)
	{
		targaPixelBytes = 4;
	}
	else
	{
		targaPixelBytes = 2;
	}

	if (numPixels > 0x7FFFFFFF / targaPixelBytes)
		return NULL;

	requiredBufferBytes = numPixels * targaPixelBytes;

	if (buffer)
	{
		targa_rgba = buffer;

		if (requiredBufferBytes > bufferSize)
		{
			COM_FreeFile(p);
			Con_DPrintf(const_cast<char*>("R_LoadTGA: texture too large (%ux%u)\n"), columns, rows);
			return NULL;
		}
	}
	else
	{
		targa_rgba = (unsigned char*)Hunk_AllocName(requiredBufferBytes, const_cast<char*>("SKYBOX"));
	}

	if (targa_header.id_length != 0)
		tgafile_mem_buffer += targa_header.id_length;  // skip TARGA image comment

	if (targa_header.image_type == 2)	// Uncompressed, RGB images
	{
		int pos;

		if (targa_header.pixel_size == 24)
			pos = numPixels * 3;
		else
			pos = numPixels * 4;

		if (&tgafile_mem_buffer[pos] > end)
		{
			COM_FreeFile(p);
			Con_DPrintf(const_cast<char*>("R_LoadTGA: file shorter than length of texture\n"));
			return NULL;
		}

		for (row = rows - 1; row >= 0; row--)
		{
			if (normalSize)
				pixbuf = targa_rgba + row * columns * 4;
			else
				pixbuf = targa_rgba + row * columns * 2;

			for (column = 0; column < columns; column++)
			{
				unsigned char red, green, blue, alphabyte;

				switch (targa_header.pixel_size)
				{
				case 24:
					blue = tgafile_mem_buffer[0];
					green = tgafile_mem_buffer[1];
					red = tgafile_mem_buffer[2];
					tgafile_mem_buffer += 3;

					if (normalSize)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
					}
					else
					{
						color.r = red;
						color.g = green;
						color.b = blue;
						*(word*)pixbuf = PutDitheredRGB(&color);
						pixbuf += 2;
					}
					break;

				case 32:
					blue = tgafile_mem_buffer[0];
					green = tgafile_mem_buffer[1];
					red = tgafile_mem_buffer[2];
					alphabyte = tgafile_mem_buffer[3];
					tgafile_mem_buffer += 4;

					if (normalSize)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
					}
					else
					{
						color.r = red;
						color.g = green;
						color.b = blue;
						*(word*)pixbuf = PutDitheredRGB(&color);
						pixbuf += 2;
					}
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
			pixbuf = targa_rgba + row * columns * 2;

			for (column = 0; column < columns; )
			{
				packetHeader = *tgafile_mem_buffer;
				tgafile_mem_buffer++;

				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80)	// run-length packet
				{
					switch (targa_header.pixel_size)
					{
					case 24:
						blue = tgafile_mem_buffer[0];
						green = tgafile_mem_buffer[1];
						red = tgafile_mem_buffer[2];

						color.r = red;
						color.g = green;
						color.b = blue;

						tgafile_mem_buffer += 3;

						alphabyte = 255;
						break;

					case 32:
						blue = tgafile_mem_buffer[0];
						green = tgafile_mem_buffer[1];
						red = tgafile_mem_buffer[2];
						alphabyte = tgafile_mem_buffer[3];

						color.r = red;
						color.g = green;
						color.b = blue;

						tgafile_mem_buffer += 4;
						break;
					}

					for (j = 0; j < packetSize; j++)
					{
						*(word*)pixbuf = PutDitheredRGB(&color);
						pixbuf += 2;

						column++;

						if (column == columns)
						{
							// run spans across rows
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							pixbuf = targa_rgba + row * columns * 2;
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
							blue = tgafile_mem_buffer[0];
							green = tgafile_mem_buffer[1];
							red = tgafile_mem_buffer[2];

							color.r = red;
							color.g = green;
							color.b = blue;

							tgafile_mem_buffer += 3;

							*(word*)pixbuf = PutDitheredRGB(&color);
							pixbuf += 2;
							break;

						case 32:
							blue = tgafile_mem_buffer[0];
							green = tgafile_mem_buffer[1];
							red = tgafile_mem_buffer[2];
							alphabyte = tgafile_mem_buffer[3];

							color.r = red;
							color.g = green;
							color.b = blue;

							tgafile_mem_buffer += 4;

							*(word*)pixbuf = PutDitheredRGB(&color);
							pixbuf += 2;
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

							pixbuf = targa_rgba + row * columns * 2;
						}
					}
				}
			}
		breakOut:;
		}
	}

	COM_FreeFile(p);

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	return targa_rgba;
}

//=========================================================

/*
================
R_LoadSkys

Loads up and setup sky textures
================
*/
// extensions come after skyname_, used to determine/store the side of sky cube
const char* suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

const int skytexorder[6] = { 0, 2, 1, 3, 4, 5 };

int gLoadSky;
int gDrawSky;

pixel_t* gSkies[6];
pixel_t* gFilteredSkies[6];

cl_entity_t gFakeEntity;

void R_LoadSkys( void )
{
	int		i;

	qboolean bBMPLoad;	// true if BMP texture was loaded
	qboolean printed;	// was skyloading data printed to console?
	
	FileHandle_t hFile;
	char	name[MAX_QPATH];

	printed = FALSE;

	if (!gLoadSky)
		return;

tryagain:
	bBMPLoad = FALSE;
	for (i = 0; i < 6; i++)
	{
		_snprintf(name, sizeof(name), "gfx/env/%s%s.tga", movevars.skyName, suf[i]);

		if (bBMPLoad)
		{
			bBMPLoad = TRUE;

			_snprintf(name, sizeof(name), "gfx/env/%s%s.bmp", movevars.skyName, suf[i]);

			hFile = FS_Open(name, "rb");

			// Look up for bitmap texture file
			if (hFile)
			{
				// Load BMP texture
				gSkies[i] = LoadBMP16(hFile, is15bit);
				
				if (gSkies[i])
				{
					if (!printed)
					{
						Con_DPrintf(const_cast<char*>("SKY:  "));
						printed = TRUE;
					}

					Con_DPrintf(const_cast<char*>("%s%s, "), movevars.skyName, suf[i]);
					// 128 KiB - max /filtered/ size -> 256x256x16bit
					gFilteredSkies[i] = (byte*)Hunk_AllocName(256 * 256 * sizeof(word), const_cast<char*>("FILTEREDSKYBOX"));
				}
				else
				{
					Con_Printf(const_cast<char*>("R_LoadSkys: Couldn't load %s\n"), name);
					gSkies[i] = NULL;

					if (i == 0 && Q_strcmp(movevars.skyName, "desert"))
					{
						Cvar_Set(const_cast<char*>("movevars.skyname"), const_cast<char*>("desert"));
						goto tryagain;
					}
				}
			}
			else
			{
				gFilteredSkies[i] = (byte*)Hunk_AllocName(256 * 256 * sizeof(word), const_cast<char*>("FILTEREDSKYBOX"));
			}
		}
		else
		{
			// Loads up, setups, and get the information about an actual targa image.
			gSkies[i] = R_LoadTGA(name, NULL, 0, 0, 0, false);

			if (gSkies[i])
			{
				if (!printed)
				{
					Con_DPrintf(const_cast<char*>("SKY:  "));
					printed = TRUE;
				}

				Con_DPrintf(const_cast<char*>("%s%s, "), movevars.skyName, suf[i]);

				gFilteredSkies[i] = (byte*)Hunk_AllocName(256 * 256 * sizeof(word), const_cast<char*>("FILTEREDSKYBOX"));
			}
			else
			{
				Con_Printf(const_cast<char*>("R_LoadSkys: Couldn't load %s\n"), name);
				gSkies[i] = NULL;

				if (i == 0 && Q_strcmp(movevars.skyName, "desert"))
				{
					Cvar_Set(const_cast<char*>("movevars.skyname"), const_cast<char*>("desert"));
					goto tryagain;
				}
			}
		}
	}

	// we are done with sky loading
	if (printed)
		Con_DPrintf(const_cast<char*>("done\n"));

	R_SetupSky();

	gLoadSky = false; // mark as loaded
}

/*
=================
R_SetupSky

=================
*/
mtexinfo_t gFakeTexInfo[6];

msurface_t gFakeSurface[6];
mplane_t gFakePlanes[6];

bedge_t gFakeEdges[4];
texture_t gFakeTex;

mvertex_t gFakeVerts[4];

const int	gFakePlaneType[6] = { 1, -1, -2, 2, 3, -3 };
void R_SetupSky( void )
{
	int		i;
	msurface_t*		psurf;
	mplane_t*		pplane;
	mtexinfo_t*		ptexinfo;

	gFakeEdges[0].v[0] = &gFakeVerts[0];
	gFakeEdges[0].v[1] = &gFakeVerts[1];
	gFakeEdges[1].v[0] = &gFakeVerts[1];
	gFakeEdges[1].v[1] = &gFakeVerts[2];
	gFakeEdges[2].v[0] = &gFakeVerts[2];
	gFakeEdges[2].v[1] = &gFakeVerts[3];
	gFakeEdges[3].v[0] = &gFakeVerts[3];
	gFakeEdges[3].v[1] = &gFakeVerts[0];

	gFakeEdges[0].pnext = &gFakeEdges[1];
	gFakeEdges[1].pnext = &gFakeEdges[2];
	gFakeEdges[2].pnext = &gFakeEdges[3];
	gFakeEdges[3].pnext = NULL;

	gFakeTex.width = 256;
	gFakeTex.height = 256;

	Q_memset(gFakeSurface, 0, sizeof(gFakeSurface));
	Q_memset(gFakeTexInfo, 0, sizeof(gFakeTexInfo));

	// Initialize all sky surfaces
	for (i = 0; i < 6; i++)
	{
		psurf = &gFakeSurface[i];
		ptexinfo = &gFakeTexInfo[i];
		pplane = &gFakePlanes[i];

		psurf->extents[0] = 256;
		psurf->extents[1] = 256;
		psurf->texturemins[0] = 0;
		psurf->texturemins[1] = 0;

		psurf->texinfo = (mtexinfo_t*)ptexinfo;
		psurf->plane = pplane;
		psurf->samples = (color24*)gSkies[skytexorder[i]];
		psurf->flags = SURF_DRAWSKY;

		VectorClear(pplane->normal);
		VectorClear(ptexinfo->vecs[0]);
		VectorClear(ptexinfo->vecs[1]);

		ptexinfo->texture = &gFakeTex;

		switch (gFakePlaneType[i])
		{
		case 1:
			ptexinfo->vecs[0][1] = -0.0625;
			ptexinfo->vecs[1][2] = -0.0625;

			pplane->normal[0] = 1;
			pplane->type = PLANE_X;

			pplane->dist = 2048;
			break;

		case -1:
			ptexinfo->vecs[0][1] = 0.0625;
			ptexinfo->vecs[1][2] = -0.0625;

			pplane->normal[0] = 1;
			pplane->type = PLANE_X;

			pplane->dist = -2048;
			break;

		case 2:
			ptexinfo->vecs[0][0] = 0.0625;
			ptexinfo->vecs[1][2] = 0.0625;

			pplane->normal[1] = 1;
			pplane->type = PLANE_Y;

			pplane->dist = 2048;
			break;

		case -2:
			ptexinfo->vecs[0][0] = -0.0625;
			ptexinfo->vecs[1][2] = 0.0625;

			pplane->normal[1] = 1;
			pplane->type = PLANE_Y;

			pplane->dist = -2048;
			break;

		case 3:
			ptexinfo->vecs[1][0] = 0.0625;
			ptexinfo->vecs[0][1] = -0.0625;

			pplane->normal[2] = 1;
			pplane->type = PLANE_Z;

			pplane->dist = 2048;
			break;

		case -3:
			ptexinfo->vecs[1][0] = -0.0625;
			ptexinfo->vecs[0][1] = -0.0625;

			pplane->normal[2] = 1;
			pplane->type = PLANE_Z;

			pplane->dist = -2048;
			break;
		}

		ptexinfo->mipadjust = 1.0;
		ptexinfo->vecs[0][3] = 128;
		ptexinfo->vecs[1][3] = 128;
	}
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
void R_DrawSkyChain(msurface_t* s)
{
	vec3_t verts[MAX_CLIP_VERTS];
	int			i, lindex;
	medge_t* pedge, * pedges;

	gDrawSky = TRUE;

	pedges = currententity->model->edges;

	if (s->numedges > 0)
	{
		for (i = 0; i < s->numedges; i++)
		{
			lindex = currententity->model->surfedges[s->firstedge + i];

			if (lindex > 0)
			{
				pedge = &pedges[lindex];
				verts[i][0] = r_pcurrentvertbase[pedge->v[0]].position[0] - r_origin[0];
				verts[i][1] = r_pcurrentvertbase[pedge->v[0]].position[1] - r_origin[1];
				verts[i][2] = r_pcurrentvertbase[pedge->v[0]].position[2] - r_origin[2];
			}
			else
			{
				lindex = -lindex;
				pedge = &pedges[lindex];
				verts[i][0] = r_pcurrentvertbase[pedge->v[1]].position[0] - r_origin[0];
				verts[i][1] = r_pcurrentvertbase[pedge->v[1]].position[1] - r_origin[1];
				verts[i][2] = r_pcurrentvertbase[pedge->v[1]].position[2] - r_origin[2];
			}
		}

		ClipSkyPolygon(s->numedges, verts[0], 0);
	}
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox(void)
{
	int		i;

	for (i = 0; i < 6; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}

	gDrawSky = FALSE;
}

void MakeSkyVec( float s, float t, int axis, float* pos )
{
	vec3_t		b;
	int			j, k;

	if (s < -1)
		s = -1;
	else if (s > 1)
		s = 1;
	if (t < -1)
		t = -1;
	else if (t > 1)
		t = 1;

	b[0] = s * 2048.0;
	b[1] = t * 2048.0;
	b[2] = 2048.0;

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			pos[j] = -b[-k - 1];
		else
			pos[j] = b[k - 1];
	}
}

/*
=================
R_DrawSkyBox
=================
*/
void R_DrawSkyBox( int sortKey )
{
	int i;
	vec3_t oldmodelorg;

	if (!gDrawSky)
		return;

	if (r_draworder.value)
		r_currentbkey = 1;
	else
		r_currentbkey = sortKey + 1;

	// Store old origin
	VectorCopy(modelorg, oldmodelorg);
	VectorCopy(r_origin, r_entorigin);
	VectorCopy(r_entorigin, gFakeEntity.origin);

	r_clipflags = ALIAS_XY_CLIP_MASK;

	VectorClear(modelorg);
	VectorClear(r_worldmodelorg);

	currententity = &gFakeEntity;
	VectorClear(gFakeEntity.angles);

	R_RotateBmodel();

	insubmodel = true;

	for (i = 0; i < 6; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		MakeSkyVec(skymins[0][i], skymins[1][i], i, gFakeVerts[0].position);
		MakeSkyVec(skymins[0][i], skymaxs[1][i], i, gFakeVerts[1].position);
		MakeSkyVec(skymaxs[0][i], skymaxs[1][i], i, gFakeVerts[2].position);
		MakeSkyVec(skymaxs[0][i], skymins[1][i], i, gFakeVerts[3].position);

		R_RenderBmodelFace(gFakeEdges, &gFakeSurface[i]);
	}

	VectorCopy(oldmodelorg, modelorg);
	VectorCopy(base_vpn, vpn);
	VectorCopy(base_vright, vright);
	VectorCopy(base_vup, vup);

	R_TransformFrustum();
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

void R_FilterSkyBox(void)
{
	int			i, j;
	colorVec	color;
	float		r, g, b;

	short* samples = NULL;
	pixel_t* pskyfiltered = NULL;

	r = filterColorRed * filterBrightness;
	g = filterColorGreen * filterBrightness;
	b = filterColorBlue * filterBrightness;

	for (i = 0; i < 6; i++)
	{
		if (filterMode && gFakeSurface[i].samples)
		{
			samples = (short*)gFakeSurface[i].samples;
			pskyfiltered = gFilteredSkies[i];

			for (j = 0; j < 65536; j++)
			{
				GetRGB(samples[j], &color);

				color.r *= r;
				color.g *= g;
				color.b *= b;

				*(short*)&pskyfiltered[2 * j] = PutDitheredRGB(&color);
			}

			gFakeSurface[i].samples = (color24*)pskyfiltered;
		}
		else
		{
			gFakeSurface[i].samples = (color24*)gSkies[skytexorder[i]];
		}
	}
}
