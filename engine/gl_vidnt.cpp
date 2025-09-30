// gl_vidnt.cpp -- NT GL vid component

#ifdef WIN32
#include "winsani_in.h"
#include <winlite.h>
#include "winsani_out.h"
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GL/glew.h>

#include "quakedef.h"
#include "tier1/strtools.h"
#include "vid.h"
#include "gl_hw.h"
#include "screen.h"
#include "sys_getmodes.h"
#include "host_cmd.h"
#include "draw.h"
#include "DetailTexture.h"


struct FBO_Container_t
{
	GLuint s_hBackBufferFBO;
	GLuint s_hBackBufferCB;
	GLuint s_hBackBufferDB;
	GLuint s_hBackBufferTex;
};

static FBO_Container_t s_MSAAFBO;
static FBO_Container_t s_BackBufferFBO;

BOOL gfMiniDriver = FALSE;


qboolean		scr_skipupdate;
qboolean		scr_skiponeupdate;

int gGLHardwareType = GL_HW_UNKNOWN;

const char* gl_vendor;
const char* gl_renderer;
const char* gl_version;
const char* gl_extensions;
int bDoMSAAFBO = TRUE;
int bDoScaledFBO = TRUE;
static qboolean s_bSupportsBlitTexturing;
qboolean g_bSupportsNPOTTextures;

cvar_t gl_ztrick = { const_cast<char*>("gl_ztrick_old"), const_cast<char*>("0") };

// Enable/Disable VSync
cvar_t gl_vsync = { const_cast<char*>("gl_vsync"), const_cast<char*>("1"), FCVAR_ARCHIVE };

int gD3DMode;

float		gldepthmin, gldepthmax;

int			window_center_x, window_center_y;
RECT		window_rect;


//int		texture_mode = GL_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int		texture_mode = GL_LINEAR;
//int		texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int		texture_mode = GL_LINEAR_MIPMAP_LINEAR;

int gl_mtexable = 0;
int TEXTURE0_SGIS = GL_ASYNC_READ_PIXELS_SGIX;
int TEXTURE1_SGIS = GL_MAX_ASYNC_TEX_IMAGE_SGIX;
int TEXTURE2_SGIS = GL_MAX_ASYNC_DRAW_PIXELS_SGIX;

extern void (*VID_FlipScreen)(void);

extern int gl_filter_min;
extern SDL_Window* pmainwindow;

static int s_bEnforceAspect = TRUE;
static float s_fXMouseAspectAdjustment = 1.0, s_fYMouseAspectAdjustment = 1.0;

float GetXMouseAspectRatioAdjustment( void )
{
	return s_fXMouseAspectAdjustment;
}

float GetYMouseAspectRatioAdjustment( void )
{
	return s_fYMouseAspectAdjustment;
}

void CheckTextureExtensions( void )
{
	qboolean	texture_ext = FALSE;

	if (gl_extensions && (Q_strstr(gl_extensions, "GL_EXT_paletted_texture") &&
		Q_strstr(gl_extensions, "GL_EXT_shared_texture_palette")))
	{
		qglColorTableEXT = (decltype(qglColorTableEXT))SDL_GL_GetProcAddress("glColorTableEXT");
		Con_Printf(const_cast<char*>("Found paletted texture extension.\n"));
	}
	else
	{
		qglColorTableEXT = NULL;
	}

	if (gl_extensions && Q_strstr(gl_extensions, "GL_EXT_texture_object "))
	{
		qglBindTexture = (decltype(qglBindTexture))SDL_GL_GetProcAddress("glBindTextureEXT");
		texture_ext = TRUE;

		if (!qglBindTexture)
		{
			Sys_Error("GetProcAddress for BindTextureEXT failed");
			return;
		}
	}
}

void CheckArrayExtensions( void )
{
	char* tmp;

	/* check for texture extension */
	tmp = (char*)(qglGetString(GL_EXTENSIONS));
	while (*tmp)
	{
		if (Q_strncmp(tmp, "GL_EXT_vertex_array", Q_strlen("GL_EXT_vertex_array")) == 0)
		{
			return;
		}
		tmp++;
	}

	Sys_Error("Vertex array extension not present");
}

extern GLenum oldtarget;

