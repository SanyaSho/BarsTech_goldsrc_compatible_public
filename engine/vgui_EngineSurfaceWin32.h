//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_ENGINESURFACEWIN32_H
#define VGUI_ENGINESURFACEWIN32_H
#ifdef _WIN32
#pragma once
#endif

namespace
{
	// Texture Data
	struct Texture
	{
		Texture*	_next; // next in chain
		int			_id;
		int			_wide;
		int			_tall;
		byte*		_rgba;
	};
}

class TextureManager
{
public:
	enum
	{
		MAX_TEXTURES = 317
	};

	TextureManager()
	{
		int i;
		for (i = 0; i < MAX_TEXTURES; i++)
		{
			m_pTexturePool[i] = NULL;
		}
	}

	int ValidTextureID( int id )
	{
		return id % MAX_TEXTURES;
	}

	void AddTextureToChain( int id )
	{
		Texture* ptex = NULL;

		ptex = m_pTexturePool[ValidTextureID(id)];

		for (; ptex; ptex = ptex->_next)
		{
			if (ptex->_id == id)
			{
				free(ptex->_rgba);
				free(ptex);
				ptex = ptex->_next;
				return;
			}
		}
	}

	void SetTexture( int id, int wide, int tall, const char* rgba )
	{
		Texture* ptex = NULL;

		AddTextureToChain(id);

		ptex = new Texture;
		ptex->_id = id;

		ptex->_rgba = new byte[wide * tall * 4];
		memcpy(ptex->_rgba, rgba, 4 * ((unsigned int)(4 * tall * wide) >> 2));

		ptex->_tall = tall;
		ptex->_wide = wide;

		ptex->_next = m_pTexturePool[ValidTextureID(id)];
		m_pTexturePool[ValidTextureID(id)] = ptex;
	}

	Texture* GetTextureById( int id )
	{
		Texture* ptex = NULL;

		ptex = m_pTexturePool[ValidTextureID(id)];

		for (; ptex; ptex = ptex->_next)
		{
			if (ptex->_id == id)
				return ptex;
		}

		return NULL;
	}

private:
	Texture* m_pTexturePool[MAX_TEXTURES];
};

//-----------------------------------------------------------------------------
// Purpose: Win32 dependant vgui surface renderer for software mode
//-----------------------------------------------------------------------------
class EngineSurfaceRenderPlat
{
public:
	int			viewport[4];		// the viewport coordinates x ,y , width, height

	TextureManager* _tmgr; 
	Texture*		_tdata;
};

#endif // VGUI_ENGINESURFACEWIN32_H