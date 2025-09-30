//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// l_studio.cpp - studio model loading

#include "quakedef.h"
#include "view.h"
#include "studio.h"

const int STUDIO_VERSION = 10;

//#define IDSTUDIOHEADER	MAKEID('I', 'D', 'S', 'T') // little-endian "IDST"
#define IDSEQGRPHEADER	MAKEID('I', 'D', 'S', 'Q') // little-endian "IDSQ"

int giTextureSize;

//=============================================================================


/*
=================
Mod_LoadStudioModel
=================
*/
void Mod_LoadStudioModel( model_t* mod, void* buffer )
{
	byte* pin, * pout;
	studiohdr_t* phdr;
	mstudiotexture_t* pTexture;

	int					version;

	pin = (byte*)buffer;

	phdr = (studiohdr_t*)pin;

	version = LittleLong(phdr->version);
	if (version != STUDIO_VERSION)
	{
		Q_memset(phdr, 0, sizeof(studiohdr_t));
		Q_strcpy(phdr->name, "bogus");
		phdr->length = sizeof(*phdr);
		phdr->texturedataindex = sizeof(*phdr);
	}

	mod->type = mod_studio;
	mod->flags = phdr->flags;

#if defined( GLQUAKE )
	int total = phdr->length;

	if (phdr->textureindex)
	{
		total = phdr->texturedataindex;
	}

	Cache_Alloc(&mod->cache, total, mod->name);
#else
	Cache_Alloc(&mod->cache, phdr->length + 1280 * phdr->numtextures, mod->name);
#endif

	pout = (byte*)mod->cache.data;

	if (pout)
	{
		if (phdr->textureindex)
		{
#if defined( GLQUAKE )
			byte* pPalette;

			char name[256];
			pTexture = (mstudiotexture_t*)((byte*)phdr + phdr->textureindex);

			for (int i = 0; i < phdr->numtextures; i++, pTexture++)
			{
				pPalette = (byte*)phdr + pTexture->index + pTexture->height * pTexture->width;
				snprintf(name, sizeof(name), "%s%s", mod->name, pTexture->name);

				qboolean mipmap = FALSE;
				if (pTexture->flags & STUDIO_NF_NOMIPS)
					mipmap = TRUE;

				if (pTexture->flags & STUDIO_NF_MASKED)
				{
					pTexture->index = GL_LoadTexture(name, GLT_STUDIO, pTexture->width, pTexture->height,
						(byte*)phdr + pTexture->index, mipmap, TEX_TYPE_ALPHA, pPalette);
				}
				else
				{
					pTexture->index = GL_LoadTexture(name, GLT_STUDIO, pTexture->width, pTexture->height,
						(byte*)phdr + pTexture->index, mipmap, TEX_TYPE_NONE, pPalette);
				}
			}
#else
			int size;

			//
			// move the complete, relocatable alias model to the cache
			//
			Q_memcpy(pout, pin, phdr->texturedataindex);

			byte* pindata, * poutdata;
			poutdata = pout + phdr->texturedataindex;
			pindata = pin + phdr->texturedataindex;

			pTexture = (mstudiotexture_t*)(pout + phdr->textureindex);

			for (int i = 0; i < phdr->numtextures; i++, pTexture++)
			{
				size = pTexture->height * pTexture->width;
				pTexture->index = poutdata - pout;

				Q_memcpy(poutdata, pindata, size);
				poutdata += size;
				pindata += size;

				for (int j = 0; j < 256; j++, pindata += 3, poutdata += 8)
				{
					((unsigned short*)poutdata)[0] = texgammatable[pindata[0]];
					((unsigned short*)poutdata)[1] = texgammatable[pindata[1]];
					((unsigned short*)poutdata)[2] = texgammatable[pindata[2]];
					((unsigned short*)poutdata)[3] = 0;
				}
			}
#endif
		}
#if !defined( GLQUAKE )
		else
		{
			Q_memcpy(pout, buffer, phdr->length);
		}
#else
		Q_memcpy(pout, buffer, total);
#endif
	}
}