void CheckMultiTextureExtensions( void )
{
	if (!gl_extensions)
	{
		Con_DPrintf(const_cast<char*>("NO Multitexture extensions found.\n"));
		return;
	}

	if (Q_strstr(gl_extensions, "GL_ARB_multitexture "))
	{
		Con_DPrintf(const_cast<char*>("ARB Multitexture extensions found.\n"));
		qglMTexCoord2fSGIS = (decltype(qglMTexCoord2fSGIS))SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		qglSelectTextureSGIS = (decltype(qglSelectTextureSGIS))SDL_GL_GetProcAddress("glActiveTextureARB");
		TEXTURE0_SGIS = GL_TEXTURE0;
		TEXTURE1_SGIS = GL_TEXTURE1;
		TEXTURE2_SGIS = GL_TEXTURE2;
		oldtarget = TEXTURE0_SGIS;

		GL_SelectTexture(TEXTURE0_SGIS);

		gl_mtexable = 2;

		if (Q_strstr(gl_extensions, "GL_ARB_texture_env_combine ") ||
			Q_strstr(gl_extensions, "GL_EXT_texture_env_combine "))
		{
			GLint num;
			qglGetIntegerv(GL_MAX_TEXTURE_UNITS, &num);

			if (num > 2)
			{
				Con_DPrintf(const_cast<char*>("%d texture units.  Detail texture supported.\n"), num);

				// Set max texture units
				gl_mtexable = num;

				// Initialize Detail Textures
				DT_Initialize();
			}
		}
	}
	else if (Q_strstr(gl_extensions, "GL_SGIS_multitexture "))
	{
		Con_DPrintf(const_cast<char*>("Multitexture extensions found.\n"));
		qglMTexCoord2fSGIS = (decltype(qglMTexCoord2fSGIS))SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
		qglSelectTextureSGIS = (decltype(qglSelectTextureSGIS))SDL_GL_GetProcAddress("glSelectTextureSGIS");
		TEXTURE0_SGIS = QGL_TEXTURE0_SGIS;
		TEXTURE1_SGIS = QGL_TEXTURE1_SGIS;
		TEXTURE2_SGIS = QGL_TEXTURE2_SGIS;
		oldtarget = TEXTURE0_SGIS;

		gl_mtexable = 2;
		GL_SelectTexture(TEXTURE0_SGIS);
	}
	else
	{
		Con_DPrintf(const_cast<char*>("NO Multitexture extensions found.\n"));
		return;
	}
}

/*
===============
CheckATINPatchExtensions
===============
*/
void CheckATINPatchExtensions( void )
{
	extern qboolean atismoothing;
	atismoothing = FALSE;
}

void GL_Config( void )
{
	char lowerCase[256];

	Cbuf_InsertText(const_cast<char*>("exec hw/opengl.cfg\n"));

	Q_strncpy(lowerCase, gl_renderer, sizeof(lowerCase) - 1);
	lowerCase[sizeof(lowerCase) - 1] = 0;

	strlwr(lowerCase);

	if (Q_strstr(gl_vendor, "3Dfx"))
	{
		gGLHardwareType = GL_HW_3Dfx;

		if (Q_strstr(gl_renderer, "Voodoo(tm)"))
		{
			Cbuf_InsertText(const_cast<char*>("exec hw/3DfxVoodoo1.cfg\n"));
		}
		else if (Q_strstr(gl_renderer, "Voodoo^2"))
		{
			Cbuf_InsertText(const_cast<char*>("exec hw/3DfxVoodoo2.cfg\n"));
		}
		else
		{
			Cbuf_InsertText(const_cast<char*>("exec hw/3Dfx.cfg\n"));
		}
	}
	else if (Q_strstr(gl_vendor, "NVIDIA"))
	{
		if (Q_strstr(gl_renderer, "RIVA 128"))
		{
			gGLHardwareType = GL_HW_RIVA128;
			Cbuf_InsertText(const_cast<char*>("exec hw/riva128.cfg\n"));
		}
		else if (Q_strstr(gl_renderer, "TNT"))
		{
			gGLHardwareType = GL_HW_RIVATNT;
			Cbuf_InsertText(const_cast<char*>("exec hw/rivaTNT.cfg\n"));
		}
		else if (strstr(gl_renderer, "Quadro") || strstr(gl_renderer, "GeForce"))
		{
			gGLHardwareType = GL_HW_GEFORCE;
			Cbuf_InsertText(const_cast<char*>("exec hw/geforce.cfg\n"));
		}
	}
	else if (Q_strstr(lowerCase, "riva tnt") || Q_strstr(lowerCase, "velocity 4400"))
	{
		gGLHardwareType = GL_HW_RIVATNT;
		Cbuf_InsertText(const_cast<char*>("exec hw/rivaTNT.cfg\n"));
	}
	else if (Q_strstr(gl_vendor, "PCX2"))
	{
		gGLHardwareType = GL_HW_PCX2;
		Cbuf_InsertText(const_cast<char*>("exec hw/PowerVRPCX2.cfg\n"));
	}
	else if (Q_strstr(gl_vendor, "PowerVR"))
	{
		gGLHardwareType = GL_HW_PVRSG;
		Cbuf_InsertText(const_cast<char*>("exec hw/PowerVRSG.cfg\n"));
	}
	else if (Q_strstr(gl_vendor, "V2200"))
	{
		gGLHardwareType = GL_HW_RENDITIONV2200;
		Cbuf_InsertText(const_cast<char*>("exec hw/V2200.cfg\n"));
	}
	else if (Q_strstr(gl_vendor, "3Dlabs"))
	{
		gGLHardwareType = GL_HW_3DLABS;
		Cbuf_InsertText(const_cast<char*>("exec hw/3Dlabs.cfg\n"));
	}
	else if (Q_strstr(gl_vendor, "Matrox"))
	{
		gGLHardwareType = GL_HW_UNKNOWN;
		Cbuf_InsertText(const_cast<char*>("exec hw/matrox.cfg\n"));
	}
	else if (Q_strstr(gl_vendor, "ATI")
		&& (Q_strstr(gl_renderer, "Rage") ||
			Q_strstr(gl_renderer, "RAGE"))
		&& Q_strstr(gl_renderer, "128"))
	{
		gGLHardwareType = GL_HW_UNKNOWN;
		Cbuf_InsertText(const_cast<char*>("exec hw/ATIRage128.cfg\n"));
	}
	else
	{
		gGLHardwareType = GL_HW_UNKNOWN;

		if (Q_strstr(gl_renderer, "Matrox") && Q_strstr(gl_renderer, "G200"))
		{
			Cbuf_InsertText(const_cast<char*>("exec hw/G200d3d.cfg\n"));
		}
		else if (Q_strstr(gl_renderer, "ATI")
			&& (Q_strstr(gl_renderer, "Rage") ||
				Q_strstr(gl_renderer, "RAGE"))
			&& Q_strstr(gl_renderer, "128"))
		{
			Cbuf_InsertText(const_cast<char*>("exec hw/ATIRage128d3d.cfg\n"));
		}
		else if (Q_strstr(gl_vendor, "NVIDIA"))
		{
			Cbuf_InsertText(const_cast<char*>("exec hw/nvidiad3d.cfg\n"));
		}
	}
}

