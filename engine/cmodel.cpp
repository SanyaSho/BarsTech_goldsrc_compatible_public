#include "quakedef.h"
#include "cmodel.h"

byte *gPAS;
byte *gPVS;
int gPVSRowBytes;
unsigned char mod_novis[MODEL_MAX_PVS];

extern void SW_Mod_Init(void);
void Mod_Init()
{
#if !defined(GLQUAKE)
	SW_Mod_Init();
#endif
	Q_memset(mod_novis, 255, sizeof(mod_novis));
}

unsigned char *Mod_DecompressVis(unsigned char *in, model_t *model)
{
	static unsigned char decompressed[MODEL_MAX_PVS];

	if (in == NULL)
	{
		return mod_novis;
	}

	// bits to bytes
	CM_DecompressPVS(in, decompressed, (model->numleafs + 7) / 8);
	return decompressed;
}

unsigned char *Mod_LeafPVS(mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
	{
		return mod_novis;
	}

	if (!gPVS)
	{
		return Mod_DecompressVis(leaf->compressed_vis, model);
	}

	int leafnum = leaf - model->leafs;
	return CM_LeafPVS(leafnum);
}

void CM_DecompressPVS(unsigned char *in, unsigned char *decompressed, int byteCount)
{
	int c;
	unsigned char *out;

	if (in == NULL)
	{
		// Make all visible
		Q_memcpy(decompressed, mod_novis, byteCount);
		return;
	}

	out = decompressed;
	while (out < decompressed + byteCount)
	{
		// Non zero is not copmpressed
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];	// TODO: Check that input buffer is correct (last byte on the buffer could be zero and we will go out of the buffer - check the model)
		in += 2;

		// Prevent buffer overrun
		if (c > decompressed + byteCount - out)
		{
			c = decompressed + byteCount - out;
		}

		// Unpack zeros
		Q_memset(out, 0, c);
		out += c;
	}
}

unsigned char *CM_LeafPVS(int leafnum)
{
	if (gPVS)
	{
		return &gPVS[gPVSRowBytes * leafnum];
	}
	return mod_novis;
}

unsigned char *CM_LeafPAS(int leafnum)
{
	if (gPAS)
	{
		return &gPAS[gPVSRowBytes * leafnum];
	}
	return mod_novis;
}

void CM_FreePAS()
{
	if (gPAS)
		Mem_Free(gPAS);
	if (gPVS)
		Mem_Free(gPVS);
	gPAS = NULL;
	gPVS = NULL;
}

void CM_CalcPAS(model_t *pModel)
{
	int rows, rowwords;
	int actualRowBytes;
	int i, j, k, l;
	int index;
	int bitbyte;
	unsigned int *dest, *src;
	unsigned char *scan;
	int count, vcount, acount;

	Con_DPrintf(const_cast<char*>("Building PAS...\n"));
	CM_FreePAS();

	// Calculate memory: matrix of each to each leaf visibility
	rows = (pModel->numleafs + 7) / 8;
	count = pModel->numleafs + 1;
	actualRowBytes = (rows + 3) & 0xFFFFFFFC;	// 4-byte align
	rowwords = actualRowBytes / 4;
	gPVSRowBytes = actualRowBytes;

	// Alloc PVS
	gPVS = (byte *)Mem_Calloc(gPVSRowBytes, count);

	// Decompress visibility data
	scan = gPVS;
	vcount = 0;
	for (i = 0; i < count; i++, scan += gPVSRowBytes)
	{
		CM_DecompressPVS(pModel->leafs[i].compressed_vis, scan, rows);

		if (i == 0)
		{
			continue;
		}

		for (j = 0; j < count; j++)
		{
			if (scan[j >> 3] & (1 << (j & 7)))
			{
				++vcount;
			}
		}
	}

	// Alloc PAS
	gPAS = (byte *)Mem_Calloc(gPVSRowBytes, count);

	// Build PAS
	acount = 0;
	scan = gPVS;
	dest = (unsigned int *)gPAS;
	for (i = 0; i < count; i++, scan += gPVSRowBytes, dest += rowwords)
	{
		Q_memcpy(dest, scan, gPVSRowBytes);

		for (j = 0; j < gPVSRowBytes; j++)	// bytes
		{
			bitbyte = scan[j];
			if (bitbyte == 0)
			{
				continue;
			}

			for (k = 0; k < 8; k++)			// bits
			{
				if (!(bitbyte & (1 << k)))
				{
					continue;
				}

				index = j * 8 + k + 1;		// bit index
				if (index >= count)
				{
					continue;
				}

				src = (unsigned int *)&gPVS[index * gPVSRowBytes];
				for (l = 0; l < rowwords; l++)
				{
					dest[l] |= src[l];
				}
			}
		}

		if (i == 0)
		{
			continue;
		}

		for (j = 0; j < count; j++)
		{
			if (((byte *)dest)[j >> 3] & (1 << (j & 7)))
			{
				++acount;
			}
		}
	}

	Con_DPrintf(const_cast<char*>("Average leaves visible / audible / total: %i / %i / %i\n"), vcount / count, acount / count, count);
}

qboolean CM_HeadnodeVisible( mnode_t* node, byte* visbits, int* first_visible_leafnum )
{
	int leafnum;
	mleaf_t* leaf;

	if (!node || node->contents == CONTENTS_SOLID)
		return FALSE;

	// add an efrag if the node is a leaf
	if (node->contents < 0)
	{
		leaf = (mleaf_t*)node;
		leafnum = (leaf - sv.worldmodel->leafs) - 1;

		if ((visbits[leafnum >> 3] & (1 << (leafnum & 7))) == 0)
			return FALSE;

		if (first_visible_leafnum)
			*first_visible_leafnum = leafnum;

		return TRUE;
	}

	if (CM_HeadnodeVisible(node->children[0], visbits, first_visible_leafnum))
		return TRUE;

	if (CM_HeadnodeVisible(node->children[1], visbits, first_visible_leafnum))
		return TRUE;

	return FALSE;
}
