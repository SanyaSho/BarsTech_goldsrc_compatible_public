#include "quakedef.h"
#include "d_local.h"
#include "r_local.h"
#include "render.h"

float           surfscale;
qboolean        r_cache_thrash;         // set if surface cache is thrashing

int                                     sc_size;
surfcache_t* sc_rover, * sc_base;

#define GUARDSIZE       4

void D_CheckCacheGuard(void)
{
	byte* s;
	int             i;

	s = (byte*)sc_base + sc_size;
	for (i = 0; i < GUARDSIZE; i++)
		if (s[i] != (byte)i)
			Sys_Error("D_CheckCacheGuard: failed");
}

void D_ClearCacheGuard(void)
{
	byte* s;
	int             i;

	s = (byte*)sc_base + sc_size;
	for (i = 0; i < GUARDSIZE; i++)
		s[i] = (byte)i;
}


/*
================
D_InitCaches

================
*/
void D_InitCaches()
{
	//	if (!msg_suppress_1)
			Con_Printf (const_cast<char*>("%ik surface cache\n"), vid_surfcachesize / 1024);

	sc_size = vid_surfcachesize - GUARDSIZE;
	sc_base = (surfcache_t*)vid_surfcache;
	sc_rover = sc_base;

	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;

	
	D_ClearCacheGuard();
}


void D_FlushCaches(void)
{
	surfcache_t* c;

	if (!sc_base)
		return;

	for (c = sc_base; c; c = c->next)
	{
		if (c->owner)
			*c->owner = NULL;
	}

	sc_rover = sc_base;
	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;

	gWaterLastPalette = NULL;
}

/*
=================
D_SCAlloc
=================
*/
surfcache_t* D_SCAlloc(int width, int size)
{
	surfcache_t* _new;
	qboolean                wrapped_this_time;

	if ((width < 0) || (width > 256))
		Sys_Error("D_SCAlloc: bad cache width %d\n", width);

	if ((size <= 0) || (size > 0x20000))
		Sys_Error("D_SCAlloc: bad cache size %d\n", size);

#ifdef __alpha__
	size = (uintptr_t)((uintptr_t)&((surfcache_t*)0)->data[size]);
#else
	size = (uintptr_t)&((surfcache_t*)0)->data[size];
#endif
	size = (size + 3) & ~3;
	if (size > sc_size)
		Sys_Error("D_SCAlloc: %i > cache size", size);

	// if there is not size bytes after the rover, reset to the start
	wrapped_this_time = false;

	if (!sc_rover || (byte*)sc_rover - (byte*)sc_base > sc_size - size)
	{
		if (sc_rover)
		{
			wrapped_this_time = true;
		}
		sc_rover = sc_base;
	}

	// colect and free surfcache_t blocks until the rover block is large enough
	_new = sc_rover;
	if (sc_rover->owner)
		*sc_rover->owner = NULL;

	while (_new->size < size)
	{
		// free another
		sc_rover = sc_rover->next;
		if (!sc_rover)
			Sys_Error("D_SCAlloc: hit the end of memory");
		if (sc_rover->owner)
			*sc_rover->owner = NULL;

		_new->size += sc_rover->size;
		_new->next = sc_rover->next;
	}

	// create a fragment out of any leftovers
	if (_new->size - size > 256)
	{
		sc_rover = (surfcache_t*)((byte*)_new + size);
		sc_rover->size = _new->size - size;
		sc_rover->next = _new->next;
		sc_rover->width = 0;
		sc_rover->owner = NULL;
		_new->next = sc_rover;
		_new->size = size;
	}
	else
		sc_rover = _new->next;

	_new->width = width;
	// DEBUG
	if (width > 0)
		_new->height = (size - sizeof(*_new) + sizeof(_new->data)) / width;

	_new->owner = NULL;              // should be set properly after return

	if (d_roverwrapped)
	{
		if (wrapped_this_time || (sc_rover >= d_initial_rover))
			r_cache_thrash = true;
	}
	else if (wrapped_this_time)
	{
		d_roverwrapped = true;
	}

	D_CheckCacheGuard();   // DEBUG
	return _new;
}

/*
=================
D_SCDump
=================
*/
void D_SCDump(void)
{
	surfcache_t* test;

	for (test = sc_base; test; test = test->next)
	{
		if (test == sc_rover)
			Sys_Printf("ROVER:\n");
		printf("%p : %i bytes     %i width\n", test, test->size, test->width);
	}
}

//=============================================================================

// if the num is not a power of 2, assume it will not repeat

int     MaskForNum(int num)
{
	if (num == 128)
		return 127;
	if (num == 64)
		return 63;
	if (num == 32)
		return 31;
	if (num == 16)
		return 15;
	return 255;
}

int D_log2(int num)
{
	int     c;

	c = 0;

	while (num >>= 1)
		c++;
	return c;
}

//=============================================================================

/*
================
D_CacheSurface
================
*/
surfcache_t* D_CacheSurface(msurface_t* surface, int miplevel)
{
	surfcache_t* cache;

	//
	// if the surface is animating or flashing, flush the cache
	//
	r_drawsurf.texture = R_TextureAnimation(surface->texinfo->texture);
	r_drawsurf.lightadj[0] = d_lightstylevalue[surface->styles[0]];
	r_drawsurf.lightadj[1] = d_lightstylevalue[surface->styles[1]];
	r_drawsurf.lightadj[2] = d_lightstylevalue[surface->styles[2]];
	r_drawsurf.lightadj[3] = d_lightstylevalue[surface->styles[3]];

	//
	// see if the cache holds apropriate data
	//
	cache = surface->cachespots[miplevel];

	if (cache)
	{
		if (cache->dlight >= surface->dlightframe - 1)
			cache->dlight = surface->dlightframe;

		if (cache->dlight == surface->dlightframe && !(surface->dlightbits & r_dlightchanged)
			&& cache->texture == r_drawsurf.texture
			&& cache->lightadj[0] == r_drawsurf.lightadj[0]
			&& cache->lightadj[1] == r_drawsurf.lightadj[1]
			&& cache->lightadj[2] == r_drawsurf.lightadj[2]
			&& cache->lightadj[3] == r_drawsurf.lightadj[3])
			{
				return cache;
			}
	}

	//
	// determine shape of surface
	//
	surfscale = 1.0 / (1 << miplevel);
	r_drawsurf.surfmip = miplevel;
	r_drawsurf.surfwidth = surface->extents[0] >> miplevel;
	r_drawsurf.rowbytes = r_drawsurf.surfwidth * 2;
	r_drawsurf.surfheight = surface->extents[1] >> miplevel;

	//
	// allocate memory if needed
	//
	if (!cache)     // if a texture just animated, don't reallocate it
	{
		cache = D_SCAlloc(r_drawsurf.surfwidth, r_drawsurf.rowbytes * r_drawsurf.surfheight);
		surface->cachespots[miplevel] = cache;
		cache->owner = &surface->cachespots[miplevel];
		cache->mipscale = surfscale;
	}

	surface->dlightbits &= r_dlightactive;
	cache->dlight = surface->dlightframe;

	r_drawsurf.surfdat = cache->data;

	cache->texture = r_drawsurf.texture;
	cache->lightadj[0] = r_drawsurf.lightadj[0];
	cache->lightadj[1] = r_drawsurf.lightadj[1];
	cache->lightadj[2] = r_drawsurf.lightadj[2];
	cache->lightadj[3] = r_drawsurf.lightadj[3];

	//
	// draw and light the surface texture
	//
	r_drawsurf.surf = surface;

	c_surf++;
	R_DrawSurface();

	return surface->cachespots[miplevel];
}