/*
=================
GL_Init

=================
*/
void GL_Init( void )
{
	int iShowExtensions;

	gl_vendor = (const char*)qglGetString(GL_VENDOR);
	if (!gl_vendor)
		Sys_Error("Failed to query gl vendor string");

	Con_DPrintf(const_cast<char*>("GL_VENDOR: %s\n"), gl_vendor);
	gl_renderer = (const char*)qglGetString(GL_RENDERER);
	Con_DPrintf(const_cast<char*>("GL_RENDERER: %s\n"), gl_renderer);

	gl_version = (const char*)qglGetString(GL_VERSION);
	Con_DPrintf(const_cast<char*>("GL_VERSION: %s\n"), gl_version);
	gl_extensions = (const char*)qglGetString(GL_EXTENSIONS);
	g_bSupportsNPOTTextures = FALSE;

	iShowExtensions = COM_CheckParm(const_cast<char*>("-glext"));
	if (iShowExtensions)
		Con_DPrintf(const_cast<char*>("GL_EXTENSIONS: %s\n"), gl_extensions);

	int r, g, b, a, depth;

	if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r))
	{
		const char* pErr = SDL_GetError();
		r = 0;
		Con_DPrintf(const_cast<char*>("Failed to get GL RED size (%s)\n"), pErr);
	}

	if (SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g))
	{
		const char* pErr = SDL_GetError();
		g = 0;
		Con_DPrintf(const_cast<char*>("Failed to get GL GREEN size (%s)\n"), pErr);
	}

	if (SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b))
	{
		const char* pErr = SDL_GetError();
		b = 0;
		Con_DPrintf(const_cast<char*>("Failed to get GL BLUE size (%s)\n"), pErr);
	}

	if (SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a))
	{
		const char* pErr = SDL_GetError();
		a = 0;
		Con_DPrintf(const_cast<char*>("Failed to get GL ALPHA size (%s)\n"), pErr);
	}

	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth))
	{
		const char* pErr = SDL_GetError();
		depth = 0;
		Con_DPrintf(const_cast<char*>("Failed to get GL DEPTH size (%s)\n"), pErr);
	}

	Con_DPrintf(const_cast<char*>("GL_SIZES:  r:%d g:%d b:%d a:%d depth:%d\n"), r, g, b, a, depth);

	// Check detail texture support
	CheckTextureExtensions();
	CheckMultiTextureExtensions();
	CheckATINPatchExtensions();

	if (gl_extensions && Q_strstr(gl_extensions, "GL_ARB_texture_non_power_of_two"))
	{
		g_bSupportsNPOTTextures = TRUE;
	}

	qglClearColor(1.0, 0.0, 0.0, 0.0);
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);

	qglAlphaFunc(GL_NOTEQUAL, 0.0);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel(GL_FLAT);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	GL_Config();
}

/*
=================
GL_BeginRendering

=================
*/
static qboolean vsync;

void GL_BeginRendering( int* x, int* y, int* width, int* height )
{
	extern cvar_t gl_clear;

	*x = *y = 0;
	if (VideoMode_IsWindowed())
	{
		*width = window_rect.right - window_rect.left;
		*height = window_rect.bottom - window_rect.top;
	}
	else
	{
		VideoMode_GetCurrentVideoMode(width, height, NULL);
	}

	vid.width = vid.conwidth = *width;
	vid.height = vid.conheight = *height;

	if (gl_vsync.value != vsync)
	{
		SDL_GL_SetSwapInterval(gl_vsync.value > 0);
		vsync = gl_vsync.value != 0;
	}

	if (s_BackBufferFBO.s_hBackBufferFBO)
	{
		if (s_MSAAFBO.s_hBackBufferFBO)
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, s_MSAAFBO.s_hBackBufferFBO);
		else
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO);

		glClearColor(0, 0, 0, 1);

		if (gl_clear.value)
		{
			glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		}
		else
		{
			qglClear(GL_DEPTH_BUFFER_BIT);
		}
	}

	GLimp_LogNewFrame();
}


