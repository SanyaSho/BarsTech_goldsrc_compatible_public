//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#define CINTERFACE
#include <stdio.h>
#define INITGUID  // this is needed only in this file to initialize the d3d guids
#include <ddraw.h>
#include "ilauncherdirectdraw.h"
#include "igame.h"
#include "sys_getmodes.h"
//#include "vmodes.h")
#include "modes.h"
#include <SDL2/SDL_syswm.h>

//-----------------------------------------------------------------------------
// Purpose: Video mode enumeration only.
// FIXME:  Should retrieve mode list from the material system?
//-----------------------------------------------------------------------------
class CLauncherDirectDraw : public ILauncherDirectDraw
{
public:
	typedef struct
	{
		int		bpp;
		bool	checkaspect;
	} DDENUMPARAMS;

						CLauncherDirectDraw( void );
	virtual				~CLauncherDirectDraw( void );

	bool				Init( void );
	void				Shutdown( void );

	void				EnumDisplayModes( int bpp );
	char*				ErrorToString( unsigned int code );

private:
	// DirectDraw object
	LPDIRECTDRAW		m_pDirectDraw;        
};

static CLauncherDirectDraw g_LauncherDirectDraw;
ILauncherDirectDraw *directdraw = ( ILauncherDirectDraw * )&g_LauncherDirectDraw;

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CLauncherDirectDraw::CLauncherDirectDraw( void )
{
	m_pDirectDraw			= (LPDIRECTDRAW)0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CLauncherDirectDraw::~CLauncherDirectDraw( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CLauncherDirectDraw::Init( void )
{
	assert(!m_pDirectDraw);

	// Create the main DirectDraw object
	if ( DirectDrawCreate( NULL, &m_pDirectDraw, NULL ) != DD_OK )
	{
		return false;
	}

	m_pDirectDraw->lpVtbl->SetCooperativeLevel( m_pDirectDraw, NULL, DDSCL_NORMAL );

	if ( !videomode->IsWindowedMode() )
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo((SDL_Window*)game->GetMainWindow(), &wmInfo);

		m_pDirectDraw->lpVtbl->SetCooperativeLevel
		(
			m_pDirectDraw,
			wmInfo.info.win.window,
			DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT
		);
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLauncherDirectDraw::Shutdown(void)
{
	if (!m_pDirectDraw)
		return;

	m_pDirectDraw->lpVtbl->FlipToGDISurface(m_pDirectDraw);
	m_pDirectDraw->lpVtbl->Release(m_pDirectDraw);
	m_pDirectDraw = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pdds - 
//			lParam - 
// Output : static HRESULT CALLBACK
//-----------------------------------------------------------------------------
static HRESULT CALLBACK ModeCallback( LPDDSURFACEDESC pdds, LPVOID lParam )
{
	int		width = pdds->dwWidth;
	int		height = pdds->dwHeight;
	int		bpp = pdds->ddpfPixelFormat.dwRGBBitCount;

	bool	proportional = false;

	CLauncherDirectDraw::DDENUMPARAMS* params = (CLauncherDirectDraw::DDENUMPARAMS*)lParam;

	int		requestedbpp = params->bpp;

	if (9 * width == 16 * height || 3 * width == 4 * height || 4 * width == 5 * height || 5 * width == 8 * height)
	{
		proportional = true;
	}

	// Promote 15 to 16 bit
	if (bpp == 15)
	{
		bpp = 16;
	}

	// The aspect ratio and bpp limits are self-imposed while the height limit is
	//  imposed by the engine
	if (( proportional == true ) && ( height >= 480 ) &&
		(requestedbpp == bpp))
	{
		// Try to add a new mode
		videomode->AddMode(width, height, bpp);
	}

	// Keep going
	return S_FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: Enumerate available video modes
// Input  : bpp - 
//-----------------------------------------------------------------------------
void CLauncherDirectDraw::EnumDisplayModes( int bpp )
{
	static DDENUMPARAMS params;
	params.bpp = bpp;

	m_pDirectDraw->lpVtbl->EnumDisplayModes(m_pDirectDraw, 0, NULL, (void*)&params, ModeCallback);
}

char* CLauncherDirectDraw::ErrorToString( unsigned int code )
{
	switch (code)
	{
	case DDERR_UNSUPPORTED:                  return const_cast<char*>("Unsupported Operation");
	case DDERR_GENERIC:                      return const_cast<char*>("Generic failure");
	case DDERR_OUTOFMEMORY:                  return const_cast<char*>("Out Of Memory");
	case DDERR_INVALIDPARAMS:                return const_cast<char*>("Invalid Params");
	case DDERR_ALREADYINITIALIZED:           return const_cast<char*>("Object already initialised");
	case DDERR_CANNOTATTACHSURFACE:          return const_cast<char*>("Cannot attack surface");
	case DDERR_CANNOTDETACHSURFACE:          return const_cast<char*>("Cannot detach surface");
	case DDERR_CURRENTLYNOTAVAIL:            return const_cast<char*>("Currently Not Available");
	case DDERR_EXCEPTION:                    return const_cast<char*>("Exception has occured");
	case DDERR_HEIGHTALIGN:                  return const_cast<char*>("Height Alignment");
	case DDERR_INCOMPATIBLEPRIMARY:          return const_cast<char*>("Incompatible Primary");
	case DDERR_INVALIDCAPS:                  return const_cast<char*>("Invalid Caps");
	case DDERR_INVALIDCLIPLIST:              return const_cast<char*>("Invlaid Clip List");
	case DDERR_INVALIDMODE:                  return const_cast<char*>("Invalid Mode");
	case DDERR_INVALIDOBJECT:                return const_cast<char*>("Invalid Object");
	case DDERR_INVALIDPIXELFORMAT:           return const_cast<char*>("Invalid Pixel Format");
	case DDERR_INVALIDRECT:                  return const_cast<char*>("Invalid Rectangle");
	case DDERR_LOCKEDSURFACES:               return const_cast<char*>("Locked Surfaces");
	case DDERR_NO3D:                         return const_cast<char*>("No 3d support present");
	case DDERR_NOALPHAHW:                    return const_cast<char*>("No Alpha Hardware");
	case DDERR_NOCLIPLIST:                   return const_cast<char*>("No Clip List");
	case DDERR_NOCOLORCONVHW:                return const_cast<char*>("No Colour Conversion Hardware");
	case DDERR_NOCOOPERATIVELEVELSET:        return const_cast<char*>("No Cooperative Level Set");
	case DDERR_NOCOLORKEY:                   return const_cast<char*>("Surface Has No Colour Key");
	case DDERR_NOCOLORKEYHW:                 return const_cast<char*>("No Hardware Colour Key");
	case DDERR_NODIRECTDRAWSUPPORT:          return const_cast<char*>("No Direct Draw Support");
	case DDERR_NOEXCLUSIVEMODE:              return const_cast<char*>("No Exclusive Mode");
	case DDERR_NOFLIPHW:                     return const_cast<char*>("No Flip Hardware");
	case DDERR_NOGDI:                        return const_cast<char*>("GDI is not present");
	case DDERR_NOMIRRORHW:                   return const_cast<char*>("No Mirror Hardware");
	case DDERR_NOTFOUND:                     return const_cast<char*>("Requested Item Was No Found");
	case DDERR_NOOVERLAYHW:                  return const_cast<char*>("No Overlay Hardware");
	case DDERR_NORASTEROPHW:                 return const_cast<char*>("No Raster Op Hardware");
	case DDERR_NOROTATIONHW:                 return const_cast<char*>("No Roatation Hardware");
	case DDERR_NOSTRETCHHW:                  return const_cast<char*>("No Stretch Hardware");
	case DDERR_NOT4BITCOLOR:                 return const_cast<char*>("Not 4 Bit Colour");
	case DDERR_NOT4BITCOLORINDEX:            return const_cast<char*>("Not 4 Bit Colour Index");
	case DDERR_NOT8BITCOLOR:                 return const_cast<char*>("Not 8 Bit Colour");
	case DDERR_NOTEXTUREHW:                  return const_cast<char*>("No Texture Hardware");
	case DDERR_NOVSYNCHW:                    return const_cast<char*>("No VSync Hardware");
	case DDERR_NOZBUFFERHW:                  return const_cast<char*>("No Z Buffer Hardware");
	case DDERR_NOZOVERLAYHW:                 return const_cast<char*>("No Z Overlay Hardware");
	case DDERR_OUTOFCAPS:                    return const_cast<char*>("Hardware Has Already Been Allocated");
	case DDERR_OUTOFVIDEOMEMORY:             return const_cast<char*>("Out Of Video Memory");
	case DDERR_OVERLAYCANTCLIP:              return const_cast<char*>("Hardware can't clip overlays");
	case DDERR_OVERLAYCOLORKEYONLYONEACTIVE: return const_cast<char*>("Can only have colour key active at one time for overlays");
	case DDERR_PALETTEBUSY:                  return const_cast<char*>("Palette Locked by another thread");
	case DDERR_COLORKEYNOTSET:               return const_cast<char*>("Colour key is not set");
	case DDERR_SURFACEALREADYATTACHED:       return const_cast<char*>("Surface already attached");
	case DDERR_SURFACEALREADYDEPENDENT:      return const_cast<char*>("Surface Already Dependent");
	case DDERR_SURFACEBUSY:                  return const_cast<char*>("Surface Is Busy");
	case DDERR_SURFACEISOBSCURED:            return const_cast<char*>("Surface Is Obscured");
	case DDERR_SURFACELOST:                  return const_cast<char*>("Surface Lost");
	case DDERR_SURFACENOTATTACHED:           return const_cast<char*>("Surface Not Attached");
	case DDERR_TOOBIGHEIGHT:                 return const_cast<char*>("Height Too Big");
	case DDERR_TOOBIGSIZE:                   return const_cast<char*>("Size Too Big - Height and Width are individually OK");
	case DDERR_TOOBIGWIDTH:                  return const_cast<char*>("Width Too Big");
	case DDERR_UNSUPPORTEDFORMAT:            return const_cast<char*>("Unsupported Format");
	case DDERR_UNSUPPORTEDMASK:              return const_cast<char*>("Unsupported Mask Format");
	case DDERR_VERTICALBLANKINPROGRESS:      return const_cast<char*>("Vertical Blank In Progress");
	case DDERR_WASSTILLDRAWING:              return const_cast<char*>("Still Drawing");
	case DDERR_XALIGN:                       return const_cast<char*>("X Align");
	case DDERR_INVALIDDIRECTDRAWGUID:        return const_cast<char*>("Invalid Direct Draw GUID");
	case DDERR_DIRECTDRAWALREADYCREATED:     return const_cast<char*>("Direct Draw Already Created");
	case DDERR_NODIRECTDRAWHW:               return const_cast<char*>("No Direct Draw HardWare");
	case DDERR_PRIMARYSURFACEALREADYEXISTS:  return const_cast<char*>("Primary Surface Already Exists");
	case DDERR_NOEMULATION:                  return const_cast<char*>("No Emulation");
	case DDERR_REGIONTOOSMALL:               return const_cast<char*>("Region Too Small");
	case DDERR_CLIPPERISUSINGHWND:           return const_cast<char*>("Clipper is using a HWND");
	case DDERR_NOCLIPPERATTACHED:            return const_cast<char*>("No Clipper Attached");
	case DDERR_NOHWND:                       return const_cast<char*>("No HWND set");
	case DDERR_HWNDSUBCLASSED:               return const_cast<char*>("HWND Sub-Classed");
	case DDERR_HWNDALREADYSET:               return const_cast<char*>("HWND Already Set");
	case DDERR_NOPALETTEATTACHED:            return const_cast<char*>("No Palette Attrached");
	case DDERR_NOPALETTEHW:                  return const_cast<char*>("No Palette Hardware");
	case DDERR_BLTFASTCANTCLIP:              return const_cast<char*>("Blit Fast Can't Clip");
	case DDERR_NOBLTHW:                      return const_cast<char*>("No Blit Hardware");
	case DDERR_NODDROPSHW:                   return const_cast<char*>("No Direct Draw Raster OP Hardware");
	case DDERR_OVERLAYNOTVISIBLE:            return const_cast<char*>("Overlay Not Visible");
	case DDERR_NOOVERLAYDEST:                return const_cast<char*>("No Overlay Destination");
	case DDERR_INVALIDPOSITION:              return const_cast<char*>("Invlaid Position");
	case DDERR_NOTAOVERLAYSURFACE:           return const_cast<char*>("Not An Overlay Surface");
	case DDERR_EXCLUSIVEMODEALREADYSET:      return const_cast<char*>("Exclusive Mode Already Set");
	case DDERR_NOTFLIPPABLE:                 return const_cast<char*>("Not Flippable");
	case DDERR_CANTDUPLICATE:                return const_cast<char*>("Can't Duplicate");
	case DDERR_NOTLOCKED:                    return const_cast<char*>("Not Locked");
	case DDERR_CANTCREATEDC:                 return const_cast<char*>("Can't Create DC");
	case DDERR_NODC:                         return const_cast<char*>("No DC has ever Existed");
	case DDERR_WRONGMODE:                    return const_cast<char*>("Wrong Mode");
	case DDERR_IMPLICITLYCREATED:            return const_cast<char*>("Surface Cannot Be Restored");
	case DDERR_NOTPALETTIZED:                return const_cast<char*>("Not Palettized");
	case DDERR_UNSUPPORTEDMODE:              return const_cast<char*>("Unsupported Mode");
	default:
		break;
	}

	return (code > 0) ? const_cast<char*>("unk") : const_cast<char*>("No error");
}