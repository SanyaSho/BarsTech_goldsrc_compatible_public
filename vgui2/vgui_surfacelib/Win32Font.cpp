#include <tier0/platform.h>

#include <vgui/ISurface.h>

//#include "Win32Font.h"
#include "vguifont.h"

static OSVERSIONINFO s_OsVersionInfo;
static bool s_bOsVersionInitialized = false;
bool s_bSupportsUnicode = false;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWin32Font::CWin32Font() : m_ExtendedABCWidthsCache(256, 0, &ExtendedABCWidthsCacheLessFunc)
{
	m_szName[0] = 0;
	m_iTall = 0;
	m_iWeight = 0;
	m_iHeight = 0;
	m_iAscent = 0;
	m_iFlags = 0;
	m_iMaxCharWidth = 0;
	m_hFont = NULL;
	m_hDC = NULL;
	m_hDIB = NULL;
	m_bAntiAliased = false;
	m_bUnderlined = false;
	m_iBlur = 0;
	m_iScanLines = 0;
	m_bRotary = false;
	m_bAdditive = false;
	m_rgiBitmapSize[0] = m_rgiBitmapSize[1] = 0;

#if defined( _X360 )
	Q_memset(m_ABCWidthsCache, 0, sizeof(m_ABCWidthsCache));
#endif

	m_ExtendedABCWidthsCache.EnsureCapacity(128);

	if (!s_bOsVersionInitialized)
	{
		// get the operating system version
		s_bOsVersionInitialized = true;
		memset(&s_OsVersionInfo, 0, sizeof(s_OsVersionInfo));
		s_OsVersionInfo.dwOSVersionInfoSize = sizeof(s_OsVersionInfo);
		GetVersionEx(&s_OsVersionInfo);

		if (s_OsVersionInfo.dwMajorVersion >= 5)
		{
			s_bSupportsUnicode = true;
		}
		else
		{
			s_bSupportsUnicode = false;
		}
	}
}

CWin32Font::~CWin32Font()
{
	if( m_hFont )
	{
		DeleteObject( m_hFont );
	}

	if( m_hDC )
	{
		DeleteObject( m_hDC );
	}

	if (m_hDIB)
	{
		DeleteObject(m_hDIB);
	}

	//TODO: free gaussian distribution array - Solokiller
}

//-----------------------------------------------------------------------------
// Purpose: Font iteration callback function
//			used to determine whether or not a font exists on the system
//-----------------------------------------------------------------------------
extern bool g_bFontFound = false;
static int CALLBACK FontEnumProc(
	const LOGFONT *lpelfe,		// logical-font data
	const TEXTMETRIC *lpntme,	// physical-font data
	DWORD FontType,				// type of font
	LPARAM lParam)				// application-defined data
{
	g_bFontFound = true;
	return 0;
}

bool CWin32Font::ExtendedABCWidthsCacheLessFunc(const abc_cache_t& lhs, const abc_cache_t& rhs)
{
	return lhs.wch < rhs.wch;
}