void GL_EndRendering( void )
{
	if (s_BackBufferFBO.s_hBackBufferFBO)
	{
		int srcWide, srcTall;
		VideoMode_GetCurrentVideoMode(&srcWide, &srcTall, NULL);

		int dstX1, dstX2, dstY1, dstY2;
		float fSrcAspect, fDstAspect;

		if (s_MSAAFBO.s_hBackBufferFBO)
		{
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO);
			qglBindFramebufferEXT(GL_READ_FRAMEBUFFER, s_MSAAFBO.s_hBackBufferFBO);
			qglBlitFramebufferEXT(0, 0, srcWide, srcTall, 0, 0, srcWide, srcTall, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		}

		qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
		qglBindFramebufferEXT(GL_READ_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO);

		glClearColor(0, 0, 0, 0);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		int nWindowWidth = window_rect.right - window_rect.left;
		int nWindowHeight = window_rect.bottom - window_rect.top;

		s_fYMouseAspectAdjustment = 1.0;
		s_fXMouseAspectAdjustment = 1.0;

		dstX1 = 0;
		dstX2 = 0;
		dstY1 = nWindowWidth;
		dstY2 = nWindowHeight;

		fSrcAspect = (float)srcWide / (float)srcTall;
		fDstAspect = (float)dstY1 / (float)dstY2;

		if (s_bEnforceAspect)
		{
			if (fDstAspect > fSrcAspect)
			{
				float fDesiredWidth = (float)dstY2 * fSrcAspect;
				float fDiff = (float)dstY1 - fDesiredWidth;

				dstX1 = fDiff / 2.0;
				dstY1 = (float)dstY1 - (fDiff / 2.0);
				s_fXMouseAspectAdjustment = fDstAspect / fSrcAspect;
			}
			else if (fDstAspect < fSrcAspect)
			{
				float fDesiredWidth = 1.0 / fSrcAspect * (float)dstY1;
				float fDiff = (float)dstY2 - fDesiredWidth;

				s_fYMouseAspectAdjustment = fSrcAspect / fDstAspect;
				dstX2 = fDiff / 2.0;
				dstY2 = (float)dstY2 - (fDiff / 2.0);
			}
		}

		if (s_bSupportsBlitTexturing)
		{
			qglBlitFramebufferEXT(0, 0, srcWide, srcTall, dstX1, dstX2, dstY1, dstY2, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		}
		else
		{
			glDisable(GL_BLEND);
			glDisable(GL_LIGHTING);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA_TEST);
			glDisable(GL_CULL_FACE);

			qglMatrixMode(GL_PROJECTION);
			qglPushMatrix();
			glLoadIdentity();
			glOrtho(0, nWindowWidth, nWindowHeight, 0, -1, 1);

			qglMatrixMode(GL_MODELVIEW);
			qglPushMatrix();
			glLoadIdentity();
			qglViewport(0, 0, nWindowWidth, nWindowHeight);

			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			qglColor4f(1, 1, 1, 1);

			glEnable(GL_TEXTURE_RECTANGLE);
			glBindTexture(GL_TEXTURE_RECTANGLE, s_BackBufferFBO.s_hBackBufferTex);

			glBegin(GL_QUADS);

			glTexCoord2f(0, srcTall);
			glVertex3f(dstX1, dstX2, 0);

			glTexCoord2f(srcWide, srcTall);
			glVertex3f(dstY1, dstX2, 0);

			glTexCoord2f(srcWide, 0);
			glVertex3f(dstY1, dstY2, 0);

			glTexCoord2f(0, 0);
			glVertex3f(dstX1, dstY2, 0);

			glEnd();

			glBindTexture(GL_TEXTURE_RECTANGLE, 0);

			qglMatrixMode(GL_PROJECTION);
			qglPopMatrix();

			qglMatrixMode(GL_MODELVIEW);
			qglPopMatrix();

			glDisable(GL_TEXTURE_RECTANGLE);
		}

		qglBindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
	}

	VID_FlipScreen();
}

/*
================
VID_ShiftPalette
================
*/
void VID_ShiftPalette( unsigned short* palette )
{
}


