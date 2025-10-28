//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// sys_getmodes.cpp

#define CINTERFACE

#if defined ( GLQUAKE )
#include <GL/glew.h>
#endif

#include <tier1/UtlVector.h>

#include "quakedef.h"
#include "vid.h"
#include "modes.h"
#include "IGame.h"
#include "IRegistry.h"
#include "host_cmd.h"
#include "screen.h"
#include "sound.h"
#include "sys_getmodes.h"
#include <SDL2/SDL_syswm.h>
#include <ddraw.h>
#if !defined(GLQUAKE)
#include "d_iface.h"
#include "r_shared.h"
#endif

void VID_UpdateWindowVars( RECT* prc, int x, int y );

extern viddef_t	vid;  		// global video state

bool bNeedsFullScreenModeSwitch = false;

//-----------------------------------------------------------------------------
// Purpose: Functionality shared by all video modes
//-----------------------------------------------------------------------------
class CVideoMode_Common : public IVideoMode
{
public:
						CVideoMode_Common( void );
	virtual 			~CVideoMode_Common( void );

public:
	virtual bool		Init( void* pvInstance );
	virtual void		PlayStartupSequence( void );
	virtual void		Shutdown( void );

	virtual bool		AddMode( int width, int height, int bpp );
	virtual vmode_t*	GetMode( int num );
	virtual int			GetModeCount( void );

	virtual bool		IsWindowedMode( void ) const;

	virtual bool		GetInitialized( void ) const;
	virtual void		SetInitialized( bool init );

	virtual	void		UpdateWindowPosition( void );

	virtual	void		FlipScreen( void );

	virtual	void		RestoreVideo( void );
	virtual void		ReleaseVideo( void );

	virtual int			MaxBitsPerPixel( void );

private:
	void				CenterEngineWindow( HWND hWndCenter, int width, int height );

	virtual void		ReleaseFullScreen( void );
	virtual void		ChangeDisplaySettingsToFullscreen( void );

private:
	struct bimage_t
	{
		byte* buffer;
		int x;
		int y;
		int width;
		int height;
		bool scaled;
	};

	enum
	{
		MAX_MODE_LIST = 32
	};

	// Master mode list
	vmode_t				m_rgModeList[MAX_MODE_LIST];
	vmode_t				m_safeMode;
	int					m_nNumModes;
	int					m_nGameMode;
	bool				m_bInitialized;

protected:
	vmode_t*			GetCurrentMode( void );
	void				AdjustWindowForMode( void );

	void				LoadStartupGraphic( void );
#if defined ( _WIN32 )
	void				BlitGraphicToHDCWithAlpha( HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1 );
#endif
	void				DrawStartupGraphic( HWND window );

	CUtlVector<CVideoMode_Common::bimage_t> m_ImageID;