bool CWin32Font::Create( const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	strncpy( m_szName, windowsFontName, ARRAYSIZE( m_szName ) );
	m_szName[ ARRAYSIZE( m_szName ) - 1 ] = '\0';

	m_bAntiAliased = ( flags & vgui2::ISurface::FONTFLAG_ANTIALIAS ) != 0;
	m_bUnderlined = ( flags & vgui2::ISurface::FONTFLAG_UNDERLINE ) != 0;
	m_iTall = tall;
	m_iDropShadowOffset = ( flags & vgui2::ISurface::FONTFLAG_DROPSHADOW ) != 0;
	m_bRotary = ( flags & vgui2::ISurface::FONTFLAG_ROTARY ) != 0;
	m_iWeight = weight;
	m_iBlur = blur;
	m_iFlags = flags;
	m_iScanLines = scanlines;
//	m_iHeight = tall;

	int charset = (flags & vgui2::ISurface::FONTFLAG_SYMBOL) ? SYMBOL_CHARSET : ANSI_CHARSET;

	// hack for japanese win98 support
	if( !stricmp( windowsFontName, "win98japanese" ) )
	{
		// use any font that contains the japanese charset
		charset = SHIFTJIS_CHARSET;
		strncpy( m_szName, "Tahoma", sizeof( m_szName ) );
	}

	// create our windows device context
	m_hDC = CreateCompatibleDC( NULL );

	// see if the font exists on the system
	LOGFONT logfont;
	logfont.lfCharSet = DEFAULT_CHARSET;
	logfont.lfPitchAndFamily = 0;
	strcpy(logfont.lfFaceName, m_szName);
	g_bFontFound = false;
	::EnumFontFamiliesEx(m_hDC, &logfont, &FontEnumProc, 0, 0);
	if (!g_bFontFound)
	{
		// needs to go to a fallback
		m_szName[0] = '\0';
		return false;
	}

	//At least one font exists with our desired settings.

	m_hFont = ::CreateFontA(tall, 0, 0, 0, 
								m_iWeight, 
								flags & vgui2::ISurface::FONTFLAG_ITALIC, 
								flags & vgui2::ISurface::FONTFLAG_UNDERLINE, 
								flags & vgui2::ISurface::FONTFLAG_STRIKEOUT, 
								charset, 
								OUT_DEFAULT_PRECIS, 
								CLIP_DEFAULT_PRECIS, 
								m_bAntiAliased ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY, 
								DEFAULT_PITCH | FF_DONTCARE, 
								windowsFontName);

	if( !m_hFont )
	{
		m_szName[ 0 ] = '\0';
		return false;
	}

	// set as the active font
	::SetMapMode(m_hDC, MM_TEXT);
	::SelectObject(m_hDC, m_hFont);
	::SetTextAlign(m_hDC, TA_LEFT | TA_TOP | TA_UPDATECP);

	// get info about the font
	::TEXTMETRIC tm;
	memset(&tm, 0, sizeof(tm));
	if (!GetTextMetrics(m_hDC, &tm))
	{
	}

	m_iHeight = tm.tmHeight + m_iDropShadowOffset;
	m_iMaxCharWidth = tm.tmMaxCharWidth;
	m_iAscent = tm.tmAscent;

	// code for rendering to a bitmap
	m_rgiBitmapSize[0] = tm.tmMaxCharWidth;
	m_rgiBitmapSize[1] = tm.tmHeight + m_iDropShadowOffset;

	::BITMAPINFOHEADER header;
	memset(&header, 0, sizeof(header));
	header.biSize = sizeof(header);
	header.biWidth = m_rgiBitmapSize[0];
	header.biHeight = -m_rgiBitmapSize[1];
	header.biPlanes = 1;
	header.biBitCount = 32;
	header.biCompression = BI_RGB;

	m_hDIB = ::CreateDIBSection(m_hDC, (BITMAPINFO*)&header, DIB_RGB_COLORS, (void**)(&m_pBuf), NULL, 0);
	::SelectObject(m_hDC, m_hDIB);

	// get char spacing
	// a is space before character (can be negative)
	// b is the width of the character
	// c is the space after the character
	memset(m_ABCWidthsCache, 0, sizeof(m_ABCWidthsCache));
	ABC abc[ABCWIDTHS_CACHE_SIZE];
	if (::GetCharABCWidthsW(m_hDC, 0, ABCWIDTHS_CACHE_SIZE - 1, &abc[0]) || ::GetCharABCWidthsA(m_hDC, 0, ABCWIDTHS_CACHE_SIZE - 1, &abc[0]))
	{
		// copy out into our formated structure
		for (int i = 0; i < ABCWIDTHS_CACHE_SIZE; i++)
		{
			m_ABCWidthsCache[i].a = abc[i].abcA - m_iBlur;
			m_ABCWidthsCache[i].b = abc[i].abcB + (m_iBlur * 2) + m_iDropShadowOffset;
			m_ABCWidthsCache[i].c = abc[i].abcC - m_iBlur - m_iDropShadowOffset;
		}
	}
	else
	{
		// since that failed, it must be fixed width, zero everything so a and c will be zeros, then
		// fill b with the value from TEXTMETRIC
		for (int i = 0; i < ABCWIDTHS_CACHE_SIZE; i++)
		{
			// failed to get width, just use the average width
			m_ABCWidthsCache[i].b = (char)tm.tmAveCharWidth;
		}
	}

	if( m_iBlur > 1 )
	{
		m_pGaussianDistribution = new float[ ( 2 * m_iBlur ) + 1 ];

		//Generate Gaussian blur.
		if( !( m_iBlur & 0x40000000 ) )
		{
			//Seems to be this: https://en.wikipedia.org/wiki/Gaussian_blur
			const double flBlur = static_cast<double>( m_iBlur ) * 0.683;
			const double flBlurSquared = flBlur * flBlur;
			const double flBlurDouble = 2 * flBlur;

			//2 * pi * flBlurSquared
			const double x = 1.0 / sqrt( 6.28 * flBlurSquared );

			for( int i = 0; i <= 2 * m_iBlur; ++i )
			{
				m_pGaussianDistribution[ i ] = x * pow(
					2.7, //Mathematical constant 'e'
					static_cast<double>( -( ( i - m_iBlur ) * ( i - m_iBlur ) ) ) / flBlurDouble );
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Adds center line to fond
//-----------------------------------------------------------------------------
void CWin32Font::ApplyRotaryEffectToTexture( int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, byte* rgba )
{
	if ( !m_bRotary )
		return;

	int y = rgbaTall * 0.5;

	unsigned char *line = &rgba[(rgbaX + ((y + rgbaY) * rgbaWide)) * 4];

	// Draw a line down middle
	for (int x = 0; x < rgbaWide; x++, line+=4)
	{
		line[0] = 127;
		line[1] = 127;
		line[2] = 127;
		line[3] = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds scanlines to the texture
//-----------------------------------------------------------------------------
void CWin32Font::ApplyScanlineEffectToTexture( int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, byte* rgba )
{
	if (m_iScanLines < 2)
		return;

	// darken all the areas except the scanlines
	for (int y = 0; y < rgbaTall; y++)
	{
		// skip the scan lines
		if (y % m_iScanLines == 0)
			continue;

		unsigned char *line = &rgba[(rgbaX + ((y + rgbaY) * rgbaWide)) * 4];

		// darken the other lines
		for (int x = 0; x < rgbaWide; x++, line += 4)
		{
			line[0] *= 0.7;
			line[1] *= 0.7;
			line[2] *= 0.7;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a dropshadow the the font texture
//-----------------------------------------------------------------------------
void CWin32Font::ApplyDropShadowToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, int charWide, int charTall, unsigned char *rgba)
{
	// walk the original image from the bottom up
	// shifting it down and right, and turning it black (the dropshadow)
	for (int y = charTall - 1; y >= m_iDropShadowOffset; y--)
	{
		for (int x = charWide - 1; x >= m_iDropShadowOffset; x--)
		{
			unsigned char *dest = &rgba[(x + rgbaX + ((rgbaY + y) * rgbaWide)) * 4];
			if (dest[3] == 0)
			{
				// there is nothing in this spot, copy in the dropshadow
				unsigned char *src = &rgba[(x + rgbaX - m_iDropShadowOffset + ((rgbaY + y - m_iDropShadowOffset) * rgbaWide)) * 4];
				dest[0] = 0;
				dest[1] = 0;
				dest[2] = 0;
				dest[3] = src[3];
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: blurs the texture
//-----------------------------------------------------------------------------
void CWin32Font::ApplyGaussianBlurToTexture( int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, byte* rgba )
{
	// generate the gaussian field
	if (!m_pGaussianDistribution)
		return;

	// alloc a new rgba
	unsigned char *src = (unsigned char *)_alloca(rgbaWide * rgbaTall * 4); // /ALIGNATION by 4/
	// copy in
	memcpy(src, rgba, rgbaWide * rgbaTall * 4);
	// incrementing destination pointer
	unsigned char *dest = rgba;
	for (int y = 0; y < rgbaTall; y++)
	{
		for (int x = 0; x < rgbaWide; x++)
		{
			// scan the source pixel
			int r = 0, g = 0, b = 0, a = 0;

			float accum = 0.0f;

			// scan the positive x direction
			int maxX = min(x + m_iBlur, rgbaWide);
			int minX = max(x - m_iBlur, 0);
			for (int i = minX; i <= maxX; i++)
			{
				int maxY = min(y + m_iBlur, rgbaTall - 1);
				int minY = max(y - m_iBlur, 0);
				for (int j = minY; j <= maxY; j++)
				{
					unsigned char *srcPos = src + ((i + (j * rgbaWide)) * 4);

					// muliply by the value matrix
					float weight = m_pGaussianDistribution[i - x + m_iBlur];
					float weight2 = m_pGaussianDistribution[j - y + m_iBlur];
					accum += (srcPos[0] * (weight * weight2));
				}
			}

			// all the values are the same for fonts, just use the calculated alpha
			r = g = b = a = (int)accum;

			int pixelCount = 1;
			dest[0] = min(r / pixelCount, 255);
			dest[1] = min(g / pixelCount, 255);
			dest[2] = min(b / pixelCount, 255);
			dest[3] = min(a / pixelCount, 255);
			dest[3] = 255;

			// move to the next
			dest += 4;
		}
	}
}

void CWin32Font::GetCharABCWidths( int ch, int& a, int& b, int& c )
{
	//Use cached entries
	if (ch < ABCWIDTHS_CACHE_SIZE)
	{
		// use the cache entry
		a = m_ABCWidthsCache[ch].a;
		b = m_ABCWidthsCache[ch].b;
		c = m_ABCWidthsCache[ch].c;
	}
	else
	{

		// look for it in the cache
		abc_cache_t finder = { (wchar_t)ch };

		unsigned short i = m_ExtendedABCWidthsCache.Find(finder);
		if (m_ExtendedABCWidthsCache.IsValidIndex(i))
		{
			a = m_ExtendedABCWidthsCache[i].abc.a;
			b = m_ExtendedABCWidthsCache[i].abc.b;
			c = m_ExtendedABCWidthsCache[i].abc.c;
			return;
		}

		// not in the cache, get from windows (this call is a little slow)
		ABC abc;
		if (::GetCharABCWidthsW(m_hDC, ch, ch, &abc) || ::GetCharABCWidthsA(m_hDC, ch, ch, &abc))
		{
			a = abc.abcA;
			b = abc.abcB;
			c = abc.abcC;
		}
		else
		{
			// wide character version failed, try the old api function
			SIZE size;
			char mbcs[6] = { 0 };
			wchar_t wch = ch;
			::WideCharToMultiByte(CP_ACP, 0, &wch, 1, mbcs, sizeof(mbcs), NULL, NULL);
			if (::GetTextExtentPoint32(m_hDC, mbcs, strlen(mbcs), &size))
			{
				a = c = 0;
				b = size.cx;
			}
			else
			{
				// failed to get width, just use the max width
				a = c = 0;
				b = m_iMaxCharWidth;
			}
		}

		// add to the cache
		finder.abc.a = a;
		finder.abc.b = b;
		finder.abc.c = c;
		m_ExtendedABCWidthsCache.Insert(finder);
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture
//-----------------------------------------------------------------------------
void CWin32Font::GetCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, byte* rgba )
{
	int a, b, c;
	GetCharABCWidths(ch, a, b, c);

	// set us up to render into our dib
	::SelectObject(m_hDC, m_hFont);

	int wide = b;
	if (m_bUnderlined)
	{
		wide += (a + c);
	}

	int tall = m_iHeight;
	GLYPHMETRICS glyphMetrics;
	MAT2 mat2 = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
	int bytesNeeded = 0;

	bool bShouldAntialias = m_bAntiAliased;

	// filter out 
	if (ch > 0x00FF)
	{
		bShouldAntialias = false;
	}

	// only antialias latin characters, since it essentially always fails for asian characters
	if (bShouldAntialias)
	{
		// try and get the glyph directly
		::SelectObject(m_hDC, m_hFont);
		bytesNeeded = ::GetGlyphOutline(m_hDC, ch, GGO_GRAY8_BITMAP, &glyphMetrics, 0, NULL, &mat2);
	}

	if (bytesNeeded > 0)
	{
		// take it
		unsigned char *lpbuf = (unsigned char *)_alloca(bytesNeeded); // /alignation BY 4/
		::GetGlyphOutline(m_hDC, ch, GGO_GRAY8_BITMAP, &glyphMetrics, bytesNeeded, lpbuf, &mat2);

		// rows are on DWORD boundaries
		wide = glyphMetrics.gmBlackBoxX;
		while (wide % 4 != 0)
		{
			wide++;
		}

		// see where we should start rendering
		int pushDown = m_iAscent - glyphMetrics.gmptGlyphOrigin.y;

		// set where we start copying from
		int xstart = 0;

		// don't copy the first set of pixels if the antialiased bmp is bigger than the char width
		if ((int)glyphMetrics.gmBlackBoxX >= b + 2)
		{
			xstart = (glyphMetrics.gmBlackBoxX - b) / 2;
		}

		// iterate through copying the generated dib into the texture
		for (unsigned int j = 0; j < glyphMetrics.gmBlackBoxY; j++)
		{
			for (unsigned int i = xstart; i < glyphMetrics.gmBlackBoxX; i++)
			{
				int x = rgbaX + i - xstart;
				int y = rgbaY + j + pushDown;

				if ((x < rgbaWide) && (y < rgbaTall))
				{
					unsigned char grayscale = lpbuf[(j*wide + i)];

					float r, g, b, a;
					if (grayscale)
					{
						r = g = b = 1.0f;
						a = (grayscale + 0) / 64.0f;
						if (a > 1.0f) a = 1.0f;
					}
					else
					{
						r = g = b = a = 0.0f;
					}

					// Don't want anything drawn for tab characters.
					if (ch == '\t')
					{
						r = g = b = 0;
					}

					unsigned char *dst = &rgba[(y*rgbaWide + x) * 4];
					dst[0] = (unsigned char)(r * 255.0f);
					dst[1] = (unsigned char)(g * 255.0f);
					dst[2] = (unsigned char)(b * 255.0f);
					dst[3] = (unsigned char)(a * 255.0f);
				}
			}
		}
	}
	else
	{
		// use render-to-bitmap to get our font texture
		::SetBkColor(m_hDC, RGB(0, 0, 0));
		::SetTextColor(m_hDC, RGB(255, 255, 255));
		::SetBkMode(m_hDC, OPAQUE);
		if (m_bUnderlined)
		{
			::MoveToEx(m_hDC, 0, 0, NULL);
		}
		else
		{
			::MoveToEx(m_hDC, -a, 0, NULL);
		}

		// render the character
		wchar_t wch = (wchar_t)ch;

		if (s_bSupportsUnicode)
		{
			// just use the unicode renderer
			::ExtTextOutW(m_hDC, 0, 0, 0, NULL, &wch, 1, NULL);
		}
		else
		{
			// clear the background first (it may not get done automatically in win98/ME
			RECT rect = { 0, 0, wide, tall };
			::ExtTextOut(m_hDC, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);

			// convert the character using the current codepage
			char mbcs[6] = { 0 };
			::WideCharToMultiByte(CP_ACP, 0, &wch, 1, mbcs, sizeof(mbcs), NULL, NULL);
			::ExtTextOutA(m_hDC, 0, 0, 0, NULL, mbcs, strlen(mbcs), NULL);
		}

		::SetBkMode(m_hDC, TRANSPARENT);

		if (wide > m_rgiBitmapSize[0])
		{
			wide = m_rgiBitmapSize[0];
		}
		if (tall > m_rgiBitmapSize[1])
		{
			tall = m_rgiBitmapSize[1];
		}

		// iterate through copying the generated dib into the texture
		for (int j = 0; j < tall; j++)
		{
			for (int i = 0; i < wide - m_iDropShadowOffset; i++)
			{
				int x = rgbaX + i;
				int y = rgbaY + j;
				if ((x < rgbaWide) && (y < rgbaTall))
				{
					unsigned char *src = &m_pBuf[(j*m_rgiBitmapSize[0] + i) * 4];

					float r = (src[0]) / 255.0f;
					float g = (src[1]) / 255.0f;
					float b = (src[2]) / 255.0f;

					// Don't want anything drawn for tab characters.
					if (ch == '\t')
					{
						r = g = b = 0;
					}

					unsigned char *dst = &rgba[(y*rgbaWide + x) * 4];
					dst[0] = (unsigned char)(r * 255.0f);
					dst[1] = (unsigned char)(g * 255.0f);
					dst[2] = (unsigned char)(b * 255.0f);
					dst[3] = (unsigned char)((r * 0.34f + g * 0.55f + b * 0.11f) * 255.0f);
				}
			}
		}

		if (m_iDropShadowOffset)
		{
			unsigned char *dst = &rgba[4 * (rgbaX + rgbaWide * (m_iHeight - 1))];

			for (int iX = wide; iX; --iX, dst += 4)
			{
				dst[0] = 0;
				dst[1] = 0;
				dst[2] = 0;
				dst[3] = 0;
			}
		}
	}

	if (m_iDropShadowOffset)
	{
		ApplyDropShadowToTexture(rgbaX, rgbaY, rgbaWide, rgbaTall, wide, tall, rgba);
	}

	ApplyGaussianBlurToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );
	ApplyScanlineEffectToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );
	ApplyRotaryEffectToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CWin32Font::IsEqualTo( const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	return !stricmp( windowsFontName, m_szName ) &&
		m_iTall == tall &&
		m_iWeight == weight &&
		m_iBlur == blur &&
		m_iFlags == flags;
}