void FreeFBOObjects( void )
{
	if (s_MSAAFBO.s_hBackBufferFBO)
		qglDeleteFramebuffersEXT(1, &s_MSAAFBO.s_hBackBufferFBO);
	s_MSAAFBO.s_hBackBufferFBO = 0;
	if (s_MSAAFBO.s_hBackBufferCB)
		qglDeleteRenderbuffersEXT(1, &s_MSAAFBO.s_hBackBufferCB);
	s_MSAAFBO.s_hBackBufferCB = 0;
	if (s_MSAAFBO.s_hBackBufferDB)
		qglDeleteRenderbuffersEXT(1, &s_MSAAFBO.s_hBackBufferDB);
	s_MSAAFBO.s_hBackBufferDB = 0;
	if (s_MSAAFBO.s_hBackBufferTex)
		qglDeleteTextures(1, &s_MSAAFBO.s_hBackBufferTex);
	s_MSAAFBO.s_hBackBufferTex = 0;
	if (s_BackBufferFBO.s_hBackBufferFBO)
		qglDeleteFramebuffersEXT(1, &s_BackBufferFBO.s_hBackBufferFBO);
	s_BackBufferFBO.s_hBackBufferFBO = 0;
	if (s_BackBufferFBO.s_hBackBufferCB)
		qglDeleteRenderbuffersEXT(1, &s_BackBufferFBO.s_hBackBufferCB);
	s_BackBufferFBO.s_hBackBufferCB = 0;
	if (s_BackBufferFBO.s_hBackBufferDB)
		qglDeleteRenderbuffersEXT(1, &s_BackBufferFBO.s_hBackBufferDB);
	s_BackBufferFBO.s_hBackBufferDB = 0;
	if (s_BackBufferFBO.s_hBackBufferTex)
		qglDeleteTextures(1, &s_BackBufferFBO.s_hBackBufferTex);
	s_BackBufferFBO.s_hBackBufferTex = 0;
}

#if defined (_WIN32)
//==========================================================================