	int					m_iBaseResX;
	int					m_iBaseResY;
	bool				m_bWindowed;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVideoMode_Common::CVideoMode_Common( void )
{
	m_nNumModes = 0;
	m_nGameMode = 0;
	m_bWindowed = false;
	m_bInitialized = false;
	m_safeMode.width = 640;
	m_safeMode.height = 480;
	m_safeMode.bpp = 32;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVideoMode_Common::~CVideoMode_Common( void )
{
#ifdef _LINUX
	if (m_pTexture != NULL)
		SDL_DestroyTexture(m_pTexture);

	if (m_bOwnRenderer && m_pRenderer != NULL)
		SDL_DestroyRenderer(m_pRenderer);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::GetInitialized( void ) const
{
	return m_bInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : init - 
//-----------------------------------------------------------------------------
void CVideoMode_Common::SetInitialized( bool init )
{
	m_bInitialized = init;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::IsWindowedMode( void ) const
{
	return m_bWindowed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vmode_t
//-----------------------------------------------------------------------------
bool CVideoMode_Common::AddMode( int width, int height, int bpp )
{
	if (m_nNumModes >= MAX_MODE_LIST)
	{
		return false;
	}

	// Check if it's already in the list;
	for (int i = 0; i < m_nNumModes; i++)
	{
		if (m_rgModeList[i].width == width &&
			m_rgModeList[i].height == height &&
			m_rgModeList[i].bpp == bpp)
		{
			return true;
		}
	}

	m_rgModeList[m_nNumModes].width = width;
	m_rgModeList[m_nNumModes].height = height;
	m_rgModeList[m_nNumModes].bpp = bpp;

	m_nNumModes++;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : num - 
//-----------------------------------------------------------------------------
vmode_t* CVideoMode_Common::GetMode( int num )
{
	if (num >= m_nNumModes || num < 0)
	{
		return &m_safeMode;
	}

	return &m_rgModeList[num];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetModeCount( void )
{
	return m_nNumModes;
}

int CVideoMode_Common::MaxBitsPerPixel( void )
{
	return 32;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : modenum - 
// Output : vmode_t
//-----------------------------------------------------------------------------
vmode_t* CVideoMode_Common::GetCurrentMode( void )
{
	return GetMode(m_nGameMode);
}

//-----------------------------------------------------------------------------
// Purpose: Compares video modes so we can sort the list
// Input  : *arg1 - 
//			*arg2 - 
// Output : static int
//-----------------------------------------------------------------------------
static int VideoModeCompare( const void* arg1, const void* arg2 )
{
	vmode_t* m1, * m2;

	m1 = (vmode_t*)arg1;
	m2 = (vmode_t*)arg2;

	if (m1->width < m2->width)
	{
		return -1;
	}

	if (m1->width == m2->width)
	{
		if (m1->height < m2->height)
		{
			return -1;
		}

		if (m1->height > m2->height)
		{
			return 1;
		}

		return 0;
	}

	return 1;
}

void SetupSDLVideoModes( void )
{
	int i, iNumModes;
	SDL_DisplayMode mode;

	SDL_InitSubSystem(SDL_INIT_VIDEO);

	iNumModes = SDL_GetNumDisplayModes(0);

	for (i = 0; i < iNumModes; i++)
	{
		if (SDL_GetDisplayMode(0, i, &mode))
		{
			break;
		}

		videomode->AddMode(mode.w, mode.h, 32);
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CVideoMode_Common::Init( void* pvInstance )
{
	// When running in stand-alone mode, create your own window 
	if (!game->CreateGameWindow())
		return false;

	int x = registry->ReadInt("ScreenWidth", 1024);
	int y = registry->ReadInt("ScreenHeight", 768);

	registry->ReadInt("ScreenBPP", 32);

	COM_CheckParm(const_cast<char*>("-16bpp"));
	COM_CheckParm(const_cast<char*>("-24bpp"));
	COM_CheckParm(const_cast<char*>("-32bpp"));

	MaxBitsPerPixel();

	m_nNumModes = 0;

	SetupSDLVideoModes();

	if (m_nNumModes >= 2)
	{
		// Sort modes
		qsort((void*)&m_rgModeList[0], m_nNumModes, sizeof(vmode_t), VideoModeCompare);
	}

	// Try and match mode based on w,h
	int iOK = -1;

	// Is the user trying to force a mode
	int iArg;
	iArg = COM_CheckParm(const_cast<char*>("-width"));
	if (iArg)
	{
		x = atoi(com_argv[iArg + 1]);
	}

	iArg = COM_CheckParm(const_cast<char*>("-w"));
	if (iArg)
	{
		x = atoi(com_argv[iArg + 1]);
	}

	iArg = COM_CheckParm(const_cast<char*>("-height"));
	if (iArg)
	{
		y = atoi(com_argv[iArg + 1]);
	}

	iArg = COM_CheckParm(const_cast<char*>("-h"));
	if (iArg)
	{
		y = atoi(com_argv[iArg + 1]);
	}

#if !defined ( GLQUAKE )
	if (x > MAXWIDTH)
		x = MAXWIDTH;
	if (y > MAXHEIGHT)
		y = MAXHEIGHT;
#endif

	// This allows you to have a window of any size.  Requires you to set both width and height for the window.
	if (m_bWindowed)
	{
		SDL_Rect rect;
		if (!SDL_GetDisplayBounds(0, &rect))
		{
			if (x > rect.w)
				x = rect.w;
			if (y > rect.h)
				y = rect.h;
		}
	}

	if (x > 0 && y < 0)
		y = (int)(x * 3.0 / 4.0);

	int i;

	for (i = 0; i < m_nNumModes; i++)
	{
		// Match width first
		if (m_rgModeList[i].width != x)
			continue;

		iOK = i;

		if (m_rgModeList[i].height != y)
			continue;

		// Found a decent match
		break;
	}

	// No match, use mode 0
	if (i >= m_nNumModes)
	{
		if (iOK != -1)
		{
			i = iOK;
		}
		else
		{
			i = 0;
		}
	}

	m_nGameMode = i;

	if (m_bWindowed || !COM_CheckParm(const_cast<char*>("-nofbo")) && Host_GetVideoLevel() <= 0)
	{
		if (COM_CheckParm(const_cast<char*>("-forceres")) || m_nNumModes < 2)
		{
			m_rgModeList[i].width = x;
			m_rgModeList[i].height = y;

			if (m_nNumModes == 0)
				m_nNumModes = 1;
		}
	}

	registry->WriteInt("ScreenWidth", m_rgModeList[i].width);
	registry->WriteInt("ScreenHeight", m_rgModeList[i].height);
	registry->WriteInt("ScreenBPP", m_rgModeList[i].bpp);

	LoadStartupGraphic();

	AdjustWindowForMode();

//	game->PlayStartupVideos();

//	DrawStartupGraphic((HWND)game->GetMainWindow());

	return true;
}

void CVideoMode_Common::PlayStartupSequence(void)
{
	if (!COM_CheckParm(const_cast<char*>("-novid")))
		game->PlayStartupVideos();

	game->GetMainWindow();

	for (int i = 0; i < m_ImageID.Size(); i++)
	{
		if (m_ImageID[i].buffer)
		{
			free(m_ImageID[i].buffer);
		}
	}

	m_ImageID.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_Common::UpdateWindowPosition( void )
{
	RECT	window_rect;
	int		x, y, w, h;

	// Get the window from the game ( right place for it? )
	game->GetWindowRect(&x, &y, &w, &h);

	window_rect.left = x;
	window_rect.right = x + w;
	window_rect.top = y;
	window_rect.bottom = y + h;

	int cx = x + w / 2;
	int cy = y + h / 2;

	// Tell the engine
	VID_UpdateWindowVars(&window_rect, cx, cy);
}

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen( void )
{
}

void CVideoMode_Common::ReleaseFullScreen( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mode - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVideoMode_Common::AdjustWindowForMode( void )
{
	vmode_t* mode = GetCurrentMode();

	assert(mode);

	Snd_ReleaseBuffer();

	vid.width = mode->width;
	vid.height = mode->height;

	int width, height;

	if (IsWindowedMode())
	{
		width = m_rgModeList[m_nGameMode].width;
		height = m_rgModeList[m_nGameMode].height;
	}
	else
	{
		width = mode->width;
		height = mode->height;

		// Use Change Display Settings to go full screen
		ChangeDisplaySettingsToFullscreen();

		SDL_SetWindowPosition((SDL_Window*)game->GetMainWindow(), 0, 0);
	}

	game->SetWindowSize(width, height);

	// Now center
	CenterEngineWindow((HWND)game->GetMainWindow(), width, height);

	Snd_AcquireBuffer();
	VOX_Init();

	// Make sure we have updated window information
	UpdateWindowPosition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_Common::Shutdown( void )
{
	ReleaseFullScreen();

	if (!GetInitialized())
		return;
	
	SetInitialized(false);

	m_nGameMode = 0;

	// REMOVE INTERFACE
	videomode = NULL;
	ReleaseVideo();
}

void CVideoMode_Common::FlipScreen( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hWndCenter - 
//			width - 
//			height - 
// Output : static void
//-----------------------------------------------------------------------------
void CVideoMode_Common::CenterEngineWindow( HWND hWndCenter, int width, int height )
{
	int     CenterX, CenterY;

	SDL_Rect rect;
	SDL_GetDisplayBounds(0, &rect);

	CenterX = (rect.w - width) / 2;
	CenterY = (rect.h - height) / 2;
	CenterX = (CenterX < 0) ? 0 : CenterX;
	CenterY = (CenterY < 0) ? 0 : CenterY;

	game->SetWindowXY(CenterX, CenterY);

	SDL_SetWindowPosition((SDL_Window*)game->GetMainWindow(), CenterX, CenterY);
}

extern qboolean LoadTGA(char* szFilename, byte* buffer, int bufferSize, int* width, int* height);
void CVideoMode_Common::LoadStartupGraphic( void )
{
	char token[512];

	FileHandle_t file;
	int		fileSize;

	file = FS_Open("resource/BackgroundLoadingLayout.txt", "rt");

	if (!file)
		return;

	fileSize = FS_Size(file);
	char* buffer = (char*)(alloca(fileSize + 16));

	memset(buffer, 0, fileSize);

	FS_Read(buffer, fileSize, 1, file);
	FS_Close(file);

	while (*buffer)
	{
		buffer = FS_ParseFile(buffer, token, NULL);
		if (!buffer || !token[0])
			break;

		// read first rule resolution
		if (!stricmp(token, "resolution"))
		{
			buffer = FS_ParseFile(buffer, token, NULL);
			m_iBaseResX = atoi(token);
			buffer = FS_ParseFile(buffer, token, NULL);
			m_iBaseResY = atoi(token);
		}
		else
		{
			bimage_t& image = m_ImageID.Element(m_ImageID.AddToTail());
			image.buffer = (byte*)malloc(512 * 512);

			if (!LoadTGA(token, image.buffer, 512 * 512, &image.width, &image.height))
			{
				m_ImageID.Remove(m_ImageID.Size() - 1);
				return;
			}

			buffer = FS_ParseFile(buffer, token, NULL);
			image.scaled = stricmp(token, "scaled") == 0;

			buffer = FS_ParseFile(buffer, token, NULL);
			image.x = atoi(token);

			buffer = FS_ParseFile(buffer, token, NULL);
			image.y = atoi(token);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Renders the startup graphic into the HWND
//-----------------------------------------------------------------------------

#if defined ( GLQUAKE )
void CVideoMode_Common::DrawStartupGraphic( HWND window )
{
	int		i;
	int		width, height;

	if (m_ImageID.Size() == 0)
		return;

	SDL_GetWindowSize((SDL_Window*)window, &width, &height);

	glViewport(0, 0, width, height);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glMatrixMode(GL_PROJECTION);
	glOrtho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	int srcWide, srcTall;
	VideoMode_GetCurrentVideoMode(&srcWide, &srcTall, NULL);

	float fSrcAspect = (float)(srcWide / srcTall);
	float fDstAspect = (float)(width / height);

	float flXDiff = 0.0f, flYDiff = 0.0f;

	if (COM_CheckParm(const_cast<char*>("-stretchaspect")) == 0)
	{
		if (fDstAspect > fSrcAspect)
		{
			flXDiff = width - (fSrcAspect * height);
			flYDiff = 0.0f;
		}
		else if (fSrcAspect > fDstAspect)
		{
			flYDiff = height - (1.0f / fSrcAspect * width);
			flXDiff = 0.0f;
		}
		else
		{
			flYDiff = 0.0f;
			flXDiff = 0.0f;
		}
	}

	float xScale = (width - flXDiff) / m_iBaseResX;
	float yScale = (height - flYDiff) / m_iBaseResY;

	CUtlVector<unsigned int> vecGLTex;

	for (i = 0; i < m_ImageID.Size(); i++)
	{
		const bimage_t& bimage = m_ImageID[i];

		int dx, dy, dw, dt;
		if (bimage.x == 0)
		{
			dx = ceil(flXDiff / 2.0);
		}
		else
		{
			dx = ceil(bimage.x * xScale + (flXDiff / 2.0));
		}

		if (bimage.y == 0)
		{
			dy = ceil(flYDiff / 2.0);
		}
		else
		{
			dy = ceil(bimage.y * yScale + (flYDiff / 2.0));
		}

		if (bimage.scaled)
		{
			dw = ceil((bimage.width + bimage.x) * xScale + (flXDiff / 2.0));
			dt = ceil((bimage.height + bimage.y) * yScale + (flYDiff / 2.0));
		}
		else
		{
			dw = bimage.width + dx;
			dt = bimage.height + dy;
		}

		int index = vecGLTex.AddToTail();

		glGenTextures(1, &vecGLTex[index]);
		glBindTexture(GL_TEXTURE_2D, vecGLTex[index]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		// get the nearest power of two that is greater than the width of the image
		int paddedImageWidth = bimage.width;
		if (!IsPowerOfTwo(paddedImageWidth))
		{
			// width is not a power of two, calculate the next highest power of two value
			int i = 1;
			while (paddedImageWidth > 1)
			{
				paddedImageWidth = paddedImageWidth >> 1;
				i++;
			}

			paddedImageWidth = paddedImageWidth << i;
		}

		// get the nearest power of two that is greater than the height of the image
		int paddedImageHeight = bimage.height;
		if (!IsPowerOfTwo(paddedImageHeight))
		{
			// height is not a power of two, calculate the next highest power of two value
			int i = 1;
			while (paddedImageHeight > 1)
			{
				paddedImageHeight = paddedImageHeight >> 1;
				i++;
			}

			paddedImageHeight = paddedImageHeight << i;
		}

		uint8_t* pToUse;
		uint8_t* pExpanded = NULL;

		float topu, topv;

		// Make sure that proportion is equal, otherwise stretch the image to a specific size
		if (paddedImageWidth == bimage.width && paddedImageHeight == bimage.height)
		{
			pToUse = bimage.buffer;
			topu = 1.0f;
			topv = 1.0f;
		}
		// Try stretch image
		else
		{
			pExpanded = new uint8_t[paddedImageWidth * paddedImageHeight * 4 /*RGBA*/];

			if (!pExpanded)
			{
				return;
			}

			Q_memset(pExpanded, 0, paddedImageWidth * paddedImageHeight * 4);

			// loop through all the pixels
			for (int row = 0; row < bimage.height; row++)
			{
				for (int column = 0; column < bimage.width; column++)
				{
					// copy the color values for this pixel to the destination buffer
					pExpanded[(row * paddedImageWidth * 4) + (column * 4) + 0] = bimage.buffer[(row * bimage.width * 4) + (column * 4) + 0];
					pExpanded[(row * paddedImageWidth * 4) + (column * 4) + 1] = bimage.buffer[(row * bimage.width * 4) + (column * 4) + 1];
					pExpanded[(row * paddedImageWidth * 4) + (column * 4) + 2] = bimage.buffer[(row * bimage.width * 4) + (column * 4) + 2];
					pExpanded[(row * paddedImageWidth * 4) + (column * 4) + 3] = bimage.buffer[(row * bimage.width * 4) + (column * 4) + 3];
				}
			}

			pToUse = pExpanded;

			// compute the amount of stretching that needs to be done to both width and height to get the image to fit
			topu = (float)(bimage.width) / (float)(paddedImageWidth);
			topv = (float)(bimage.height) / (float)(paddedImageHeight);
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, paddedImageWidth, paddedImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pToUse);

		if (pExpanded)
		{
			delete[] pExpanded;
		}

		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, topv);
		glVertex3i(dx, dt, 0);
		glTexCoord2f(topu, topv);
		glVertex3i(dw, dt, 0);
		glTexCoord2f(topu, 0.0f);
		glVertex3i(dw, dy, 0);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3i(dx, dy, 0);
		glEnd();
	}

	SDL_GL_SwapWindow((SDL_Window*)window);

	for (i = 0; i < vecGLTex.Size(); i++)
	{
		glDeleteTextures(1, &vecGLTex[i]);
	}

	for (i = 0; i < m_ImageID.Size(); i++)
	{
		if (m_ImageID[i].buffer)
		{
			free(m_ImageID[i].buffer);
		}
	}

	m_ImageID.RemoveAll();
}
#else
//-----------------------------------------------------------------------------
// Purpose: Renders the startup graphic into the HWND for SW
//-----------------------------------------------------------------------------
void CVideoMode_Common::DrawStartupGraphic( HWND window )
{
	HDC		hdc;
	int		dx, dy;
	int		width, height;

	bimage_t* pImage = NULL;

	if (m_ImageID.Size() == 0)
		return;

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window*)window, &wmInfo);

	// Get the window from the game
	VideoMode_GetCurrentVideoMode(&width, &height, NULL);

	hdc = ::GetDC(wmInfo.info.win.window);

	HDC memdc = ::CreateCompatibleDC(hdc);

	HBITMAP bm = ::CreateCompatibleBitmap(hdc, width, height);

	::SelectObject(memdc, bm);

	float xScale, yScale;
	xScale = (float)width / (float)m_iBaseResX;
	yScale = (float)height / (float)m_iBaseResY;

	int i;
	for (i = 0; i < m_ImageID.Size(); i++)
	{
		pImage = &m_ImageID[i];

		dx = pImage->x;	
		if (dx)
			dx = ceil((float)dx * xScale);
		else
			dx = 0;

		dy = pImage->y;
		if (dy)
			dy = ceil((float)dy * yScale);
		else
			dy = 0;

		int dw, dt;
		if (pImage->scaled)
		{
			dw = ceil((float)(pImage->x + pImage->width) * xScale);
			dt = ceil((float)(pImage->y + pImage->height) * yScale);
		}
		else
		{
			dw = dx + pImage->width;
			dt = dy + pImage->height;
		}

		byte* rgba = pImage->buffer;
		assert(rgba);

		BlitGraphicToHDCWithAlpha(memdc, rgba, pImage->width, pImage->height, dx, dy, dw, dt);
	}

	SetViewportOrgEx(hdc, 0, 0, NULL);

	BitBlt(hdc, 0, 0, width, height, memdc, 0, 0, SRCCOPY);
	
	DeleteObject(bm);
	DeleteDC(memdc);
	ReleaseDC(window, hdc);

	for (i = 0; i < m_ImageID.Size(); i++)
	{
		if (m_ImageID[i].buffer)
		{
			free(m_ImageID[i].buffer);
		}
	}

	m_ImageID.RemoveAll();
}
#endif

#if defined ( _WIN32 )
//-----------------------------------------------------------------------------
// Purpose: Blits an image to the loading window hdc
//-----------------------------------------------------------------------------
void CVideoMode_Common::BlitGraphicToHDCWithAlpha( HDC hdc, byte* rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1 )
{
	int x = x0;
	int y = y0;
	int wide = x1 - x0;
	int tall = y1 - y0;

	float xTiles = (float)imageWidth / (float)wide;
	float yTiles = (float)imageHeight / (float)tall;

	int texwby4 = imageWidth << 2;

	for (int v = 0; v < tall; v++)
	{
		int* src = (int*)(rgba + ((int)(v * yTiles) * texwby4));
		int xaccum = 0;

		for (int u = 0; u < wide; u++)
		{
			byte* xsrc = (byte*)(src + xaccum / 256);
			::SetPixel(hdc, x + u, y + v, RGB(xsrc[0], xsrc[1], xsrc[2]));
			xaccum += (xTiles * 256);
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_Common::RestoreVideo( void )
{
}

void CVideoMode_Common::ReleaseVideo( void )
{
}

BOOL R_UseLegacyRender()
{
	return game->OnlyLegacyRender();
}

#if defined ( GLQUAKE )

//-----------------------------------------------------------------------------
// Purpose: OpenGL Video Mode Class
//-----------------------------------------------------------------------------
class CVideoMode_OpenGL : public CVideoMode_Common
{
public:
	typedef CVideoMode_Common BaseClass;

	CVideoMode_OpenGL( bool windowed );

	virtual const char* GetName( void ) { return "gl"; }

	virtual bool		Init( void* pvInstance );

	virtual void		ReleaseVideo( void );
	virtual	void		RestoreVideo( void );

	virtual void		FlipScreen( void );

private:
	virtual void		ReleaseFullScreen( void );
	virtual void		ChangeDisplaySettingsToFullscreen( void );
};

CVideoMode_OpenGL::CVideoMode_OpenGL( bool windowed )
{
	m_bWindowed = windowed;
}

bool CVideoMode_OpenGL::Init( void* pvInstance )
{
	bool bret = BaseClass::Init(pvInstance);

	SetInitialized(bret);

	return bret;
}

void CVideoMode_OpenGL::ReleaseVideo( void )
{
	if (IsWindowedMode())
		return;

	ReleaseFullScreen();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_OpenGL::RestoreVideo( void )
{
	if (IsWindowedMode())
		return;
	
	AdjustWindowForMode();
	SDL_ShowWindow((SDL_Window*)game->GetMainWindow());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_OpenGL::ReleaseFullScreen( void )
{
	if (!IsWindowedMode())
	{
		SDL_SetWindowFullscreen((SDL_Window*)game->GetMainWindow(), 0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use Change Display Settings to go Full Screen
// Input  : *mode - 
// Output : static void
//-----------------------------------------------------------------------------
void CVideoMode_OpenGL::ChangeDisplaySettingsToFullscreen( void )
{
	static Uint32 iLastScreenMode = SDL_WINDOW_FULLSCREEN_DESKTOP;

	if (IsWindowedMode())
		return;

	vmode_t* mode = GetCurrentMode();

	if (!mode)
		return;

	Uint32 flags = SDL_WINDOW_FULLSCREEN;

	if (!bNeedsFullScreenModeSwitch)
		flags = SDL_WINDOW_FULLSCREEN_DESKTOP;

	// Force to windowed first before switching to fullscreen mode to force full update
	if (iLastScreenMode != flags)
	{
		SDL_SetWindowFullscreen((SDL_Window*)game->GetMainWindow(), 0);
	}

	SDL_SetWindowFullscreen((SDL_Window*)game->GetMainWindow(), flags);

	iLastScreenMode = flags;
}

void CVideoMode_OpenGL::FlipScreen( void )
{
	Sys_Error("This shouldn't be called\n");
}

//-----------------------------------------------------------------------------
// Purpose: Windowed D3D Video Mode Class
//-----------------------------------------------------------------------------
class VideoMode_Direct3DWindowed : public CVideoMode_Common
{
public:
	typedef CVideoMode_Common BaseClass;

	VideoMode_Direct3DWindowed( bool windowed );

	virtual const char* GetName( void ) { return "d3d"; }

	virtual bool		Init( void* pvInstance );
};

VideoMode_Direct3DWindowed::VideoMode_Direct3DWindowed( bool windowed )
{
	m_bWindowed = windowed;
}

bool VideoMode_Direct3DWindowed::Init( void* pvInstance )
{
	bool bret = BaseClass::Init(pvInstance);

	SetInitialized(bret);

	return bret;
}

#endif

#if defined(_LINUX)
class CVideoMode_SoftwareLinux : public CVideoMode_Common
{
public:
	typedef CVideoMode_Common BaseClass;

	CVideoMode_SoftwareLinux();
	~CVideoMode_SoftwareLinux()
	{
		BaseClass::~CVideoMode_Common();
	}

	virtual const char* GetName(void) { return "software"; }

	virtual bool		Init(void* pvInstance);
	virtual void		Shutdown(void);

	virtual bool		AddMode(int width, int height, int bpp)
	{
		return CVideoMode_Common::AddMode(width, height, bpp);
	}

	virtual void		FlipScreen(void);

	virtual	void		RestoreVideo(void);
	virtual void		ReleaseVideo(void);

	virtual int			MaxBitsPerPixel(void);
private:
	SDL_Texture* m_pTexture;
	SDL_Renderer* m_pRenderer;
	bool m_bOwnRenderer;
	int m_lastW;
	int m_lastH;
};

CVideoMode_SoftwareLinux::CVideoMode_SoftwareLinux()
{

	
}

bool CVideoMode_SoftwareLinux::Init(void* pvInstance)
{
	bool bret = BaseClass::Init(pvInstance);

	if (bret)
	{

	}

	SetInitialized(bret);

	return bret;
}
#endif

#if !defined ( GLQUAKE )
#define CINTERFACE
#include <ddraw.h>
#include <ilauncherdirectdraw.h>

//-----------------------------------------------------------------------------
// Purpose: Fullscreen Software Video Mode Class
//-----------------------------------------------------------------------------
class CVideoMode_SoftwareFullScreen : public CVideoMode_Common
{
public:
	typedef CVideoMode_Common BaseClass;

	CVideoMode_SoftwareFullScreen();
	~CVideoMode_SoftwareFullScreen()
	{
		BaseClass::~CVideoMode_Common();
	}

	virtual const char* GetName( void ) { return "software"; }

	virtual bool		Init( void* pvInstance );
	virtual void		Shutdown( void );

	virtual bool		AddMode( int width, int height, int bpp )
	{
#if defined(SW_EXTEND_LIMITS)
		if (width >= MAXWIDTH || height >= MAXHEIGHT)
			return true;
#else
		if (width >= 1600 || height >= 1200)
			return true;
#endif

		return CVideoMode_Common::AddMode(width, height, bpp);
	}

	virtual void		FlipScreen( void );

	virtual	void		RestoreVideo( void );
	virtual void		ReleaseVideo( void );

	virtual int			MaxBitsPerPixel( void );

private:
	bool				DDRAW_PreInit( void );
	bool				DDRAW_Init( void );
	void				DDRAW_Shutdown( void );

	void				RestoreSurfaces( void );
	void				ReleaseSurfaces( void );

	LPDIRECTDRAW		m_pDirectDraw;			// pointer to DirectDraw object
	LPDIRECTDRAWSURFACE m_pDDSFrontBuffer;		// video card display memory front buffer
	LPDIRECTDRAWSURFACE m_pDDSBackBuffer;		// system memory backbuffer
	LPDIRECTDRAWSURFACE m_pDDSOffScreenBuffer;	// system memory backbuffer

};

CVideoMode_SoftwareFullScreen::CVideoMode_SoftwareFullScreen()
{
	m_pDirectDraw = (LPDIRECTDRAW)0;
	m_pDDSFrontBuffer = (LPDIRECTDRAWSURFACE)0;
	m_pDDSBackBuffer = (LPDIRECTDRAWSURFACE)0;
	m_pDDSOffScreenBuffer = (LPDIRECTDRAWSURFACE)0;

	m_bWindowed = false;
}

bool CVideoMode_SoftwareFullScreen::Init( void* pvInstance )
{
	bool bret = BaseClass::Init(pvInstance);

	if (bret)
	{
		bret = DDRAW_PreInit();

		if (bret)
		{
			bret = DDRAW_Init();
		}
	}

	SetInitialized(bret);

	return bret;
}

void CVideoMode_SoftwareFullScreen::RestoreVideo( void )
{
	if (!GetInitialized())
		return;

	RestoreSurfaces();

	if (!m_bWindowed)
	{
		SDL_SetWindowFullscreen((SDL_Window*)game->GetMainWindow(), SDL_WINDOW_FULLSCREEN);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_SoftwareFullScreen::Shutdown( void )
{
	DDRAW_Shutdown();

	CVideoMode_Common::Shutdown();
}

void CVideoMode_SoftwareFullScreen::FlipScreen( void )
{
	RECT r;
	DDSURFACEDESC ddsd;
	qboolean initialized = FALSE;

	r.left = 0;
	r.top = 0;
	r.right = vid.width;
	r.bottom = vid.height;

	if (!game->IsActiveApp())
		return;

	if (!m_pDDSOffScreenBuffer || !m_pDDSBackBuffer)
		RestoreSurfaces();

	if (!m_pDDSOffScreenBuffer || !m_pDDSBackBuffer)
	{
		return;
	}

	m_pDDSOffScreenBuffer->lpVtbl->Unlock(m_pDDSOffScreenBuffer, NULL);

	if (vid.numpages == 1)
	{
		if (m_pDDSBackBuffer->lpVtbl->BltFast(m_pDDSBackBuffer, 0, 0, m_pDDSOffScreenBuffer, &r, DDBLTFAST_WAIT) == DDERR_SURFACELOST)
		{
			RestoreSurfaces();
			m_pDDSBackBuffer->lpVtbl->BltFast(m_pDDSBackBuffer, 0, 0, m_pDDSOffScreenBuffer, &r, DDBLTFAST_WAIT);
			initialized = TRUE;
		}

		if (m_pDDSFrontBuffer->lpVtbl->Flip(m_pDDSFrontBuffer, 0, DDFLIP_WAIT) == DDERR_SURFACELOST)
		{
			RestoreSurfaces();
			m_pDDSFrontBuffer->lpVtbl->Flip(m_pDDSFrontBuffer, 0, DDFLIP_WAIT);
			return;
		}
	}
	else
	{
		if (m_pDDSBackBuffer->lpVtbl->BltFast(m_pDDSBackBuffer, 0, 0, m_pDDSOffScreenBuffer, &r, DDBLTFAST_WAIT) == DDERR_SURFACELOST)
		{
			RestoreSurfaces();
			m_pDDSBackBuffer->lpVtbl->BltFast(m_pDDSBackBuffer, 0, 0, m_pDDSOffScreenBuffer, &r, DDBLTFAST_WAIT);
			return;
		}
	}

	if (initialized)
		return;
	
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);

	m_pDDSOffScreenBuffer->lpVtbl->Lock(m_pDDSOffScreenBuffer, NULL, &ddsd, DDLOCK_WAIT, NULL);

	vid.buffer = vid.conbuffer = vid.direct = (pixel_t*)ddsd.lpSurface;
	vid.rowbytes = ddsd.dwLinearSize;
}

bool CVideoMode_SoftwareFullScreen::DDRAW_PreInit( void )
{
	// Create the main DirectDraw object
	if (DirectDrawCreate(NULL, &m_pDirectDraw, NULL) != DD_OK)
	{
		return false;
	}

	vmode_t* mode = videomode->GetCurrentMode();
	if (!mode)
	{
		return false;
	}

	if (r_pixbytes == 1)
		mode->bpp = 16;
	else
		mode->bpp = r_pixbytes * 8;

	if (m_bWindowed)
	{
		m_pDirectDraw->lpVtbl->SetCooperativeLevel(m_pDirectDraw, NULL, DDSCL_NORMAL);
		m_pDirectDraw->lpVtbl->FlipToGDISurface(m_pDirectDraw);
		return true;
	}

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window*)game->GetMainWindow(), &wmInfo);

	m_pDirectDraw->lpVtbl->SetCooperativeLevel(m_pDirectDraw, wmInfo.info.win.window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);

	if (m_pDirectDraw->lpVtbl->SetDisplayMode(m_pDirectDraw, mode->width, mode->height, mode->bpp) != DD_OK)
	{
		return false;
	}

	// Set fullscreen
	SDL_SetWindowFullscreen((SDL_Window*)game->GetMainWindow(), SDL_WINDOW_FULLSCREEN);

	m_pDirectDraw->lpVtbl->FlipToGDISurface(m_pDirectDraw);
	return true;
}

void CVideoMode_SoftwareFullScreen::ReleaseVideo( void )
{
	if (m_pDirectDraw)
		m_pDirectDraw->lpVtbl->RestoreDisplayMode(m_pDirectDraw);

	if (!m_bWindowed)
	{
		SDL_SetWindowFullscreen((SDL_Window*)game->GetMainWindow(), 0);
	}
}

int CVideoMode_SoftwareFullScreen::MaxBitsPerPixel( void )
{
	return 16;
}

void CVideoMode_SoftwareFullScreen::DDRAW_Shutdown( void )
{
	if (m_pDirectDraw)
	{
		m_pDirectDraw->lpVtbl->FlipToGDISurface(m_pDirectDraw);
		ReleaseSurfaces();

		m_pDirectDraw->lpVtbl->Release(m_pDirectDraw);
		m_pDirectDraw = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Builds our DDRAW stuff
//-----------------------------------------------------------------------------
bool CVideoMode_SoftwareFullScreen::DDRAW_Init( void )
{
	HRESULT ddrval;
	DDSURFACEDESC ddsd;
	DDSCAPS ddscaps;

	if (!m_pDirectDraw)
	{
		return false;
	}

	vmode_t* mode = GetCurrentMode();

	if (r_pixbytes == 1)
		mode->bpp = 16;
	else
		mode->bpp = r_pixbytes * 8;

	ReleaseSurfaces();

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window*)game->GetMainWindow(), &wmInfo);

	ddrval = m_pDirectDraw->lpVtbl->SetCooperativeLevel(m_pDirectDraw, wmInfo.info.win.window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);
	if (ddrval != DD_OK)
	{
		if (ddrval == DDERR_HWNDALREADYSET)
		{
			DestroyWindow(wmInfo.info.win.window);
			MessageBox(NULL, "Restart the game to use your newly selected mode.", "Error", 0);
		}

		Con_Printf(const_cast<char*>("%s\n"), directdraw->ErrorToString(ddrval));
		return false;
	}

	/*
	** try changing the display mode normally
	*/
	ddrval = m_pDirectDraw->lpVtbl->SetDisplayMode(m_pDirectDraw, mode->width, mode->height, mode->bpp);
	if (ddrval != DD_OK)
	{
		Con_Printf(const_cast<char*>("display:%s\n"), directdraw->ErrorToString(ddrval));
		return false;
	}

	/*
	** create our front buffer
	*/
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 1;

	ddrval = m_pDirectDraw->lpVtbl->CreateSurface(m_pDirectDraw, &ddsd, &m_pDDSFrontBuffer, NULL);
	if (ddrval != DD_OK)
	{
		Con_Printf(const_cast<char*>("surface:%s\n"), directdraw->ErrorToString(ddrval));
		return false;
	}

	DDBLTFX ddbf;
	ddbf.dwSize = sizeof(ddbf);
	ddbf.dwFillColor = 0;
	m_pDDSFrontBuffer->lpVtbl->Blt(m_pDDSFrontBuffer, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbf);

	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	ddrval = m_pDDSFrontBuffer->lpVtbl->GetAttachedSurface(m_pDDSFrontBuffer, &ddscaps, &m_pDDSBackBuffer);
	vid.numpages = ddrval == DD_OK;

	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	ddsd.dwWidth = mode->width;
	ddsd.dwHeight = mode->height;

	ddrval = m_pDirectDraw->lpVtbl->CreateSurface(m_pDirectDraw, &ddsd, &m_pDDSOffScreenBuffer, NULL);
	if (ddrval != DD_OK)
	{
		Con_Printf(const_cast<char*>("%s\n"), directdraw->ErrorToString(ddrval));
		return false;
	}

	ddrval = m_pDDSOffScreenBuffer->lpVtbl->Lock(m_pDDSOffScreenBuffer, NULL, &ddsd, DDLOCK_WAIT, NULL);
	if (ddrval != DD_OK)
	{
		Con_Printf(const_cast<char*>("%s\n"), directdraw->ErrorToString(ddrval));
		return false;
	}

	DDPIXELFORMAT ddpf;
	ddpf.dwSize = sizeof(ddpf);

	ddrval = m_pDDSOffScreenBuffer->lpVtbl->GetPixelFormat(m_pDDSOffScreenBuffer, &ddpf);
	if (ddrval != DD_OK)
	{
		Con_Printf(const_cast<char*>("%s\n"), directdraw->ErrorToString(ddrval));
		return false;
	}

	if (r_pixbytes == 1)
	{
		// Figure out bpp (RGB555 or RGB565)
		vid.is15bit = ddpf.dwGBitMask == 0x3E0;
	}
	else
	{
		vid.is15bit = false;
	}

	// Initialize width/height/BPP for the global VID state
	vid.width = vid.conwidth = mode->width;
	vid.height = vid.conheight = mode->height;
	vid.bits = mode->bpp;

	vid.conrowbytes = ddsd.lPitch;
	vid.rowbytes = ddsd.dwLinearSize;
	vid.buffer = vid.conbuffer = vid.direct = (pixel_t*)ddsd.lpSurface;

	if (r_pixbytes == 1)
	{
		// Clear screen buffer
		memset(vid.buffer, 0, mode->height * mode->width * 2);
	}
	else
	{
		memset(vid.buffer, 0, mode->height * mode->width * r_pixbytes);
	}

	vid.direct = vid.buffer;
	vid.conbuffer = vid.buffer;
	vid.vidtype = VT_Software;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	return true;
}

void CVideoMode_SoftwareFullScreen::ReleaseSurfaces( void )
{
	if (m_pDDSOffScreenBuffer)
	{
		m_pDDSOffScreenBuffer->lpVtbl->Unlock(m_pDDSOffScreenBuffer, NULL);
		m_pDDSOffScreenBuffer->lpVtbl->Release(m_pDDSOffScreenBuffer);
		m_pDDSOffScreenBuffer = NULL;
	}
	
	if (m_pDDSFrontBuffer)
	{
		m_pDDSFrontBuffer->lpVtbl->DeleteAttachedSurface(m_pDDSFrontBuffer, 0, m_pDDSBackBuffer);

		m_pDDSBackBuffer->lpVtbl->Release(m_pDDSBackBuffer);
		m_pDDSBackBuffer = NULL;

		m_pDDSFrontBuffer->lpVtbl->Release(m_pDDSFrontBuffer);
		m_pDDSFrontBuffer = NULL;
	}
}

void CVideoMode_SoftwareFullScreen::RestoreSurfaces( void )
{
	if (!m_pDirectDraw)
		return;
	
	if (m_pDDSBackBuffer->lpVtbl->Restore(m_pDDSBackBuffer) != 0)
	{
		DDRAW_Init();
		m_pDDSBackBuffer->lpVtbl->Restore(m_pDDSBackBuffer);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Windowed Software Video Mode Class
//-----------------------------------------------------------------------------
class CVideoMode_SoftwareWindowed : public CVideoMode_Common
{
public:
	typedef CVideoMode_Common BaseClass;

	CVideoMode_SoftwareWindowed();
	~CVideoMode_SoftwareWindowed()
	{
		BaseClass::~CVideoMode_Common();
	}

	virtual const char* GetName( void ) { return "software"; }

	virtual bool		Init( void* pvInstance );
	virtual void		Shutdown( void );

	virtual void		FlipScreen( void );

private:
	bool				DIB_PreInit( void );
	bool				DIB_Init( void );
	void				DIB_Shutdown( void );

	struct dibinfo_t
	{
		BITMAPINFOHEADER	header;
		int					acolors[256];
	};

	HDC					ghdcDIB;				// global DC we're using
	HDC					m_hdcDIBSection;	// DC compatible with DIB section
	HBITMAP				m_hDIBSection;		// DIB section
	void*				m_pDIBBase;			// DIB base pointer, NOT used directly for rendering!
	dibinfo_t			m_pDIBHeader;
};

CVideoMode_SoftwareWindowed::CVideoMode_SoftwareWindowed()
{
	ghdcDIB = NULL;
	m_hdcDIBSection = NULL;
	m_hDIBSection = NULL;
	m_pDIBBase = NULL;

	m_bWindowed = true;
}

bool CVideoMode_SoftwareWindowed::Init( void* pvInstance )
{
	bool bret = BaseClass::Init(pvInstance);

	if (bret)
	{
		bret = DIB_PreInit();

		if (bret)
		{
			bret = DIB_Init();
		}
	}

	SetInitialized(bret);
	
	return bret;
}

bool CVideoMode_SoftwareWindowed::DIB_PreInit( void )
{
	return true;
}

void CVideoMode_SoftwareWindowed::Shutdown( void )
{
	DIB_Shutdown();

	CVideoMode_Common::Shutdown();
}

void CVideoMode_SoftwareWindowed::FlipScreen( void )
{
	int i, h;

	assert(ghdcDIB);

	h = vid.height - 1;

	for (i = 0; i < (int)vid.height; i++)
	{
		BitBlt(ghdcDIB, 0, i, vid.width, 1, m_hdcDIBSection, 0, h, SRCCOPY);
		h--;
	}
}

void CVideoMode_SoftwareWindowed::DIB_Shutdown( void )
{
	if (m_hdcDIBSection)
	{
		DeleteDC(m_hdcDIBSection);
		m_hdcDIBSection = NULL;
	}

	if (m_hDIBSection)
	{
		DeleteObject(m_hDIBSection);
		m_hDIBSection = NULL;
		m_pDIBBase = NULL;
	}

	if (ghdcDIB)
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo((SDL_Window*)game->GetMainWindow(), &wmInfo);

		ReleaseDC(wmInfo.info.win.window, ghdcDIB);
		ghdcDIB = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Builds our DIB section
//-----------------------------------------------------------------------------
bool CVideoMode_SoftwareWindowed::DIB_Init(void)
{
	int		width, height, bpp;

	// Get the window from the game
	VideoMode_GetCurrentVideoMode(&width, &height, &bpp);

	// Initialize width/height/BPP for the global VID state
	vid.width = vid.conwidth = width;
	vid.height = vid.conheight = height;
	vid.bits = bpp;

	vid.buffer = NULL;
	vid.rowbytes = vid.conrowbytes = 0;

	vid.numpages = 1;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window*)game->GetMainWindow(), &wmInfo);

	/*
	** grab a DC
	*/
	ghdcDIB = GetDC(wmInfo.info.win.window);

	memset(&m_pDIBHeader, 0, sizeof(m_pDIBHeader));

	/*
	** fill in the BITMAPINFO struct
	*/
	m_pDIBHeader.header.biSize = sizeof(BITMAPINFOHEADER);
	m_pDIBHeader.header.biWidth = vid.width;
	m_pDIBHeader.header.biHeight = vid.height;
	m_pDIBHeader.header.biPlanes = 1;

	if (r_pixbytes == 1)
		m_pDIBHeader.header.biBitCount = 16;
	else
		m_pDIBHeader.header.biBitCount = r_pixbytes * 8;

	m_pDIBHeader.header.biCompression = BI_BITFIELDS;
	m_pDIBHeader.header.biSizeImage = 0;
	m_pDIBHeader.header.biXPelsPerMeter = 0;
	m_pDIBHeader.header.biYPelsPerMeter = 0;
	m_pDIBHeader.header.biClrUsed = 0;
	m_pDIBHeader.header.biClrImportant = 0;

	/*
	** fill in the palette
	*/
	if (r_pixbytes == 1)
	{
		m_pDIBHeader.acolors[0] = 63488;
		m_pDIBHeader.acolors[1] = 2016;
		m_pDIBHeader.acolors[2] = 31;
	}
	else
	{
		m_pDIBHeader.acolors[0] = 0xff0000;
		m_pDIBHeader.acolors[1] = 0x00ff00;
		m_pDIBHeader.acolors[2] = 0x0000ff;
	}

	/*
	** create the DIB section
	*/
	m_hDIBSection = ::CreateDIBSection(ghdcDIB, (BITMAPINFO*)&m_pDIBHeader, DIB_RGB_COLORS, (void**)(&m_pDIBBase), NULL, 0);

	if (!m_hDIBSection)
		return false;

	vid.buffer = (pixel_t*)m_pDIBBase;
	vid.is15bit = false;

	if (r_pixbytes == 1)
	{
		vid.rowbytes = 2 * vid.width;
		vid.conrowbytes = 2 * vid.width;

		/*
		** clear the DIB memory buffer
		*/
		memset(vid.buffer, 0xFF, vid.width * vid.height);
	}
	else
	{
		vid.rowbytes = r_pixbytes * vid.width;
		vid.conrowbytes = r_pixbytes * vid.width;

		/*
		** clear the DIB memory buffer
		*/
		memset(vid.buffer, 0xFF, vid.width * vid.height);
	}

	m_hdcDIBSection = CreateCompatibleDC(ghdcDIB);
	
	if (!m_hdcDIBSection)
		return false;

	SelectObject(m_hdcDIBSection, m_hDIBSection);

	vid.direct = vid.buffer;
	vid.conbuffer = vid.buffer;
	vid.vidtype = VT_Software;

	return true;
}

#endif

IVideoMode* videomode = NULL;

void VideoMode_Create( void )
{
	// Assume full screen
	bool windowed = false;

	// Get last setting
	windowed = registry->ReadInt("ScreenWindowed", 0) ? true : false;

	// Check for windowed mode command line override
	if (COM_CheckParm(const_cast<char*>("-sw")) ||
		COM_CheckParm(const_cast<char*>("-startwindowed")) ||
		COM_CheckParm(const_cast<char*>("-windowed")) ||
		COM_CheckParm(const_cast<char*>("-window")))
	{
		windowed = true;
	}
	// Check for fullscreen override
	else if (COM_CheckParm(const_cast<char*>("-full")) ||
		COM_CheckParm(const_cast<char*>("-fullscreen")))
	{
		windowed = false;
	}

	// Store actual mode back out to registry
	registry->WriteInt("ScreenWindowed", windowed ? 1 : 0);

#if defined ( GLQUAKE )
	extern int gD3DMode;
	gD3DMode = 0;

	registry->ReadInt("EngineD3D", 0);

	if (!COM_CheckParm(const_cast<char*>("-d3d")))
		COM_CheckParm(const_cast<char*>("-gl"));

	videomode = new CVideoMode_OpenGL(windowed);
#else
	// Windowed Software mode
	if (windowed)
	{
		videomode = new CVideoMode_SoftwareWindowed();
	}
	else
	{
		videomode = new CVideoMode_SoftwareFullScreen();
	}
#endif
}

void VideoMode_SwitchMode( int hardware, int windowed )
{
	if (hardware)
	{
		registry->WriteString("EngineDLL", "hw.dll");
		registry->WriteInt("EngineD3D", hardware == VM_D3D);
	}
	else
	{
		registry->WriteString("EngineDLL", "sw.dll");
		registry->WriteInt("EngineD3D", VM_Software);
	}

	registry->WriteInt("ScreenWindowed", windowed);
}

void VideoMode_SetVideoMode( int width, int height, int bpp )
{
	registry->WriteInt("ScreenWidth", width);
	registry->WriteInt("ScreenHeight", height);
	registry->WriteInt("ScreenBPP", bpp);
}

void VideoMode_GetVideoModes( vmode_t** liststart, int* count )
{
	*count = videomode->GetModeCount();
	*liststart = videomode->GetMode(0);
}

void VideoMode_GetCurrentVideoMode( int* wide, int* tall, int* bpp )
{
	vmode_t* mode = videomode->GetCurrentMode();
	if (!mode)
		return;

	if (wide)
	{
		*wide = mode->width;
	}
	if (tall)
	{
		*tall = mode->height;
	}
	if (bpp)
	{
		*bpp = mode->bpp;
	}
}

void VideoMode_GetCurrentRenderer( char* name, int namelen, int* windowed, int* hdmodels, int* addons_folder, int* vid_level )
{
	if (namelen > 0 && name)
	{
		strncpy(name, videomode->GetName(), namelen);
	}

	if (windowed)
	{
		*windowed = videomode->IsWindowedMode();
	}
	if (hdmodels)
	{
		*hdmodels = registry->ReadInt("hdmodels", 1) > 0;
	}
	if (addons_folder)
	{
		*addons_folder = registry->ReadInt("addons_folder", 0) > 0;
	}
	if (vid_level)
	{
		*vid_level = registry->ReadInt("vid_level", 0);
	}
}

bool VideoMode_IsWindowed( void )
{
	return videomode && videomode->IsWindowedMode();
}

void VideoMode_RestoreVideo( void )
{
	videomode->RestoreVideo();
}