BOOL bSetupPixelFormat( HDC hDC )
{
	static PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),		// size of this pfd
		1,									// version number
		PFD_DRAW_TO_WINDOW 					// support window
		| PFD_SUPPORT_OPENGL 				// support OpenGL
		| PFD_DOUBLEBUFFER,					// double buffered
		PFD_TYPE_RGBA,						// RGBA type
		32,									// 32-bit color depth
		0, 0, 0, 0, 0, 0,					// color bits ignored
		0,									// no alpha buffer
		0,									// shift bit ignored
		0,									// no accumulation buffer
		0, 0, 0, 0, 						// accum bits ignored
		32,									// 32-bit z-buffer	
		0,									// no stencil buffer
		0,									// no auxiliary buffer
		PFD_MAIN_PLANE,						// main layer
		0,									// reserved
		0, 0, 0								// layer masks ignored
	};

	static qboolean bInitialized = FALSE;

	if (bInitialized)
		return TRUE;

	bInitialized = TRUE;

	int pixelformat;

	if (gfMiniDriver)
	{
		if ((pixelformat = qwglChoosePixelFormat(hDC, &pfd)) == 0)
		{
			MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
			return FALSE;
		}

		if (!qwglSetPixelFormat(hDC, pixelformat, &pfd))
		{
			MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
			return FALSE;
		}

		qwglDescribePixelFormat(hDC, pixelformat, sizeof(pfd), &pfd);
		return TRUE;
	}

	if ((pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0)
	{
		MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
	{
		MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	DescribePixelFormat(hDC, pixelformat, sizeof(pfd), &pfd);

	// If not a generic Software driver or a hardware accelerated generic driver, we can use OpenGL
	if (!(pfd.dwFlags & PFD_GENERIC_FORMAT) || (pfd.dwFlags & PFD_GENERIC_ACCELERATED))
		return TRUE;

	return FALSE;
}
#endif

int GL_SetMode( SDL_Window* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, int fD3D, const char* pszDriver, const char* pszCmdLine )
{
	int iVidCmd;
	int vid_level;

	gfMiniDriver = FALSE;

	if (pmaindc)
		*pmaindc = NULL;
	if (pbaseRC)
		*pbaseRC = NULL;

	*pbaseRC = NULL;
	*pmaindc = NULL;

	if (!gD3DMode)
	{
		s_bEnforceAspect = COM_CheckParm(const_cast<char*>("-stretchaspect")) == 0;

		iVidCmd = COM_CheckParm(const_cast<char*>("-nomsaa"));
		if (iVidCmd)
			bDoMSAAFBO = FALSE;

		iVidCmd = COM_CheckParm(const_cast<char*>("-nofbo"));
		if (iVidCmd)
			bDoScaledFBO = FALSE;

		vid_level = Host_GetVideoLevel();
		if (vid_level > 0)
		{
			bDoScaledFBO = FALSE;
		}

#ifdef WIN32
		int iFreq = COM_CheckParm(const_cast<char*>("-freq"));

		if (iFreq && bDoScaledFBO)
		{
			if (VideoMode_IsWindowed())
			{
				Con_DPrintf(const_cast<char*>("Setting frequency in windowed mode is unsupported\n"));
			}
			else
			{
				DEVMODEA DevMode;
				memset(&DevMode, 0, sizeof(DevMode));

				DevMode.dmSize = sizeof(DevMode);

				EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &DevMode);

				DevMode.dmDisplayFrequency = atoi(com_argv[iFreq + 1]);
				DevMode.dmFields |= DM_DISPLAYFREQUENCY;

				Con_DPrintf(const_cast<char*>("Setting monitor frequency to %d\n"), DevMode.dmDisplayFrequency);

				if (ChangeDisplaySettingsEx(NULL, &DevMode, NULL, CDS_FULLSCREEN, NULL) &&
					ChangeDisplaySettings(&DevMode, CDS_FULLSCREEN))
				{
					Con_DPrintf(const_cast<char*>("Frequency %d is not supported by your monitor\n"), DevMode.dmDisplayFrequency);
				}
			}
		}
#endif

		QGL_Init(pszDriver, pszCmdLine);

		gl_extensions = (const char*)qglGetString(GL_EXTENSIONS);

		s_bSupportsBlitTexturing = FALSE;
		if (gl_extensions && Q_strstr(gl_extensions, "GL_EXT_framebuffer_multisample_blit_scaled"))
		{
			s_bSupportsBlitTexturing = TRUE;
		}

		iVidCmd = COM_CheckParm(const_cast<char*>("-directblit"));
		if (iVidCmd)
			s_bSupportsBlitTexturing = TRUE;
		iVidCmd = COM_CheckParm(const_cast<char*>("-nodirectblit"));
		if (iVidCmd)
			s_bSupportsBlitTexturing = FALSE;

		if (!qglGenFramebuffersEXT || !qglBindFramebufferEXT || !qglBlitFramebufferEXT)
			bDoScaledFBO = FALSE;

		if (gl_extensions &&
			!Q_strstr(gl_extensions, "GL_ARB_texture_rectangle") &&
			!Q_strstr(gl_extensions, "GL_NV_texture_rectangle"))
		{
			bDoScaledFBO = FALSE;
		}

		if (VideoMode_IsWindowed())
		{
			s_MSAAFBO.s_hBackBufferFBO = 0;
			return TRUE;
		}

		s_MSAAFBO.s_hBackBufferFBO = 0;
		s_MSAAFBO.s_hBackBufferCB = 0;
		s_MSAAFBO.s_hBackBufferDB = 0;
		s_MSAAFBO.s_hBackBufferTex = 0;
		s_BackBufferFBO.s_hBackBufferFBO = 0;	
		s_BackBufferFBO.s_hBackBufferCB = 0;
		s_BackBufferFBO.s_hBackBufferDB = 0;
		s_BackBufferFBO.s_hBackBufferTex = 0;

		int wide, tall;
		VideoMode_GetCurrentVideoMode(&wide, &tall, NULL);

		if (qglRenderbufferStorageMultisampleEXT && bDoMSAAFBO > 0 &&
			gl_extensions && Q_strstr(gl_extensions, "GL_EXT_framebuffer_multisample"))
		{
			qglGenFramebuffersEXT(1, &s_MSAAFBO.s_hBackBufferFBO);
			qglBindFramebufferEXT(GL_FRAMEBUFFER, s_MSAAFBO.s_hBackBufferFBO);

			qglGenRenderbuffersEXT(1, &s_MSAAFBO.s_hBackBufferCB);
			qglBindRenderbufferEXT(GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferCB);

			qglRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, 4, GL_RGBA8, wide, tall);
			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferCB);

			qglGenRenderbuffersEXT(1, &s_MSAAFBO.s_hBackBufferDB);
			qglBindRenderbufferEXT(GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferDB);

			qglRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, wide, tall);

			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_MSAAFBO.s_hBackBufferDB);

			if (qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				if (s_MSAAFBO.s_hBackBufferFBO)
					qglDeleteFramebuffersEXT(1, &s_MSAAFBO.s_hBackBufferFBO);
				s_MSAAFBO.s_hBackBufferFBO = 0;
				if (s_MSAAFBO.s_hBackBufferCB)
					qglDeleteRenderbuffersEXT(1, &s_MSAAFBO.s_hBackBufferCB);
				s_MSAAFBO.s_hBackBufferCB = 0;
				if (s_MSAAFBO.s_hBackBufferDB)
					qglDeleteRenderbuffersEXT(1, &s_MSAAFBO.s_hBackBufferDB);
				s_MSAAFBO.s_hBackBufferDB = 0;
				if (s_MSAAFBO.s_hBackBufferTex)
					qglDeleteTextures(1, &s_MSAAFBO.s_hBackBufferTex);
				s_MSAAFBO.s_hBackBufferTex = 0;
				Con_DPrintf(const_cast<char*>("Error initializing MSAA frame buffer\n"));
				s_MSAAFBO.s_hBackBufferFBO = 0;
			}
		}
		else
		{
			Con_DPrintf(const_cast<char*>("MSAA backbuffer rendering disabled.\n"));
			s_MSAAFBO.s_hBackBufferFBO = 0;
		}

		if (bDoScaledFBO > 0)
		{
			if (s_MSAAFBO.s_hBackBufferFBO)
				glEnable(GL_MULTISAMPLE);

			glEnable(GL_TEXTURE_RECTANGLE);
			qglGenFramebuffersEXT(1, &s_BackBufferFBO.s_hBackBufferFBO);
			qglBindFramebufferEXT(GL_FRAMEBUFFER, s_BackBufferFBO.s_hBackBufferFBO);

			if (!s_MSAAFBO.s_hBackBufferFBO)
			{
				qglGenRenderbuffersEXT(1, &s_BackBufferFBO.s_hBackBufferDB);
				qglBindRenderbufferEXT(GL_RENDERBUFFER, s_BackBufferFBO.s_hBackBufferDB);
				qglRenderbufferStorageEXT(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, wide, tall);
				qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_BackBufferFBO.s_hBackBufferDB);
			}

			glGenTextures(1, &s_BackBufferFBO.s_hBackBufferTex);

			glBindTexture(GL_TEXTURE_RECTANGLE, s_BackBufferFBO.s_hBackBufferTex);
			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

			glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, wide, tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, s_BackBufferFBO.s_hBackBufferTex, 0);

			glBindTexture(GL_TEXTURE_RECTANGLE, 0);

			glDisable(GL_TEXTURE_RECTANGLE);
		}

		if (!bDoScaledFBO || qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			int iFreq;
			int refresh = 0;

			FreeFBOObjects();

			iFreq = COM_CheckParm(const_cast<char*>("-freq"));
			if (iFreq)
			{
				refresh = atoi(com_argv[iFreq + 1]);
				Con_DPrintf(const_cast<char*>("Setting refresh rate to %dHz\n"), refresh);
			}

			SDL_DisplayMode requestedMode;
			SDL_DisplayMode mode;

			requestedMode.w = wide;
			requestedMode.h = tall;
			requestedMode.refresh_rate = refresh;
			requestedMode.format = 0;

			if (!SDL_GetClosestDisplayMode(0, &requestedMode, &mode))
			{
				Sys_Error("Error initializing Main frame buffer\n");
			}

			Con_DPrintf(const_cast<char*>("Setting closest display mode: width:%d height:%d freq:%d format:%u\n"),
				mode.w, mode.h, mode.refresh_rate, mode.format);

			if (bDoScaledFBO)
				Con_DPrintf(const_cast<char*>("FBO backbuffer rendering disabled due to create error.\n"));
			else
				Con_DPrintf(const_cast<char*>("FBO backbuffer rendering disabled.\n"));

			SDL_SetWindowDisplayMode((SDL_Window*)mainwindow, &mode);

			bNeedsFullScreenModeSwitch = TRUE;
			VideoMode_RestoreVideo();
		}

		if (bDoScaledFBO > 0)
		{
			qglBindFramebufferEXT(GL_FRAMEBUFFER, 0);
		}

		return TRUE;
	}

#ifdef WIN32
	//QGL_D3DInit();

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window*)mainwindow, &wmInfo);

	HDC hDC = GetDC(wmInfo.info.win.window);

	*pmaindc = hDC;

	s_BackBufferFBO.s_hBackBufferFBO = 0;

	if (!bSetupPixelFormat(hDC))
	{
		return FALSE;
	}

	HGLRC context = qwglCreateContext(hDC);

	*pbaseRC = context;

	if (!context)
		return FALSE;

	return qwglMakeCurrent(hDC, context) != FALSE;
#else
	return TRUE;
#endif
}

void GL_Shutdown( SDL_Window* hwnd, HDC hdc, HGLRC hglrc )
{
	FreeFBOObjects();
}

/*
================
VID_Init
================
*/
int VID_Init( unsigned short* palette )
{
	int iVidCmd;

	Cvar_RegisterVariable(&gl_ztrick);
	Cvar_RegisterVariable(&gl_vsync);

	iVidCmd = COM_CheckParm(const_cast<char*>("-gl_log"));
	if (iVidCmd)
	{
		Cmd_AddCommand(const_cast<char*>("gl_log"), GLimp_EnableLogging);
	}

	return TRUE;
}

/*
===================
VID_TakeSnapshot

Write vid.buffer out as a windows bitmap file
*/
void VID_TakeSnapshot( const char* pFilename )
{
	int				i;
	FileHandle_t	fp;
	BITMAPFILEHEADER hdr;
	BITMAPINFOHEADER bi;
	int				imageSize;

	unsigned char* hp = NULL;
	unsigned char b;
	unsigned char* pBlue = NULL;
	unsigned char* pRed = NULL;

	int width, height;
	if (bDoScaledFBO > 0 || VideoMode_IsWindowed())
	{
		width = window_rect.right - window_rect.left;
		height = window_rect.bottom - window_rect.top;
	}
	else
	{
		width = vid.width;
		height = vid.height;
	}

	imageSize = width * height;

	fp = FS_OpenPathID(pFilename, "wb", "GAMECONFIG");
	if (!fp)
		Sys_Error("Couldn't create file for snapshot.\n");

	// file header
	hdr.bfType = 0x4D42;	// 'BM'
	hdr.bfSize = 3 * imageSize + sizeof(hdr) + sizeof(bi);
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = sizeof(hdr) + sizeof(bi);

	if (FS_Write(&hdr, sizeof(hdr), 1, fp) != sizeof(hdr))
		Sys_Error("Couldn't write file header to snapshot.\n");

	bi.biSize = sizeof(bi);
	bi.biWidth = width;
	bi.biHeight = height;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = 0;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	if (FS_Write(&bi, sizeof(bi), 1, fp) != sizeof(bi))
		Sys_Error("Couldn't write bitmap header to snapshot.\n");

	hp = (unsigned char*)Mem_Malloc(3 * imageSize + 4);

	if (!hp)
		Sys_Error("Couldn't allocate bitmap header to snapshot.\n");

	qglPixelStorei(GL_PACK_ALIGNMENT, 1);
	qglReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, hp);

	// swap RGB to BGR
	for (i = 0; i < imageSize; i++)
	{
		b = hp[0];
		hp[0] = hp[2];
		hp[2] = b;

		hp += 3;
	}

	// write to the file
	for (i = 0; i < height; i++)
	{
		int newWidth = 3 * width;

		if (newWidth + (4 - newWidth % 4) % 4 != FS_Write(hp, newWidth + (4 - newWidth % 4) % 4, 1, fp))
			Sys_Error("Couldn't write bitmap data snapshot.\n");

		hp += newWidth;
	}

	Mem_Free(hp);

	// close the file
	FS_Close(fp);
}


void VID_TakeSnapshotRect( const char* pFilename, int x, int y, int w, int h )
{
	int				i;
	FileHandle_t	fp;
	BITMAPFILEHEADER hdr;
	BITMAPINFOHEADER bi;
	int				imageSize;

	unsigned char* hp = NULL;
	unsigned char b;
	unsigned char* pBlue = NULL;
	unsigned char* pRed = NULL;

	imageSize = h * w;

	fp = FS_OpenPathID(pFilename, "wb", "GAMECONFIG");
	if (!fp)
		Sys_Error("Couldn't create file for snapshot.\n");

	// file header
	hdr.bfType = 0x4D42;	// 'BM'
	hdr.bfSize = 3 * imageSize + sizeof(hdr) + sizeof(bi);
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = sizeof(hdr) + sizeof(bi);

	if (FS_Write(&hdr, sizeof(hdr), 1, fp) != sizeof(hdr))
		Sys_Error("Couldn't write file header to snapshot.\n");

	bi.biSize = sizeof(bi);
	bi.biWidth = w;
	bi.biHeight = h;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = 0;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	if (FS_Write(&bi, sizeof(bi), 1, fp) != sizeof(bi))
		Sys_Error("Couldn't write bitmap header to snapshot.\n");

	hp = (unsigned char*)Mem_Malloc(3 * imageSize);

	if (!hp)
		Sys_Error("Couldn't allocate bitmap header to snapshot.\n");

	qglPixelStorei(GL_PACK_ALIGNMENT, 1);
	qglReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, hp);

	// swap RGB to BGR
	for (i = 0; i < imageSize; i++)
	{
		b = hp[0];
		hp[0] = hp[2];
		hp[2] = b;

		hp += 3;
	}

	// write to the file
	for (i = 0; i < h; i++)
	{
		int newWidth = 3 * w;

		if (newWidth + (4 - newWidth % 4) % 4 != FS_Write(hp, newWidth + (4 - newWidth % 4) % 4, 1, fp))
			Sys_Error("Couldn't write bitmap data snapshot.\n");

		hp += newWidth;
	}

	Mem_Free(hp);

	// close the file
	FS_Close(fp);
}

void VID_WriteBuffer( const char* pFilename )
{
	static char basefilename[MAX_PATH];
	static int filenumber;

	char filename[MAX_PATH];

	if (pFilename)
	{
		Q_strncpy(basefilename, pFilename, sizeof(basefilename) - 1);
		filename[sizeof(basefilename) - 1] = 0;

		filenumber = 1;
	}

	snprintf(filename, sizeof(filename) - 1, "%s%05d.bmp", basefilename, filenumber);
	filename[sizeof(filename) - 1] = 0;

	VID_TakeSnapshot(filename);

	filenumber++;
}

void VID_UpdateWindowVars( RECT* prc, int x, int y )
{
	window_rect = *prc;

	if (pmainwindow)
	{
		int w, h;
		SDL_GetWindowSize((SDL_Window*)pmainwindow, &w, &h);

		int x, y;
		SDL_GetWindowPosition((SDL_Window*)pmainwindow, &x, &y);

		prc->right = x + w;
		prc->bottom = y + h;
		prc->top = y;
		prc->left = x;

		window_center_x = x + w / 2;
		window_center_y = y + h / 2;
	}
	else
	{
		window_center_x = x;
		window_center_y = y;
	}
}

int VID_AllocBuffers( void )
{
	return TRUE;
}